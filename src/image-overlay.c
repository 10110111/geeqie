/*
 * GQview
 * (C) 2004 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "gqview.h"
#include "image-overlay.h"

#include "collect.h"
#include "filelist.h"
#include "image.h"
#include "img-view.h"
#include "layout.h"
#include "pixbuf_util.h"


/*
 *----------------------------------------------------------------------------
 * image overlay
 *----------------------------------------------------------------------------
 */

typedef struct _OverlayUpdate OverlayUpdate;
struct _OverlayUpdate {
	ImageWindow *imd;
	gint id;
	gint idle_id;
	gulong destroy_id;
};

#define IMAGE_OVERLAY_UPDATE_KEY "image-overlay-update"

#define IMAGE_OVERLAY_X 10
#define IMAGE_OVERLAY_Y -10


static GdkPixbuf *image_overlay_info_render(ImageWindow *imd)
{
	GdkPixbuf *pixbuf;
	gint width, height;
	PangoLayout *layout;
	const gchar *name;
	gchar *name_escaped;
	gchar *text;
	gchar *size;
	gint n, t;
	CollectionData *cd;
	CollectInfo *info;
	gchar *ct;

	name = image_get_name(imd);
	if (name)
		{
		name_escaped = g_markup_escape_text(name, -1);
		}
	else
		{
		name_escaped = NULL;
		}

	cd = image_get_collection(imd, &info);
	if (cd)
		{
		gchar *buf;

		t = g_list_length(cd->list);
		n = g_list_index(cd->list, info) + 1;
		buf = g_markup_escape_text((cd->name) ? cd->name : _("Untitled"), -1);
		ct = g_strdup_printf("<i>%s</i>\n", buf);
		g_free(buf);
		}
	else
		{
		LayoutWindow *lw;

		lw = layout_find_by_image(imd);
		if (lw)
			{
			if (lw->slideshow)
				{
				n = g_list_length(lw->slideshow->list_done);
				t = n + g_list_length(lw->slideshow->list);
				}
			else
				{
				t = layout_list_count(lw, NULL);
				n = layout_list_get_index(lw, image_get_path(lw->image)) + 1;
				}
			}
		else if (view_window_find_image(imd, &n, &t))
			{
			n++;
			}
		else
			{
			t = 1;
			n = 1;
			}

		if (n < 1) n = 1;
		if (t < 1) t = 1;

		ct = g_strdup("");
		}

	size = text_from_size_abrev(imd->size);
	if (!name_escaped)
		{
		text = g_strdup_printf(_("Untitled"));
		}
	else if (imd->unknown)
		{
		text = g_strdup_printf("%s(%d/%d) <b>%s</b>\n%s - %s", ct,
				       n, t, name_escaped,
				       text_from_time(imd->mtime), size);
		}
	else
		{
		gint w, h;

		if (imd->delay_flip &&
		    imd->il && imd->il->pixbuf &&
		    imd->pixbuf != imd->il->pixbuf)
			{
			w = gdk_pixbuf_get_width(imd->il->pixbuf);
			h = gdk_pixbuf_get_height(imd->il->pixbuf);
			}
		else
			{
			w = imd->image_width;
			h = imd->image_height;
			}

		text = g_strdup_printf("%s(%d/%d) <b>%s</b>\n%d x %d - %s - %s", ct,
				       n, t, name_escaped,
				       w, h,
				       text_from_time(imd->mtime), size);
		}
	g_free(size);
	g_free(ct);
	g_free(name_escaped);

	layout = gtk_widget_create_pango_layout(imd->image, NULL);
	pango_layout_set_markup(layout, text, -1);
	g_free(text);

	pango_layout_get_pixel_size(layout, &width, &height);

	width += 10;
	height += 10;

	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
	pixbuf_set_rect_fill(pixbuf, 3, 3, width-6, height-6, 240, 240, 240, 210);
	pixbuf_set_rect(pixbuf, 0, 0, width, height, 240, 240, 240, 80, 1, 1, 1, 1);
	pixbuf_set_rect(pixbuf, 1, 1, width-2, height-2, 240, 240, 240, 130, 1, 1, 1, 1);
	pixbuf_set_rect(pixbuf, 2, 2, width-4, height-4, 240, 240, 240, 180, 1, 1, 1, 1);
	pixbuf_pixel_set(pixbuf, 0, 0, 0, 0, 0, 0);
	pixbuf_pixel_set(pixbuf, width - 1, 0, 0, 0, 0, 0);
	pixbuf_pixel_set(pixbuf, 0, height - 1, 0, 0, 0, 0);
	pixbuf_pixel_set(pixbuf, width - 1, height - 1, 0, 0, 0, 0);

	pixbuf_draw_layout(pixbuf, layout, imd->image, 5, 5, 0, 0, 0, 255);

	g_object_unref(G_OBJECT(layout));

	return pixbuf;
}

static void image_overlay_update_destroy_cb(GtkWidget *widget, gpointer data)
{
	OverlayUpdate *ou = data;

	g_source_remove(ou->idle_id);
	g_free(ou);
}

static gint image_overlay_update_cb(gpointer data)
{
	OverlayUpdate *ou = data;
	GdkPixbuf *pixbuf;

	pixbuf = image_overlay_info_render(ou->imd);
	image_overlay_set(ou->imd, ou->id, pixbuf, IMAGE_OVERLAY_X, IMAGE_OVERLAY_Y);
	g_object_unref(pixbuf);

	g_object_set_data(G_OBJECT(ou->imd->image), IMAGE_OVERLAY_UPDATE_KEY, NULL);
	g_signal_handler_disconnect(ou->imd->image, ou->destroy_id);
	g_free(ou);

	return FALSE;
}

static void image_overlay_update_schedule(ImageWindow *imd, gint id)
{
	OverlayUpdate *ou;

	ou = g_object_get_data(G_OBJECT(imd->image), IMAGE_OVERLAY_UPDATE_KEY);
	if (ou) return;

	ou = g_new0(OverlayUpdate, 1);
	ou->imd = imd;
	ou->id = id;
	ou->idle_id = g_idle_add_full(G_PRIORITY_HIGH, image_overlay_update_cb, ou, NULL);
	ou->destroy_id = g_signal_connect(G_OBJECT(imd->image), "destroy",
					  G_CALLBACK(image_overlay_update_destroy_cb), ou);
	g_object_set_data(G_OBJECT(imd->image), IMAGE_OVERLAY_UPDATE_KEY, ou);
}

void image_overlay_update(ImageWindow *imd, gint id)
{
	if (id < 0) return;
	image_overlay_update_schedule(imd, id);
}

static void image_overlay_upate_cb(ImageWindow *imd, gpointer data)
{
	gint id;

	id = GPOINTER_TO_INT(data);
	image_overlay_update_schedule(imd, id);
}

gint image_overlay_info_enable(ImageWindow *imd)
{
	gint id;
	GdkPixbuf *pixbuf;

	pixbuf = image_overlay_info_render(imd);
	id = image_overlay_add(imd, pixbuf, IMAGE_OVERLAY_X, IMAGE_OVERLAY_Y, TRUE, FALSE);
	g_object_unref(pixbuf);

	image_set_new_func(imd, image_overlay_upate_cb, GINT_TO_POINTER(id));

	return id;
}

void image_overlay_info_disable(ImageWindow *imd, gint id)
{
	image_set_new_func(imd, NULL, NULL);
	image_overlay_remove(imd, id);
}

