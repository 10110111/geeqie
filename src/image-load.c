/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "image-load.h"

#include "exif.h"
#include "filedata.h"
#include "ui_fileops.h"
#include "gq-marshal.h"

#include <fcntl.h>
#include <sys/mman.h>

#define IMAGE_LOADER_READ_BUFFER_SIZE_DEFAULT 	4096
#define IMAGE_LOADER_IDLE_READ_LOOP_COUNT_DEFAULT 	1


/**************************************************************************************/
/* image loader class */


enum {
	SIGNAL_AREA_READY = 0,
	SIGNAL_ERROR,
	SIGNAL_DONE,
	SIGNAL_PERCENT,
	SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = { 0 };

static void image_loader_init(GTypeInstance *instance, gpointer g_class);
static void image_loader_class_init(ImageLoaderClass *class);
static void image_loader_finalize(GObject *object);
static void image_loader_stop(ImageLoader *il);

GType image_loader_get_type(void)
{
	static GType type = 0;
	if (type == 0) 
		{
		static const GTypeInfo info = {
			sizeof(ImageLoaderClass),
			NULL,   /* base_init */
			NULL,   /* base_finalize */
			(GClassInitFunc)image_loader_class_init, /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			sizeof(ImageLoader),
			0,      /* n_preallocs */
			(GInstanceInitFunc)image_loader_init, /* instance_init */
			NULL	/* value_table */
			};
		type = g_type_register_static(G_TYPE_OBJECT, "ImageLoaderType", &info, 0);
		}
	return type;
}

static void image_loader_init(GTypeInstance *instance, gpointer g_class)
{
	ImageLoader *il = (ImageLoader *)instance;

	il->pixbuf = NULL;
	il->idle_id = 0;
	il->idle_priority = G_PRIORITY_DEFAULT_IDLE;
	il->done = FALSE;
	il->loader = NULL;

	il->bytes_read = 0;
	il->bytes_total = 0;

	il->idle_done_id = 0;

	il->idle_read_loop_count = IMAGE_LOADER_IDLE_READ_LOOP_COUNT_DEFAULT;
	il->read_buffer_size = IMAGE_LOADER_READ_BUFFER_SIZE_DEFAULT;
	il->mapped_file = NULL;

	il->requested_width = 0;
	il->requested_height = 0;
	il->shrunk = FALSE;

	il->can_destroy = TRUE;

#ifdef HAVE_GTHREAD
	il->data_mutex = g_mutex_new();
	il->can_destroy_cond = g_cond_new();
#endif
	DEBUG_1("new image loader %p, bufsize=%" G_GSIZE_FORMAT " idle_loop=%u", il, il->read_buffer_size, il->idle_read_loop_count);
}

static void image_loader_class_init(ImageLoaderClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);

//	gobject_class->set_property = image_loader_set_property;
//	gobject_class->get_property = image_loader_get_property;

	gobject_class->finalize = image_loader_finalize;


	signals[SIGNAL_AREA_READY] =
		g_signal_new("area_ready",
			     G_OBJECT_CLASS_TYPE(gobject_class),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(ImageLoaderClass, area_ready),
			     NULL, NULL,
			     gq_marshal_VOID__INT_INT_INT_INT,
			     G_TYPE_NONE, 4,
			     G_TYPE_INT,
			     G_TYPE_INT,
			     G_TYPE_INT,
			     G_TYPE_INT);

	signals[SIGNAL_ERROR] =
		g_signal_new("error",
			     G_OBJECT_CLASS_TYPE(gobject_class),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(ImageLoaderClass, error),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

	signals[SIGNAL_DONE] =
		g_signal_new("done",
			     G_OBJECT_CLASS_TYPE(gobject_class),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(ImageLoaderClass, done),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

	signals[SIGNAL_PERCENT] =
		g_signal_new("percent",
			     G_OBJECT_CLASS_TYPE(gobject_class),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(ImageLoaderClass, percent),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__DOUBLE,
			     G_TYPE_NONE, 1,
			     G_TYPE_DOUBLE);

}

static void image_loader_finalize(GObject *object)
{
	ImageLoader *il = (ImageLoader *)object;

	image_loader_stop(il);

	if (il->error) DEBUG_1("%s", image_loader_get_error(il));

	DEBUG_1("freeing image loader %p bytes_read=%" G_GSIZE_FORMAT, il, il->bytes_read);

	if (il->idle_done_id)
		{
		g_source_remove(il->idle_done_id);
		il->idle_done_id = 0;
		}

	while (g_source_remove_by_user_data(il)) 
		{
		DEBUG_2("pending signals detected");
		}
	
	while (il->area_param_list) 
		{
		DEBUG_1("pending area_ready signals detected");
		while (g_source_remove_by_user_data(il->area_param_list->data)) {}
		g_free(il->area_param_list->data);
		il->area_param_list = g_list_delete_link(il->area_param_list, il->area_param_list);
		}

	while (il->area_param_delayed_list) 
		{
		g_free(il->area_param_delayed_list->data);
		il->area_param_delayed_list = g_list_delete_link(il->area_param_delayed_list, il->area_param_delayed_list);
		}

	if (il->pixbuf) g_object_unref(il->pixbuf);
	
	if (il->error) g_error_free(il->error);

	file_data_unref(il->fd);
#ifdef HAVE_GTHREAD
	g_mutex_free(il->data_mutex);
	g_cond_free(il->can_destroy_cond);
#endif
}

void image_loader_free(ImageLoader *il)
{
	if (!il) return;
	g_object_unref(G_OBJECT(il));
}


ImageLoader *image_loader_new(FileData *fd)
{
	ImageLoader *il;

	if (!fd) return NULL;

	il = (ImageLoader *) g_object_new(TYPE_IMAGE_LOADER, NULL);
	
	il->fd = file_data_ref(fd);
	
	return il;
}

/**************************************************************************************/
/* send signals via idle calbacks - the callback are executed in the main thread */

typedef struct _ImageLoaderAreaParam ImageLoaderAreaParam;
struct _ImageLoaderAreaParam {
	ImageLoader *il;
	guint x;
	guint y;
	guint w;
	guint h;
};


static gboolean image_loader_emit_area_ready_cb(gpointer data)
{
	ImageLoaderAreaParam *par = data;
	ImageLoader *il = par->il;
	g_signal_emit(il, signals[SIGNAL_AREA_READY], 0, par->x, par->y, par->w, par->h);
	g_mutex_lock(il->data_mutex);
	il->area_param_list = g_list_remove(il->area_param_list, par);
	g_free(par);
	g_mutex_unlock(il->data_mutex);
	
	return FALSE;
}

static gboolean image_loader_emit_done_cb(gpointer data)
{
	ImageLoader *il = data;
	g_signal_emit(il, signals[SIGNAL_DONE], 0);
	return FALSE;
}

static gboolean image_loader_emit_error_cb(gpointer data)
{
	ImageLoader *il = data;
	g_signal_emit(il, signals[SIGNAL_ERROR], 0);
	return FALSE;
}

static gboolean image_loader_emit_percent_cb(gpointer data)
{
	ImageLoader *il = data;
	g_signal_emit(il, signals[SIGNAL_PERCENT], 0, image_loader_get_percent(il));
	return FALSE;
}

/* DONE and ERROR are emited only once, thus they can have normal priority
   PERCENT and AREA_READY should be processed ASAP
*/

static void image_loader_emit_done(ImageLoader *il)
{
	g_idle_add_full(il->idle_priority, image_loader_emit_done_cb, il, NULL);
}

static void image_loader_emit_error(ImageLoader *il)
{
	g_idle_add_full(il->idle_priority, image_loader_emit_error_cb, il, NULL);
}

static void image_loader_emit_percent(ImageLoader *il)
{
	g_idle_add_full(G_PRIORITY_HIGH, image_loader_emit_percent_cb, il, NULL);
}

/* this function expects that il->data_mutex is locked by caller */
static void image_loader_emit_area_ready(ImageLoader *il, guint x, guint y, guint w, guint h)
{
	ImageLoaderAreaParam *par = g_new0(ImageLoaderAreaParam, 1);
	par->il = il;
	par->x = x;
	par->y = y;
	par->w = w;
	par->h = h;
	
	il->area_param_list = g_list_prepend(il->area_param_list, par);
	
	g_idle_add_full(G_PRIORITY_HIGH, image_loader_emit_area_ready_cb, par, NULL);
}

/**************************************************************************************/
/* the following functions may be executed in separate thread */

/* this function expects that il->data_mutex is locked by caller */
static void image_loader_queue_delayed_erea_ready(ImageLoader *il, guint x, guint y, guint w, guint h)
{
	ImageLoaderAreaParam *par = g_new0(ImageLoaderAreaParam, 1);
	par->il = il;
	par->x = x;
	par->y = y;
	par->w = w;
	par->h = h;
	
	il->area_param_delayed_list = g_list_prepend(il->area_param_delayed_list, par);
}



static gboolean image_loader_get_stopping(ImageLoader *il)
{
	gboolean ret;
	if (!il) return FALSE;

	g_mutex_lock(il->data_mutex);
	ret = il->stopping;
	g_mutex_unlock(il->data_mutex);

	return ret;
}


static void image_loader_sync_pixbuf(ImageLoader *il)
{
	GdkPixbuf *pb;
	
	g_mutex_lock(il->data_mutex);
	
	if (!il->loader) 
		{
		g_mutex_unlock(il->data_mutex);
		return;
		}

	pb = gdk_pixbuf_loader_get_pixbuf(il->loader);

	if (pb == il->pixbuf)
		{
		g_mutex_unlock(il->data_mutex);
		return;
		}

	if (il->pixbuf) g_object_unref(il->pixbuf);

	il->pixbuf = pb;
	if (il->pixbuf) g_object_ref(il->pixbuf);

	g_mutex_unlock(il->data_mutex);
}

static void image_loader_area_updated_cb(GdkPixbufLoader *loader,
				 guint x, guint y, guint w, guint h,
				 gpointer data)
{
	ImageLoader *il = data;

	if (!image_loader_get_pixbuf(il))
		{
		image_loader_sync_pixbuf(il);
		if (!image_loader_get_pixbuf(il))
			{
			log_printf("critical: area_ready signal with NULL pixbuf (out of mem?)\n");
			}
		}

	g_mutex_lock(il->data_mutex);
	if (il->delay_area_ready)
		image_loader_queue_delayed_erea_ready(il, x, y, w, h);
	else
		image_loader_emit_area_ready(il, x, y, w, h);
	g_mutex_unlock(il->data_mutex);
}

static void image_loader_area_prepared_cb(GdkPixbufLoader *loader, gpointer data)
{
	GdkPixbuf *pb;
	guchar *pix;
	size_t h, rs;
	
	/* a workaround for 
	   http://bugzilla.gnome.org/show_bug.cgi?id=547669 
	   http://bugzilla.gnome.org/show_bug.cgi?id=589334
	*/
	gchar *format = gdk_pixbuf_format_get_name(gdk_pixbuf_loader_get_format(loader));
	if (strcmp(format, "svg") == 0 ||
	    strcmp(format, "xpm") == 0)
		{
		g_free(format);
		return;
		}
	
	g_free(format);

	pb = gdk_pixbuf_loader_get_pixbuf(loader);
	
	h = gdk_pixbuf_get_height(pb);
	rs = gdk_pixbuf_get_rowstride(pb);
	pix = gdk_pixbuf_get_pixels(pb);
	
	memset(pix, 0, rs * h); /*this should be faster than pixbuf_fill */

}

static void image_loader_size_cb(GdkPixbufLoader *loader,
				 gint width, gint height, gpointer data)
{
	ImageLoader *il = data;
	GdkPixbufFormat *format;
	gchar **mime_types;
	gboolean scale = FALSE;
	gint n;

	g_mutex_lock(il->data_mutex);
	if (il->requested_width < 1 || il->requested_height < 1) 
		{
		g_mutex_unlock(il->data_mutex);
		return;
		}
	g_mutex_unlock(il->data_mutex);

	format = gdk_pixbuf_loader_get_format(loader);
	if (!format) return;

	mime_types = gdk_pixbuf_format_get_mime_types(format);
	n = 0;
	while (mime_types[n])
		{
		if (strstr(mime_types[n], "jpeg")) scale = TRUE;
		n++;
		}
	g_strfreev(mime_types);

	if (!scale) return;

	g_mutex_lock(il->data_mutex);

	if (width > il->requested_width || height > il->requested_height)
		{
		gint nw, nh;

		if (((gdouble)il->requested_width / width) < ((gdouble)il->requested_height / height))
			{
			nw = il->requested_width;
			nh = (gdouble)nw / width * height;
			if (nh < 1) nh = 1;
			}
		else
			{
			nh = il->requested_height;
			nw = (gdouble)nh / height * width;
			if (nw < 1) nw = 1;
			}

		gdk_pixbuf_loader_set_size(loader, nw, nh);
		il->shrunk = TRUE;
		}
	g_mutex_unlock(il->data_mutex);

}

static void image_loader_stop_loader(ImageLoader *il)
{
	if (!il) return;

	if (il->loader)
		{
		/* some loaders do not have a pixbuf till close, order is important here */
		gdk_pixbuf_loader_close(il->loader, il->error ? NULL : &il->error); /* we are interested in the first error only */
		image_loader_sync_pixbuf(il);
		g_object_unref(G_OBJECT(il->loader));
		il->loader = NULL;
		}
	g_mutex_lock(il->data_mutex);
	il->done = TRUE;
	g_mutex_unlock(il->data_mutex);
}

static void image_loader_setup_loader(ImageLoader *il)
{
	g_mutex_lock(il->data_mutex);
	il->loader = gdk_pixbuf_loader_new();

	g_signal_connect(G_OBJECT(il->loader), "area_updated",
			 G_CALLBACK(image_loader_area_updated_cb), il);
	g_signal_connect(G_OBJECT(il->loader), "size_prepared",
			 G_CALLBACK(image_loader_size_cb), il);
	g_signal_connect(G_OBJECT(il->loader), "area_prepared",
			 G_CALLBACK(image_loader_area_prepared_cb), il);
	g_mutex_unlock(il->data_mutex);
}


static void image_loader_done(ImageLoader *il)
{
	image_loader_stop_loader(il);

	image_loader_emit_done(il);
}

static void image_loader_error(ImageLoader *il)
{
	image_loader_stop_loader(il);

	DEBUG_1("pixbuf_loader reported load error for: %s", il->fd->path);

	image_loader_emit_error(il);
}

static gboolean image_loader_continue(ImageLoader *il)
{
	gint b;
	gint c;

	if (!il) return FALSE;

	c = il->idle_read_loop_count ? il->idle_read_loop_count : 1;
	while (c > 0 && !image_loader_get_stopping(il))
		{
		b = MIN(il->read_buffer_size, il->bytes_total - il->bytes_read);

		if (b == 0)
			{
			image_loader_done(il);
			return FALSE;
			}

		if (b < 0 || (b > 0 && !gdk_pixbuf_loader_write(il->loader, il->mapped_file + il->bytes_read, b, &il->error)))
			{
			image_loader_error(il);
			return FALSE;
			}

		il->bytes_read += b;

		c--;
		}

	if (il->bytes_total > 0)
		{
		image_loader_emit_percent(il);
		}

	return TRUE;
}

static gboolean image_loader_begin(ImageLoader *il)
{
	gssize b;
	
	if (il->pixbuf) return FALSE;

	b = MIN(il->read_buffer_size, il->bytes_total - il->bytes_read);
	if (b < 1) return FALSE;

	image_loader_setup_loader(il);

	if (!gdk_pixbuf_loader_write(il->loader, il->mapped_file + il->bytes_read, b, &il->error))
		{
		image_loader_stop_loader(il);
		return FALSE;
		}

	il->bytes_read += b;

	/* read until size is known */
	while (il->loader && !gdk_pixbuf_loader_get_pixbuf(il->loader) && b > 0 && !image_loader_get_stopping(il))
		{
		b = MIN(il->read_buffer_size, il->bytes_total - il->bytes_read);
		if (b < 0 || (b > 0 && !gdk_pixbuf_loader_write(il->loader, il->mapped_file + il->bytes_read, b, &il->error)))
			{
			image_loader_stop_loader(il);
			return FALSE;
			}
		il->bytes_read += b;
		}
	if (!il->pixbuf) image_loader_sync_pixbuf(il);

	if (il->bytes_read == il->bytes_total || b < 1)
		{
		/* done, handle (broken) loaders that do not have pixbuf till close */
		image_loader_stop_loader(il);

		if (!il->pixbuf) return FALSE;

		image_loader_done(il);
		return TRUE;
		}

	if (!il->pixbuf)
		{
		image_loader_stop_loader(il);
		return FALSE;
		}

	return TRUE;
}

/**************************************************************************************/
/* the following functions are always executed in the main thread */


static gboolean image_loader_setup_source(ImageLoader *il)
{
	struct stat st;
	gchar *pathl;

	if (!il || il->loader || il->mapped_file) return FALSE;

	il->mapped_file = NULL;

	if (il->fd)
		{
		ExifData *exif = exif_read_fd(il->fd);

		if (options->thumbnails.use_exif)
			il->mapped_file = exif_get_preview(exif, (guint *)&il->bytes_total, il->requested_width, il->requested_height);
		else
			il->mapped_file = exif_get_preview(exif, (guint *)&il->bytes_total, 0, 0); /* get the largest available preview image or NULL for normal images*/

		if (il->mapped_file)
			{
			il->preview = TRUE;
			DEBUG_1("Usable reduced size (preview) image loaded from file %s", il->fd->path);
			}
		exif_free_fd(il->fd, exif);
		}

	
	if (!il->mapped_file)
		{
		/* normal file */
		gint load_fd;
	
		pathl = path_from_utf8(il->fd->path);
		load_fd = open(pathl, O_RDONLY | O_NONBLOCK);
		g_free(pathl);
		if (load_fd == -1) return FALSE;

		if (fstat(load_fd, &st) == 0)
			{
			il->bytes_total = st.st_size;
			}
		else
			{
			close(load_fd);
			return FALSE;
			}
		
		il->mapped_file = mmap(0, il->bytes_total, PROT_READ|PROT_WRITE, MAP_PRIVATE, load_fd, 0);
		close(load_fd);
		if (il->mapped_file == MAP_FAILED)
			{
			il->mapped_file = 0;
			return FALSE;
			}
		il->preview = FALSE;
		}
		
	return TRUE;
}

static void image_loader_stop_source(ImageLoader *il)
{
	if (!il) return;
	
	if (il->mapped_file)
		{
		if (il->preview)
			{
			exif_free_preview(il->mapped_file);
			}
		else
			{
			munmap(il->mapped_file, il->bytes_total);
			}
		il->mapped_file = NULL;
		}
}

static void image_loader_stop(ImageLoader *il)
{
	if (!il) return;

	if (il->idle_id)
		{
		g_source_remove(il->idle_id);
		il->idle_id = 0;
		}
		
	if (il->thread)
		{
		/* stop loader in the other thread */
		g_mutex_lock(il->data_mutex);
		il->stopping = TRUE;
		while (!il->can_destroy) g_cond_wait(il->can_destroy_cond, il->data_mutex);
		g_mutex_unlock(il->data_mutex);
		}

	image_loader_stop_loader(il);
	image_loader_stop_source(il);
	
}

void image_loader_delay_area_ready(ImageLoader *il, gboolean enable)
{
	g_mutex_lock(il->data_mutex);
	il->delay_area_ready = enable;
	if (!enable)
		{
		/* send delayed */
		GList *list, *work;
		list = g_list_reverse(il->area_param_delayed_list);
		il->area_param_delayed_list = NULL;
		g_mutex_unlock(il->data_mutex);

		work = list;

		while (work)
			{
			ImageLoaderAreaParam *par = work->data;
			work = work->next;
			
			g_signal_emit(il, signals[SIGNAL_AREA_READY], 0, par->x, par->y, par->w, par->h);
			g_free(par);
			}
		g_list_free(list);
		}
	else
		{
		/* just unlock */
		g_mutex_unlock(il->data_mutex);
		}
}


/**************************************************************************************/
/* execution via idle calls */

static gboolean image_loader_idle_cb(gpointer data)
{
	gboolean ret = FALSE;
	ImageLoader *il = data;

	if (il->idle_id)
		{
		ret = image_loader_continue(il);
		}
	
	if (!ret)
		{
		image_loader_stop_source(il);
		}
	
	return ret;
}


static gboolean image_loader_start_idle(ImageLoader *il)
{
	gboolean ret;
	
	if (!il) return FALSE;

	if (!il->fd) return FALSE;

	if (!image_loader_setup_source(il)) return FALSE;
	
	ret = image_loader_begin(il);

	if (ret && !il->done) il->idle_id = g_idle_add_full(il->idle_priority, image_loader_idle_cb, il, NULL);
	return ret;
}

/**************************************************************************************/
/* execution via thread */

#ifdef HAVE_GTHREAD
static GThreadPool *image_loader_thread_pool = NULL;

static GCond *image_loader_prio_cond = NULL;
static GMutex *image_loader_prio_mutex = NULL;
static gint image_loader_prio_num = 0;


static void image_loader_thread_enter_high(void)
{
	g_mutex_lock(image_loader_prio_mutex);
	image_loader_prio_num++;
	g_mutex_unlock(image_loader_prio_mutex);
}

static void image_loader_thread_leave_high(void)
{
	g_mutex_lock(image_loader_prio_mutex);
	image_loader_prio_num--;
	if (image_loader_prio_num == 0) g_cond_broadcast(image_loader_prio_cond); /* wake up all low prio threads */
	g_mutex_unlock(image_loader_prio_mutex);
}

static void image_loader_thread_wait_high(void)
{
	g_mutex_lock(image_loader_prio_mutex);
	while (image_loader_prio_num) 
		{
		g_cond_wait(image_loader_prio_cond, image_loader_prio_mutex);
		}

	g_mutex_unlock(image_loader_prio_mutex);
}


static void image_loader_thread_run(gpointer data, gpointer user_data)
{
	ImageLoader *il = data;
	gboolean cont;
	gboolean err;
	
	if (il->idle_priority > G_PRIORITY_DEFAULT_IDLE) 
		{
		/* low prio, wait untill high prio tasks finishes */
		image_loader_thread_wait_high();
		}
	else
		{
		/* high prio */
		image_loader_thread_enter_high();
		}
	
	err = !image_loader_begin(il);
	
	if (err)
		{
		/* 
		loader failed, we have to send signal 
		(idle mode returns the image_loader_begin return value directly)
		(success is always reported indirectly from image_loader_begin)
		*/
		image_loader_emit_error(il);
		}
	
	cont = !err;
	
	while (cont && !image_loader_get_is_done(il) && !image_loader_get_stopping(il))
		{
		if (il->idle_priority > G_PRIORITY_DEFAULT_IDLE) 
			{
			/* low prio, wait untill high prio tasks finishes */
			image_loader_thread_wait_high();
			}
		cont = image_loader_continue(il);
		}
	image_loader_stop_loader(il);

	if (il->idle_priority <= G_PRIORITY_DEFAULT_IDLE) 
		{
		/* high prio */
		image_loader_thread_leave_high();
		}

	g_mutex_lock(il->data_mutex);
	il->can_destroy = TRUE;
	g_cond_signal(il->can_destroy_cond);
	g_mutex_unlock(il->data_mutex);

}


static gboolean image_loader_start_thread(ImageLoader *il)
{
	if (!il) return FALSE;

	if (!il->fd) return FALSE;

	il->thread = TRUE;
	
	if (!image_loader_setup_source(il)) return FALSE;

        if (!image_loader_thread_pool) 
		{
		image_loader_thread_pool = g_thread_pool_new(image_loader_thread_run, NULL, -1, FALSE, NULL);
		image_loader_prio_cond = g_cond_new();
		image_loader_prio_mutex = g_mutex_new();
		}

	il->can_destroy = FALSE; /* ImageLoader can't be freed until image_loader_thread_run finishes */

	g_thread_pool_push(image_loader_thread_pool, il, NULL);
	DEBUG_1("Thread pool num threads: %d", g_thread_pool_get_num_threads(image_loader_thread_pool));
		
	return TRUE;
}
#endif /* HAVE_GTHREAD */


/**************************************************************************************/
/* public interface */


gboolean image_loader_start(ImageLoader *il)
{
	if (!il) return FALSE;

	if (!il->fd) return FALSE;

#ifdef HAVE_GTHREAD
	return image_loader_start_thread(il);
#else
	return image_loader_start_idle(il);
#endif
}


/* don't forget to gdk_pixbuf_ref() it if you want to use it after image_loader_free() */
GdkPixbuf *image_loader_get_pixbuf(ImageLoader *il)
{
	GdkPixbuf *ret;
	if (!il) return NULL;
	
	g_mutex_lock(il->data_mutex);
	ret = il->pixbuf;
	g_mutex_unlock(il->data_mutex);
	return ret;
}

gchar *image_loader_get_format(ImageLoader *il)
{
	GdkPixbufFormat *format;
	gchar **mimev;
	gchar *mime;

	if (!il || !il->loader) return NULL;

	format = gdk_pixbuf_loader_get_format(il->loader);
	if (!format) return NULL;

	mimev = gdk_pixbuf_format_get_mime_types(format);
	if (!mimev) return NULL;

	/* return first member of mimev, as GdkPixbufLoader has no way to tell us which exact one ? */
	mime = g_strdup(mimev[0]);
	g_strfreev(mimev);

	return mime;
}

void image_loader_set_requested_size(ImageLoader *il, gint width, gint height)
{
	if (!il) return;

	g_mutex_lock(il->data_mutex);
	il->requested_width = width;
	il->requested_height = height;
	g_mutex_unlock(il->data_mutex);
}

void image_loader_set_buffer_size(ImageLoader *il, guint count)
{
	if (!il) return;

	g_mutex_lock(il->data_mutex);
	il->idle_read_loop_count = count ? count : 1;
	g_mutex_unlock(il->data_mutex);
}

void image_loader_set_priority(ImageLoader *il, gint priority)
{
	if (!il) return;

	if (il->thread) return; /* can't change prio if the thread already runs */
	il->idle_priority = priority;
}


gdouble image_loader_get_percent(ImageLoader *il)
{
	gdouble ret;
	if (!il) return 0.0;
	
	g_mutex_lock(il->data_mutex);
	if (il->bytes_total == 0) 
		{
		ret = 0.0;
		}
	else
		{
		ret = (gdouble)il->bytes_read / il->bytes_total;
		}
	g_mutex_unlock(il->data_mutex);
	return ret;
}

gboolean image_loader_get_is_done(ImageLoader *il)
{
	gboolean ret;
	if (!il) return FALSE;

	g_mutex_lock(il->data_mutex);
	ret = il->done;
	g_mutex_unlock(il->data_mutex);

	return ret;
}

FileData *image_loader_get_fd(ImageLoader *il)
{
	FileData *ret;
	if (!il) return NULL;

	g_mutex_lock(il->data_mutex);
	ret = il->fd;
	g_mutex_unlock(il->data_mutex);

	return ret;
}

gboolean image_loader_get_shrunk(ImageLoader *il)
{
	gboolean ret;
	if (!il) return FALSE;

	g_mutex_lock(il->data_mutex);
	ret = il->shrunk;
	g_mutex_unlock(il->data_mutex);
	return ret;
}

const gchar *image_loader_get_error(ImageLoader *il)
{
	const gchar *ret = NULL;
	if (!il) return NULL;
	g_mutex_lock(il->data_mutex);
	if (il->error) ret = il->error->message;
	g_mutex_unlock(il->data_mutex);
	return ret;
}


/* FIXME - this can be rather slow and blocks until the size is known */
gboolean image_load_dimensions(FileData *fd, gint *width, gint *height)
{
	ImageLoader *il;
	gboolean success;

	il = image_loader_new(fd);

	success = image_loader_start_idle(il);

	if (success && il->pixbuf)
		{
		if (width) *width = gdk_pixbuf_get_width(il->pixbuf);
		if (height) *height = gdk_pixbuf_get_height(il->pixbuf);;
		}
	else
		{
		if (width) *width = -1;
		if (height) *height = -1;
		}

	image_loader_free(il);

	return success;
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
