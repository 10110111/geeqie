/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "image.h"


#include "collect.h"
#include "color-man.h"
#include "exif.h"
#include "histogram.h"
#include "image-load.h"
#include "image-overlay.h"
#include "layout.h"
#include "layout_image.h"
#include "pixbuf-renderer.h"
#include "pixbuf_util.h"
#include "ui_fileops.h"

#include "filedata.h"
#include "filecache.h"

#include <math.h>

static GList *image_list = NULL;

static void image_update_title(ImageWindow *imd);
static void image_read_ahead_start(ImageWindow *imd);
static void image_cache_set(ImageWindow *imd, FileData *fd);

/*
 *-------------------------------------------------------------------
 * 'signals'
 *-------------------------------------------------------------------
 */

static void image_click_cb(PixbufRenderer *pr, GdkEventButton *event, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->func_button)
		{
		imd->func_button(imd, event, imd->data_button);
		}
}

static void image_drag_cb(PixbufRenderer *pr, GdkEventButton *event, gpointer data)
{
	ImageWindow *imd = data;
	gint width, height;

	pixbuf_renderer_get_scaled_size(pr, &width, &height);

	if (imd->func_drag)
		{
		imd->func_drag(imd, event,
			       (gfloat)(pr->drag_last_x - event->x) / width,
			       (gfloat)(pr->drag_last_y - event->y) / height,
			       imd->data_button);
		}
}

static void image_scroll_notify_cb(PixbufRenderer *pr, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->func_scroll_notify && pr->scale)
		{
		imd->func_scroll_notify(imd,
					(gint)((gdouble)pr->x_scroll / pr->scale),
					(gint)((gdouble)pr->y_scroll / pr->scale),
					(gint)((gdouble)pr->image_width - pr->vis_width / pr->scale),
					(gint)((gdouble)pr->image_height - pr->vis_height / pr->scale),
					imd->data_scroll_notify);
		}
}

static void image_update_util(ImageWindow *imd)
{
	if (imd->func_update) imd->func_update(imd, imd->data_update);
}

static void image_zoom_cb(PixbufRenderer *pr, gdouble zoom, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->title_show_zoom) image_update_title(imd);
	if (imd->overlay_show_zoom) image_osd_update(imd);

	image_update_util(imd);
}

static void image_complete_util(ImageWindow *imd, gint preload)
{
	if (imd->il && image_get_pixbuf(imd) != image_loader_get_pixbuf(imd->il)) return;

	DEBUG_1("%s image load completed \"%s\" (%s)", get_exec_time(),
			  (preload) ? (imd->read_ahead_fd ? imd->read_ahead_fd->path : "null") :
				      (imd->image_fd ? imd->image_fd->path : "null"),
			  (preload) ? "preload" : "current");

	if (!preload) imd->completed = TRUE;
	if (imd->func_complete) imd->func_complete(imd, preload, imd->data_complete);
}

static void image_render_complete_cb(PixbufRenderer *pr, gpointer data)
{
	ImageWindow *imd = data;

	image_complete_util(imd, FALSE);
}

static void image_state_set(ImageWindow *imd, ImageState state)
{
	if (state == IMAGE_STATE_NONE)
		{
		imd->state = state;
		}
	else
		{
		imd->state |= state;
		}
	if (imd->func_state) imd->func_state(imd, state, imd->data_state);
}

static void image_state_unset(ImageWindow *imd, ImageState state)
{
	imd->state &= ~state;
	if (imd->func_state) imd->func_state(imd, state, imd->data_state);
}

/*
 *-------------------------------------------------------------------
 * misc
 *-------------------------------------------------------------------
 */

static void image_update_title(ImageWindow *imd)
{
	gchar *title = NULL;
	gchar *zoom = NULL;
	gchar *collection = NULL;

	if (!imd->top_window) return;

	if (imd->collection && collection_to_number(imd->collection) >= 0)
		{
		const gchar *name = imd->collection->name;
		if (!name) name = _("Untitled");
		collection = g_strdup_printf(_(" (Collection %s)"), name);
		}

	if (imd->title_show_zoom)
		{
		gchar *buf = image_zoom_get_as_text(imd);
		zoom = g_strconcat(" [", buf, "]", NULL);
		g_free(buf);
		}

	title = g_strdup_printf("%s%s%s%s%s%s",
		imd->title ? imd->title : "",
		imd->image_fd ? imd->image_fd->name : "",
		zoom ? zoom : "",
		collection ? collection : "",
		imd->image_fd ? " - " : "",
		imd->title_right ? imd->title_right : "");

	gtk_window_set_title(GTK_WINDOW(imd->top_window), title);

	g_free(title);
	g_free(zoom);
	g_free(collection);
}

/*
 *-------------------------------------------------------------------
 * rotation, flip, etc.
 *-------------------------------------------------------------------
 */

static gint image_post_process_color(ImageWindow *imd, gint start_row, ExifData *exif, gint run_in_bg)
{
	ColorMan *cm;
	ColorManProfileType input_type;
	ColorManProfileType screen_type;
	const gchar *input_file;
	const gchar *screen_file;
	guchar *profile = NULL;
	guint profile_len;

	if (imd->cm) return FALSE;

	if (imd->color_profile_input >= COLOR_PROFILE_FILE &&
	    imd->color_profile_input <  COLOR_PROFILE_FILE + COLOR_PROFILE_INPUTS)
		{
		gint n;

		n = imd->color_profile_input - COLOR_PROFILE_FILE;
		if (!options->color_profile.input_file[n]) return FALSE;

		input_type = COLOR_PROFILE_FILE;
		input_file = options->color_profile.input_file[n];
		}
	else if (imd->color_profile_input >= COLOR_PROFILE_SRGB &&
		 imd->color_profile_input <  COLOR_PROFILE_FILE)
		{
		input_type = imd->color_profile_input;
		input_file = NULL;
		}
	else
		{
		return FALSE;
		}

	if (imd->color_profile_screen == 1 &&
	    options->color_profile.screen_file)
		{
		screen_type = COLOR_PROFILE_FILE;
		screen_file = options->color_profile.screen_file;
		}
	else if (imd->color_profile_screen == 0)
		{
		screen_type = COLOR_PROFILE_SRGB;
		screen_file = NULL;
		}
	else
		{
		return FALSE;
		}
	imd->color_profile_from_image = COLOR_PROFILE_NONE;

	if (imd->color_profile_use_image && exif)
		{
		profile = exif_get_color_profile(exif, &profile_len);
		if (!profile)
			{
			gint cs;
			gchar *interop_index;

			/* ColorSpace == 1 specifies sRGB per EXIF 2.2 */
			if (!exif_get_integer(exif, "Exif.Photo.ColorSpace", &cs)) cs = 0;
			interop_index = exif_get_data_as_text(exif, "Exif.Iop.InteroperabilityIndex");

			if (cs == 1)
				{
				input_type = COLOR_PROFILE_SRGB;
				input_file = NULL;
				imd->color_profile_from_image = COLOR_PROFILE_SRGB;

				DEBUG_1("Found EXIF ColorSpace of sRGB");
				}
			if (cs == 2 || (interop_index && !strcmp(interop_index, "R03")))
				{
				input_type = COLOR_PROFILE_ADOBERGB;
				input_file = NULL;
				imd->color_profile_from_image = COLOR_PROFILE_ADOBERGB;

				DEBUG_1("Found EXIF ColorSpace of AdobeRGB");
				}

			g_free(interop_index);
			}
		}

	if (profile)
		{
		DEBUG_1("Found embedded color profile");
		imd->color_profile_from_image = COLOR_PROFILE_MEM;

		cm = color_man_new_embedded(run_in_bg ? imd : NULL, NULL,
					    profile, profile_len,
					    screen_type, screen_file);
		g_free(profile);
		}
	else
		{
		cm = color_man_new(run_in_bg ? imd : NULL, NULL,
				   input_type, input_file,
				   screen_type, screen_file);
		}

	if (cm)
		{
		if (start_row > 0)
			{
			cm->row = start_row;
			cm->incremental_sync = TRUE;
			}

		imd->cm = (gpointer)cm;
#if 0
		if (run_in_bg) color_man_start_bg(imd->cm, image_post_process_color_cb, imd);
#endif
		return TRUE;
		}

	return FALSE;
}


static void image_post_process_tile_color_cb(PixbufRenderer *pr, GdkPixbuf **pixbuf, gint x, gint y, gint w, gint h, gpointer data)
{
	ImageWindow *imd = (ImageWindow *)data;
	if (imd->cm) color_man_correct_region(imd->cm, *pixbuf, x, y, w, h);
	if (imd->desaturate) pixbuf_desaturate_rect(*pixbuf, x, y, w, h);

}

void image_alter(ImageWindow *imd, AlterType type)
{
	static const gint rotate_90[]    = {1,   6, 7, 8, 5, 2, 3, 4, 1};
	static const gint rotate_90_cc[] = {1,   8, 5, 6, 7, 4, 1, 2, 3};
	static const gint rotate_180[]   = {1,   3, 4, 1, 2, 7, 8, 5, 6};
	static const gint mirror[]       = {1,   2, 1, 4, 3, 6, 5, 8, 7};
	static const gint flip[]         = {1,   4, 3, 2, 1, 8, 7, 6, 5};


	if (!imd || !imd->pr || !imd->image_fd) return;

	if (imd->orientation < 1 || imd->orientation > 8) imd->orientation = 1;

	switch (type)
		{
		case ALTER_ROTATE_90:
			imd->orientation = rotate_90[imd->orientation];
			break;
		case ALTER_ROTATE_90_CC:
			imd->orientation = rotate_90_cc[imd->orientation];
			break;
		case ALTER_ROTATE_180:
			imd->orientation = rotate_180[imd->orientation];
			break;
		case ALTER_MIRROR:
			imd->orientation = mirror[imd->orientation];
			break;
		case ALTER_FLIP:
			imd->orientation = flip[imd->orientation];
			break;
		case ALTER_DESATURATE:
			imd->desaturate = !imd->desaturate;
			break;
		case ALTER_NONE:
			imd->orientation = imd->image_fd->exif_orientation ? imd->image_fd->exif_orientation : 1;
			imd->desaturate = FALSE;
			break;
		default:
			return;
			break;
		}

	if (type != ALTER_NONE && type != ALTER_DESATURATE)
		{
		if (imd->image_fd->user_orientation == 0) file_data_ref(imd->image_fd);
		imd->image_fd->user_orientation = imd->orientation;
		}
	else
		{
		if (imd->image_fd->user_orientation != 0) file_data_unref(imd->image_fd);
		imd->image_fd->user_orientation = 0;
		}

	pixbuf_renderer_set_orientation((PixbufRenderer *)imd->pr, imd->orientation);
	if (imd->cm || imd->desaturate)
		pixbuf_renderer_set_post_process_func((PixbufRenderer *)imd->pr, image_post_process_tile_color_cb, (gpointer) imd, (imd->cm != NULL) );
	else
		pixbuf_renderer_set_post_process_func((PixbufRenderer *)imd->pr, NULL, NULL, TRUE);
}


/*
 *-------------------------------------------------------------------
 * read ahead (prebuffer)
 *-------------------------------------------------------------------
 */

static void image_read_ahead_cancel(ImageWindow *imd)
{
	DEBUG_1("%s read ahead cancelled for :%s", get_exec_time(), imd->read_ahead_fd ? imd->read_ahead_fd->path : "null");

	image_loader_free(imd->read_ahead_il);
	imd->read_ahead_il = NULL;

	file_data_unref(imd->read_ahead_fd);
	imd->read_ahead_fd = NULL;
}

static void image_read_ahead_done_cb(ImageLoader *il, gpointer data)
{
	ImageWindow *imd = data;

	DEBUG_1("%s read ahead done for :%s", get_exec_time(), imd->read_ahead_fd->path);

	if (!imd->read_ahead_fd->pixbuf)
		{
		imd->read_ahead_fd->pixbuf = image_loader_get_pixbuf(imd->read_ahead_il);
		if (imd->read_ahead_fd->pixbuf)
			{
			g_object_ref(imd->read_ahead_fd->pixbuf);
			image_cache_set(imd, imd->read_ahead_fd);
			}
		}
	image_loader_free(imd->read_ahead_il);
	imd->read_ahead_il = NULL;

	image_complete_util(imd, TRUE);
}

static void image_read_ahead_error_cb(ImageLoader *il, gpointer data)
{
	/* we even treat errors as success, maybe at least some of the file was ok */
	image_read_ahead_done_cb(il, data);
}

static void image_read_ahead_start(ImageWindow *imd)
{
	/* already started ? */
	if (!imd->read_ahead_fd || imd->read_ahead_il || imd->read_ahead_fd->pixbuf) return;

	/* still loading ?, do later */
	if (imd->il /*|| imd->cm*/) return;

	DEBUG_1("%s read ahead started for :%s", get_exec_time(), imd->read_ahead_fd->path);

	imd->read_ahead_il = image_loader_new(imd->read_ahead_fd);
	
	image_loader_delay_area_ready(imd->read_ahead_il, TRUE); /* we will need the area_ready signals later */

	g_signal_connect (G_OBJECT(imd->read_ahead_il), "error", (GCallback)image_read_ahead_error_cb, imd);
	g_signal_connect (G_OBJECT(imd->read_ahead_il), "done", (GCallback)image_read_ahead_done_cb, imd);

	if (!image_loader_start(imd->read_ahead_il))
		{
		image_read_ahead_cancel(imd);
		image_complete_util(imd, TRUE);
		}
}

static void image_read_ahead_set(ImageWindow *imd, FileData *fd)
{
	if (imd->read_ahead_fd && fd && imd->read_ahead_fd == fd) return;

	image_read_ahead_cancel(imd);

	imd->read_ahead_fd = file_data_ref(fd);

	DEBUG_1("read ahead set to :%s", imd->read_ahead_fd->path);

	image_read_ahead_start(imd);
}

/*
 *-------------------------------------------------------------------
 * post buffering
 *-------------------------------------------------------------------
 */

static void image_cache_release_cb(FileData *fd)
{
	g_object_unref(fd->pixbuf);
	fd->pixbuf = NULL;
}

static FileCacheData *image_get_cache()
{
	static FileCacheData *cache = NULL;
	if (!cache) cache = file_cache_new(image_cache_release_cb, 1);
	file_cache_set_max_size(cache, (gulong)options->image.image_cache_max * 1048576); /* update from options */
	return cache;
}

static void image_cache_set(ImageWindow *imd, FileData *fd)
{
	g_assert(fd->pixbuf);

	file_cache_put(image_get_cache(), fd, (gulong)gdk_pixbuf_get_rowstride(fd->pixbuf) * (gulong)gdk_pixbuf_get_height(fd->pixbuf));
}

static gint image_cache_get(ImageWindow *imd)
{
	gint success;

	success = file_cache_get(image_get_cache(), imd->image_fd);
	if (success)
		{
		g_assert(imd->image_fd->pixbuf);
		image_change_pixbuf(imd, imd->image_fd->pixbuf, image_zoom_get(imd), FALSE);
		}
	
	file_cache_dump(image_get_cache());
	return success;
}

/*
 *-------------------------------------------------------------------
 * loading
 *-------------------------------------------------------------------
 */

static void image_load_pixbuf_ready(ImageWindow *imd)
{
	if (image_get_pixbuf(imd) || !imd->il) return;

	image_change_pixbuf(imd, image_loader_get_pixbuf(imd->il), image_zoom_get(imd), FALSE);
}

static void image_load_area_cb(ImageLoader *il, guint x, guint y, guint w, guint h, gpointer data)
{
	ImageWindow *imd = data;
	PixbufRenderer *pr;

	pr = (PixbufRenderer *)imd->pr;

	if (imd->delay_flip &&
	    pr->pixbuf != image_loader_get_pixbuf(il))
		{
		return;
		}

	if (!pr->pixbuf) image_change_pixbuf(imd, image_loader_get_pixbuf(imd->il), image_zoom_get(imd), TRUE);

	pixbuf_renderer_area_changed(pr, x, y, w, h);
}

static void image_load_done_cb(ImageLoader *il, gpointer data)
{
	ImageWindow *imd = data;

	DEBUG_1("%s image done", get_exec_time());

	g_object_set(G_OBJECT(imd->pr), "loading", FALSE, NULL);
	image_state_unset(imd, IMAGE_STATE_LOADING);

	if (options->image.enable_read_ahead && imd->image_fd && !imd->image_fd->pixbuf && image_loader_get_pixbuf(imd->il))
		{
		imd->image_fd->pixbuf = g_object_ref(image_loader_get_pixbuf(imd->il));
		image_cache_set(imd, imd->image_fd);
		}

	if (!image_loader_get_pixbuf(imd->il))
		{
		GdkPixbuf *pixbuf;

		pixbuf = pixbuf_inline(PIXBUF_INLINE_BROKEN);
		image_change_pixbuf(imd, pixbuf, image_zoom_get(imd), FALSE);
		g_object_unref(pixbuf);

		imd->unknown = TRUE;
		}
	else if (imd->delay_flip &&
	    image_get_pixbuf(imd) != image_loader_get_pixbuf(imd->il))
		{
		g_object_set(G_OBJECT(imd->pr), "complete", FALSE, NULL);
		image_change_pixbuf(imd, image_loader_get_pixbuf(imd->il), image_zoom_get(imd), FALSE);
		}

	image_loader_free(imd->il);
	imd->il = NULL;

//	image_post_process(imd, TRUE);

	image_read_ahead_start(imd);
}

static void image_load_error_cb(ImageLoader *il, gpointer data)
{
	DEBUG_1("%s image error", get_exec_time());

	/* even on error handle it like it was done,
	 * since we have a pixbuf with _something_ */

	image_load_done_cb(il, data);
}

/* this read ahead is located here merely for the callbacks, above */

static gint image_read_ahead_check(ImageWindow *imd)
{
	if (!imd->read_ahead_fd) return FALSE;
	if (imd->il) return FALSE;

	if (!imd->image_fd || imd->read_ahead_fd != imd->image_fd)
		{
		image_read_ahead_cancel(imd);
		return FALSE;
		}

	if (imd->read_ahead_il)
		{
		imd->il = imd->read_ahead_il;
		imd->read_ahead_il = NULL;

		/* override the old signals */
		g_signal_handlers_disconnect_matched(G_OBJECT(imd->il), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, imd);
		g_signal_connect (G_OBJECT(imd->il), "area_ready", (GCallback)image_load_area_cb, imd);
		g_signal_connect (G_OBJECT(imd->il), "error", (GCallback)image_load_error_cb, imd);
		g_signal_connect (G_OBJECT(imd->il), "done", (GCallback)image_load_done_cb, imd);

		g_object_set(G_OBJECT(imd->pr), "loading", TRUE, NULL);
		image_state_set(imd, IMAGE_STATE_LOADING);

		if (!imd->delay_flip)
			{
			image_change_pixbuf(imd, image_loader_get_pixbuf(imd->il), image_zoom_get(imd), TRUE);
			}

		image_loader_delay_area_ready(imd->il, FALSE); /* send the delayed area_ready signals */

		file_data_unref(imd->read_ahead_fd);
		imd->read_ahead_fd = NULL;
		return TRUE;
		}
	else if (imd->read_ahead_fd->pixbuf)
		{
		image_change_pixbuf(imd, imd->read_ahead_fd->pixbuf, image_zoom_get(imd), FALSE);

		file_data_unref(imd->read_ahead_fd);
		imd->read_ahead_fd = NULL;

//		image_post_process(imd, FALSE);
		return TRUE;
		}

	image_read_ahead_cancel(imd);
	return FALSE;
}

static gint image_load_begin(ImageWindow *imd, FileData *fd)
{
	DEBUG_1("%s image begin", get_exec_time());

	if (imd->il) return FALSE;

	imd->completed = FALSE;
	g_object_set(G_OBJECT(imd->pr), "complete", FALSE, NULL);

	if (image_cache_get(imd))
		{
		DEBUG_1("from cache: %s", imd->image_fd->path);
		return TRUE;
		}

	if (image_read_ahead_check(imd))
		{
		DEBUG_1("from read ahead buffer: %s", imd->image_fd->path);
		return TRUE;
		}

	if (!imd->delay_flip && image_get_pixbuf(imd))
		{
		PixbufRenderer *pr;

		pr = PIXBUF_RENDERER(imd->pr);
		if (pr->pixbuf) g_object_unref(pr->pixbuf);
		pr->pixbuf = NULL;
		}

	g_object_set(G_OBJECT(imd->pr), "loading", TRUE, NULL);

	imd->il = image_loader_new(fd);

	g_signal_connect (G_OBJECT(imd->il), "area_ready", (GCallback)image_load_area_cb, imd);
	g_signal_connect (G_OBJECT(imd->il), "error", (GCallback)image_load_error_cb, imd);
	g_signal_connect (G_OBJECT(imd->il), "done", (GCallback)image_load_done_cb, imd);

	if (!image_loader_start(imd->il))
		{
		DEBUG_1("image start error");

		g_object_set(G_OBJECT(imd->pr), "loading", FALSE, NULL);

		image_loader_free(imd->il);
		imd->il = NULL;

		image_complete_util(imd, FALSE);

		return FALSE;
		}

	image_state_set(imd, IMAGE_STATE_LOADING);

/*
	if (!imd->delay_flip && !image_get_pixbuf(imd) && image_loader_get_pixbuf(imd->il))
		{
		image_change_pixbuf(imd, image_loader_get_pixbuf(imd->il), image_zoom_get(imd), TRUE);
		}
*/	
	return TRUE;
}

static void image_reset(ImageWindow *imd)
{
	/* stops anything currently being done */

	DEBUG_1("%s image reset", get_exec_time());

	g_object_set(G_OBJECT(imd->pr), "loading", FALSE, NULL);

	image_loader_free(imd->il);
	imd->il = NULL;

	color_man_free((ColorMan *)imd->cm);
	imd->cm = NULL;

	imd->delay_alter_type = ALTER_NONE;

	image_state_set(imd, IMAGE_STATE_NONE);
}

/*
 *-------------------------------------------------------------------
 * image changer
 *-------------------------------------------------------------------
 */

static void image_change_complete(ImageWindow *imd, gdouble zoom, gint new)
{
	image_reset(imd);

	if (imd->image_fd && isfile(imd->image_fd->path))
		{
		PixbufRenderer *pr;

		pr = PIXBUF_RENDERER(imd->pr);
		pr->zoom = zoom;	/* store the zoom, needed by the loader */

		if (image_load_begin(imd, imd->image_fd))
			{
			imd->unknown = FALSE;
			}
		else
			{
			GdkPixbuf *pixbuf;

			pixbuf = pixbuf_inline(PIXBUF_INLINE_BROKEN);
			image_change_pixbuf(imd, pixbuf, zoom, FALSE);
			g_object_unref(pixbuf);

			imd->unknown = TRUE;
			}
		}
	else
		{
		if (imd->image_fd)
			{
			GdkPixbuf *pixbuf;

			pixbuf = pixbuf_inline(PIXBUF_INLINE_BROKEN);
			image_change_pixbuf(imd, pixbuf, zoom, FALSE);
			g_object_unref(pixbuf);
			}
		else
			{
			image_change_pixbuf(imd, NULL, zoom, FALSE);
			}
		imd->unknown = TRUE;
		}

	image_update_util(imd);
}

static void image_change_real(ImageWindow *imd, FileData *fd,
			      CollectionData *cd, CollectInfo *info, gdouble zoom)
{

	imd->collection = cd;
	imd->collection_info = info;

	if (imd->auto_refresh && imd->image_fd)
		file_data_unregister_real_time_monitor(imd->image_fd);

	file_data_unref(imd->image_fd);
	imd->image_fd = file_data_ref(fd);


	image_change_complete(imd, zoom, TRUE);

	image_update_title(imd);
	image_state_set(imd, IMAGE_STATE_IMAGE);

	if (imd->auto_refresh && imd->image_fd)
		file_data_register_real_time_monitor(imd->image_fd);
}

/*
 *-------------------------------------------------------------------
 * focus stuff
 *-------------------------------------------------------------------
 */

static void image_focus_paint(ImageWindow *imd, gint has_focus, GdkRectangle *area)
{
	GtkWidget *widget;

	widget = imd->widget;
	if (!widget->window) return;

	if (has_focus)
		{
		gtk_paint_focus(widget->style, widget->window, GTK_STATE_ACTIVE,
				area, widget, "image_window",
				widget->allocation.x, widget->allocation.y,
				widget->allocation.width - 1, widget->allocation.height - 1);
		}
	else
		{
		gtk_paint_shadow(widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_IN,
				 area, widget, "image_window",
				 widget->allocation.x, widget->allocation.y,
				 widget->allocation.width - 1, widget->allocation.height - 1);
		}
}

static gint image_focus_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	ImageWindow *imd = data;

	image_focus_paint(imd, GTK_WIDGET_HAS_FOCUS(widget), &event->area);
	return TRUE;
}

static gint image_focus_in_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	ImageWindow *imd = data;

	GTK_WIDGET_SET_FLAGS(imd->widget, GTK_HAS_FOCUS);
	image_focus_paint(imd, TRUE, NULL);

	return TRUE;
}

static gint image_focus_out_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	ImageWindow *imd = data;

	GTK_WIDGET_UNSET_FLAGS(imd->widget, GTK_HAS_FOCUS);
	image_focus_paint(imd, FALSE, NULL);

	return TRUE;
}

static gint image_scroll_cb(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->func_scroll &&
	    event && event->type == GDK_SCROLL)
		{
		imd->func_scroll(imd, event, imd->data_scroll);
		return TRUE;
		}

	return FALSE;
}

/*
 *-------------------------------------------------------------------
 * public interface
 *-------------------------------------------------------------------
 */

void image_attach_window(ImageWindow *imd, GtkWidget *window,
			 const gchar *title, const gchar *title_right, gint show_zoom)
{
	imd->top_window = window;
	g_free(imd->title);
	imd->title = g_strdup(title);
	g_free(imd->title_right);
	imd->title_right = g_strdup(title_right);
	imd->title_show_zoom = show_zoom;

	if (!options->image.fit_window_to_image) window = NULL;

	pixbuf_renderer_set_parent((PixbufRenderer *)imd->pr, (GtkWindow *)window);

	image_update_title(imd);
}

void image_set_update_func(ImageWindow *imd,
			   void (*func)(ImageWindow *imd, gpointer data),
			   gpointer data)
{
	imd->func_update = func;
	imd->data_update = data;
}

void image_set_complete_func(ImageWindow *imd,
			     void (*func)(ImageWindow *imd, gint preload, gpointer data),
			     gpointer data)
{
	imd->func_complete = func;
	imd->data_complete = data;
}

void image_set_state_func(ImageWindow *imd,
			void (*func)(ImageWindow *imd, ImageState state, gpointer data),
			gpointer data)
{
	imd->func_state = func;
	imd->data_state = data;
}


void image_set_button_func(ImageWindow *imd,
			   void (*func)(ImageWindow *, GdkEventButton *event, gpointer),
			   gpointer data)
{
	imd->func_button = func;
	imd->data_button = data;
}

void image_set_drag_func(ImageWindow *imd,
			   void (*func)(ImageWindow *, GdkEventButton *event, gdouble dx, gdouble dy, gpointer),
			   gpointer data)
{
	imd->func_drag = func;
	imd->data_drag = data;
}

void image_set_scroll_func(ImageWindow *imd,
			   void (*func)(ImageWindow *, GdkEventScroll *event, gpointer),
			   gpointer data)
{
	imd->func_scroll = func;
	imd->data_scroll = data;
}

void image_set_scroll_notify_func(ImageWindow *imd,
				  void (*func)(ImageWindow *imd, gint x, gint y, gint width, gint height, gpointer data),
				  gpointer data)
{
	imd->func_scroll_notify = func;
	imd->data_scroll_notify = data;
}

/* path, name */

const gchar *image_get_path(ImageWindow *imd)
{
	if (imd->image_fd == NULL) return NULL;
	return imd->image_fd->path;
}

const gchar *image_get_name(ImageWindow *imd)
{
	if (imd->image_fd == NULL) return NULL;
	return imd->image_fd->name;
}

FileData *image_get_fd(ImageWindow *imd)
{
	return imd->image_fd;
}

/* merely changes path string, does not change the image! */
void image_set_fd(ImageWindow *imd, FileData *fd)
{
	if (imd->auto_refresh && imd->image_fd)
		file_data_unregister_real_time_monitor(imd->image_fd);

	file_data_unref(imd->image_fd);
	imd->image_fd = file_data_ref(fd);

	image_update_title(imd);
	image_state_set(imd, IMAGE_STATE_IMAGE);

	if (imd->auto_refresh && imd->image_fd)
		file_data_register_real_time_monitor(imd->image_fd);
}

/* load a new image */

void image_change_fd(ImageWindow *imd, FileData *fd, gdouble zoom)
{
	if (imd->image_fd == fd) return;

	image_change_real(imd, fd, NULL, NULL, zoom);
}

gint image_get_image_size(ImageWindow *imd, gint *width, gint *height)
{
	return pixbuf_renderer_get_image_size(PIXBUF_RENDERER(imd->pr), width, height);
}

GdkPixbuf *image_get_pixbuf(ImageWindow *imd)
{
	return pixbuf_renderer_get_pixbuf((PixbufRenderer *)imd->pr);
}

void image_change_pixbuf(ImageWindow *imd, GdkPixbuf *pixbuf, gdouble zoom, gint lazy)
{

	ExifData *exif = NULL;
	gint read_exif_for_color_profile = (imd->color_profile_enable && imd->color_profile_use_image);
	gint read_exif_for_orientation = FALSE;

	if (imd->image_fd && imd->image_fd->user_orientation)
		imd->orientation = imd->image_fd->user_orientation;
	else if (options->image.exif_rotate_enable)
		read_exif_for_orientation = TRUE;

	if (read_exif_for_color_profile || read_exif_for_orientation)
		{
		gint orientation;

		exif = exif_read_fd(imd->image_fd);

		if (exif && read_exif_for_orientation)
			{
			if (exif_get_integer(exif, "Exif.Image.Orientation", &orientation))
				imd->orientation = orientation;
			else
				imd->orientation = 1;
			imd->image_fd->exif_orientation = imd->orientation;
			}
		}

	pixbuf_renderer_set_post_process_func((PixbufRenderer *)imd->pr, NULL, NULL, FALSE);
	if (imd->cm)
		{
		color_man_free(imd->cm);
		imd->cm = NULL;
		}

	if (lazy)
		{
		pixbuf_renderer_set_pixbuf_lazy((PixbufRenderer *)imd->pr, pixbuf, zoom, imd->orientation);
		}
	else
		{
		pixbuf_renderer_set_pixbuf((PixbufRenderer *)imd->pr, pixbuf, zoom);
		pixbuf_renderer_set_orientation((PixbufRenderer *)imd->pr, imd->orientation);
		}

	if (imd->color_profile_enable)
		{
		if (!image_post_process_color(imd, 0, exif, FALSE))
			{
			/* fixme: note error to user */
//			image_state_set(imd, IMAGE_STATE_COLOR_ADJ);
			}
		}

	if (read_exif_for_color_profile || read_exif_for_orientation)
		exif_free_fd(imd->image_fd, exif);

	if (imd->cm || imd->desaturate)
		pixbuf_renderer_set_post_process_func((PixbufRenderer *)imd->pr, image_post_process_tile_color_cb, (gpointer) imd, (imd->cm != NULL) );

	image_state_set(imd, IMAGE_STATE_IMAGE);
}

void image_change_from_collection(ImageWindow *imd, CollectionData *cd, CollectInfo *info, gdouble zoom)
{
	if (!cd || !info || !g_list_find(cd->list, info)) return;

	image_change_real(imd, info->fd, cd, info, zoom);
}

CollectionData *image_get_collection(ImageWindow *imd, CollectInfo **info)
{
	if (collection_to_number(imd->collection) >= 0)
		{
		if (g_list_find(imd->collection->list, imd->collection_info) != NULL)
			{
			if (info) *info = imd->collection_info;
			}
		else
			{
			if (info) *info = NULL;
			}
		return imd->collection;
		}

	if (info) *info = NULL;
	return NULL;
}

static void image_loader_sync_read_ahead_data(ImageLoader *il, gpointer old_data, gpointer data)
{
	if (g_signal_handlers_disconnect_by_func(G_OBJECT(il), (GCallback)image_read_ahead_error_cb, old_data))
		g_signal_connect (G_OBJECT(il), "error", (GCallback)image_read_ahead_error_cb, data);

	if (g_signal_handlers_disconnect_by_func(G_OBJECT(il), (GCallback)image_read_ahead_done_cb, old_data))
		g_signal_connect (G_OBJECT(il), "done", (GCallback)image_read_ahead_done_cb, data);
}

static void image_loader_sync_data(ImageLoader *il, gpointer old_data, gpointer data)
{		
	if (g_signal_handlers_disconnect_by_func(G_OBJECT(il), (GCallback)image_load_area_cb, old_data))
		g_signal_connect (G_OBJECT(il), "area_ready", (GCallback)image_load_area_cb, data);

	if (g_signal_handlers_disconnect_by_func(G_OBJECT(il), (GCallback)image_load_error_cb, old_data))
		g_signal_connect (G_OBJECT(il), "error", (GCallback)image_load_error_cb, data);

	if (g_signal_handlers_disconnect_by_func(G_OBJECT(il), (GCallback)image_load_done_cb, old_data))
		g_signal_connect (G_OBJECT(il), "done", (GCallback)image_load_done_cb, data);
}

/* this is more like a move function
 * it moves most data from source to imd
 */
void image_change_from_image(ImageWindow *imd, ImageWindow *source)
{
	if (imd == source) return;

	imd->unknown = source->unknown;

	imd->collection = source->collection;
	imd->collection_info = source->collection_info;

	image_loader_free(imd->il);
	imd->il = NULL;

	image_set_fd(imd, image_get_fd(source));


	if (source->il)
		{
		imd->il = source->il;
		source->il = NULL;

		image_loader_sync_data(imd->il, source, imd);

		imd->delay_alter_type = source->delay_alter_type;
		source->delay_alter_type = ALTER_NONE;
		}

	imd->color_profile_enable = source->color_profile_enable;
	imd->color_profile_input = source->color_profile_input;
	imd->color_profile_screen = source->color_profile_screen;
	imd->color_profile_use_image = source->color_profile_use_image;
	color_man_free((ColorMan *)imd->cm);
	imd->cm = NULL;
	if (source->cm)
		{
		ColorMan *cm;

		imd->cm = source->cm;
		source->cm = NULL;

		cm = (ColorMan *)imd->cm;
		cm->imd = imd;
		cm->func_done_data = imd;
		}

	image_loader_free(imd->read_ahead_il);
	imd->read_ahead_il = source->read_ahead_il;
	source->read_ahead_il = NULL;
	if (imd->read_ahead_il) image_loader_sync_read_ahead_data(imd->read_ahead_il, source, imd);

	file_data_unref(imd->read_ahead_fd);
	imd->read_ahead_fd = source->read_ahead_fd;
	source->read_ahead_fd = NULL;

	imd->completed = source->completed;
	imd->state = source->state;
	source->state = IMAGE_STATE_NONE;

	imd->orientation = source->orientation;
	imd->desaturate = source->desaturate;

	pixbuf_renderer_move(PIXBUF_RENDERER(imd->pr), PIXBUF_RENDERER(source->pr));

	if (imd->cm || imd->desaturate)
		pixbuf_renderer_set_post_process_func((PixbufRenderer *)imd->pr, image_post_process_tile_color_cb, (gpointer) imd, (imd->cm != NULL) );
	else
		pixbuf_renderer_set_post_process_func((PixbufRenderer *)imd->pr, NULL, NULL, TRUE);

}

/* manipulation */

void image_area_changed(ImageWindow *imd, gint x, gint y, gint width, gint height)
{
	pixbuf_renderer_area_changed((PixbufRenderer *)imd->pr, x, y, width, height);
}

void image_reload(ImageWindow *imd)
{
	if (pixbuf_renderer_get_tiles((PixbufRenderer *)imd->pr)) return;

	image_change_complete(imd, image_zoom_get(imd), FALSE);
}

void image_scroll(ImageWindow *imd, gint x, gint y)
{
	pixbuf_renderer_scroll((PixbufRenderer *)imd->pr, x, y);
}

void image_scroll_to_point(ImageWindow *imd, gint x, gint y,
			   gdouble x_align, gdouble y_align)
{
	pixbuf_renderer_scroll_to_point((PixbufRenderer *)imd->pr, x, y, x_align, y_align);
}

void image_get_scroll_center(ImageWindow *imd, gdouble *x, gdouble *y)
{
	pixbuf_renderer_get_scroll_center(PIXBUF_RENDERER(imd->pr), x, y);
}

void image_set_scroll_center(ImageWindow *imd, gdouble x, gdouble y)
{
	pixbuf_renderer_set_scroll_center(PIXBUF_RENDERER(imd->pr), x, y);
}

void image_zoom_adjust(ImageWindow *imd, gdouble increment)
{
	pixbuf_renderer_zoom_adjust((PixbufRenderer *)imd->pr, increment);
}

void image_zoom_adjust_at_point(ImageWindow *imd, gdouble increment, gint x, gint y)
{
	pixbuf_renderer_zoom_adjust_at_point((PixbufRenderer *)imd->pr, increment, x, y);
}

void image_zoom_set_limits(ImageWindow *imd, gdouble min, gdouble max)
{
	pixbuf_renderer_zoom_set_limits((PixbufRenderer *)imd->pr, min, max);
}

void image_zoom_set(ImageWindow *imd, gdouble zoom)
{
	pixbuf_renderer_zoom_set((PixbufRenderer *)imd->pr, zoom);
}

void image_zoom_set_fill_geometry(ImageWindow *imd, gint vertical)
{
	PixbufRenderer *pr;
	gdouble zoom;
	gint width, height;

	pr = (PixbufRenderer *)imd->pr;

	if (!pixbuf_renderer_get_pixbuf(pr) ||
	    !pixbuf_renderer_get_image_size(pr, &width, &height)) return;

	if (vertical)
		{
		zoom = (gdouble)pr->window_height / height;
		}
	else
		{
		zoom = (gdouble)pr->window_width / width;
		}

	if (zoom < 1.0)
		{
		zoom = 0.0 - 1.0 / zoom;
		}

	pixbuf_renderer_zoom_set(pr, zoom);
}

gdouble image_zoom_get(ImageWindow *imd)
{
	return pixbuf_renderer_zoom_get((PixbufRenderer *)imd->pr);
}

gdouble image_zoom_get_real(ImageWindow *imd)
{
	return pixbuf_renderer_zoom_get_scale((PixbufRenderer *)imd->pr);
}

gchar *image_zoom_get_as_text(ImageWindow *imd)
{
	gdouble zoom;
	gdouble scale;
	gdouble l = 1.0;
	gdouble r = 1.0;
	gint pl = 0;
	gint pr = 0;
	gchar *approx = " ";

	zoom = image_zoom_get(imd);
	scale = image_zoom_get_real(imd);

	if (zoom > 0.0)
		{
		l = zoom;
		}
	else if (zoom < 0.0)
		{
		r = 0.0 - zoom;
		}
	else if (zoom == 0.0 && scale != 0.0)
		{
		if (scale >= 1.0)
			{
			l = scale;
			}
		else
			{
			r = 1.0 / scale;
			}
		approx = "~";
		}

	if (rint(l) != l) pl = 1;
	if (rint(r) != r) pr = 1;

	return g_strdup_printf("%.*f :%s%.*f", pl, l, approx, pr, r);
}

gdouble image_zoom_get_default(ImageWindow *imd)
{
	gdouble zoom = 1.0;

	switch (options->image.zoom_mode)
	{
	case ZOOM_RESET_ORIGINAL:
		break;
	case ZOOM_RESET_FIT_WINDOW:
		zoom = 0.0;
		break;
	case ZOOM_RESET_NONE:
		if (imd) zoom = image_zoom_get(imd);
		break;
	}

	return zoom;
}

/* read ahead */

void image_prebuffer_set(ImageWindow *imd, FileData *fd)
{
	if (pixbuf_renderer_get_tiles((PixbufRenderer *)imd->pr)) return;

	if (fd)
		{
		if (!file_cache_get(image_get_cache(), fd))
			{
			image_read_ahead_set(imd, fd);
			}
		}
	else
		{
		image_read_ahead_cancel(imd);
		}
}

static void image_notify_cb(FileData *fd, NotifyType type, gpointer data)
{
	ImageWindow *imd = data;

	if (!imd || !image_get_pixbuf(imd) ||
	    /* imd->il || */ /* loading in progress - do not check - it should start from the beginning anyway */
	    !imd->image_fd || /* nothing to reload */
	    imd->state == IMAGE_STATE_NONE /* loading not started, no need to reload */
	    ) return;

	if (type == NOTIFY_TYPE_REREAD && fd == imd->image_fd)
		{
		image_reload(imd);
		}
}

void image_auto_refresh_enable(ImageWindow *imd, gboolean enable)
{
	if (!enable && imd->auto_refresh && imd->image_fd)
		file_data_unregister_real_time_monitor(imd->image_fd);

	if (enable && !imd->auto_refresh && imd->image_fd)
		file_data_register_real_time_monitor(imd->image_fd);

	imd->auto_refresh = enable;
}

void image_top_window_set_sync(ImageWindow *imd, gint allow_sync)
{
	imd->top_window_sync = allow_sync;

	g_object_set(G_OBJECT(imd->pr), "window_fit", allow_sync, NULL);
}

void image_background_set_color(ImageWindow *imd, GdkColor *color)
{
	pixbuf_renderer_set_color((PixbufRenderer *)imd->pr, color);
}

void image_color_profile_set(ImageWindow *imd,
			     gint input_type, gint screen_type,
			     gint use_image)
{
	if (!imd) return;

	if (input_type < 0 || input_type >= COLOR_PROFILE_FILE + COLOR_PROFILE_INPUTS ||
	    screen_type < 0 || screen_type > 1)
		{
		return;
		}

	imd->color_profile_input = input_type;
	imd->color_profile_screen = screen_type;
	imd->color_profile_use_image = use_image;
}

gint image_color_profile_get(ImageWindow *imd,
			     gint *input_type, gint *screen_type,
			     gint *use_image)
{
	if (!imd) return FALSE;

	if (input_type) *input_type = imd->color_profile_input;
	if (screen_type) *screen_type = imd->color_profile_screen;
	if (use_image) *use_image = imd->color_profile_use_image;

	return TRUE;
}

void image_color_profile_set_use(ImageWindow *imd, gint enable)
{
	if (!imd) return;

	if (imd->color_profile_enable == enable) return;

	imd->color_profile_enable = enable;
}

gint image_color_profile_get_use(ImageWindow *imd)
{
	if (!imd) return FALSE;

	return imd->color_profile_enable;
}

gint image_color_profile_get_from_image(ImageWindow *imd)
{
	if (!imd) return FALSE;

	return imd->color_profile_from_image;
}

void image_set_delay_flip(ImageWindow *imd, gint delay)
{
	if (!imd ||
	    imd->delay_flip == delay) return;

	imd->delay_flip = delay;

	g_object_set(G_OBJECT(imd->pr), "delay_flip", delay, NULL);

	if (!imd->delay_flip && imd->il)
		{
		PixbufRenderer *pr;

		pr = PIXBUF_RENDERER(imd->pr);
		if (pr->pixbuf) g_object_unref(pr->pixbuf);
		pr->pixbuf = NULL;

		image_load_pixbuf_ready(imd);
		}
}

void image_to_root_window(ImageWindow *imd, gint scaled)
{
	GdkScreen *screen;
	GdkWindow *rootwindow;
	GdkPixmap *pixmap;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pb;
	gint width, height;

	if (!imd) return;

	pixbuf = image_get_pixbuf(imd);
	if (!pixbuf) return;

	screen = gtk_widget_get_screen(imd->widget);
	rootwindow = gdk_screen_get_root_window(screen);
	if (gdk_drawable_get_visual(rootwindow) != gdk_visual_get_system()) return;

	if (scaled)
		{
		width = gdk_screen_width();
		height = gdk_screen_height();
		}
	else
		{
		pixbuf_renderer_get_scaled_size((PixbufRenderer *)imd->pr, &width, &height);
		}

	pb = gdk_pixbuf_scale_simple(pixbuf, width, height, (GdkInterpType)options->image.zoom_quality);

	gdk_pixbuf_render_pixmap_and_mask(pb, &pixmap, NULL, 128);
	gdk_window_set_back_pixmap(rootwindow, pixmap, FALSE);
	gdk_window_clear(rootwindow);
	g_object_unref(pb);
	g_object_unref(pixmap);

	gdk_flush();
}

void image_select(ImageWindow *imd, gboolean select)
{
	if (imd->has_frame)
		{
		if (select)
			{
			gtk_widget_set_state(imd->widget, GTK_STATE_SELECTED);
			gtk_widget_set_state(imd->pr, GTK_STATE_NORMAL); /* do not propagate */
			}
		else
			gtk_widget_set_state(imd->widget, GTK_STATE_NORMAL);
		}
}



void image_set_selectable(ImageWindow *imd, gboolean selectable)
{
	if (imd->has_frame)
		{
		if (selectable)
			{
			gtk_frame_set_shadow_type(GTK_FRAME(imd->frame), GTK_SHADOW_NONE);
			gtk_container_set_border_width(GTK_CONTAINER(imd->frame), 4);
			}
		else
			{
			gtk_frame_set_shadow_type(GTK_FRAME(imd->frame), GTK_SHADOW_NONE);
			gtk_container_set_border_width(GTK_CONTAINER(imd->frame), 0);
			}
		}
}

/*
 *-------------------------------------------------------------------
 * prefs sync
 *-------------------------------------------------------------------
 */

static void image_options_set(ImageWindow *imd)
{
	g_object_set(G_OBJECT(imd->pr), "zoom_quality", options->image.zoom_quality,
					"zoom_2pass", options->image.zoom_2pass,
					"zoom_expand", options->image.zoom_to_fit_allow_expand,
					"dither_quality", options->image.dither_quality,
					"scroll_reset", options->image.scroll_reset_method,
					"cache_display", options->image.tile_cache_max,
					"window_fit", (imd->top_window_sync && options->image.fit_window_to_image),
					"window_limit", options->image.limit_window_size,
					"window_limit_value", options->image.max_window_size,
					"autofit_limit", options->image.limit_autofit_size,
					"autofit_limit_value", options->image.max_autofit_size,

					NULL);

	pixbuf_renderer_set_parent((PixbufRenderer *)imd->pr, (GtkWindow *)imd->top_window);
}

void image_options_sync(void)
{
	GList *work;

	work = image_list;
	while (work)
		{
		ImageWindow *imd;

		imd = work->data;
		work = work->next;

		image_options_set(imd);
		}
}

/*
 *-------------------------------------------------------------------
 * init / destroy
 *-------------------------------------------------------------------
 */

static void image_free(ImageWindow *imd)
{
	image_list = g_list_remove(image_list, imd);

	if (imd->auto_refresh && imd->image_fd)
		file_data_unregister_real_time_monitor(imd->image_fd);

	file_data_unregister_notify_func(image_notify_cb, imd);

	image_reset(imd);

	image_read_ahead_cancel(imd);

	file_data_unref(imd->image_fd);
	g_free(imd->title);
	g_free(imd->title_right);
	g_free(imd);
}

static void image_destroy_cb(GtkObject *widget, gpointer data)
{
	ImageWindow *imd = data;
	image_free(imd);
}

gboolean selectable_frame_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	gtk_paint_flat_box(widget->style,
			   widget->window,
			   widget->state,
			   (GTK_FRAME(widget))->shadow_type,
			   NULL,
			   widget,
			   NULL,
			   widget->allocation.x + 3, widget->allocation.y + 3,
			   widget->allocation.width - 6, widget->allocation.height - 6);


	return FALSE;
}


void image_set_frame(ImageWindow *imd, gboolean frame)
{
	frame = !!frame;

	if (frame == imd->has_frame) return;

	gtk_widget_hide(imd->pr);

	if (frame)
		{
		imd->frame = gtk_frame_new(NULL);
#if GTK_CHECK_VERSION(2,12,0)
        g_object_ref(imd->pr);
#else
        gtk_widget_ref(imd->pr);
#endif
		if (imd->has_frame != -1) gtk_container_remove(GTK_CONTAINER(imd->widget), imd->pr);
		gtk_container_add(GTK_CONTAINER(imd->frame), imd->pr);

#if GTK_CHECK_VERSION(2,12,0)
        g_object_unref(imd->pr);
#else
        gtk_widget_unref(imd->pr);
#endif
		g_signal_connect(G_OBJECT(imd->frame), "expose_event",
		    		 G_CALLBACK(selectable_frame_expose_cb), NULL);

		GTK_WIDGET_SET_FLAGS(imd->frame, GTK_CAN_FOCUS);
		g_signal_connect(G_OBJECT(imd->frame), "focus_in_event",
				 G_CALLBACK(image_focus_in_cb), imd);
		g_signal_connect(G_OBJECT(imd->frame), "focus_out_event",
				 G_CALLBACK(image_focus_out_cb), imd);

		g_signal_connect_after(G_OBJECT(imd->frame), "expose_event",
				       G_CALLBACK(image_focus_expose), imd);


		gtk_box_pack_start_defaults(GTK_BOX(imd->widget), imd->frame);
		gtk_widget_show(imd->frame);
		}
	else
		{
#if GTK_CHECK_VERSION(2,12,0)
		g_object_ref(imd->pr);
#else
		gtk_widget_ref(imd->pr);
#endif
		if (imd->frame)
			{
			gtk_container_remove(GTK_CONTAINER(imd->frame), imd->pr);
			gtk_widget_destroy(imd->frame);
			imd->frame = NULL;
			}
		gtk_box_pack_start_defaults(GTK_BOX(imd->widget), imd->pr);

#if GTK_CHECK_VERSION(2,12,0)
	g_object_unref(imd->pr);
#else
	gtk_widget_unref(imd->pr);
#endif
		}

	gtk_widget_show(imd->pr);

	imd->has_frame = frame;
}

ImageWindow *image_new(gint frame)
{
	ImageWindow *imd;

	imd = g_new0(ImageWindow, 1);

	imd->top_window = NULL;
	imd->title = NULL;
	imd->title_right = NULL;
	imd->title_show_zoom = FALSE;

	imd->unknown = TRUE;

	imd->has_frame = -1; /* not initialized; for image_set_frame */
	imd->top_window_sync = FALSE;

	imd->delay_alter_type = ALTER_NONE;

	imd->read_ahead_il = NULL;
	imd->read_ahead_fd = NULL;

	imd->completed = FALSE;
	imd->state = IMAGE_STATE_NONE;

	imd->color_profile_enable = FALSE;
	imd->color_profile_input = 0;
	imd->color_profile_screen = 0;
	imd->color_profile_use_image = FALSE;
	imd->color_profile_from_image = COLOR_PROFILE_NONE;

	imd->auto_refresh = FALSE;

	imd->delay_flip = FALSE;

	imd->func_update = NULL;
	imd->func_complete = NULL;
	imd->func_tile_request = NULL;
	imd->func_tile_dispose = NULL;

	imd->func_button = NULL;
	imd->func_scroll = NULL;

	imd->orientation = 1;

	imd->pr = GTK_WIDGET(pixbuf_renderer_new());

	image_options_set(imd);


	imd->widget = gtk_vbox_new(0, 0);

	image_set_frame(imd, frame);

	image_set_selectable(imd, 0);

	g_signal_connect(G_OBJECT(imd->pr), "clicked",
			 G_CALLBACK(image_click_cb), imd);
	g_signal_connect(G_OBJECT(imd->pr), "scroll_notify",
			 G_CALLBACK(image_scroll_notify_cb), imd);

	g_signal_connect(G_OBJECT(imd->pr), "scroll_event",
			 G_CALLBACK(image_scroll_cb), imd);

	g_signal_connect(G_OBJECT(imd->pr), "destroy",
			 G_CALLBACK(image_destroy_cb), imd);

	g_signal_connect(G_OBJECT(imd->pr), "zoom",
			 G_CALLBACK(image_zoom_cb), imd);
	g_signal_connect(G_OBJECT(imd->pr), "render_complete",
			 G_CALLBACK(image_render_complete_cb), imd);
	g_signal_connect(G_OBJECT(imd->pr), "drag",
			 G_CALLBACK(image_drag_cb), imd);

	file_data_register_notify_func(image_notify_cb, imd, NOTIFY_PRIORITY_LOW);

	image_list = g_list_append(image_list, imd);

	return imd;
}
