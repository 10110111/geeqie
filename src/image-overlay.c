/*
 * GQview
 * (C) 2006 John Ellis
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
#include "pixbuf-renderer.h"
#include "pixbuf_util.h"


/*
 *----------------------------------------------------------------------------
 * image overlay
 *----------------------------------------------------------------------------
 */

typedef struct _OverlayStateData OverlayStateData;
struct _OverlayStateData {
	ImageWindow *imd;
	ImageState changed_states;

	gint show_info;
	gint show_status;

	gint ovl_info;
	gint ovl_color;
	gint ovl_rotate;
	gint ovl_end;

	gint idle_id;
	gint timer_id;
	gulong destroy_id;
};

#define OSD_DATA "overlay-data"

#define OSD_INFO_X 10
#define OSD_INFO_Y -10


static GdkPixbuf *image_osd_info_render(ImageWindow *imd)
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
		    image_get_pixbuf(imd) != imd->il->pixbuf)
			{
			w = gdk_pixbuf_get_width(imd->il->pixbuf);
			h = gdk_pixbuf_get_height(imd->il->pixbuf);
			}
		else
			{
			pixbuf_renderer_get_image_size(PIXBUF_RENDERER(imd->pr), &w, &h);
			}

		text = g_strdup_printf("%s(%d/%d) <b>%s</b>\n%d x %d - %s - %s", ct,
				       n, t, name_escaped,
				       w, h,
				       text_from_time(imd->mtime), size);
		}
	g_free(size);
	g_free(ct);
	g_free(name_escaped);

	layout = gtk_widget_create_pango_layout(imd->pr, NULL);
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

	pixbuf_draw_layout(pixbuf, layout, imd->pr, 5, 5, 0, 0, 0, 255);

	g_object_unref(G_OBJECT(layout));

	return pixbuf;
}

static void image_osd_ovl_reset(OverlayStateData *osd, gint *id)
{
	if (*id)
		{
		image_overlay_remove(osd->imd, *id);
		*id = 0;
		}
}

static gint image_osd_update_cb(gpointer data)
{
	OverlayStateData *osd = data;

	if (osd->show_info)
		{
		if (osd->changed_states & IMAGE_STATE_IMAGE)
			{
			GdkPixbuf *pixbuf;

			pixbuf = image_osd_info_render(osd->imd);
			if (osd->ovl_info == 0)
				{
				osd->ovl_info = image_overlay_add(osd->imd, pixbuf,
								  OSD_INFO_X, OSD_INFO_Y, TRUE, FALSE);
				}
			else
				{
				image_overlay_set(osd->imd, osd->ovl_info, pixbuf, OSD_INFO_X, OSD_INFO_Y);
				}
			g_object_unref(pixbuf);
			}
		}
	else
		{
		image_osd_ovl_reset(osd, & osd->ovl_info);
		}

	if (osd->show_status)
		{
		}
	else
		{
		image_osd_ovl_reset(osd, & osd->ovl_color);
		image_osd_ovl_reset(osd, & osd->ovl_rotate);
		image_osd_ovl_reset(osd, & osd->ovl_end);
		}

	osd->idle_id = -1;
	return FALSE;
}

static void image_osd_update_schedule(OverlayStateData *osd, gint force)
{
	if (force) osd->changed_states |= IMAGE_STATE_IMAGE;

	if (osd->idle_id == -1)
		{
		osd->idle_id = g_idle_add_full(G_PRIORITY_HIGH, image_osd_update_cb, osd, NULL);
		}
}

void image_osd_update(ImageWindow *imd)
{
	OverlayStateData *osd;

	if (!imd) return;

	osd = g_object_get_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA");
	if (!osd) return;

	image_osd_update_schedule(osd, TRUE);
}

static void image_osd_state_cb(ImageWindow *imd, ImageState state, gpointer data)
{
	OverlayStateData *osd = data;

	osd->changed_states |= state;
	image_osd_update_schedule(osd, FALSE);
}

static void image_osd_free(OverlayStateData *osd)
{
	if (!osd) return;

	if (osd->idle_id != -1) g_source_remove(osd->idle_id);

	if (osd->imd)
		{
		g_object_set_data(G_OBJECT(osd->imd->pr), "IMAGE_OVERLAY_DATA", NULL);
		g_signal_handler_disconnect(osd->imd->pr, osd->destroy_id);

		image_set_state_func(osd->imd, NULL, NULL);
		image_overlay_remove(osd->imd, osd->ovl_info);
		}

	g_free(osd);
}

static void image_osd_remove(ImageWindow *imd)
{
	OverlayStateData *osd;

	osd = g_object_get_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA");
	image_osd_free(osd);
}

static void image_osd_destroy_cb(GtkWidget *widget, gpointer data)
{
	OverlayStateData *osd = data;

	osd->imd = NULL;
	image_osd_free(osd);
}

static void image_osd_enable(ImageWindow *imd, gint info, gint status)
{
	OverlayStateData *osd;

	osd = g_object_get_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA");
	if (!osd)
		{
		osd = g_new0(OverlayStateData, 1);
		osd->imd = imd;
		osd->idle_id = -1;
		osd->timer_id = -1;

		osd->destroy_id = g_signal_connect(G_OBJECT(imd->pr), "destroy",
						   G_CALLBACK(image_osd_destroy_cb), osd);
		g_object_set_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA", osd);

		image_set_state_func(osd->imd, image_osd_state_cb, osd);
		}

	if (osd->show_info != info ||
	    osd->show_status != status)
		{
		osd->show_info = info;
		osd->show_status = status;

		image_osd_update_schedule(osd, TRUE);
		}
}

void image_osd_set(ImageWindow *imd, gint info, gint status)
{
	if (!imd) return;

	if (!info && !status)
		{
		image_osd_remove(imd);
		return;
		}

	image_osd_enable(imd, info, status);
}

gint image_osd_get(ImageWindow *imd, gint *info, gint *status)
{
	OverlayStateData *osd;

	if (!imd) return FALSE;

	osd = g_object_get_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA");
	if (!osd) return FALSE;

	if (info) *info = osd->show_info;
	if (status) *status = osd->show_status;

	return TRUE;
}

