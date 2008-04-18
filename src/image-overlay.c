/*
 * Geeqie
 * (C) 2006 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "image-overlay.h"

#include "collect.h"
#include "exif.h"
#include "filelist.h"
#include "image.h"
#include "img-view.h"
#include "layout.h"
#include "pixbuf-renderer.h"
#include "pixbuf_util.h"
#include "histogram.h"


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

	gint icon_time[IMAGE_OSD_COUNT];
	gint icon_id[IMAGE_OSD_COUNT];

	gint idle_id;
	gint timer_id;
	gulong destroy_id;
};


typedef struct _OSDIcon OSDIcon;
struct _OSDIcon {
	gint reset;	/* reset on new image */
	gint x;		/* x, y offset */
	gint y;
	gchar *key;	/* inline pixbuf */
};

static OSDIcon osd_icons[] = {
	{  TRUE,   0,   0, NULL },			/* none */
	{  TRUE, -10, -10, NULL },			/* auto rotated */
	{  TRUE, -10, -10, NULL },			/* user rotated */
	{  TRUE, -40, -10, NULL },			/* color embedded */
	{  TRUE, -70, -10, NULL },			/* first image */
	{  TRUE, -70, -10, NULL },			/* last image */
	{ FALSE, -70, -10, NULL },			/* osd enabled */
	{ FALSE, 0, 0, NULL }
};

#define OSD_DATA "overlay-data"

#define OSD_INFO_X 10
#define OSD_INFO_Y -10

#define IMAGE_OSD_DEFAULT_DURATION 30

#define HISTOGRAM_HEIGHT 140
/*
 *----------------------------------------------------------------------------
 * image histogram
 *----------------------------------------------------------------------------
 */


#define HIST_PREPARE(imd, lw)                          \
       LayoutWindow *lw = NULL;                        \
       if (imd)                                        \
               lw = layout_find_by_image(imd);

void image_osd_histogram_onoff_toggle(ImageWindow *imd, gint x)
{
	HIST_PREPARE(imd, lw)
	if (lw) 
    		{
        	lw->histogram_enabled = !!(x);
		if (lw->histogram_enabled && !lw->histogram)
			lw->histogram = histogram_new();
		}
}

gint image_osd_histogram_onoff_status(ImageWindow *imd)
{
       HIST_PREPARE(imd, lw)
       return lw ?  lw->histogram_enabled : FALSE;
}

void image_osd_histogram_chan_toggle(ImageWindow *imd)
{
       HIST_PREPARE(imd, lw)
       if (lw && lw->histogram) 
               histogram_set_channel(lw->histogram, (histogram_get_channel(lw->histogram) +1)%HCHAN_COUNT);
}

void image_osd_histogram_log_toggle(ImageWindow *imd)
{
       HIST_PREPARE(imd,lw)
       if (lw && lw->histogram) 
               histogram_set_mode(lw->histogram, !histogram_get_mode(lw->histogram));
}



static void image_osd_timer_schedule(OverlayStateData *osd);

static gchar *image_osd_mkinfo(const gchar *str, ImageWindow *imd, GHashTable *vars)
{
	gchar delim = '%', imp = '|', sep[] = " - ";
	gchar *start, *end;
	gint pos, prev;
	gint last;
	gchar *name, *data;
	GString *new;
	gchar *ret;
	ExifData *exif;

	if (!str || !*str) return g_strdup("");
	
	new = g_string_new(str);

	exif = exif_read_fd(imd->image_fd, FALSE);
	prev = 0;
	last = FALSE;

	while (TRUE)
		{
		gint was_digit = 0;
		gint limit = 0;
		gchar *trunc = NULL;
		gchar *p;

		start = strchr(new->str, delim);
		if (!start)
			break;
		end = strchr(start+1, delim);
		if (!end)
			break;
		
		for (p = end; p > start; p--)
			{
			if (*p == ':' && was_digit)
				{
				trunc = p;
				break;
				}
			was_digit = g_ascii_isdigit(*p);
			}

		if (trunc) limit = atoi(trunc+1);

		name = g_strndup(start+1, ((limit > 0) ? trunc : end)-start-1);

		pos = start-new->str;
		data = g_strdup(g_hash_table_lookup(vars, name));
		if (!data && exif)
			data = exif_get_data_as_text(exif, name);
		if (data && *data && limit > 0 && strlen(data) > limit + 3)
			{
			gchar *new_data = g_strdup_printf("%-*.*s...", limit, limit, data);
			g_free(data);
			data = new_data;
			}
		
		g_string_erase(new, pos, end-start+1);
		if (data)
			g_string_insert(new, pos, data);
		if (pos-prev == 2 && new->str[pos-1] == imp)
			{
			g_string_erase(new, --pos, 1);
			if (last && data)
				{
				g_string_insert(new, pos, sep);
				pos += strlen(sep);
				}
			}

		prev = data ? pos+strlen(data)-1 : pos-1;
		last = data ? TRUE : last;
		g_free(name);
		g_free(data);
		}
	
	/* search and destroy empty lines */
	end = new->str;
	while ((start = strchr(end, '\n')))
		{
		end = start;
		while (*++(end) == '\n')
			;
		g_string_erase(new, start-new->str, end-start-1);
		}

	g_strchomp(new->str);

	ret = new->str;
	g_string_free(new, FALSE);

	return ret;
}

static GdkPixbuf *image_osd_info_render(ImageWindow *imd)
{
	GdkPixbuf *pixbuf = NULL;
	gint width, height;
	PangoLayout *layout;
	const gchar *name;
	gchar *name_escaped;
	gchar *text;
	gchar *size;
	gint n, t;
	CollectionData *cd;
	CollectInfo *info;
	GdkPixbuf *imgpixbuf = NULL;
	LayoutWindow *lw = NULL;
	gint with_hist = 0;
    	gchar *ct;
    	gint w, h;
	GHashTable *vars;

	vars = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

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
		lw = layout_find_by_image(imd);
		if (lw)
			{
			if (lw->slideshow)
				{
				n = g_list_length(lw->slideshow->list_done);
				t = n + g_list_length(lw->slideshow->list);
				if (n == 0) n = t;
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
	if (!imd->unknown)
		{
		if (imd->delay_flip &&
		    imd->il && imd->il->pixbuf &&
		    image_get_pixbuf(imd) != imd->il->pixbuf)
			{
			w = gdk_pixbuf_get_width(imd->il->pixbuf);
			h = gdk_pixbuf_get_height(imd->il->pixbuf);
			imgpixbuf = imd->il->pixbuf;
			}
		else
			{
			pixbuf_renderer_get_image_size(PIXBUF_RENDERER(imd->pr), &w, &h);
			imgpixbuf = (PIXBUF_RENDERER(imd->pr))->pixbuf;
			}
		if (!lw)
			lw = layout_find_by_image(imd);

		if (imgpixbuf && lw->histogram && lw->histogram_enabled 
			      && (!imd->il || imd->il->done)) 
			with_hist=1;

 		g_hash_table_insert(vars, "width", g_strdup_printf("%d", w));
 		g_hash_table_insert(vars, "height", g_strdup_printf("%d", h));
 		g_hash_table_insert(vars, "res", g_strdup_printf("%d × %d", w, h));
 		}
  
 	g_hash_table_insert(vars, "collection", g_strdup(ct));
 	g_hash_table_insert(vars, "number", g_strdup_printf("%d", n));
 	g_hash_table_insert(vars, "total", g_strdup_printf("%d", t));
 	g_hash_table_insert(vars, "name", g_strdup(name_escaped));
 	g_hash_table_insert(vars, "date", g_strdup(text_from_time(imd->mtime)));
 	g_hash_table_insert(vars, "size", g_strdup(size));
  
 	if (!name_escaped)
 		{
 		text = g_strdup_printf(_("Untitled"));
 		}
 	else
 		{
 		text = image_osd_mkinfo(options->fullscreen.info, imd, vars);
		}

	g_free(size);
  	g_free(ct);
  	g_free(name_escaped);
	g_hash_table_destroy(vars);

	{
	FileData *fd = image_get_fd(imd);

	if (fd) /* fd may be null after file deletion */
		{
		gint active_marks = 0;
		gint mark;
		gchar *text2;

		for (mark = 0; mark < FILEDATA_MARKS_SIZE; mark++) 
			{
			active_marks += fd->marks[mark];
			}


		if (active_marks > 0)
			{
			GString *buf = g_string_sized_new(FILEDATA_MARKS_SIZE * 2);
	        
			for (mark = 0; mark < FILEDATA_MARKS_SIZE; mark++) 
				{
				g_string_append_printf(buf, fd->marks[mark] ? " <span background='#FF00FF'>%c</span>" : " %c", '1' + mark);
    				}

			if (*text)
				text2 = g_strdup_printf("%s\n%s", text, buf->str);
			else
				text2 = g_strdup(buf->str);
			g_string_free(buf, TRUE);
			g_free(text);
			text = text2;
			}

    		if (with_hist)
			{
			if (*text)
				text2 = g_strdup_printf("%s\n%s", text, histogram_label(lw->histogram));
			else
				text2 = g_strdup(histogram_label(lw->histogram));
			g_free(text);
			text = text2;
			}
		}
	}

	layout = gtk_widget_create_pango_layout(imd->pr, NULL);
	pango_layout_set_markup(layout, text, -1);
printf("|%s|\n",text);
	g_free(text);
    
	pango_layout_get_pixel_size(layout, &width, &height);
	/* with empty text width is set to 0, but not height) */
	if (width == 0)
		height = 0;
	else if (height == 0)
		width = 0;
	if (width > 0) width += 10;
	if (height > 0) height += 10;

	if (with_hist)
		{
		histogram_read(lw->histogram, imgpixbuf);
		if (width < 266) width = 266;
		height += HISTOGRAM_HEIGHT + 5;
		}

	if (width > 0 && height > 0)
		{
		/* TODO: make osd color configurable --Zas */
		pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
		pixbuf_set_rect_fill(pixbuf, 3, 3, width-6, height-6, 240, 240, 240, 210);
		pixbuf_set_rect(pixbuf, 0, 0, width, height, 240, 240, 240, 80, 1, 1, 1, 1);
		pixbuf_set_rect(pixbuf, 1, 1, width-2, height-2, 240, 240, 240, 130, 1, 1, 1, 1);
		pixbuf_set_rect(pixbuf, 2, 2, width-4, height-4, 240, 240, 240, 180, 1, 1, 1, 1);
		pixbuf_pixel_set(pixbuf, 0, 0, 0, 0, 0, 0);
		pixbuf_pixel_set(pixbuf, width - 1, 0, 0, 0, 0, 0);
		pixbuf_pixel_set(pixbuf, 0, height - 1, 0, 0, 0, 0);
		pixbuf_pixel_set(pixbuf, width - 1, height - 1, 0, 0, 0, 0);
	
		if (with_hist)
			histogram_draw(lw->histogram, pixbuf, 5, height - HISTOGRAM_HEIGHT - 5 , width - 10, HISTOGRAM_HEIGHT);
		
		pixbuf_draw_layout(pixbuf, layout, imd->pr, 5, 5, 0, 0, 0, 255);
	}

	g_object_unref(G_OBJECT(layout));

	return pixbuf;
}

static GdkPixbuf *image_osd_icon_pixbuf(ImageOSDFlag flag)
{
	static GdkPixbuf **icons = NULL;
	GdkPixbuf *icon = NULL;

	if (!icons) icons = g_new0(GdkPixbuf *, IMAGE_OSD_COUNT);
	if (icons[flag]) return icons[flag];

	if (osd_icons[flag].key)
		{
		icon = pixbuf_inline(osd_icons[flag].key);
		}

	if (!icon)
		{
		icon = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 24, 24);
		pixbuf_set_rect_fill(icon, 1, 1, 22, 22, 255, 255, 255, 200);
		pixbuf_set_rect(icon, 0, 0, 24, 24, 0, 0, 0, 128, 1, 1, 1, 1);
		switch (flag)
			{
			case IMAGE_OSD_ROTATE_AUTO:
				pixbuf_set_rect(icon, 3, 8, 11, 12,
						0, 0, 0, 255,
						3, 0, 3, 0);
				pixbuf_draw_triangle(icon, 14, 3, 6, 12,
						     20, 9, 14, 15, 14, 3,
						     0, 0, 0, 255);
				break;
			case IMAGE_OSD_ROTATE_USER:
				break;
			case IMAGE_OSD_COLOR:
				pixbuf_set_rect_fill(icon, 3, 3, 18, 6, 200, 0, 0, 255);
				pixbuf_set_rect_fill(icon, 3, 9, 18, 6, 0, 200, 0, 255);
				pixbuf_set_rect_fill(icon, 3, 15, 18, 6, 0, 0, 200, 255);
				break;
			case IMAGE_OSD_FIRST:
				pixbuf_set_rect(icon, 3, 3, 18, 18, 0, 0, 0, 200, 3, 3, 3, 0);
				pixbuf_draw_triangle(icon, 6, 5, 12, 6,
						     12, 5, 18, 11, 6, 11,
						     0, 0, 0, 255);
				break;
			case IMAGE_OSD_LAST:
				pixbuf_set_rect(icon, 3, 3, 18, 18, 0, 0, 0, 200, 3, 3, 0, 3);
				pixbuf_draw_triangle(icon, 6, 12, 12, 6,
						     12, 18, 6, 12, 18, 12,
						     0, 0, 0, 255);
				break;
			case IMAGE_OSD_ICON:
				pixbuf_set_rect_fill(icon, 11, 3, 3, 12, 0, 0, 0, 255);
				pixbuf_set_rect_fill(icon, 11, 17, 3, 3, 0, 0, 0, 255);
				break;
			default:
				break;
			}
		}

	icons[flag] = icon;

	return icon;
}

static void image_osd_icon_show(OverlayStateData *osd, ImageOSDFlag flag)
{
	GdkPixbuf *pixbuf;

	if (osd->icon_id[flag]) return;

	pixbuf = image_osd_icon_pixbuf(flag);
	if (!pixbuf) return;

	osd->icon_id[flag] = image_overlay_add(osd->imd, pixbuf,
					       osd_icons[flag].x, osd_icons[flag].y,
					       TRUE, FALSE);
}

static void image_osd_icon_hide(OverlayStateData *osd, ImageOSDFlag flag)
{
	if (osd->icon_id[flag])
		{
		image_overlay_remove(osd->imd, osd->icon_id[flag]);
		osd->icon_id[flag] = 0;
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
			if (pixbuf)
				{
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
			else if (osd->ovl_info)
				{
				image_overlay_remove(osd->imd, osd->ovl_info);
				osd->ovl_info = 0;
				}
			}
		}
	else
		{
		if (osd->ovl_info)
			{
			image_overlay_remove(osd->imd, osd->ovl_info);
			osd->ovl_info = 0;
			}
		}

	if (osd->show_status)
		{
		gint i;

		if (osd->changed_states & IMAGE_STATE_IMAGE)
			{
			for (i = 0; i < IMAGE_OSD_COUNT; i++)
				{
				if (osd_icons[i].reset)	osd->icon_time[i] = 0;
				}
			}

		if (osd->changed_states & IMAGE_STATE_COLOR_ADJ)
			{
			osd->icon_time[IMAGE_OSD_COLOR] = IMAGE_OSD_DEFAULT_DURATION + 1;
			image_osd_timer_schedule(osd);
			}

		if (osd->changed_states & IMAGE_STATE_ROTATE_AUTO)
			{
			gint n = 0;

			if (osd->imd->state & IMAGE_STATE_ROTATE_AUTO)
				{
				n = 1;
				if (!osd->imd->cm) n += IMAGE_OSD_DEFAULT_DURATION;
				}

			osd->icon_time[IMAGE_OSD_ROTATE_AUTO] = n;
			image_osd_timer_schedule(osd);
			}

		for (i = 0; i < IMAGE_OSD_COUNT; i++)
			{
			if (osd->icon_time[i] > 0)
				{
				image_osd_icon_show(osd, i);
				}
			else
				{
				image_osd_icon_hide(osd, i);
				}
			}
		}
	else
		{
		gint i;

		for (i = 0; i < IMAGE_OSD_COUNT; i++)
			{
			image_osd_icon_hide(osd, i);
			}
		}

	if (osd->imd->il && osd->imd->il->done)
		osd->changed_states = IMAGE_STATE_NONE;
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

static gint image_osd_timer_cb(gpointer data)
{
	OverlayStateData *osd = data;
	gint done = TRUE;
	gint changed = FALSE;
	gint i;

	for (i = 0; i < IMAGE_OSD_COUNT; i++)
		{
		if (osd->icon_time[i] > 1)
			{
			osd->icon_time[i]--;
			if (osd->icon_time[i] < 2)
				{
				osd->icon_time[i] = 0;
				changed = TRUE;
				}
			else
				{
				done = FALSE;
				}
			}
		}

	if (changed) image_osd_update_schedule(osd, FALSE);

	if (done)
		{
		osd->timer_id = -1;
		return FALSE;
		}

	return TRUE;
}

static void image_osd_timer_schedule(OverlayStateData *osd)
{
	if (osd->timer_id == -1)
		{
		osd->timer_id = g_timeout_add(100, image_osd_timer_cb, osd);
		}
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
	if (osd->timer_id != -1) g_source_remove(osd->timer_id);

	if (osd->imd)
		{
		gint i;

		g_object_set_data(G_OBJECT(osd->imd->pr), "IMAGE_OVERLAY_DATA", NULL);
		g_signal_handler_disconnect(osd->imd->pr, osd->destroy_id);

		image_set_state_func(osd->imd, NULL, NULL);
		image_overlay_remove(osd->imd, osd->ovl_info);

		for (i = 0; i < IMAGE_OSD_COUNT; i++)
			{
			image_osd_icon_hide(osd, i);
			}
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

/* duration:
    0 = hide
    1 = show
   2+ = show for duration tenths of a second
   -1 = use default duration
 */
void image_osd_icon(ImageWindow *imd, ImageOSDFlag flag, gint duration)
{
	OverlayStateData *osd;

	if (!imd) return;

	osd = g_object_get_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA");
	if (!osd) return;

	if (flag < IMAGE_OSD_NONE || flag >= IMAGE_OSD_COUNT) return;
	if (duration < 0) duration = IMAGE_OSD_DEFAULT_DURATION;
	if (duration > 1) duration += 1;

	osd->icon_time[flag] = duration;

	image_osd_update_schedule(osd, FALSE);
	image_osd_timer_schedule(osd);
}
