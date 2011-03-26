/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2010 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef IMAGE_LOAD_H
#define IMAGE_LOAD_H

#define TYPE_IMAGE_LOADER		(image_loader_get_type())


typedef struct _ImageLoaderBackend ImageLoaderBackend;
struct _ImageLoaderBackend
{
	gpointer (*loader_new)(GCallback area_updated_cb, GCallback size_cb, GCallback area_prepared_cb, gpointer data);
	void (*set_size)(gpointer loader, int width, int height);
	gboolean (*write)(gpointer loader, const guchar *buf, gsize count, GError **error);
	GdkPixbuf* (*get_pixbuf)(gpointer loader);
	gboolean (*close)(gpointer loader, GError **error);

	gchar* (*get_format_name)(gpointer loader);
	gchar** (*get_format_mime_types)(gpointer loader);
};


//typedef struct _ImageLoader ImageLoader;
typedef struct _ImageLoaderClass ImageLoaderClass;

struct _ImageLoader
{
	GObject parent;
	
	/*< private >*/
	GdkPixbuf *pixbuf;
	FileData *fd;
	gchar *path;

	gsize bytes_read;
	gsize bytes_total;

	gboolean preview;

	gint requested_width;
	gint requested_height;
	gboolean shrunk;

	gboolean done;
	guint idle_id; /* event source id */
	gint idle_priority;

	gpointer *loader;
	GError *error;
	ImageLoaderBackend backend;

	guint idle_done_id; /* event source id */
	GList *area_param_list;
	GList *area_param_delayed_list;
	
	gboolean delay_area_ready;
	
	GMutex *data_mutex;
	gboolean stopping;
	gboolean can_destroy;
	GCond *can_destroy_cond;
	gboolean thread;

	guchar *mapped_file;
	gsize read_buffer_size;
	guint idle_read_loop_count;
};

struct _ImageLoaderClass {
	GObjectClass parent;
	
	/* class members */
	void (*area_ready)(ImageLoader *, guint x, guint y, guint w, guint h, gpointer);
	void (*error)(ImageLoader *, gpointer);
	void (*done)(ImageLoader *, gpointer);
	void (*percent)(ImageLoader *, gdouble, gpointer);
};

GType image_loader_get_type(void);

ImageLoader *image_loader_new(FileData *fd);

void image_loader_free(ImageLoader *il);

/* delay area_ready signals */
void image_loader_delay_area_ready(ImageLoader *il, gboolean enable);

/* Speed up loading when you only need at most width x height size image,
 * only the jpeg GdkPixbuf loader benefits from it - so there is no
 * guarantee that the image will scale down to the requested size..
 */
void image_loader_set_requested_size(ImageLoader *il, gint width, gint height);

void image_loader_set_buffer_size(ImageLoader *il, guint size);

/* this only has effect if used before image_loader_start()
 * default is G_PRIORITY_DEFAULT_IDLE
 */
void image_loader_set_priority(ImageLoader *il, gint priority);

gboolean image_loader_start(ImageLoader *il);


GdkPixbuf *image_loader_get_pixbuf(ImageLoader *il);
gchar *image_loader_get_format(ImageLoader *il);
gdouble image_loader_get_percent(ImageLoader *il);
gboolean image_loader_get_is_done(ImageLoader *il);
FileData *image_loader_get_fd(ImageLoader *il);
gboolean image_loader_get_shrunk(ImageLoader *il);
const gchar *image_loader_get_error(ImageLoader *il);

gboolean image_load_dimensions(FileData *fd, gint *width, gint *height);

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
