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
#include "image-overlay.h"

#include "bar_info.h"
#include "collect.h"
#include "exif.h"
#include "filedata.h"
#include "histogram.h"
#include "image.h"
#include "img-view.h"
#include "layout.h"
#include "pixbuf-renderer.h"
#include "pixbuf_util.h"
#include "ui_fileops.h"

/*
 *----------------------------------------------------------------------------
 * image overlay
 *----------------------------------------------------------------------------
 */


typedef struct _OverlayStateData OverlayStateData;
struct _OverlayStateData {
	ImageWindow *imd;
	ImageState changed_states;

	Histogram *histogram;

	OsdShowFlags show;

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
#define HISTOGRAM_WIDTH  256

static void image_osd_timer_schedule(OverlayStateData *osd);


void set_default_image_overlay_template_string(ConfOptions *options)
{
	if (options->image_overlay.common.template_string) g_free(options->image_overlay.common.template_string);
	options->image_overlay.common.template_string = g_strdup(DEFAULT_OVERLAY_INFO);
}

static OverlayStateData *image_get_osd_data(ImageWindow *imd)
{
	OverlayStateData *osd;

	if (!imd) return NULL;

	g_assert(imd->pr);

	osd = g_object_get_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA");
	return osd;
}

static void image_set_osd_data(ImageWindow *imd, OverlayStateData *osd)
{
	g_assert(imd);
	g_assert(imd->pr);
	g_object_set_data(G_OBJECT(imd->pr), "IMAGE_OVERLAY_DATA", osd);
}

/*
 *----------------------------------------------------------------------------
 * image histogram
 *----------------------------------------------------------------------------
 */


void image_osd_histogram_chan_toggle(ImageWindow *imd)
{
	OverlayStateData *osd = image_get_osd_data(imd);

	if (!osd || !osd->histogram) return;

	histogram_set_channel(osd->histogram, (histogram_get_channel(osd->histogram) +1)%HCHAN_COUNT);
	image_osd_update(imd);
}

void image_osd_histogram_log_toggle(ImageWindow *imd)
{
	OverlayStateData *osd = image_get_osd_data(imd);

	if (!osd || !osd->histogram) return;

	histogram_set_mode(osd->histogram, !histogram_get_mode(osd->histogram));
	image_osd_update(imd);
}

void image_osd_toggle(ImageWindow *imd)
{
	OverlayStateData *osd;

	if (!imd) return;

	osd = image_get_osd_data(imd);
	if (!osd)
		{
		image_osd_set(imd, OSD_SHOW_INFO | OSD_SHOW_STATUS);
		return;
		}

	if (osd->show != OSD_SHOW_NOTHING)
		{
		if (osd->show & OSD_SHOW_HISTOGRAM)
			{
			image_osd_set(imd, OSD_SHOW_NOTHING);
			}
		else
			{
			image_osd_set(imd, osd->show | OSD_SHOW_HISTOGRAM);
			}
		}
}

static gchar *keywords_to_string(FileData *fd)
{
	GList *keywords;
	GString *kwstr = NULL;
	gchar *ret = NULL;

	g_assert(fd);

	if (comment_read(fd, &keywords, NULL))
		{
		GList *work = keywords;

		while (work)
			{
			gchar *kw = work->data;
			work = work->next;
					
			if (!kw) continue;
			if (!kwstr)
				kwstr = g_string_new("");
			else
				g_string_append(kwstr, ", ");
			
			g_string_append(kwstr, kw);
			}
		}

	if (kwstr)
		{
		ret = kwstr->str;
		g_string_free(kwstr, FALSE);
		}

	return ret;
}

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

	exif = exif_read_fd(imd->image_fd);
	prev = 0;
	last = FALSE;

	while (TRUE)
		{
		gint limit = 0;
		gchar *trunc = NULL;
		gchar *limpos = NULL;
		gchar *extra = NULL;
		gchar *extrapos = NULL;
		gchar *p;

		start = strchr(new->str, delim);
		if (!start)
			break;
		end = strchr(start+1, delim);
		if (!end)
			break;

		/* Search for optionnal modifiers
		 * %name:99:extra% -> name = "name", limit=99, extra = "extra"
		 */
		for (p = start + 1; p < end; p++)
			{
			if (p[0] == ':')
				{
				if (g_ascii_isdigit(p[1]) && !limpos)
					{
					limpos = p + 1;
					if (!trunc) trunc = p;
					}
				else
					{
					extrapos = p + 1;
					if (!trunc) trunc = p;
					break;
					}
				}
			}

		if (limpos)
			limit = atoi(limpos);

		if (extrapos)
			extra = g_strndup(extrapos, end - extrapos);
					
		name = g_strndup(start+1, (trunc ? trunc : end)-start-1);
		pos = start - new->str;
		data = NULL;

		if (strcmp(name, "keywords") == 0)
			{
			data = keywords_to_string(imd->image_fd);
			}
		else if (strcmp(name, "comment") == 0)
			{
			comment_read(imd->image_fd, NULL, &data);
			}
		else
			{
			data = g_strdup(g_hash_table_lookup(vars, name));
			if (data && strcmp(name, "zoom") == 0) imd->overlay_show_zoom = TRUE;
			if (!data && exif)
				data = exif_get_data_as_text(exif, name);
			}
		if (data && *data && limit > 0 && strlen(data) > limit + 3)
			{
			gchar *new_data = g_strdup_printf("%-*.*s...", limit, limit, data);
			g_free(data);
			data = new_data;
			}
	
		if (data)
			{
			/* Since we use pango markup to display, we need to escape here */
			gchar *escaped = g_markup_escape_text(data, -1);
			g_free(data);
			data = escaped;
			}

		if (extra)
			{
			if (data && *data)
				{
				/* Display data between left and right parts of extra string
				 * the data is expressed by a '*' character.
				 * If no "*" is present, the extra string is just appended to data string.
				 * Pango mark up is accepted in left and right parts.
				 * Any \n is replaced by a newline
				 * Examples:
				 * "<i>*</i>\n" -> data is displayed in italics ended with a newline
				 * "\n" 	-> ended with newline
				 * "ISO *"	-> prefix data with "ISO " (ie. "ISO 100")
				 * "Collection <b>*</b>\n" -> display data in bold prefixed by "Collection " and a newline is appended
				 *
				 * FIXME: using background / foreground colors lead to weird results.
				 */
				gchar *new_data;
				gchar *left = NULL;
				gchar *right = extra;
				gchar *p;
				gint len = strlen(extra);
				
				/* Search and replace "\n" by a newline character */
				for (p = extra; *p; p++, len--)
					if (p[0] == '\\' && p[1] == 'n')
						{
						memmove(p+1, p+2, --len);
						*p = '\n';
						}

				/* Search for left and right parts */
				for (p = extra; *p; p++)
					if (*p == '*')
						{
						*p = '\0';
						p++;
						right = p;
						left = extra;
						break;
						}
				
				new_data = g_strdup_printf("%s%s%s", left ? left : "", data, right);
				g_free(data);
				data = new_data;
				}
			g_free(extra);
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

	exif_free(exif);
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

static GdkPixbuf *image_osd_info_render(OverlayStateData *osd)
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
	ImageWindow *imd = osd->imd;
	FileData *fd = image_get_fd(imd);

	if (!fd) return NULL;

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
		gchar *collection_name;

		t = g_list_length(cd->list);
		n = g_list_index(cd->list, info) + 1;
		if (cd->name)
			{
			if (file_extension_match(cd->name, ".gqv"))
				collection_name = remove_extension_from_path(cd->name);
			else
				collection_name = g_strdup(cd->name);
			}
		else
			{
			collection_name = g_strdup(_("Untitled"));
			}

		ct = g_markup_escape_text(collection_name, -1);
		g_free(collection_name);
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
			image_get_image_size(imd, &w, &h);
			imgpixbuf = (PIXBUF_RENDERER(imd->pr))->pixbuf;
			}
	
		if (imgpixbuf && (osd->show & OSD_SHOW_HISTOGRAM) && osd->histogram
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
	g_hash_table_insert(vars, "zoom", image_zoom_get_as_text(imd));

 	if (!name_escaped)
 		{
 		text = g_strdup_printf(_("Untitled"));
 		}
 	else
 		{
 		text = image_osd_mkinfo(options->image_overlay.common.template_string, imd, vars);
		}

	g_free(size);
  	g_free(ct);
  	g_free(name_escaped);
	g_hash_table_destroy(vars);

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
			gchar *escaped_histogram_label = g_markup_escape_text(histogram_label(osd->histogram), -1);
			if (*text)
				text2 = g_strdup_printf("%s\n%s", text, escaped_histogram_label);
			else
				text2 = g_strdup(escaped_histogram_label);
			g_free(escaped_histogram_label);
			g_free(text);
			text = text2;
			}
	}

	layout = gtk_widget_create_pango_layout(imd->pr, NULL);
	pango_layout_set_markup(layout, text, -1);
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
		histogram_read(osd->histogram, imgpixbuf);
		if (width < HISTOGRAM_WIDTH + 10) width = HISTOGRAM_WIDTH + 10;
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
			{
			gint x = 5;
			gint y = height - HISTOGRAM_HEIGHT - 5;
			gint w = width - 10;
			gint xoffset = 0;
			gint subdiv = 5;
			gint c = 160;
			gint alpha = 250;
			gint i;

			for (i = 0; i < subdiv; i++)
				{
				gint d = (i > 0 ? 0 : 1);
				gint div_width = d + w / subdiv;

				pixbuf_set_rect(pixbuf, x + xoffset, y, div_width, HISTOGRAM_HEIGHT, c, c, c, alpha, d, 1, 1, 1);
				xoffset += div_width;
				}
						
			histogram_draw(osd->histogram, pixbuf, x, y, w, HISTOGRAM_HEIGHT);
			}
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

	osd->imd->overlay_show_zoom = FALSE;

	if (osd->show & OSD_SHOW_INFO)
		{
		if (osd->changed_states & IMAGE_STATE_IMAGE)
			{
			GdkPixbuf *pixbuf;

			pixbuf = image_osd_info_render(osd);
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

	if (osd->show & OSD_SHOW_STATUS)
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
	OverlayStateData *osd = image_get_osd_data(imd);

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

		image_set_osd_data(osd->imd, NULL);
		g_signal_handler_disconnect(osd->imd->pr, osd->destroy_id);

		image_set_state_func(osd->imd, NULL, NULL);
		image_overlay_remove(osd->imd, osd->ovl_info);

		for (i = 0; i < IMAGE_OSD_COUNT; i++)
			{
			image_osd_icon_hide(osd, i);
			}
		}

	if (osd->histogram) histogram_free(osd->histogram);

	g_free(osd);
}

static void image_osd_remove(ImageWindow *imd)
{
	OverlayStateData *osd = image_get_osd_data(imd);

	if (osd) image_osd_free(osd);
}

static void image_osd_destroy_cb(GtkWidget *widget, gpointer data)
{
	OverlayStateData *osd = data;

	osd->imd = NULL;
	image_osd_free(osd);
}

static void image_osd_enable(ImageWindow *imd, OsdShowFlags show)
{
	OverlayStateData *osd = image_get_osd_data(imd);

	if (!osd)
		{
		osd = g_new0(OverlayStateData, 1);
		osd->imd = imd;
		osd->idle_id = -1;
		osd->timer_id = -1;
		osd->show = OSD_SHOW_NOTHING;
		osd->histogram = NULL;

		osd->destroy_id = g_signal_connect(G_OBJECT(imd->pr), "destroy",
						   G_CALLBACK(image_osd_destroy_cb), osd);
		image_set_osd_data(imd, osd);

		image_set_state_func(osd->imd, image_osd_state_cb, osd);
		}

	if (show & OSD_SHOW_HISTOGRAM)
		osd->histogram = histogram_new();
	else if (osd->histogram)
		{
		histogram_free(osd->histogram);
		osd->histogram = NULL;
		}

	if (show & OSD_SHOW_STATUS)
		image_osd_icon(imd, IMAGE_OSD_ICON, -1);

	if (show != osd->show)
		image_osd_update_schedule(osd, TRUE);

	osd->show = show;
}

void image_osd_set(ImageWindow *imd, OsdShowFlags show)
{
	if (!imd) return;

	if (show == OSD_SHOW_NOTHING)
		{
		image_osd_remove(imd);
		return;
		}

	image_osd_enable(imd, show);
}

OsdShowFlags image_osd_get(ImageWindow *imd)
{
	OverlayStateData *osd = image_get_osd_data(imd);

	return osd ? osd->show : OSD_SHOW_NOTHING;
}

/* duration:
    0 = hide
    1 = show
   2+ = show for duration tenths of a second
   -1 = use default duration
 */
void image_osd_icon(ImageWindow *imd, ImageOSDFlag flag, gint duration)
{
	OverlayStateData *osd = image_get_osd_data(imd);

	if (!osd) return;

	if (flag < IMAGE_OSD_NONE || flag >= IMAGE_OSD_COUNT) return;
	if (duration < 0) duration = IMAGE_OSD_DEFAULT_DURATION;
	if (duration > 1) duration += 1;

	osd->icon_time[flag] = duration;

	image_osd_update_schedule(osd, FALSE);
	image_osd_timer_schedule(osd);
}
