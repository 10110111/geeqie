/*
 * GQview
 * (C) 2005 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "gqview.h"
#include "pan-view.h"

#include "cache.h"
#include "dnd.h"
#include "editors.h"
#include "filelist.h"
#include "fullscreen.h"
#include "image.h"
#include "image-load.h"
#include "img-view.h"
#include "info.h"
#include "menu.h"
#include "pixbuf_util.h"
#include "thumb.h"
#include "utilops.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_misc.h"
#include "ui_tabcomp.h"

#include <gdk/gdkkeysyms.h> /* for keyboard values */
#include <math.h>


#define PAN_WINDOW_DEFAULT_WIDTH 720
#define PAN_WINDOW_DEFAULT_HEIGHT 500

#define PAN_TILE_SIZE 512

#define PAN_THUMB_SIZE_DOTS 4
#define PAN_THUMB_SIZE_NONE 24
#define PAN_THUMB_SIZE_SMALL 64
#define PAN_THUMB_SIZE_NORMAL 128
#define PAN_THUMB_SIZE_LARGE 256
#define PAN_THUMB_SIZE pw->thumb_size

#define PAN_THUMB_GAP_DOTS 2
#define PAN_THUMB_GAP_SMALL 14
#define PAN_THUMB_GAP_NORMAL 30
#define PAN_THUMB_GAP_LARGE 40
#define PAN_THUMB_GAP_HUGE 50
#define PAN_THUMB_GAP pw->thumb_gap

#define PAN_SHADOW_OFFSET 6
#define PAN_SHADOW_FADE 5
#define PAN_SHADOW_COLOR 0, 0, 0
#define PAN_SHADOW_ALPHA 64

#define PAN_OUTLINE_THICKNESS 1
#define PAN_OUTLINE_COLOR_1 255, 255, 255
#define PAN_OUTLINE_COLOR_2 64, 64, 64
#define PAN_OUTLINE_ALPHA 180

#define PAN_BACKGROUND_COLOR 255, 255, 230

#define PAN_GRID_SIZE 10
#define PAN_GRID_COLOR 0, 0, 0
#define PAN_GRID_ALPHA 20

#define PAN_FOLDER_BOX_COLOR 0, 0, 255
#define PAN_FOLDER_BOX_ALPHA 10
#define PAN_FOLDER_BOX_BORDER 20

#define PAN_FOLDER_BOX_OUTLINE_THICKNESS 4
#define PAN_FOLDER_BOX_OUTLINE_COLOR 0, 0, 255
#define PAN_FOLDER_BOX_OUTLINE_ALPHA 64

#define PAN_TEXT_BORDER_SIZE 4
#define PAN_TEXT_COLOR 0, 0, 0

#define PAN_POPUP_COLOR 255, 255, 220
#define PAN_POPUP_ALPHA 255
#define PAN_POPUP_BORDER 1
#define PAN_POPUP_BORDER_COLOR 0, 0, 0
#define PAN_POPUP_TEXT_COLOR 0, 0, 0

#define PAN_GROUP_MAX 16

#define ZOOM_INCREMENT 1.0
#define ZOOM_LABEL_WIDTH 64


#define PAN_PREF_GROUP "pan_view_options"
#define PAN_PREF_HIDE_WARNING "hide_performance_warning"


typedef enum {
	LAYOUT_TIMELINE = 0,
	LAYOUT_CALENDAR,
	LAYOUT_FOLDERS_LINEAR,
	LAYOUT_FOLDERS_FLOWER,
	LAYOUT_GRID,
} LayoutType;

typedef enum {
	LAYOUT_SIZE_THUMB_DOTS = 0,
	LAYOUT_SIZE_THUMB_NONE,
	LAYOUT_SIZE_THUMB_SMALL,
	LAYOUT_SIZE_THUMB_NORMAL,
	LAYOUT_SIZE_THUMB_LARGE,
	LAYOUT_SIZE_10,
	LAYOUT_SIZE_25,
	LAYOUT_SIZE_33,
	LAYOUT_SIZE_50,
	LAYOUT_SIZE_100
} LayoutSize;

typedef enum {
	ITEM_NONE,
	ITEM_THUMB,
	ITEM_BOX,
	ITEM_TRIANGLE,
	ITEM_TEXT,
	ITEM_IMAGE
} ItemType;

typedef enum {
	TEXT_ATTR_NONE = 0,
	TEXT_ATTR_BOLD = 1 << 0,
	TEXT_ATTR_HEADING = 1 << 1,
	TEXT_ATTR_MARKUP = 1 << 2
} TextAttrType;

enum {
	BORDER_NONE = 0,
	BORDER_1 = 1 << 0,
	BORDER_2 = 1 << 1,
	BORDER_3 = 1 << 2,
	BORDER_4 = 1 << 3
};

typedef struct _PanItem PanItem;
struct _PanItem {
	ItemType type;
	gint x;
	gint y;
	gint width;
	gint height;
	gchar *key;

	FileData *fd;

	GdkPixbuf *pixbuf;
	gint refcount;

	gchar *text;
	TextAttrType text_attr;

	guint8 color_r;
	guint8 color_g;
	guint8 color_b;
	guint8 color_a;

	guint8 color2_r;
	guint8 color2_g;
	guint8 color2_b;
	guint8 color2_a;
	gint border;

	gpointer data;

	gint queued;
};

typedef struct _PanWindow PanWindow;
struct _PanWindow
{
	GtkWidget *window;
	ImageWindow *imd;
	ImageWindow *imd_normal;
	FullScreenData *fs;

	GtkWidget *path_entry;

	GtkWidget *label_message;
	GtkWidget *label_zoom;

	GtkWidget *search_box;
	GtkWidget *search_entry;
	GtkWidget *search_label;
	GtkWidget *search_button;
	GtkWidget *search_button_arrow;

	GtkWidget *scrollbar_h;
	GtkWidget *scrollbar_v;

	gint overlay_id;

	gchar *path;
	LayoutType layout;
	LayoutSize size;
	gint thumb_size;
	gint thumb_gap;
	gint image_size;

	GList *list;

	GList *cache_list;
	GList *cache_todo;
	gint cache_count;
	gint cache_total;
	gint cache_tick;

	ImageLoader *il;
	ThumbLoader *tl;
	PanItem *queue_pi;
	GList *queue;

	PanItem *click_pi;

	gint idle_id;
};

typedef struct _PanCacheData PanCacheData;
struct _PanCacheData {
	FileData fd;
	CacheData *cd;
};


static GList *pan_window_list = NULL;


static GList *pan_window_layout_list(const gchar *path, SortType sort, gint ascend);

static GList *pan_layout_intersect(PanWindow *pw, gint x, gint y, gint width, gint height);

static GtkWidget *pan_popup_menu(PanWindow *pw);
static void pan_fullscreen_toggle(PanWindow *pw, gint force_off);
static void pan_overlay_toggle(PanWindow *pw);

static void pan_window_close(PanWindow *pw);

static void pan_window_dnd_init(PanWindow *pw);


static gint util_clip_region(gint x, gint y, gint w, gint h,
			     gint clip_x, gint clip_y, gint clip_w, gint clip_h,
			     gint *rx, gint *ry, gint *rw, gint *rh)
{
	if (clip_x + clip_w <= x ||
	    clip_x >= x + w ||
	    clip_y + clip_h <= y ||
	    clip_y >= y + h)
		{
		return FALSE;
		}

	*rx = MAX(x, clip_x);
	*rw = MIN((x + w), (clip_x + clip_w)) - *rx;

	*ry = MAX(y, clip_y);
	*rh = MIN((y + h), (clip_y + clip_h)) - *ry;

	return TRUE;
}

static gint util_clip_region_test(gint x, gint y, gint w, gint h,
				  gint clip_x, gint clip_y, gint clip_w, gint clip_h)
{
	gint rx, ry, rw, rh;

	return util_clip_region(x, y, w, h,
				clip_x, clip_y, clip_w, clip_h,
				&rx, &ry, &rw, &rh);
}

typedef enum {
	DATE_LENGTH_EXACT,
	DATE_LENGTH_HOUR,
	DATE_LENGTH_DAY,
	DATE_LENGTH_WEEK,
	DATE_LENGTH_MONTH,
	DATE_LENGTH_YEAR
} DateLengthType;

static gint date_compare(time_t a, time_t b, DateLengthType length)
{
	struct tm ta;
	struct tm tb;

	if (length == DATE_LENGTH_EXACT) return (a == b);

	if (!localtime_r(&a, &ta) ||
	    !localtime_r(&b, &tb)) return FALSE;

	if (ta.tm_year != tb.tm_year) return FALSE;
	if (length == DATE_LENGTH_YEAR) return TRUE;

	if (ta.tm_mon != tb.tm_mon) return FALSE;
	if (length == DATE_LENGTH_MONTH) return TRUE;

	if (length == DATE_LENGTH_WEEK) return (ta.tm_yday / 7 == tb.tm_yday / 7);

	if (ta.tm_mday != tb.tm_mday) return FALSE;
	if (length == DATE_LENGTH_DAY) return TRUE;

	return (ta.tm_hour == tb.tm_hour);
}

static gint date_value(time_t d, DateLengthType length)
{
	struct tm td;

	if (!localtime_r(&d, &td)) return -1;

	switch (length)
		{
		case DATE_LENGTH_DAY:
			return td.tm_mday;
			break;
		case DATE_LENGTH_WEEK:
			return td.tm_wday;
			break;
		case DATE_LENGTH_MONTH:
			return td.tm_mon + 1;
			break;
		case DATE_LENGTH_YEAR:
			return td.tm_year + 1900;
			break;
		case DATE_LENGTH_EXACT:
		default:
			break;
		}

	return -1;
}

static gchar *date_value_string(time_t d, DateLengthType length)
{
	struct tm td;
	gchar buf[128];
	gchar *format = NULL;

	if (!localtime_r(&d, &td)) return g_strdup("");

	switch (length)
		{
		case DATE_LENGTH_DAY:
			return g_strdup_printf("%d", td.tm_mday);
			break;
		case DATE_LENGTH_WEEK:
			format = "%A %e";
			break;
		case DATE_LENGTH_MONTH:
			format = "%B %Y";
			break;
		case DATE_LENGTH_YEAR:
			return g_strdup_printf("%d", td.tm_year + 1900);
			break;
		case DATE_LENGTH_EXACT:
		default:
			return g_strdup(text_from_time(d));
			break;
		}


	if (format && strftime(buf, sizeof(buf), format, &td) > 0)
		{
		gchar *ret = g_locale_to_utf8(buf, -1, NULL, NULL, NULL);
		if (ret) return ret;
		}

	return g_strdup("");
}

static time_t date_to_time(gint year, gint month, gint day)
{
	struct tm lt;

	lt.tm_sec = 0;
	lt.tm_min = 0;
	lt.tm_hour = 0;
	lt.tm_mday = (day >= 1 && day <= 31) ? day : 1;
	lt.tm_mon = (month >= 1 && month <= 12) ? month - 1 : 0;
	lt.tm_year = year - 1900;
	lt.tm_isdst = 0;

	return mktime(&lt);
}

/*
 *-----------------------------------------------------------------------------
 * drawing utils
 *-----------------------------------------------------------------------------
 */

static void triangle_rect_region(gint x1, gint y1, gint x2, gint y2, gint x3, gint y3,
				 gint *rx, gint *ry, gint *rw, gint *rh)
{
	gint tx, ty, tw, th;

	tx = MIN(x1, x2);
	tx = MIN(tx, x3);
	ty = MIN(y1, y2);
	ty = MIN(ty, y3);
	tw = MAX(abs(x1 - x2), abs(x2 - x3));
	tw = MAX(tw, abs(x3 - x1));
	th = MAX(abs(y1 - y2), abs(y2 - y3));
	th = MAX(th, abs(y3 - y1));

	*rx = tx;
	*ry = ty;
	*rw = tw;
	*rh = th;
}

static void pixbuf_draw_triangle(GdkPixbuf *pb,
				 gint clip_x, gint clip_y, gint clip_w, gint clip_h,
				 gint x1, gint y1, gint x2, gint y2, gint x3, gint y3,
				 guint8 r, guint8 g, guint8 b, guint8 a)
{
	gint p_alpha;
	gint pw, ph, prs;
	gint rx, ry, rw, rh;
	gint tx, ty, tw, th;
	gint fx1, fy1;
	gint fx2, fy2;
	gint fw, fh;
	guchar *p_pix;
	guchar *pp;
	gint p_step;
	gint i, j;

	if (!pb) return;

	pw = gdk_pixbuf_get_width(pb);
	ph = gdk_pixbuf_get_height(pb);

	if (!util_clip_region(0, 0, pw, ph,
			      clip_x, clip_y, clip_w, clip_h,
			      &rx, &ry, &rw, &rh)) return;

	triangle_rect_region(x1, y1, x2, y2, x3, y3,
			     &tx, &ty, &tw, &th);

	if (!util_clip_region(rx, ry, rw, rh,
			      tx, ty, tw, th,
			      &fx1, &fy1, &fw, &fh)) return;
	fx2 = fx1 + fw;
	fy2 = fy1 + fh;

	p_alpha = gdk_pixbuf_get_has_alpha(pb);
	prs = gdk_pixbuf_get_rowstride(pb);
	p_pix = gdk_pixbuf_get_pixels(pb);

	p_step = (p_alpha) ? 4 : 3;
	for (i = fy1; i < fy2; i++)
		{
		pp = p_pix + i * prs + (fx1 * p_step);
		for (j = fx1; j < fx2; j++)
			{
			gint z1, z2;

			z1 = (y1 - y2)*(j - x2) + (x2 - x1)*(i - y2);
			z2 = (y2 - y3)*(j - x3) + (x3 - x2)*(i - y3);
			if ((z1 ^ z2) >= 0)
				{
				z2 = (y3 - y1)*(j - x1) + (x1 - x3)*(i - y1);
				if ((z1 ^ z2) >= 0)
					{
					pp[0] = (r * a + pp[0] * (256-a)) >> 8;
					pp[1] = (g * a + pp[1] * (256-a)) >> 8;
					pp[2] = (b * a + pp[2] * (256-a)) >> 8;
					}
				}
			pp += p_step;
			}
		}
}

static void pixbuf_draw_line(GdkPixbuf *pb,
			     gint clip_x, gint clip_y, gint clip_w, gint clip_h,
			     gint x1, gint y1, gint x2, gint y2,
			     guint8 r, guint8 g, guint8 b, guint8 a)
{
	gint p_alpha;
	gint pw, ph, prs;
	gint rx, ry, rw, rh;
	gint fx1, fy1, fx2, fy2;
	guchar *p_pix;
	guchar *pp;
	gint p_step;
	gint xd, yd;
	gint xa, ya;
	gdouble xstep, ystep;
	gdouble i, j;
	gint n, nt;
	gint x, y;

	if (!pb) return;

	pw = gdk_pixbuf_get_width(pb);
	ph = gdk_pixbuf_get_height(pb);

	if (!util_clip_region(0, 0, pw, ph,
			      clip_x, clip_y, clip_w, clip_h,
			      &rx, &ry, &rw, &rh)) return;

	fx1 = rx;
	fy1 = ry;
	fx2 = rx + rw;
	fy2 = ry + rh;

	xd = x2 - x1;
	yd = y2 - y1;
	xa = abs(xd);
	ya = abs(yd);

	if (xa == 0 && ya == 0) return;
#if 0
	nt = sqrt(xd * xd + yd * yd);
#endif
	nt = (xa > ya) ? xa : ya;
	xstep = (double)xd / nt;
	ystep = (double)yd / nt;

	p_alpha = gdk_pixbuf_get_has_alpha(pb);
	prs = gdk_pixbuf_get_rowstride(pb);
	p_pix = gdk_pixbuf_get_pixels(pb);

	p_step = (p_alpha) ? 4 : 3;

	i = (double)y1;
	j = (double)x1;
	for (n = 0; n < nt; n++)
		{
		x = (gint)(j + 0.5);
		y = (gint)(i + 0.5);

		if (x >= fx1 && x < fx2 &&
		    y >= fy1 && y < fy2)
			{
			pp = p_pix + y * prs + x * p_step;
			*pp = (r * a + *pp * (256-a)) >> 8;
			pp++;
			*pp = (g * a + *pp * (256-a)) >> 8;
			pp++;
			*pp = (b * a + *pp * (256-a)) >> 8;
			}
		i += ystep;
		j += xstep;
		}
}

static void pixbuf_draw_fade_linear(guchar *p_pix, gint prs, gint p_alpha,
				    gint s, gint vertical, gint border,
			 	    gint x1, gint y1, gint x2, gint y2,
				    guint8 r, guint8 g, guint8 b, guint8 a)
{
	guchar *pp;
	gint p_step;
	guint8 n = a;
	gint i, j;

	p_step = (p_alpha) ? 4 : 3;
	for (j = y1; j < y2; j++)
		{
		pp = p_pix + j * prs + x1 * p_step;
		if (!vertical) n = a - a * abs(j - s) / border;
		for (i = x1; i < x2; i++)
			{
			if (vertical) n = a - a * abs(i - s) / border;
			*pp = (r * n + *pp * (256-n)) >> 8;
			pp++;
			*pp = (g * n + *pp * (256-n)) >> 8;
			pp++;
			*pp = (b * n + *pp * (256-n)) >> 8;
			pp++;
			if (p_alpha) pp++;
			}
		}
}

static void pixbuf_draw_fade_radius(guchar *p_pix, gint prs, gint p_alpha,
				    gint sx, gint sy, gint border,
			 	    gint x1, gint y1, gint x2, gint y2,
				    guint8 r, guint8 g, guint8 b, guint8 a)
{
	guchar *pp;
	gint p_step;
	gint i, j;

	p_step = (p_alpha) ? 4 : 3;
	for (j = y1; j < y2; j++)
		{
		pp = p_pix + j * prs + x1 * p_step;
		for (i = x1; i < x2; i++)
			{
			guint8 n;
			gint r;

			r = MIN(border, (gint)sqrt((i-sx)*(i-sx) + (j-sy)*(j-sy)));
			n = a - a * r / border;
			*pp = (r * n + *pp * (256-n)) >> 8;
			pp++;
			*pp = (g * n + *pp * (256-n)) >> 8;
			pp++;
			*pp = (b * n + *pp * (256-n)) >> 8;
			pp++;
			if (p_alpha) pp++;
			}
		}
}

static void pixbuf_draw_shadow(GdkPixbuf *pb,
			       gint clip_x, gint clip_y, gint clip_w, gint clip_h,
			       gint x, gint y, gint w, gint h, gint border,
			       guint8 r, guint8 g, guint8 b, guint8 a)
{
	gint p_alpha;
	gint pw, ph, prs;
	gint rx, ry, rw, rh;
	gint fx, fy, fw, fh;
	guchar *p_pix;

	if (!pb) return;

	pw = gdk_pixbuf_get_width(pb);
	ph = gdk_pixbuf_get_height(pb);

	if (!util_clip_region(0, 0, pw, ph,
			      clip_x, clip_y, clip_w, clip_h,
			      &rx, &ry, &rw, &rh)) return;

	p_alpha = gdk_pixbuf_get_has_alpha(pb);
	prs = gdk_pixbuf_get_rowstride(pb);
	p_pix = gdk_pixbuf_get_pixels(pb);

	if (util_clip_region(x + border, y + border, w - border * 2, h - border * 2,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_rect_fill(pb, fx, fy, fw, fh, r, g, b, a);
		}

	if (border < 1) return;

	if (util_clip_region(x, y + border, border, h - border * 2,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_linear(p_pix, prs, p_alpha,
					x + border, TRUE, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x + w - border, y + border, border, h - border * 2,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_linear(p_pix, prs, p_alpha,
					x + w - border, TRUE, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x + border, y, w - border * 2, border,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_linear(p_pix, prs, p_alpha,
					y + border, FALSE, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x + border, y + h - border, w - border * 2, border,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_linear(p_pix, prs, p_alpha,
					y + h - border, FALSE, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x, y, border, border,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_radius(p_pix, prs, p_alpha,
					x + border, y + border, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x + w - border, y, border, border,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_radius(p_pix, prs, p_alpha,
					x + w - border, y + border, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x, y + h - border, border, border,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_radius(p_pix, prs, p_alpha,
					x + border, y + h - border, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
	if (util_clip_region(x + w - border, y + h - border, border, border,
			     rx, ry, rw, rh,
			     &fx, &fy, &fw, &fh))
		{
		pixbuf_draw_fade_radius(p_pix, prs, p_alpha,
					x + w - border, y + h - border, border,
					fx, fy, fx + fw, fy + fh,
					r, g, b, a);
		}
}
				

/*
 *-----------------------------------------------------------------------------
 * cache
 *-----------------------------------------------------------------------------
 */

static void pan_cache_free(PanWindow *pw)
{
	GList *work;

	work = pw->cache_list;
	while (work)
		{
		PanCacheData *pc;

		pc = work->data;
		work = work->next;

		cache_sim_data_free(pc->cd);
		file_data_free((FileData *)pc);
		}

	g_list_free(pw->cache_list);
	pw->cache_list = NULL;

	filelist_free(pw->cache_todo);
	pw->cache_todo = NULL;

	pw->cache_count = 0;
	pw->cache_total = 0;
	pw->cache_tick = 0;
}

static void pan_cache_fill(PanWindow *pw, const gchar *path)
{
	GList *list;

	pan_cache_free(pw);

	list = pan_window_layout_list(path, SORT_NAME, TRUE);
	pw->cache_todo = g_list_reverse(list);

	pw->cache_total = g_list_length(pw->cache_todo);
}

static gint pan_cache_step(PanWindow *pw)
{
	FileData *fd;
	PanCacheData *pc;
	CacheData *cd = NULL;

	if (!pw->cache_todo) return FALSE;

	fd = pw->cache_todo->data;
	pw->cache_todo = g_list_remove(pw->cache_todo, fd);

	if (enable_thumb_caching)
		{
		gchar *found;

		found = cache_find_location(CACHE_TYPE_SIM, fd->path);
		if (found && filetime(found) == fd->date)
			{
			cd = cache_sim_data_load(found);
			}
		g_free(found);
		}

	if (!cd) cd = cache_sim_data_new();

	if (!cd->dimensions)
		{
		cd->dimensions = image_load_dimensions(fd->path, &cd->width, &cd->height);
		if (enable_thumb_caching &&
		    cd->dimensions)
			{
			gchar *base;
			mode_t mode = 0755;

			base = cache_get_location(CACHE_TYPE_SIM, fd->path, FALSE, &mode);
			if (cache_ensure_dir_exists(base, mode))
				{
				g_free(cd->path);
				cd->path = cache_get_location(CACHE_TYPE_SIM, fd->path, TRUE, NULL);
				if (cache_sim_data_save(cd))
					{
					filetime_set(cd->path, filetime(fd->path));
					}
				}
			g_free(base);
			}

		pw->cache_tick = 9;
		}

	pc = g_new0(PanCacheData, 1);
	memcpy(pc, fd, sizeof(FileData));
	g_free(fd);

	pc->cd = cd;

	pw->cache_list = g_list_prepend(pw->cache_list, pc);

	return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 * item objects
 *-----------------------------------------------------------------------------
 */

static void pan_item_free(PanItem *pi)
{
	if (!pi) return;

	if (pi->pixbuf) g_object_unref(pi->pixbuf);
	if (pi->fd) file_data_free(pi->fd);
	g_free(pi->text);
	g_free(pi->key);
	g_free(pi->data);

	g_free(pi);
}

static void pan_window_items_free(PanWindow *pw)
{
	GList *work;

	work = pw->list;
	while (work)
		{
		PanItem *pi = work->data;
		work = work->next;

		pan_item_free(pi);
		}

	g_list_free(pw->list);
	pw->list = NULL;

	g_list_free(pw->queue);
	pw->queue = NULL;
	pw->queue_pi = NULL;

	image_loader_free(pw->il);
	pw->il = NULL;

	thumb_loader_free(pw->tl);
	pw->tl = NULL;

	pw->click_pi = NULL;
}

static PanItem *pan_item_new_thumb(PanWindow *pw, FileData *fd, gint x, gint y)
{
	PanItem *pi;

	pi = g_new0(PanItem, 1);
	pi->type = ITEM_THUMB;
	pi->fd = fd;
	pi->x = x;
	pi->y = y;
	pi->width = PAN_THUMB_SIZE + PAN_SHADOW_OFFSET * 2;
	pi->height = PAN_THUMB_SIZE + PAN_SHADOW_OFFSET * 2;

	pi->pixbuf = NULL;

	pi->queued = FALSE;

	pw->list = g_list_prepend(pw->list, pi);

	return pi;
}

static PanItem *pan_item_new_box(PanWindow *pw, FileData *fd, gint x, gint y, gint width, gint height,
				 gint border_size,
				 guint8 base_r, guint8 base_g, guint8 base_b, guint8 base_a,
				 guint8 bord_r, guint8 bord_g, guint8 bord_b, guint8 bord_a)
{
	PanItem *pi;

	pi = g_new0(PanItem, 1);
	pi->type = ITEM_BOX;
	pi->fd = fd;
	pi->x = x;
	pi->y = y;
	pi->width = width;
	pi->height = height;

	pi->color_r = base_r;
	pi->color_g = base_g;
	pi->color_b = base_b;
	pi->color_a = base_a;

	pi->color2_r = bord_r;
	pi->color2_g = bord_g;
	pi->color2_b = bord_b;
	pi->color2_a = bord_a;
	pi->border = border_size;

	pw->list = g_list_prepend(pw->list, pi);

	return pi;
}

static void pan_item_box_shadow(PanItem *pi, gint offset, gint fade)
{
	gint *shadow;

	if (!pi || pi->type != ITEM_BOX) return;

	shadow = pi->data;
	if (shadow)
		{
		pi->width -= shadow[0];
		pi->height -= shadow[0];
		}

	shadow = g_new0(gint, 2);
	shadow[0] = offset;
	shadow[1] = fade;

	pi->width += offset;
	pi->height += offset;

	g_free(pi->data);
	pi->data = shadow;
}

static PanItem *pan_item_new_tri(PanWindow *pw, FileData *fd, gint x, gint y, gint width, gint height,
				 gint x1, gint y1, gint x2, gint y2, gint x3, gint y3,
				 guint8 r, guint8 g, guint8 b, guint8 a)
{
	PanItem *pi;
	gint *coord;

	pi = g_new0(PanItem, 1);
	pi->type = ITEM_TRIANGLE;
	pi->x = x;
	pi->y = y;
	pi->width = width;
	pi->height = height;

	pi->color_r = r;
	pi->color_g = g;
	pi->color_b = b;
	pi->color_a = a;

	coord = g_new0(gint, 6);
	coord[0] = x1;
	coord[1] = y1;
	coord[2] = x2;
	coord[3] = y2;
	coord[4] = x3;
	coord[5] = y3;

	pi->data = coord;

	pi->border = BORDER_NONE;

	pw->list = g_list_prepend(pw->list, pi);

	return pi;
}

static void pan_item_tri_border(PanItem *pi, gint borders,
				guint8 r, guint8 g, guint8 b, guint8 a)
{
	if (!pi || pi->type != ITEM_TRIANGLE) return;

	pi->border = borders;

	pi->color2_r = r;
	pi->color2_g = g;
	pi->color2_b = b;
	pi->color2_a = a;
}

static PangoLayout *pan_item_text_layout(PanItem *pi, GtkWidget *widget)
{
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout(widget, NULL);

	if (pi->text_attr & TEXT_ATTR_MARKUP)
		{
		pango_layout_set_markup(layout, pi->text, -1);
		return layout;
		}

	if (pi->text_attr & TEXT_ATTR_BOLD ||
	    pi->text_attr & TEXT_ATTR_HEADING)
		{
		PangoAttrList *pal;
		PangoAttribute *pa;
		
		pal = pango_attr_list_new();
		if (pi->text_attr & TEXT_ATTR_BOLD)
			{
			pa = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
			pa->start_index = 0;
			pa->end_index = G_MAXINT;
			pango_attr_list_insert(pal, pa);
			}
		if (pi->text_attr & TEXT_ATTR_HEADING)
			{
			pa = pango_attr_scale_new(PANGO_SCALE_LARGE);
			pa->start_index = 0;
			pa->end_index = G_MAXINT;
			pango_attr_list_insert(pal, pa);
			}
		pango_layout_set_attributes(layout, pal);
		pango_attr_list_unref(pal);
		}

	pango_layout_set_text(layout, pi->text, -1);
	return layout;
}

static void pan_item_text_compute_size(PanItem *pi, GtkWidget *widget)
{
	PangoLayout *layout;

	if (!pi || !pi->text || !widget) return;

	layout = pan_item_text_layout(pi, widget);
	pango_layout_get_pixel_size(layout, &pi->width, &pi->height);
	g_object_unref(G_OBJECT(layout));

	pi->width += PAN_TEXT_BORDER_SIZE * 2;
	pi->height += PAN_TEXT_BORDER_SIZE * 2;
}

static PanItem *pan_item_new_text(PanWindow *pw, gint x, gint y, const gchar *text, TextAttrType attr,
				  guint8 r, guint8 g, guint8 b, guint8 a)
{
	PanItem *pi;

	pi = g_new0(PanItem, 1);
	pi->type = ITEM_TEXT;
	pi->x = x;
	pi->y = y;
	pi->text = g_strdup(text);
	pi->text_attr = attr;

	pi->color_r = r;
	pi->color_g = g;
	pi->color_b = b;
	pi->color_a = a;

	pan_item_text_compute_size(pi, pw->imd->widget);

	pw->list = g_list_prepend(pw->list, pi);

	return pi;
}

static void pan_item_set_key(PanItem *pi, const gchar *key)
{
	gchar *tmp;

	if (!pi) return;

	tmp = pi->key;
	pi->key = g_strdup(key);
	g_free(tmp);
}

static void pan_item_added(PanWindow *pw, PanItem *pi)
{
	if (!pi) return;
	image_area_changed(pw->imd, pi->x, pi->y, pi->width, pi->height);
}

static void pan_item_remove(PanWindow *pw, PanItem *pi)
{
	if (!pi) return;

	if (pw->click_pi == pi) pw->click_pi = NULL;
	if (pw->queue_pi == pi)	pw->queue_pi = NULL;
	pw->queue = g_list_remove(pw->queue, pi);

	pw->list = g_list_remove(pw->list, pi);
	image_area_changed(pw->imd, pi->x, pi->y, pi->width, pi->height);
	pan_item_free(pi);
}

static void pan_item_size_by_item(PanItem *pi, PanItem *child, gint border)
{
	if (!pi || !child) return;

	if (pi->x + pi->width < child->x + child->width + border)
		pi->width = child->x + child->width + border - pi->x;

	if (pi->y + pi->height < child->y + child->height + border)
		pi->height = child->y + child->height + border - pi->y;
}

static void pan_item_size_coordinates(PanItem *pi, gint border, gint *w, gint *h)
{
	if (!pi) return;

	if (*w < pi->x + pi->width + border) *w = pi->x + pi->width + border;
	if (*h < pi->y + pi->height + border) *h = pi->y + pi->height + border;
}

static void pan_item_image_find_size(PanWindow *pw, PanItem *pi, gint w, gint h)
{
	GList *work;

	pi->width = w;
	pi->height = h;

	if (!pi->fd) return;

	work = pw->cache_list;
	while (work)
		{
		PanCacheData *pc;
		gchar *path;

		pc = work->data;
		work = work->next;

		path = ((FileData *)pc)->path;

		if (pc->cd && pc->cd->dimensions &&
		    path && strcmp(path, pi->fd->path) == 0)
			{
			pi->width = MAX(1, pc->cd->width * pw->image_size / 100);
			pi->height = MAX(1, pc->cd->height * pw->image_size / 100);

			pw->cache_list = g_list_remove(pw->cache_list, pc);
			cache_sim_data_free(pc->cd);
			file_data_free((FileData *)pc);
			return;
			}
		}
}

static PanItem *pan_item_new_image(PanWindow *pw, FileData *fd, gint x, gint y, gint w, gint h)
{
	PanItem *pi;

	pi = g_new0(PanItem, 1);
	pi->type = ITEM_IMAGE;
	pi->fd = fd;
	pi->x = x;
	pi->y = y;

	pan_item_image_find_size(pw, pi, w, h);

	pw->list = g_list_prepend(pw->list, pi);

	return pi;
}

static PanItem *pan_item_find_by_key(PanWindow *pw, ItemType type, const gchar *key)
{
	GList *work;

	if (!key) return NULL;

	work = g_list_last(pw->list);
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		if ((pi->type == type || type == ITEM_NONE) &&
		     pi->key && strcmp(pi->key, key) == 0)
			{
			return pi;
			}
		work = work->prev;
		}

	return NULL;
}

/* when ignore_case and partial are TRUE, path should be converted to lower case */
static GList *pan_item_find_by_path(PanWindow *pw, ItemType type, const gchar *path,
				    gint ignore_case, gint partial)
{
	GList *list = NULL;
	GList *work;

	if (!path) return NULL;
	if (partial && path[0] == '/') return NULL;

	work = g_list_last(pw->list);
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		if ((pi->type == type || type == ITEM_NONE) && pi->fd)
			{
			gint match = FALSE;

			if (path[0] == '/')
				{
				if (pi->fd->path && strcmp(path, pi->fd->path) == 0) match = TRUE;
				}
			else if (pi->fd->name)
				{
				if (partial)
					{
					if (ignore_case)
						{
						gchar *haystack;

						haystack = g_utf8_strdown(pi->fd->name, -1);
						match = (strstr(haystack, path) != NULL);
						g_free(haystack);
						}
					else
						{
						if (strstr(pi->fd->name, path)) match = TRUE;
						}
					}
				else if (ignore_case)
					{
					if (strcasecmp(path, pi->fd->name) == 0) match = TRUE;
					}
				else
					{
					if (strcmp(path, pi->fd->name) == 0) match = TRUE;
					}
				}

			if (match) list = g_list_prepend(list, pi);
			}
		work = work->prev;
		}

	return g_list_reverse(list);
}

static PanItem *pan_item_find_by_coord(PanWindow *pw, ItemType type, gint x, gint y, const gchar *key)
{
	GList *work;

	if (x < 0 || x >= pw->imd->image_width ||
	    y < 0 || y >= pw->imd->image_height) return  NULL;

	work = pw->list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		if ((pi->type == type || type == ITEM_NONE) &&
		     x >= pi->x && x < pi->x + pi->width &&
		     y >= pi->y && y < pi->y + pi->height &&
		    (!key || (pi->key && strcmp(pi->key, key) == 0)))
			{
			return pi;
			}
		work = work->next;
		}

	return NULL;
}

/*
 *-----------------------------------------------------------------------------
 * layout generation
 *-----------------------------------------------------------------------------
 */

static GList *pan_window_layout_list(const gchar *path, SortType sort, gint ascend)
{
	GList *flist = NULL;
	GList *dlist = NULL;
	GList *result;
	GList *folders;

	filelist_read(path, &flist, &dlist);
	if (sort != SORT_NONE)
		{
		flist = filelist_sort(flist, sort, ascend);
		dlist = filelist_sort(dlist, sort, ascend);
		}

	result = flist;
	folders = dlist;
	while (folders)
		{
		FileData *fd;

		fd = folders->data;
		folders = g_list_remove(folders, fd);

		if (filelist_read(fd->path, &flist, &dlist))
			{
			if (sort != SORT_NONE)
				{
				flist = filelist_sort(flist, sort, ascend);
				dlist = filelist_sort(dlist, sort, ascend);
				}

			result = g_list_concat(result, flist);
			folders = g_list_concat(dlist, folders);
			}

		file_data_free(fd);
		}

	return result;
}

static void pan_window_layout_compute_grid(PanWindow *pw, const gchar *path, gint *width, gint *height)
{
	GList *list;
	GList *work;
	gint x, y;
	gint grid_size;
	gint next_y;

	list = pan_window_layout_list(path, SORT_NAME, TRUE);

	grid_size = (gint)sqrt((double)g_list_length(list));
	if (pw->size > LAYOUT_SIZE_THUMB_LARGE)
		{
		grid_size = grid_size * (512 + PAN_THUMB_GAP) * pw->image_size / 100;
		}
	else
		{
		grid_size = grid_size * (PAN_THUMB_SIZE + PAN_THUMB_GAP);
		}

	next_y = 0;

	*width = PAN_FOLDER_BOX_BORDER * 2;
	*height = PAN_FOLDER_BOX_BORDER * 2;

	x = PAN_THUMB_GAP;
	y = PAN_THUMB_GAP;
	work = list;
	while (work)
		{
		FileData *fd;
		PanItem *pi;

		fd = work->data;
		work = work->next;

		if (pw->size > LAYOUT_SIZE_THUMB_LARGE)
			{
			pi = pan_item_new_image(pw, fd, x, y, 10, 10);

			x += pi->width + PAN_THUMB_GAP;
			if (y + pi->height + PAN_THUMB_GAP > next_y) next_y = y + pi->height + PAN_THUMB_GAP;
			if (x > grid_size)
				{
				x = PAN_THUMB_GAP;
				y = next_y;
				}
			}
		else
			{
			pi = pan_item_new_thumb(pw, fd, x, y);

			x += PAN_THUMB_SIZE + PAN_THUMB_GAP;
			if (x > grid_size)
				{
				x = PAN_THUMB_GAP;
				y += PAN_THUMB_SIZE + PAN_THUMB_GAP;
				}
			}
		pan_item_size_coordinates(pi, PAN_THUMB_GAP, width, height);
		}

	g_list_free(list);
}

static void pan_window_Layout_compute_folders_flower_size(PanWindow *pw, gint *width, gint *height)
{
	GList *work;
	gint x1, y1, x2, y2;

	x1 = 0;
	y1 = 0;
	x2 = 0;
	y2 = 0;

	work = pw->list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		if (x1 > pi->x) x1 = pi->x;
		if (y1 > pi->y) y1 = pi->y;
		if (x2 < pi->x + pi->width) x2 = pi->x + pi->width;
		if (y2 < pi->y + pi->height) y2 = pi->y + pi->height;
		}

	x1 -= PAN_FOLDER_BOX_BORDER;
	y1 -= PAN_FOLDER_BOX_BORDER;
	x2 += PAN_FOLDER_BOX_BORDER;
	y2 += PAN_FOLDER_BOX_BORDER;

	work = pw->list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		pi->x -= x1;
		pi->y -= y1;

		if (pi->type == ITEM_TRIANGLE && pi->data)
			{
			gint *coord;

			coord = pi->data;
			coord[0] -= x1;
			coord[1] -= y1;
			coord[2] -= x1;
			coord[3] -= y1;
			coord[4] -= x1;
			coord[5] -= y1;
			}
		}

	if (width) *width = x2 - x1;
	if (height) *height = y2 - y1;
}

typedef struct _FlowerGroup FlowerGroup;
struct _FlowerGroup {
	GList *items;
	GList *children;
	gint x;
	gint y;
	gint width;
	gint height;

	gdouble angle;
	gint circumference;
	gint diameter;
};

static void pan_window_layout_compute_folder_flower_move(FlowerGroup *group, gint x, gint y)
{
	GList *work;

	work = group->items;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		pi->x += x;
		pi->y += y;
		}

	group->x += x;
	group->y += y;
}

#define PI 3.14159

static void pan_window_layout_compute_folder_flower_position(FlowerGroup *group, FlowerGroup *parent,
							     gint *result_x, gint *result_y)
{
	gint x, y;
	gint radius;
	gdouble a;

	radius = parent->circumference / (2*PI);
	radius = MAX(radius, parent->diameter / 2 + group->diameter / 2);

	a = 2*PI * group->diameter / parent->circumference;

	x = (gint)((double)radius * cos(parent->angle + a / 2));
	y = (gint)((double)radius * sin(parent->angle + a / 2));

	parent->angle += a;

	x += parent->x;
	y += parent->y;

	x += parent->width / 2;
	y += parent->height / 2;

	x -= group->width / 2;
	y -= group->height / 2;

	*result_x = x;
	*result_y = y;
}

static void pan_window_layout_compute_folder_flower_build(PanWindow *pw, FlowerGroup *group, FlowerGroup *parent)
{
	GList *work;
	gint x, y;

	if (!group) return;

	if (parent && parent->children)
		{
		pan_window_layout_compute_folder_flower_position(group, parent, &x, &y);
		}
	else
		{
		x = 0;
		y = 0;
		}

	pan_window_layout_compute_folder_flower_move(group, x, y);

	if (parent)
		{
		PanItem *pi;
		gint px, py, gx, gy;
		gint x1, y1, x2, y2;

		px = parent->x + parent->width / 2;
		py = parent->y + parent->height / 2;

		gx = group->x + group->width / 2;
		gy = group->y + group->height / 2;

		x1 = MIN(px, gx);
		y1 = MIN(py, gy);

		x2 = MAX(px, gx + 5);
		y2 = MAX(py, gy + 5);

		pi = pan_item_new_tri(pw, NULL, x1, y1, x2 - x1, y2 - y1,
				      px, py, gx, gy, gx + 5, gy + 5,
				      255, 40, 40, 128);
		pan_item_tri_border(pi, BORDER_1 | BORDER_3,
				    255, 0, 0, 128);
		}

	pw->list = g_list_concat(group->items, pw->list);
	group->items = NULL;

	group->circumference = 0;
	work = group->children;
	while (work)
		{
		FlowerGroup *child;

		child = work->data;
		work = work->next;

		group->circumference += child->diameter;
		}

	work = g_list_last(group->children);
	while (work)
		{
		FlowerGroup *child;

		child = work->data;
		work = work->prev;

		pan_window_layout_compute_folder_flower_build(pw, child, group);
		}

	g_list_free(group->children);
	g_free(group);
}

static FlowerGroup *pan_window_layout_compute_folders_flower_path(PanWindow *pw, const gchar *path,
								  gint x, gint y)
{
	FlowerGroup *group;
	GList *f;
	GList *d;
	GList *work;
	PanItem *pi_box;
	gint x_start;
	gint y_height;
	gint grid_size;
	gint grid_count;

	if (!filelist_read(path, &f, &d)) return NULL;
	if (!f && !d) return NULL;

	f = filelist_sort(f, SORT_NAME, TRUE);
	d = filelist_sort(d, SORT_NAME, TRUE);

	pi_box = pan_item_new_text(pw, x, y, path, TEXT_ATTR_NONE,
				   PAN_TEXT_COLOR, 255);

	y += pi_box->height;

	pi_box = pan_item_new_box(pw, file_data_new_simple(path),
				  x, y,
				  PAN_FOLDER_BOX_BORDER * 2, PAN_FOLDER_BOX_BORDER * 2,
				  PAN_FOLDER_BOX_OUTLINE_THICKNESS,
				  PAN_FOLDER_BOX_COLOR, PAN_FOLDER_BOX_ALPHA,
				  PAN_FOLDER_BOX_OUTLINE_COLOR, PAN_FOLDER_BOX_OUTLINE_ALPHA);

	x += PAN_FOLDER_BOX_BORDER;
	y += PAN_FOLDER_BOX_BORDER;

	grid_size = (gint)(sqrt(g_list_length(f)) + 0.9);
	grid_count = 0;
	x_start = x;
	y_height = y;

	work = f;
	while (work)
		{
		FileData *fd;
		PanItem *pi;

		fd = work->data;
		work = work->next;

		if (pw->size > LAYOUT_SIZE_THUMB_LARGE)
			{
			pi = pan_item_new_image(pw, fd, x, y, 10, 10);
			x += pi->width + PAN_THUMB_GAP;
			if (pi->height > y_height) y_height = pi->height;
			}
		else
			{
			pi = pan_item_new_thumb(pw, fd, x, y);
			x += PAN_THUMB_SIZE + PAN_THUMB_GAP;
			y_height = PAN_THUMB_SIZE;
			}

		grid_count++;
		if (grid_count >= grid_size)
			{
			grid_count = 0;
			x = x_start;
			y += y_height + PAN_THUMB_GAP;
			y_height = 0;
			}

		pan_item_size_by_item(pi_box, pi, PAN_FOLDER_BOX_BORDER);
		}

	g_list_free(f);

	group = g_new0(FlowerGroup, 1);
	group->items = pw->list;
	pw->list = NULL;

	group->width = pi_box->width;
	group->height = pi_box->y + pi_box->height;
	group->diameter = (int)sqrt(group->width * group->width + group->height * group->height);

	group->children = NULL;

	work = d;
	while (work)
		{
		FileData *fd;
		FlowerGroup *child;

		fd = work->data;
		work = work->next;

		child = pan_window_layout_compute_folders_flower_path(pw, fd->path, 0, 0);
		if (child) group->children = g_list_prepend(group->children, child);
		}

	filelist_free(d);

	return group;
}

static void pan_window_layout_compute_folders_flower(PanWindow *pw, const gchar *path,
						     gint *width, gint *height,
						     gint *scroll_x, gint *scroll_y)
{
	FlowerGroup *group;
	GList *list;

	group = pan_window_layout_compute_folders_flower_path(pw, path, 0, 0);
	pan_window_layout_compute_folder_flower_build(pw, group, NULL);

	pan_window_Layout_compute_folders_flower_size(pw, width, height);

	list = pan_item_find_by_path(pw, ITEM_BOX, path, FALSE, FALSE);
	if (list)
		{
		PanItem *pi = list->data;
		*scroll_x = pi->x + pi->width / 2;
		*scroll_y = pi->y + pi->height / 2;
		}
	g_list_free(list);
}

static void pan_window_layout_compute_folders_linear_path(PanWindow *pw, const gchar *path,
							  gint *x, gint *y, gint *level,
							  PanItem *parent,
							  gint *width, gint *height)
{
	GList *f;
	GList *d;
	GList *work;
	PanItem *pi_box;
	gint y_height = 0;

	if (!filelist_read(path, &f, &d)) return;
	if (!f && !d) return;

	f = filelist_sort(f, SORT_NAME, TRUE);
	d = filelist_sort(d, SORT_NAME, TRUE);

	*x = PAN_FOLDER_BOX_BORDER + ((*level) * MAX(PAN_FOLDER_BOX_BORDER, PAN_THUMB_GAP));

	pi_box = pan_item_new_text(pw, *x, *y, path, TEXT_ATTR_NONE,
				   PAN_TEXT_COLOR, 255);

	*y += pi_box->height;

	pi_box = pan_item_new_box(pw, file_data_new_simple(path),
				  *x, *y,
				  PAN_FOLDER_BOX_BORDER, PAN_FOLDER_BOX_BORDER,
				  PAN_FOLDER_BOX_OUTLINE_THICKNESS,
				  PAN_FOLDER_BOX_COLOR, PAN_FOLDER_BOX_ALPHA,
				  PAN_FOLDER_BOX_OUTLINE_COLOR, PAN_FOLDER_BOX_OUTLINE_ALPHA);

	*x += PAN_FOLDER_BOX_BORDER;
	*y += PAN_FOLDER_BOX_BORDER;

	work = f;
	while (work)
		{
		FileData *fd;
		PanItem *pi;

		fd = work->data;
		work = work->next;

		if (pw->size > LAYOUT_SIZE_THUMB_LARGE)
			{
			pi = pan_item_new_image(pw, fd, *x, *y, 10, 10);
			*x += pi->width + PAN_THUMB_GAP;
			if (pi->height > y_height) y_height = pi->height;
			}
		else
			{
			pi = pan_item_new_thumb(pw, fd, *x, *y);
			*x += PAN_THUMB_SIZE + PAN_THUMB_GAP;
			y_height = PAN_THUMB_SIZE;
			}

		pan_item_size_by_item(pi_box, pi, PAN_FOLDER_BOX_BORDER);
		}

	if (f) *y = pi_box->y + pi_box->height;

	g_list_free(f);

	work = d;
	while (work)
		{
		FileData *fd;

		fd = work->data;
		work = work->next;

		*level = *level + 1;
		pan_window_layout_compute_folders_linear_path(pw, fd->path, x, y, level,
							      pi_box, width, height);
		*level = *level - 1;
		}

	filelist_free(d);

	pan_item_size_by_item(parent, pi_box, PAN_FOLDER_BOX_BORDER);

	if (*y < pi_box->y + pi_box->height + PAN_FOLDER_BOX_BORDER)
		*y = pi_box->y + pi_box->height + PAN_FOLDER_BOX_BORDER;

	pan_item_size_coordinates(pi_box, PAN_FOLDER_BOX_BORDER, width, height);
}

static void pan_window_layout_compute_folders_linear(PanWindow *pw, const gchar *path, gint *width, gint *height)
{
	gint x, y;
	gint level;
	gint w, h;

	level = 0;
	x = PAN_FOLDER_BOX_BORDER;
	y = PAN_FOLDER_BOX_BORDER;
	w = PAN_FOLDER_BOX_BORDER * 2;
	h = PAN_FOLDER_BOX_BORDER * 2;

	pan_window_layout_compute_folders_linear_path(pw, path, &x, &y, &level, NULL, &w, &h);

	if (width) *width = w;
	if (height) *height = h;
}

/*
 *-----------------------------------------------------------------------------
 * calendar
 *-----------------------------------------------------------------------------
 */

#define PAN_CAL_DAY_WIDTH 100
#define PAN_CAL_DAY_HEIGHT 80
#define PAN_CAL_DOT_SIZE 3
#define PAN_CAL_DOT_GAP 2
#define PAN_CAL_DOT_COLOR 0, 0, 0
#define PAN_CAL_DOT_ALPHA 32

static void pan_calendar_update(PanWindow *pw, PanItem *pi_day)
{
	PanItem *pbox;
	PanItem *pi;
	GList *list;
	GList *work;
	gint x1, y1, x2, y2, x3, y3;
	gint x, y, w, h;
	gint grid;
	gint column;
	
	while ((pi = pan_item_find_by_key(pw, ITEM_NONE, "day_bubble"))) pan_item_remove(pw, pi);

	if (!pi_day || pi_day->type != ITEM_BOX ||
	    !pi_day->key || strcmp(pi_day->key, "day") != 0) return;

	list = pan_layout_intersect(pw, pi_day->x, pi_day->y, pi_day->width, pi_day->height);

	work = list;
	while (work)
		{
		PanItem *dot;
		GList *node;

		dot = work->data;
		node = work;
		work = work->next;

		if (dot->type != ITEM_BOX || !dot->fd ||
		    !dot->key || strcmp(dot->key, "dot") != 0)
			{
			list = g_list_delete_link(list, node);
			}
		}

	if (!list) return;

	grid = (gint)(sqrt(g_list_length(list)) + 0.5);

	x = pi_day->x + pi_day->width + 4;
	y = pi_day->y;

#if 0
	if (y + grid * (PAN_THUMB_SIZE + PAN_THUMB_GAP) + PAN_FOLDER_BOX_BORDER * 4 > pw->imd->image_height)
		{
		y = pw->imd->image_height - (grid * (PAN_THUMB_SIZE + PAN_THUMB_GAP) + PAN_FOLDER_BOX_BORDER * 4);
		}
#endif

	pbox = pan_item_new_box(pw, NULL, x, y, PAN_FOLDER_BOX_BORDER, PAN_FOLDER_BOX_BORDER,
				PAN_POPUP_BORDER,
				PAN_POPUP_COLOR, PAN_POPUP_ALPHA,
				PAN_POPUP_BORDER_COLOR, PAN_POPUP_ALPHA);
	pan_item_set_key(pbox, "day_bubble");

	pi = list->data;
	if (pi->fd)
		{
		PanItem *plabel;
		gchar *buf;

		buf = date_value_string(pi->fd->date, DATE_LENGTH_WEEK);
		plabel = pan_item_new_text(pw, x, y, buf, TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
					   PAN_POPUP_TEXT_COLOR, 255);
		pan_item_set_key(plabel, "day_bubble");
		g_free(buf);

		pan_item_size_by_item(pbox, plabel, 0);

		y += plabel->height;
		}

	column = 0;

	x += PAN_FOLDER_BOX_BORDER;
	y += PAN_FOLDER_BOX_BORDER;

	work = list;
	while (work)
		{
		PanItem *dot;

		dot = work->data;
		work = work->next;

		if (dot->fd)
			{
			PanItem *pimg;

			pimg = pan_item_new_thumb(pw, file_data_new_simple(dot->fd->path), x, y);
			pan_item_set_key(pimg, "day_bubble");

			pan_item_size_by_item(pbox, pimg, PAN_FOLDER_BOX_BORDER);

			column++;
			if (column < grid)
				{
				x += pimg->width + PAN_THUMB_GAP;
				}
			else
				{
				column = 0;
				x = pbox->x + PAN_FOLDER_BOX_BORDER;
				y += pimg->height + PAN_THUMB_GAP;
				}
			}
		}

	x1 = pi_day->x + pi_day->width - 8;
	y1 = pi_day->y + 8;
	x2 = pbox->x + 1;
	y2 = pbox->y + 36;
	x3 = pbox->x + 1;
	y3 = pbox->y + 12;
	triangle_rect_region(x1, y1, x2, y2, x3, y3,
			     &x, &y, &w, &h);

	pi = pan_item_new_tri(pw, NULL, x, y, w, h,
			      x1, y1, x2, y2, x3, y3,
			      PAN_POPUP_COLOR, PAN_POPUP_ALPHA);
	pan_item_tri_border(pi, BORDER_1 | BORDER_3, PAN_POPUP_BORDER_COLOR, PAN_POPUP_ALPHA);
	pan_item_set_key(pi, "day_bubble");
	pan_item_added(pw, pi);

	pan_item_box_shadow(pbox, PAN_SHADOW_OFFSET * 2, PAN_SHADOW_FADE * 2);
	pan_item_added(pw, pbox);
}

static void pan_window_layout_compute_calendar(PanWindow *pw, const gchar *path, gint *width, gint *height)
{
	GList *list;
	GList *work;
	gint x, y;
	time_t tc;
	gint count;
	gint day_max;
	gint day_width;
	gint day_height;
	gint grid;
	gint year = 0;
	gint month = 0;
	gint end_year = 0;
	gint end_month = 0;

	pw->cache_list = filelist_sort(pw->cache_list, SORT_TIME, TRUE);

	list = pan_window_layout_list(path, SORT_NONE, TRUE);
	list = filelist_sort(list, SORT_TIME, TRUE);

	day_max = 0;
	count = 0;
	tc = 0;
	work = list;
	while (work)
		{
		FileData *fd;

		fd = work->data;
		work = work->next;

		if (!date_compare(fd->date, tc, DATE_LENGTH_DAY))
			{
			count = 0;
			tc = fd->date;
			}
		else
			{
			count++;
			if (day_max < count) day_max = count;
			}
		}

	printf("biggest day contains %d images\n", day_max);

	grid = (gint)(sqrt((double)day_max) + 0.5) * (PAN_THUMB_SIZE + PAN_SHADOW_OFFSET * 2 + PAN_THUMB_GAP);
	day_width = MAX(PAN_CAL_DAY_WIDTH, grid);
	day_height = MAX(PAN_CAL_DAY_HEIGHT, grid);

	if (list)
		{
		FileData *fd = list->data;

		year = date_value(fd->date, DATE_LENGTH_YEAR);
		month = date_value(fd->date, DATE_LENGTH_MONTH);
		}

	work = g_list_last(list);
	if (work)
		{
		FileData *fd = work->data;
		end_year = date_value(fd->date, DATE_LENGTH_YEAR);
		end_month = date_value(fd->date, DATE_LENGTH_MONTH);
		}

	*width = PAN_FOLDER_BOX_BORDER * 2;
	*height = PAN_FOLDER_BOX_BORDER * 2;

	x = PAN_FOLDER_BOX_BORDER;
	y = PAN_FOLDER_BOX_BORDER;

	work = list;
	while (work && (year < end_year || (year == end_year && month <= end_month)))
		{
		PanItem *pi_month;
		PanItem *pi_text;
		gint day;
		gint days;
		gint col;
		gint row;
		time_t dt;
		gchar *buf;

		dt = date_to_time((month == 12) ? year + 1 : year, (month == 12) ? 1 : month + 1, 1);
		dt -= 60 * 60 * 24;
		days = date_value(dt, DATE_LENGTH_DAY);
		dt = date_to_time(year, month, 1);
		col = date_value(dt, DATE_LENGTH_WEEK);
		row = 1;

		x = PAN_FOLDER_BOX_BORDER;

		pi_month = pan_item_new_box(pw, NULL, x, y, PAN_CAL_DAY_WIDTH * 7, PAN_CAL_DAY_HEIGHT / 4,
					    PAN_FOLDER_BOX_OUTLINE_THICKNESS,
					    PAN_FOLDER_BOX_COLOR, PAN_FOLDER_BOX_ALPHA,
					    PAN_FOLDER_BOX_OUTLINE_COLOR, PAN_FOLDER_BOX_OUTLINE_ALPHA);
		buf = date_value_string(dt, DATE_LENGTH_MONTH);
		pi_text = pan_item_new_text(pw, x, y, buf,
					     TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
					     PAN_TEXT_COLOR, 255);
		g_free(buf);
		pi_text->x = pi_month->x + (pi_month->width - pi_text->width) / 2;

		pi_month->height = pi_text->y + pi_text->height - pi_month->y;

		x = PAN_FOLDER_BOX_BORDER + col * PAN_CAL_DAY_WIDTH;
		y = pi_month->y + pi_month->height + PAN_FOLDER_BOX_BORDER;

		for (day = 1; day <= days; day++)
			{
			FileData *fd;
			PanItem *pi_day;
			gint dx, dy;
			gint n = 0;

			dt = date_to_time(year, month, day);

			pi_day = pan_item_new_box(pw, NULL, x, y, PAN_CAL_DAY_WIDTH, PAN_CAL_DAY_HEIGHT,
						  PAN_FOLDER_BOX_OUTLINE_THICKNESS,
						  PAN_FOLDER_BOX_COLOR, PAN_FOLDER_BOX_ALPHA,
						  PAN_FOLDER_BOX_OUTLINE_COLOR, PAN_FOLDER_BOX_OUTLINE_ALPHA);
			pan_item_set_key(pi_day, "day");

			dx = x + PAN_CAL_DOT_GAP * 2;
			dy = y + PAN_CAL_DOT_GAP * 2;

			fd = (work) ? work->data : NULL;
			while (fd && date_compare(fd->date, dt, DATE_LENGTH_DAY))
				{
				PanItem *pi;

				pi = pan_item_new_box(pw, fd, dx, dy, PAN_CAL_DOT_SIZE, PAN_CAL_DOT_SIZE,
						      0,
						      PAN_CAL_DOT_COLOR, PAN_CAL_DOT_ALPHA,
						      0, 0, 0, 0);
				pan_item_set_key(pi, "dot");

				dx += PAN_CAL_DOT_SIZE + PAN_CAL_DOT_GAP;
				if (dx + PAN_CAL_DOT_SIZE > pi_day->x + pi_day->width - PAN_CAL_DOT_GAP * 2)
					{
					dx = x + PAN_CAL_DOT_GAP * 2;
					dy += PAN_CAL_DOT_SIZE + PAN_CAL_DOT_GAP;
					}
				if (dy + PAN_CAL_DOT_SIZE > pi_day->y + pi_day->height - PAN_CAL_DOT_GAP * 2)
					{
					/* must keep all dots within respective day even if it gets ugly */
					dy = y + PAN_CAL_DOT_GAP * 2;
					}

				pi_day->color_a = MIN(PAN_FOLDER_BOX_ALPHA + 64 + n, 255);
				n++;

				work = work->next;
				fd = (work) ? work->data : NULL;
				}

			buf = g_strdup_printf("%d", day);
			pan_item_new_text(pw, x + 4, y + 4, buf, TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
					  PAN_TEXT_COLOR, 255);
			g_free(buf);


			pan_item_size_coordinates(pi_day, PAN_FOLDER_BOX_BORDER, width, height);

			col++;
			if (col > 6)
				{
				col = 0;
				row++;
				x = PAN_FOLDER_BOX_BORDER;
				y += PAN_CAL_DAY_HEIGHT;
				}
			else
				{
				x += PAN_CAL_DAY_WIDTH;
				}
			}

		if (col > 0) y += PAN_CAL_DAY_HEIGHT;
		y += PAN_FOLDER_BOX_BORDER * 2;

		month ++;
		if (month > 12)
			{
			year++;
			month = 1;
			}
		}

	*width += grid;
	*height = MAX(*height, grid + PAN_FOLDER_BOX_BORDER * 2 * 2);

	g_list_free(list);
}

static void pan_window_layout_compute_timeline(PanWindow *pw, const gchar *path, gint *width, gint *height)
{
	GList *list;
	GList *work;
	gint x, y;
	time_t tc;
	gint total;
	gint count;
	PanItem *pi_month = NULL;
	PanItem *pi_day = NULL;
	gint month_start;
	gint day_start;
	gint x_width;
	gint y_height;

	pw->cache_list = filelist_sort(pw->cache_list, SORT_TIME, TRUE);

	list = pan_window_layout_list(path, SORT_NONE, TRUE);
	list = filelist_sort(list, SORT_TIME, TRUE);

	*width = PAN_FOLDER_BOX_BORDER * 2;
	*height = PAN_FOLDER_BOX_BORDER * 2;

	x = 0;
	y = 0;
	month_start = y;
	day_start = month_start;
	x_width = 0;
	y_height = 0;
	tc = 0;
	total = 0;
	count = 0;
	work = list;
	while (work)
		{
		FileData *fd;
		PanItem *pi;

		fd = work->data;
		work = work->next;

		if (!date_compare(fd->date, tc, DATE_LENGTH_DAY))
			{
			GList *needle;
			gchar *buf;

			if (!date_compare(fd->date, tc, DATE_LENGTH_MONTH))
				{
				pi_day = NULL;

				if (pi_month)
					{
					x = pi_month->x + pi_month->width + PAN_FOLDER_BOX_BORDER;
					}
				else
					{
					x = PAN_FOLDER_BOX_BORDER;
					}

				y = PAN_FOLDER_BOX_BORDER;

				buf = date_value_string(fd->date, DATE_LENGTH_MONTH);
				pi = pan_item_new_text(pw, x, y, buf,
						       TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
						       PAN_TEXT_COLOR, 255);
				y += pi->height;

				pi_month = pan_item_new_box(pw, file_data_new_simple(fd->path),
							    x, y, 0, 0,
							    PAN_FOLDER_BOX_OUTLINE_THICKNESS,
							    PAN_FOLDER_BOX_COLOR, PAN_FOLDER_BOX_ALPHA,
							    PAN_FOLDER_BOX_OUTLINE_COLOR, PAN_FOLDER_BOX_OUTLINE_ALPHA);

				x += PAN_FOLDER_BOX_BORDER;
				y += PAN_FOLDER_BOX_BORDER;
				month_start = y;
				}

			if (pi_day) x = pi_day->x + pi_day->width + PAN_FOLDER_BOX_BORDER;

			tc = fd->date;
			total = 1;
			count = 0;

			needle = work;
			while (needle)
				{
				FileData *nfd;

				nfd = needle->data;
				if (date_compare(nfd->date, tc, DATE_LENGTH_DAY))
					{
					needle = needle->next;
					total++;
					}
				else
					{
					needle = NULL;
					}
				}

			buf = date_value_string(fd->date, DATE_LENGTH_WEEK);
			pi = pan_item_new_text(pw, x, y, buf, TEXT_ATTR_NONE,
					       PAN_TEXT_COLOR, 255);
			g_free(buf);

			y += pi->height;

			pi_day = pan_item_new_box(pw, file_data_new_simple(fd->path), x, y, 0, 0,
						  PAN_FOLDER_BOX_OUTLINE_THICKNESS,
						  PAN_FOLDER_BOX_COLOR, PAN_FOLDER_BOX_ALPHA,
						  PAN_FOLDER_BOX_OUTLINE_COLOR, PAN_FOLDER_BOX_OUTLINE_ALPHA);

			x += PAN_FOLDER_BOX_BORDER;
			y += PAN_FOLDER_BOX_BORDER;
			day_start = y;
			}

		if (pw->size > LAYOUT_SIZE_THUMB_LARGE)
			{
			pi = pan_item_new_image(pw, fd, x, y, 10, 10);
			if (pi->width > x_width) x_width = pi->width;
			y_height = pi->height;
			}
		else
			{
			pi = pan_item_new_thumb(pw, fd, x, y);
			x_width = PAN_THUMB_SIZE;
			y_height = PAN_THUMB_SIZE;
			}

		pan_item_size_by_item(pi_day, pi, PAN_FOLDER_BOX_BORDER);
		pan_item_size_by_item(pi_month, pi_day, PAN_FOLDER_BOX_BORDER);

		total--;
		count++;

		if (total > 0 && count < PAN_GROUP_MAX)
			{
			y += y_height + PAN_THUMB_GAP;
			}
		else
			{
			x += x_width + PAN_THUMB_GAP;
			x_width = 0;
			count = 0;

			if (total > 0)
				y = day_start;
			else
				y = month_start;
			}

		pan_item_size_coordinates(pi_month, PAN_FOLDER_BOX_BORDER, width, height);
		}

	g_list_free(list);
}

static void pan_window_layout_compute(PanWindow *pw, const gchar *path,
				      gint *width, gint *height,
				      gint *scroll_x, gint *scroll_y)
{
	pan_window_items_free(pw);

	switch (pw->size)
		{
		case LAYOUT_SIZE_THUMB_DOTS:
			pw->thumb_size = PAN_THUMB_SIZE_DOTS;
			pw->thumb_gap = PAN_THUMB_GAP_DOTS;
			break;
		case LAYOUT_SIZE_THUMB_NONE:
			pw->thumb_size = PAN_THUMB_SIZE_NONE;
			pw->thumb_gap = PAN_THUMB_GAP_SMALL;
			break;
		case LAYOUT_SIZE_THUMB_SMALL:
			pw->thumb_size = PAN_THUMB_SIZE_SMALL;
			pw->thumb_gap = PAN_THUMB_GAP_SMALL;
			break;
		case LAYOUT_SIZE_THUMB_NORMAL:
		default:
			pw->thumb_size = PAN_THUMB_SIZE_NORMAL;
			pw->thumb_gap = PAN_THUMB_GAP_NORMAL;
			break;
		case LAYOUT_SIZE_THUMB_LARGE:
			pw->thumb_size = PAN_THUMB_SIZE_LARGE;
			pw->thumb_gap = PAN_THUMB_GAP_LARGE;
			break;
		case LAYOUT_SIZE_10:
			pw->image_size = 10;
			pw->thumb_gap = PAN_THUMB_GAP_NORMAL;
			break;
		case LAYOUT_SIZE_25:
			pw->image_size = 25;
			pw->thumb_gap = PAN_THUMB_GAP_NORMAL;
			break;
		case LAYOUT_SIZE_33:
			pw->image_size = 33;
			pw->thumb_gap = PAN_THUMB_GAP_LARGE;
			break;
		case LAYOUT_SIZE_50:
			pw->image_size = 50;
			pw->thumb_gap = PAN_THUMB_GAP_HUGE;
			break;
		case LAYOUT_SIZE_100:
			pw->image_size = 100;
			pw->thumb_gap = PAN_THUMB_GAP_HUGE;
			break;
		}

	*width = 0;
	*height = 0;
	*scroll_x = 0;
	*scroll_y = 0;

	switch (pw->layout)
		{
		case LAYOUT_GRID:
		default:
			pan_window_layout_compute_grid(pw, path, width, height);
			break;
		case LAYOUT_FOLDERS_LINEAR:
			pan_window_layout_compute_folders_linear(pw, path, width, height);
			break;
		case LAYOUT_FOLDERS_FLOWER:
			pan_window_layout_compute_folders_flower(pw, path, width, height, scroll_x, scroll_y);
			break;
		case LAYOUT_CALENDAR:
			pan_window_layout_compute_calendar(pw, path, width, height);
			break;
		case LAYOUT_TIMELINE:
			pan_window_layout_compute_timeline(pw, path, width, height);
			break;
		}

	pan_cache_free(pw);

	printf("computed %d objects\n", g_list_length(pw->list));
}

static GList *pan_layout_intersect(PanWindow *pw, gint x, gint y, gint width, gint height)
{
	GList *list = NULL;
	GList *work;

	work = pw->list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		if (util_clip_region_test(x, y, width, height,
					  pi->x, pi->y, pi->width, pi->height))
			{
			list = g_list_prepend(list, pi);
			}
		}

	return list;
}



/*
 *-----------------------------------------------------------------------------
 * tile generation
 *-----------------------------------------------------------------------------
 */

static gint pan_layout_queue_step(PanWindow *pw);


static void pan_layout_queue_thumb_done_cb(ThumbLoader *tl, gpointer data)
{
	PanWindow *pw = data;

	if (pw->queue_pi)
		{
		PanItem *pi;
		gint rc;

		pi = pw->queue_pi;
		pw->queue_pi = NULL;

		pi->queued = FALSE;

		if (pi->pixbuf) g_object_unref(pi->pixbuf);
		pi->pixbuf = thumb_loader_get_pixbuf(tl, TRUE);

		rc = pi->refcount;
		image_area_changed(pw->imd, pi->x, pi->y, pi->width, pi->height);
		pi->refcount = rc;
		}

	thumb_loader_free(pw->tl);
	pw->tl = NULL;

	while (pan_layout_queue_step(pw));
}

static void pan_layout_queue_image_done_cb(ImageLoader *il, gpointer data)
{
	PanWindow *pw = data;

	if (pw->queue_pi)
		{
		PanItem *pi;
		gint rc;

		pi = pw->queue_pi;
		pw->queue_pi = NULL;

		pi->queued = FALSE;

		if (pi->pixbuf) g_object_unref(pi->pixbuf);
		pi->pixbuf = image_loader_get_pixbuf(pw->il);
		if (pi->pixbuf) g_object_ref(pi->pixbuf);

		if (pi->pixbuf && pw->size != LAYOUT_SIZE_100 &&
		    (gdk_pixbuf_get_width(pi->pixbuf) > pi->width ||
		     gdk_pixbuf_get_height(pi->pixbuf) > pi->height))
			{
			GdkPixbuf *tmp;

			tmp = pi->pixbuf;
			pi->pixbuf = gdk_pixbuf_scale_simple(tmp, pi->width, pi->height,
							     (GdkInterpType)zoom_quality);
			g_object_unref(tmp);
			}

		rc = pi->refcount;
		image_area_changed(pw->imd, pi->x, pi->y, pi->width, pi->height);
		pi->refcount = rc;
		}

	image_loader_free(pw->il);
	pw->il = NULL;

	while (pan_layout_queue_step(pw));
}

#if 0
static void pan_layout_queue_image_area_cb(ImageLoader *il, guint x, guint y,
					   guint width, guint height, gpointer data)
{
	PanWindow *pw = data;

	if (pw->queue_pi)
		{
		PanItem *pi;
		gint rc;

		pi = pw->queue_pi;

		if (!pi->pixbuf)
			{
			pi->pixbuf = image_loader_get_pixbuf(pw->il);
			if (pi->pixbuf) g_object_ref(pi->pixbuf);
			}

		rc = pi->refcount;
		image_area_changed(pw->imd, pi->x + x, pi->y + y, width, height);
		pi->refcount = rc;
		}
}
#endif

static gint pan_layout_queue_step(PanWindow *pw)
{
	PanItem *pi;

	if (!pw->queue) return FALSE;

	pi = pw->queue->data;
	pw->queue = g_list_remove(pw->queue, pi);
	pw->queue_pi = pi;

	if (!pw->queue_pi->fd)
		{
		pw->queue_pi->queued = FALSE;
		pw->queue_pi = NULL;
		return TRUE;
		}

	image_loader_free(pw->il);
	pw->il = NULL;
	thumb_loader_free(pw->tl);
	pw->tl = NULL;

	if (pi->type == ITEM_IMAGE)
		{
		pw->il = image_loader_new(pi->fd->path);

		if (pw->size != LAYOUT_SIZE_100)
			{
			image_loader_set_requested_size(pw->il, pi->width, pi->height);
			}

#if 0
		image_loader_set_area_ready_func(pw->il, pan_layout_queue_image_area_cb, pw);
#endif
		image_loader_set_error_func(pw->il, pan_layout_queue_image_done_cb, pw);

		if (image_loader_start(pw->il, pan_layout_queue_image_done_cb, pw)) return FALSE;

		image_loader_free(pw->il);
		pw->il = NULL;
		}
	else if (pi->type == ITEM_THUMB)
		{
		pw->tl = thumb_loader_new(PAN_THUMB_SIZE, PAN_THUMB_SIZE);

		if (!pw->tl->standard_loader)
			{
			/* The classic loader will recreate a thumbnail any time we
			 * request a different size than what exists. This view will
			 * almost never use the user configured sizes so disable cache.
			 */
			thumb_loader_set_cache(pw->tl, FALSE, FALSE, FALSE);
			}

		thumb_loader_set_callbacks(pw->tl,
					   pan_layout_queue_thumb_done_cb,
					   pan_layout_queue_thumb_done_cb,
					   NULL, pw);

		if (thumb_loader_start(pw->tl, pi->fd->path)) return FALSE;

		thumb_loader_free(pw->tl);
		pw->tl = NULL;
		}

	pw->queue_pi->queued = FALSE;
	pw->queue_pi = NULL;
	return TRUE;
}

static void pan_layout_queue(PanWindow *pw, PanItem *pi)
{
	if (!pi || pi->queued || pi->pixbuf) return;
	if (pw->size <= LAYOUT_SIZE_THUMB_NONE) return;

	pi->queued = TRUE;
	pw->queue = g_list_prepend(pw->queue, pi);

	if (!pw->tl && !pw->il) while(pan_layout_queue_step(pw));
}

static gint pan_window_request_tile_cb(ImageWindow *imd, gint x, gint y, gint width, gint height,
				       GdkPixbuf *pixbuf, gpointer data)
{
	PanWindow *pw = data;
	GList *list;
	GList *work;
	gint i;

	pixbuf_draw_rect_fill(pixbuf,
			 0, 0, width, height,
			 PAN_BACKGROUND_COLOR, 255);

	for (i = (x / PAN_GRID_SIZE) * PAN_GRID_SIZE; i < x + width; i += PAN_GRID_SIZE)
		{
		gint rx, ry, rw, rh;

		if (util_clip_region(x, y, width, height,
				     i, y, 1, height,
				     &rx, &ry, &rw, &rh))
			{
			pixbuf_draw_rect_fill(pixbuf,
					      rx - x, ry - y, rw, rh,
					      PAN_GRID_COLOR, PAN_GRID_ALPHA);
			}
		}
	for (i = (y / PAN_GRID_SIZE) * PAN_GRID_SIZE; i < y + height; i += PAN_GRID_SIZE)
		{
		gint rx, ry, rw, rh;

		if (util_clip_region(x, y, width, height,
				     x, i, width, 1,
				     &rx, &ry, &rw, &rh))
			{
			pixbuf_draw_rect_fill(pixbuf,
					      rx - x, ry - y, rw, rh,
					      PAN_GRID_COLOR, PAN_GRID_ALPHA);
			}
		}

	list = pan_layout_intersect(pw, x, y, width, height);
	work = list;
	while (work)
		{
		PanItem *pi;
		gint tx, ty, tw, th;
		gint rx, ry, rw, rh;

		pi = work->data;
		work = work->next;

		pi->refcount++;

		if (pi->type == ITEM_THUMB && pi->pixbuf)
			{
			tw = gdk_pixbuf_get_width(pi->pixbuf);
			th = gdk_pixbuf_get_height(pi->pixbuf);

			tx = pi->x + (pi->width - tw) / 2;
			ty = pi->y + (pi->height - th) / 2;

			if (gdk_pixbuf_get_has_alpha(pi->pixbuf))
				{
				if (util_clip_region(x, y, width, height,
						     tx + PAN_SHADOW_OFFSET, ty + PAN_SHADOW_OFFSET, tw, th,
						     &rx, &ry, &rw, &rh))
					{
					pixbuf_draw_shadow(pixbuf,
							   rx - x, ry - y, rw, rh,
							   tx + PAN_SHADOW_OFFSET - x, ty + PAN_SHADOW_OFFSET - y, tw, th,
							   PAN_SHADOW_FADE,
							   PAN_SHADOW_COLOR, PAN_SHADOW_ALPHA);
					}
				}
			else
				{
				if (util_clip_region(x, y, width, height,
						     tx + tw, ty + PAN_SHADOW_OFFSET,
						     PAN_SHADOW_OFFSET, th - PAN_SHADOW_OFFSET,
						     &rx, &ry, &rw, &rh))
					{
					pixbuf_draw_shadow(pixbuf,
							   rx - x, ry - y, rw, rh,
							   tx + PAN_SHADOW_OFFSET - x, ty + PAN_SHADOW_OFFSET - y, tw, th,
							   PAN_SHADOW_FADE,
							   PAN_SHADOW_COLOR, PAN_SHADOW_ALPHA);
					}
				if (util_clip_region(x, y, width, height,
						     tx + PAN_SHADOW_OFFSET, ty + th, tw, PAN_SHADOW_OFFSET,
						     &rx, &ry, &rw, &rh))
					{
					pixbuf_draw_shadow(pixbuf,
							   rx - x, ry - y, rw, rh,
							   tx + PAN_SHADOW_OFFSET - x, ty + PAN_SHADOW_OFFSET - y, tw, th,
							   PAN_SHADOW_FADE,
							   PAN_SHADOW_COLOR, PAN_SHADOW_ALPHA);
					}
				}

			if (util_clip_region(x, y, width, height,
					     tx, ty, tw, th,
					     &rx, &ry, &rw, &rh))
				{
				gdk_pixbuf_composite(pi->pixbuf, pixbuf, rx - x, ry - y, rw, rh,
						     (double) tx - x,
						     (double) ty - y,
						     1.0, 1.0, GDK_INTERP_NEAREST,
						     255);
				}

			if (util_clip_region(x, y, width, height,
					     tx, ty, tw, PAN_OUTLINE_THICKNESS,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      PAN_OUTLINE_COLOR_1, PAN_OUTLINE_ALPHA);
				}
			if (util_clip_region(x, y, width, height,
					     tx, ty, PAN_OUTLINE_THICKNESS, th,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      PAN_OUTLINE_COLOR_1, PAN_OUTLINE_ALPHA);
				}
			if (util_clip_region(x, y, width, height,
					     tx + tw - PAN_OUTLINE_THICKNESS, ty +  PAN_OUTLINE_THICKNESS,
					     PAN_OUTLINE_THICKNESS, th - PAN_OUTLINE_THICKNESS,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      PAN_OUTLINE_COLOR_2, PAN_OUTLINE_ALPHA);
				}
			if (util_clip_region(x, y, width, height,
					     tx +  PAN_OUTLINE_THICKNESS, ty + th - PAN_OUTLINE_THICKNESS,
					     tw - PAN_OUTLINE_THICKNESS * 2, PAN_OUTLINE_THICKNESS,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      PAN_OUTLINE_COLOR_2, PAN_OUTLINE_ALPHA);
				}
			}
		else if (pi->type == ITEM_THUMB)
			{
			tw = pi->width - PAN_SHADOW_OFFSET * 2;
			th = pi->height - PAN_SHADOW_OFFSET * 2;
			tx = pi->x + PAN_SHADOW_OFFSET;
			ty = pi->y + PAN_SHADOW_OFFSET;

			if (util_clip_region(x, y, width, height,
					     tx, ty, tw, th,
					     &rx, &ry, &rw, &rh))
				{
				gint d;

				d = (pw->size <= LAYOUT_SIZE_THUMB_NONE) ? 2 : 8;
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      PAN_SHADOW_COLOR,
						      PAN_SHADOW_ALPHA / d);
				}

			pan_layout_queue(pw, pi);
			}
		else if (pi->type == ITEM_IMAGE)
			{
			if (util_clip_region(x, y, width, height,
					     pi->x, pi->y, pi->width, pi->height,
					     &rx, &ry, &rw, &rh))
				{
				if (pi->pixbuf)
					{
					gdk_pixbuf_composite(pi->pixbuf, pixbuf, rx - x, ry - y, rw, rh,
							     (double) pi->x - x,
							     (double) pi->y - y,
							     1.0, 1.0, GDK_INTERP_NEAREST,
							     255);
					}
				else
					{
					pixbuf_draw_rect_fill(pixbuf,
							      rx - x, ry - y, rw, rh,
							      PAN_SHADOW_COLOR, PAN_SHADOW_ALPHA / 2);
					pan_layout_queue(pw, pi);
					}
				}
			}
		else if (pi->type == ITEM_BOX)
			{
			gint bw, bh;
			gint *shadow;

			bw = pi->width;
			bh = pi->height;

			shadow = pi->data;
			if (shadow)
				{
				bw -= shadow[0];
				bh -= shadow[0];

				if (pi->color_a > 254)
					{
					pixbuf_draw_shadow(pixbuf, pi->x - x + bw, pi->y - y + shadow[0],
							   shadow[0], bh - shadow[0],
							   pi->x - x + shadow[0], pi->y - y + shadow[0], bw, bh,
							   shadow[1],
							   PAN_SHADOW_COLOR, PAN_SHADOW_ALPHA);
					pixbuf_draw_shadow(pixbuf, pi->x - x + shadow[0], pi->y - y + bh,
							   bw, shadow[0],
							   pi->x - x + shadow[0], pi->y - y + shadow[0], bw, bh,
							   shadow[1],
							   PAN_SHADOW_COLOR, PAN_SHADOW_ALPHA);
					}
				else
					{
					gint a;
					a = pi->color_a * PAN_SHADOW_ALPHA >> 8;
					pixbuf_draw_shadow(pixbuf, pi->x - x + shadow[0], pi->y - y + shadow[0],
							   bw, bh,
							   pi->x - x + shadow[0], pi->y - y + shadow[0], bw, bh,
							   shadow[1],
							   PAN_SHADOW_COLOR, a);
					}
				}

			if (util_clip_region(x, y, width, height,
					     pi->x, pi->y, bw, bh,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      pi->color_r, pi->color_g, pi->color_b, pi->color_a);
				}
			if (util_clip_region(x, y, width, height,
					     pi->x, pi->y, bw, pi->border,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
				}
			if (util_clip_region(x, y, width, height,
					     pi->x, pi->y + pi->border, pi->border, bh - pi->border * 2,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
				}
			if (util_clip_region(x, y, width, height,
					     pi->x + bw - pi->border, pi->y + pi->border,
					     pi->border, bh - pi->border * 2,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
				}
			if (util_clip_region(x, y, width, height,
					     pi->x, pi->y + bh - pi->border,
					     bw,  pi->border,
					     &rx, &ry, &rw, &rh))
				{
				pixbuf_draw_rect_fill(pixbuf,
						      rx - x, ry - y, rw, rh,
						      pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
				}
			}
		else if (pi->type == ITEM_TRIANGLE)
			{
			if (util_clip_region(x, y, width, height,
					     pi->x, pi->y, pi->width, pi->height,
					     &rx, &ry, &rw, &rh) && pi->data)
				{
				gint *coord = pi->data;
				pixbuf_draw_triangle(pixbuf,
						     rx - x, ry - y, rw, rh,
						     coord[0] - x, coord[1] - y,
						     coord[2] - x, coord[3] - y,
						     coord[4] - x, coord[5] - y,
						     pi->color_r, pi->color_g, pi->color_b, pi->color_a);

				if (pi->border & BORDER_1)
					{
					pixbuf_draw_line(pixbuf,
							 rx - x, ry - y, rw, rh,
							 coord[0] - x, coord[1] - y,
							 coord[2] - x, coord[3] - y,
							 pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
					}
				if (pi->border & BORDER_2)
					{
					pixbuf_draw_line(pixbuf,
							 rx - x, ry - y, rw, rh,
							 coord[2] - x, coord[3] - y,
							 coord[4] - x, coord[5] - y,
							 pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
					}
				if (pi->border & BORDER_3)
					{
					pixbuf_draw_line(pixbuf,
							 rx - x, ry - y, rw, rh,
							 coord[4] - x, coord[5] - y,
							 coord[0] - x, coord[1] - y,
							 pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
					}
				}
			}
		else if (pi->type == ITEM_TEXT && pi->text)
			{
			PangoLayout *layout;

			layout = pan_item_text_layout(pi, imd->image);
			pixbuf_draw_layout(pixbuf, layout, imd->image,
					   pi->x - x + PAN_TEXT_BORDER_SIZE, pi->y - y + PAN_TEXT_BORDER_SIZE,
					   pi->color_r, pi->color_g, pi->color_b, pi->color_a);
			g_object_unref(G_OBJECT(layout));
			}
		}
	g_list_free(list);

	if (0)
		{
		static gint count = 0;
		PangoLayout *layout;
		gint lx, ly;
		gint lw, lh;
		GdkPixmap *pixmap;
		gchar *buf;

		layout = gtk_widget_create_pango_layout(imd->image, NULL);

		buf = g_strdup_printf("%d,%d\n(#%d)", x, y,
				      (x / imd->source_tile_width) +
				      (y / imd->source_tile_height * (imd->image_width/imd->source_tile_width + 1)));
		pango_layout_set_text(layout, buf, -1);
		g_free(buf);

		pango_layout_get_pixel_size(layout, &lw, &lh);

		pixmap = gdk_pixmap_new(imd->widget->window, lw, lh, -1);
		gdk_draw_rectangle(pixmap, imd->widget->style->black_gc, TRUE, 0, 0, lw, lh);
		gdk_draw_layout(pixmap, imd->widget->style->white_gc, 0, 0, layout);
		g_object_unref(G_OBJECT(layout));

		lx = MAX(0, width / 2 - lw / 2);
		ly = MAX(0, height / 2 - lh / 2);
		lw = MIN(lw, width - lx);
		lh = MIN(lh, height - ly);
		gdk_pixbuf_get_from_drawable(pixbuf, pixmap, gdk_drawable_get_colormap(imd->image->window),
					     0, 0, lx, ly, lw, lh);
		g_object_unref(pixmap);

		count++;
		}

	return TRUE;
}

static void pan_window_dispose_tile_cb(ImageWindow *imd, gint x, gint y, gint width, gint height,
				       GdkPixbuf *pixbuf, gpointer data)
{
	PanWindow *pw = data;
	GList *list;
	GList *work;

	list = pan_layout_intersect(pw, x, y, width, height);
	work = list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		if (pi->refcount > 0)
			{
			pi->refcount--;

			if ((pi->type == ITEM_THUMB || pi->type == ITEM_IMAGE) &&
			    pi->refcount == 0)
				{
				if (pi->queued)
					{
					pw->queue = g_list_remove(pw->queue, pi);
					pi->queued = FALSE;
					}
				if (pw->queue_pi == pi) pw->queue_pi = NULL;
				if (pi->pixbuf)
					{
					g_object_unref(pi->pixbuf);
					pi->pixbuf = NULL;
					}
				}
			}
		}

	g_list_free(list);
}


/*
 *-----------------------------------------------------------------------------
 * misc
 *-----------------------------------------------------------------------------
 */ 

static void pan_window_message(PanWindow *pw, const gchar *text)
{
	GList *work;
	gint count = 0;
	gint64 size = 0;
	gchar *ss;
	gchar *buf;

	if (text)
		{
		gtk_label_set_text(GTK_LABEL(pw->label_message), text);
		return;
		}

	work = pw->list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		if (pi->fd &&
		    (pi->type == ITEM_THUMB || pi->type == ITEM_IMAGE))
			{
			size += pi->fd->size;
			count++;
			}
		}

	ss = text_from_size_abrev(size);
	buf = g_strdup_printf(_("%d images, %s"), count, ss);
	g_free(ss);
	gtk_label_set_text(GTK_LABEL(pw->label_message), buf);
	g_free(buf);
}

static ImageWindow *pan_window_active_image(PanWindow *pw)
{
	if (pw->fs) return pw->fs->imd;

	return pw->imd;
}

static void pan_window_zoom_limit(PanWindow *pw)
{
	gdouble min;

	switch (pw->size)
		{
		case LAYOUT_SIZE_THUMB_DOTS:
		case LAYOUT_SIZE_THUMB_NONE:
		case LAYOUT_SIZE_THUMB_SMALL:
		case LAYOUT_SIZE_THUMB_NORMAL:
#if 0
			/* easily requires > 512mb ram when window size > 1024x768 and zoom is <= -8 */
			min = -16.0;
			break;
#endif
		case LAYOUT_SIZE_THUMB_LARGE:
			min = -6.0;
			break;
		case LAYOUT_SIZE_10:
		case LAYOUT_SIZE_25:
			min = -4.0;
			break;
		case LAYOUT_SIZE_33:
		case LAYOUT_SIZE_50:
		case LAYOUT_SIZE_100:
		default:
			min = -2.0;
			break;
		}

	image_zoom_set_limits(pw->imd, min, 32.0);
}

static gint pan_window_layout_update_idle_cb(gpointer data)
{
	PanWindow *pw = data;
	gint width;
	gint height;
	gint scroll_x;
	gint scroll_y;

	if (pw->size > LAYOUT_SIZE_THUMB_LARGE)
		{
		if (!pw->cache_list && !pw->cache_todo)
			{
			pan_cache_fill(pw, pw->path);
			if (pw->cache_todo)
				{
				pan_window_message(pw, _("Reading dimensions..."));
				return TRUE;
				}
			}
		if (pan_cache_step(pw))
			{
			pw->cache_count++;
			pw->cache_tick++;
			if (pw->cache_count == pw->cache_total)
				{
				pan_window_message(pw, _("Sorting images..."));
				}
			else if (pw->cache_tick > 9)
				{
				gchar *buf;

				buf = g_strdup_printf("%s %d", _("Reading dimensions..."),
						      pw->cache_total - pw->cache_count);
				pan_window_message(pw, buf);
				g_free(buf);

				pw->cache_tick = 0;
				}

			return TRUE;
			}
		}

	pan_window_layout_compute(pw, pw->path, &width, &height, &scroll_x, &scroll_y);

	pan_window_zoom_limit(pw);

	if (width > 0 && height > 0)
		{
		gdouble align;

		image_set_image_as_tiles(pw->imd, width, height,
					 PAN_TILE_SIZE, PAN_TILE_SIZE, 8,
					 pan_window_request_tile_cb,
					 pan_window_dispose_tile_cb, pw, 1.0);

		if (scroll_x == 0 && scroll_y == 0)
			{
			align = 0.0;
			}
		else
			{
			align = 0.5;
			}
		image_scroll_to_point(pw->imd, scroll_x, scroll_y, align, align);
		}

	pan_window_message(pw, NULL);

	pw->idle_id = -1;

	return FALSE;
}

static void pan_window_layout_update_idle(PanWindow *pw)
{
	if (pw->idle_id == -1)
		{
		pan_window_message(pw, _("Sorting images..."));
		pw->idle_id = g_idle_add(pan_window_layout_update_idle_cb, pw);
		}
}

/*
 *-----------------------------------------------------------------------------
 * pan window keyboard
 *-----------------------------------------------------------------------------
 */

static const gchar *pan_menu_click_path(PanWindow *pw)
{
	if (pw->click_pi && pw->click_pi->fd) return pw->click_pi->fd->path;
	return NULL;
}

static void pan_window_menu_pos_cb(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	PanWindow *pw = data;
	ImageWindow *imd;

	imd = pan_window_active_image(pw);
	gdk_window_get_origin(imd->image->window, x, y);
	popup_menu_position_clamp(menu, x, y, 0);
}

static gint pan_window_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	PanWindow *pw = data;
	ImageWindow *imd;
	const gchar *path;
	gint stop_signal = FALSE;
	GtkWidget *menu;
	gint x = 0;
	gint y = 0;
	gint focused;

	focused = (pw->fs || GTK_WIDGET_HAS_FOCUS(pw->imd->widget));

	imd = pan_window_active_image(pw);
	path = pan_menu_click_path(pw);

	if (focused)
		{
		switch (event->keyval)
			{
			case GDK_Left: case GDK_KP_Left:
				x -= 1;
				stop_signal = TRUE;
				break;
			case GDK_Right: case GDK_KP_Right:
				x += 1;
				stop_signal = TRUE;
				break;
			case GDK_Up: case GDK_KP_Up:
				y -= 1;
				stop_signal = TRUE;
				break;
			case GDK_Down: case GDK_KP_Down:
				y += 1;
				stop_signal = TRUE;
				break;
			case GDK_Page_Up: case GDK_KP_Page_Up:
				image_scroll(imd, 0, 0-imd->vis_height / 2);
				break;
			case GDK_Page_Down: case GDK_KP_Page_Down:
				image_scroll(imd, 0, imd->vis_height / 2);
				break;
			case GDK_Home: case GDK_KP_Home:
				image_scroll(imd, 0-imd->vis_width / 2, 0);
				break;
			case GDK_End: case GDK_KP_End:
				image_scroll(imd, imd->vis_width / 2, 0);
				break;
			}
		}

	if (focused && !(event->state & GDK_CONTROL_MASK) )
	    switch (event->keyval)
		{
		case '+': case '=': case GDK_KP_Add:
			image_zoom_adjust(imd, ZOOM_INCREMENT);
			break;
		case '-': case GDK_KP_Subtract:
			image_zoom_adjust(imd, -ZOOM_INCREMENT);
			break;
		case 'Z': case 'z': case GDK_KP_Divide: case '1':
			image_zoom_set(imd, 1.0);
			break;
		case '2':
			image_zoom_set(imd, 2.0);
			break;
		case '3':
			image_zoom_set(imd, 3.0);
			break;
		case '4':
			image_zoom_set(imd, 4.0);
			break;
		case '7':
			image_zoom_set(imd, -4.0);
			break;
		case '8':
			image_zoom_set(imd, -3.0);
			break;
		case '9':
			image_zoom_set(imd, -2.0);
			break;
		case 'F': case 'f':
		case 'V': case 'v':
			pan_fullscreen_toggle(pw, FALSE);
			stop_signal = TRUE;
			break;
		case 'I': case 'i':
			pan_overlay_toggle(pw);
			break;
		case GDK_Delete: case GDK_KP_Delete:
			break;
		case '/':
			if (!pw->fs)
				{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw->search_button), TRUE);
				stop_signal = TRUE;
				}
			break;
		case GDK_Escape:
			if (pw->fs)
				{
				pan_fullscreen_toggle(pw, TRUE);
				stop_signal = TRUE;
				}
			else if (GTK_WIDGET_VISIBLE(pw->search_entry))
				{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw->search_button), FALSE);
				stop_signal = TRUE;
				}
			break;
		case GDK_Menu:
		case GDK_F10:
			menu = pan_popup_menu(pw);
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, pan_window_menu_pos_cb, pw, 0, GDK_CURRENT_TIME);
			stop_signal = TRUE;
			break;
		}

	if (event->state & GDK_CONTROL_MASK)
		{
		gint n = -1;
		switch (event->keyval)
			{
			case '1':
				n = 0;
				break;
			case '2':
				n = 1;
				break;
			case '3':
				n = 2;
				break;
			case '4':
				n = 3;
				break;
			case '5':
				n = 4;
				break;
			case '6':
				n = 5;
				break;
			case '7':
				n = 6;
				break;
			case '8':
				n = 7;
				break;
			case '9':
				n = 8;
				break;
			case '0':
				n = 9;
				break;
			case 'C': case 'c':
				if (path) file_util_copy(path, NULL, NULL, imd->widget);
				break;
			case 'M': case 'm':
				if (path) file_util_move(path, NULL, NULL, imd->widget);
				break;
			case 'R': case 'r':
				if (path) file_util_rename(path, NULL, imd->widget);
				break;
			case 'D': case 'd':
				if (path) file_util_delete(path, NULL, imd->widget);
				break;
			case 'P': case 'p':
				if (path) info_window_new(path, NULL);
				break;
			case 'W': case 'w':
				pan_window_close(pw);
				break;
			}
		if (n != -1 && path)
			{
			pan_fullscreen_toggle(pw, TRUE);
			start_editor_from_file(n, path);
			stop_signal = TRUE;
			}
		}
	else if (event->state & GDK_SHIFT_MASK)
		{
		x *= 3;
		y *= 3;
		}
	else if (!focused)
		{
		switch (event->keyval)
			{
			case GDK_Escape:
				if (pw->fs)
					{
					pan_fullscreen_toggle(pw, TRUE);
					stop_signal = TRUE;
					}
				else if (GTK_WIDGET_HAS_FOCUS(pw->search_entry))
					{
					gtk_widget_grab_focus(pw->imd->widget);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw->search_button), FALSE);
					stop_signal = TRUE;
					}
			break;
			default:
				break;
			}
		}

	if (x != 0 || y!= 0)
		{
		keyboard_scroll_calc(&x, &y, event);
		image_scroll(imd, x, y);
		}

	return stop_signal;
}

/*
 *-----------------------------------------------------------------------------
 * info popup
 *-----------------------------------------------------------------------------
 */

static void pan_info_update(PanWindow *pw, PanItem *pi)
{
	PanItem *pbox;
	PanItem *plabel;
	PanItem *p;
	gchar *buf;
	gint x1, y1, x2, y2, x3, y3;
	gint x, y, w, h;

	if (pw->click_pi == pi) return;
	if (pi && !pi->fd) pi = NULL;

	while ((p = pan_item_find_by_key(pw, ITEM_NONE, "info"))) pan_item_remove(pw, p);
	pw->click_pi = pi;

	if (!pi) return;

	printf("info set to %s\n", pi->fd->path);

	pbox = pan_item_new_box(pw, NULL, pi->x + pi->width + 4, pi->y, 10, 10,
			     PAN_POPUP_BORDER,
			     PAN_POPUP_COLOR, PAN_POPUP_ALPHA,
			     PAN_POPUP_BORDER_COLOR, PAN_POPUP_ALPHA);
	pan_item_set_key(pbox, "info");

	if (pi->type == ITEM_THUMB && pi->pixbuf)
		{
		w = gdk_pixbuf_get_width(pi->pixbuf);
		h = gdk_pixbuf_get_height(pi->pixbuf);

		x1 = pi->x + pi->width - (pi->width - w) / 2 - 8;
		y1 = pi->y + (pi->height - h) / 2 + 8;
		}
	else
		{
		x1 = pi->x + pi->width - 8;
		y1 = pi->y + 8;
		}

	x2 = pbox->x + 1;
	y2 = pbox->y + 36;
	x3 = pbox->x + 1;
	y3 = pbox->y + 12;
	triangle_rect_region(x1, y1, x2, y2, x3, y3,
			     &x, &y, &w, &h);

	p = pan_item_new_tri(pw, NULL, x, y, w, h,
			     x1, y1, x2, y2, x3, y3,
			     PAN_POPUP_COLOR, PAN_POPUP_ALPHA);
	pan_item_tri_border(p, BORDER_1 | BORDER_3, PAN_POPUP_BORDER_COLOR, PAN_POPUP_ALPHA);
	pan_item_set_key(p, "info");
	pan_item_added(pw, p);

	plabel = pan_item_new_text(pw, pbox->x, pbox->y,
				   _("Filename:"), TEXT_ATTR_BOLD,
				   PAN_POPUP_TEXT_COLOR, 255);
	pan_item_set_key(plabel, "info");
	p = pan_item_new_text(pw, plabel->x + plabel->width, plabel->y,
			      pi->fd->name, TEXT_ATTR_NONE,
			      PAN_POPUP_TEXT_COLOR, 255);
	pan_item_set_key(p, "info");
	pan_item_size_by_item(pbox, p, 0);

	plabel = pan_item_new_text(pw, plabel->x, plabel->y + plabel->height,
				   _("Date:"), TEXT_ATTR_BOLD,
				   PAN_POPUP_TEXT_COLOR, 255);
	pan_item_set_key(plabel, "info");
	p = pan_item_new_text(pw, plabel->x + plabel->width, plabel->y,
			      text_from_time(pi->fd->date), TEXT_ATTR_NONE,
			      PAN_POPUP_TEXT_COLOR, 255);
	pan_item_set_key(p, "info");
	pan_item_size_by_item(pbox, p, 0);

	plabel = pan_item_new_text(pw, plabel->x, plabel->y + plabel->height,
				   _("Size:"), TEXT_ATTR_BOLD,
				   PAN_POPUP_TEXT_COLOR, 255);
	pan_item_set_key(plabel, "info");
	buf = text_from_size(pi->fd->size);
	p = pan_item_new_text(pw, plabel->x + plabel->width, plabel->y,
			      buf, TEXT_ATTR_NONE,
			      PAN_POPUP_TEXT_COLOR, 255);
	g_free(buf);
	pan_item_set_key(p, "info");
	pan_item_size_by_item(pbox, p, 0);

	pan_item_box_shadow(pbox, PAN_SHADOW_OFFSET * 2, PAN_SHADOW_FADE * 2);
	pan_item_added(pw, pbox);
}


/*
 *-----------------------------------------------------------------------------
 * search
 *-----------------------------------------------------------------------------
 */

static void pan_search_status(PanWindow *pw, const gchar *text)
{
	gtk_label_set_text(GTK_LABEL(pw->search_label), (text) ? text : "");
}

static gint pan_search_by_path(PanWindow *pw, const gchar *path)
{
	PanItem *pi;
	GList *list;
	GList *found;
	ItemType type;
	gchar *buf;

	type = (pw->size > LAYOUT_SIZE_THUMB_LARGE) ? ITEM_IMAGE : ITEM_THUMB;

	list = pan_item_find_by_path(pw, type, path, FALSE, FALSE);
	if (!list) return FALSE;

	found = g_list_find(list, pw->click_pi);
	if (found && found->next)
		{
		found = found->next;
		pi = found->data;
		}
	else
		{
		pi = list->data;
		}

	pan_info_update(pw, pi);
	image_scroll_to_point(pw->imd, pi->x + pi->width / 2, pi->y + pi->height / 2, 0.5, 0.5);

	buf = g_strdup_printf("%s ( %d / %d )",
			      (path[0] == '/') ? _("path found") : _("filename found"),
			      g_list_index(list, pi) + 1,
			      g_list_length(list));
	pan_search_status(pw, buf);
	g_free(buf);

	g_list_free(list);

	return TRUE;
}

static gint pan_search_by_partial(PanWindow *pw, const gchar *text)
{
	PanItem *pi;
	GList *list;
	GList *found;
	ItemType type;
	gchar *buf;

	type = (pw->size > LAYOUT_SIZE_THUMB_LARGE) ? ITEM_IMAGE : ITEM_THUMB;

	list = pan_item_find_by_path(pw, type, text, TRUE, FALSE);
	if (!list) list = pan_item_find_by_path(pw, type, text, FALSE, TRUE);
	if (!list)
		{
		gchar *needle;

		needle = g_utf8_strdown(text, -1);
		list = pan_item_find_by_path(pw, type, needle, TRUE, TRUE);
		g_free(needle);
		}
	if (!list) return FALSE;

	found = g_list_find(list, pw->click_pi);
	if (found && found->next)
		{
		found = found->next;
		pi = found->data;
		}
	else
		{
		pi = list->data;
		}

	pan_info_update(pw, pi);
	image_scroll_to_point(pw->imd, pi->x + pi->width / 2, pi->y + pi->height / 2, 0.5, 0.5);

	buf = g_strdup_printf("%s ( %d / %d )",
			      _("partial match"),
			      g_list_index(list, pi) + 1,
			      g_list_length(list));
	pan_search_status(pw, buf);
	g_free(buf);

	g_list_free(list);

	return TRUE;
}

static gint valid_date_separator(gchar c)
{
	return (c == '/' || c == '-' || c == ' ' || c == '.' || c == ',');
}

static GList *pan_search_by_date_val(PanWindow *pw, ItemType type, gint year, gint month, gint day)
{
	GList *list = NULL;
	GList *work;

	work = g_list_last(pw->list);
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->prev;

		if (pi->fd && (pi->type == type || type == ITEM_NONE))
			{
			struct tm *tl;

			tl = localtime(&pi->fd->date);
			if (tl)
				{
				gint match;

				match = (tl->tm_year == year - 1900);
				if (match && month >= 0) match = (tl->tm_mon == month - 1);
				if (match && day > 0) match = (tl->tm_mday == day);

				if (match) list = g_list_prepend(list, pi);
				}
			}
		}

	return g_list_reverse(list);
}

static gint pan_search_by_date(PanWindow *pw, const gchar *text)
{
	PanItem *pi = NULL;
	GList *list;
	GList *found;
	gint year;
	gint month = -1;
	gint day = -1;
	gchar *ptr;
	gchar *mptr;
	struct tm *lt;
	time_t t;
	gchar *message;
	gchar *buf;
	gchar *buf_count;
	ItemType type;

	if (!text) return FALSE;

	ptr = (gchar *)text;
	while (*ptr != '\0')
		{
		if (!g_unichar_isdigit(*ptr) && !valid_date_separator(*ptr)) return FALSE;
		ptr++;
		}

	t = time(NULL);
	if (t == -1) return FALSE;
	lt = localtime(&t);
	if (!lt) return FALSE;

	if (valid_date_separator(*text))
		{
		year = -1;
		mptr = (gchar *)text;
		}
	else
		{
		year = (gint)strtol(text, &mptr, 10);
		if (mptr == text) return FALSE;
		}

	if (*mptr != '\0' && valid_date_separator(*mptr))
		{
		gchar *dptr;

		mptr++;
		month = strtol(mptr, &dptr, 10);
		if (dptr == mptr)
			{
			if (valid_date_separator(*dptr))
				{
				month = lt->tm_mon + 1;
				dptr++;
				}
			else
				{
				month = -1;
				}
			}
		if (dptr != mptr && *dptr != '\0' && valid_date_separator(*dptr))
			{
			gchar *eptr;
			dptr++;
			day = strtol(dptr, &eptr, 10);
			if (dptr == eptr)
				{
				day = lt->tm_mday;
				}
			}
		}

	if (year == -1)
		{
		year = lt->tm_year + 1900;
		}
	else if (year < 100)
		{
		if (year > 70)
			year+= 1900;
		else
			year+= 2000;
		}

	if (year < 1970 ||
	    month < -1 || month == 0 || month > 12 ||
	    day < -1 || day == 0 || day > 31) return FALSE;

	t = date_to_time(year, month, day);
	if (t < 0) return FALSE;

	type = (pw->size > LAYOUT_SIZE_THUMB_LARGE) ? ITEM_IMAGE : ITEM_THUMB;

	list = pan_search_by_date_val(pw, type, year, month, day);
	if (list)
		{
		found = g_list_find(list, pw->click_pi);
		if (found && found->next)
			{
			found = found->next;
			pi = found->data;
			}
		else
			{
			pi = list->data;
			}
		}

	if (pi)
		{
		pan_info_update(pw, pi);
		image_scroll_to_point(pw->imd,
				      pi->x - PAN_FOLDER_BOX_BORDER * 5 / 2,
				      pi->y, 0.0, 0.5);
		}

	if (month > 0)
		{
		buf = date_value_string(t, DATE_LENGTH_MONTH);
		if (day > 0)
			{
			gchar *tmp;
			tmp = buf;
			buf = g_strdup_printf("%d %s", day, tmp);
			g_free(tmp);
			}
		}
	else
		{
		buf = date_value_string(t, DATE_LENGTH_YEAR);
		}

	if (pi)
		{
		buf_count = g_strdup_printf("( %d / %d )",
					    g_list_index(list, pi) + 1,
					    g_list_length(list));
		}
	else
		{
		buf_count = g_strdup_printf("(%s)", _("no match"));
		}

	message = g_strdup_printf("%s %s %s", _("Date:"), buf, buf_count);
	g_free(buf);
	g_free(buf_count);
	pan_search_status(pw, message);
	g_free(message);

	g_list_free(list);

	return TRUE;
}

static void pan_search_activate_cb(const gchar *text, gpointer data)
{
	PanWindow *pw = data;

	if (!text) return;

	tab_completion_append_to_history(pw->search_entry, text);

	if (pan_search_by_path(pw, text)) return;

	if ((pw->layout == LAYOUT_TIMELINE ||
	     pw->layout == LAYOUT_CALENDAR) &&
	    pan_search_by_date(pw, text))
		{
		return;
		}

	if (pan_search_by_partial(pw, text)) return;

	pan_search_status(pw, _("no match"));
}

static void pan_search_toggle_cb(GtkWidget *button, gpointer data)
{
	PanWindow *pw = data;
	gint visible;

	visible = GTK_WIDGET_VISIBLE(pw->search_box);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == visible) return;

	if (visible)
		{
		gtk_widget_hide(pw->search_box);
		gtk_arrow_set(GTK_ARROW(pw->search_button_arrow), GTK_ARROW_UP, GTK_SHADOW_NONE);
		}
	else
		{
		gtk_widget_show(pw->search_box);
		gtk_arrow_set(GTK_ARROW(pw->search_button_arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
		gtk_widget_grab_focus(pw->search_entry);
		}
}


/*
 *-----------------------------------------------------------------------------
 * view window main routines
 *-----------------------------------------------------------------------------
 */ 

static void button_cb(ImageWindow *imd, gint button, guint32 time,
		      gdouble x, gdouble y, guint state, gpointer data)
{
	PanWindow *pw = data;
	PanItem *pi = NULL;
	GtkWidget *menu;
	gint rx, ry;

	rx = ry = 0;
	if (pw->imd->scale)
		{
		rx = (double)(pw->imd->x_scroll + x - pw->imd->x_offset) / pw->imd->scale;
		ry = (double)(pw->imd->y_scroll + y - pw->imd->y_offset) / pw->imd->scale;
		}

	pi = pan_item_find_by_coord(pw, (pw->size > LAYOUT_SIZE_THUMB_LARGE) ? ITEM_IMAGE : ITEM_THUMB,
				    rx, ry, NULL);

	switch (button)
		{
		case 1:
			pan_info_update(pw, pi);

			if (!pi && pw->layout == LAYOUT_CALENDAR)
				{
				pi = pan_item_find_by_coord(pw, ITEM_BOX, rx, ry, "day");
				pan_calendar_update(pw, pi);
				}
			break;
		case 2:
			break;
		case 3:
			pan_info_update(pw, pi);
			menu = pan_popup_menu(pw);
			gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, time);
			break;
		default:
			break;
		}
}

static void scroll_cb(ImageWindow *imd, GdkScrollDirection direction, guint32 time,
		      gdouble x, gdouble y, guint state, gpointer data)
{
#if 0
	PanWindow *pw = data;
#endif
	gint w, h;

	w = imd->vis_width;
	h = imd->vis_height;

	if (!(state & GDK_SHIFT_MASK))
		{
		w /= 3;
		h /= 3;
		}

	if (state & GDK_CONTROL_MASK)
		{
		switch (direction)
			{
			case GDK_SCROLL_UP:
				image_zoom_adjust_at_point(imd, ZOOM_INCREMENT, x, y);
				break;
			case GDK_SCROLL_DOWN:
				image_zoom_adjust_at_point(imd, -ZOOM_INCREMENT, x, y);
				break;
			default:
				break;
			}
		}
	else
		{
		switch (direction)
			{
			case GDK_SCROLL_UP:
				image_scroll(imd, 0, -h);
				break;
			case GDK_SCROLL_DOWN:
				image_scroll(imd, 0, h);
				break;
			case GDK_SCROLL_LEFT:
				image_scroll(imd, -w, 0);
				break;
			case GDK_SCROLL_RIGHT:
				image_scroll(imd, w, 0);
				break;
			default:
				break;
			}
		}
}

static void pan_image_set_buttons(PanWindow *pw, ImageWindow *imd)
{
	image_set_button_func(imd, button_cb, pw);
	image_set_scroll_func(imd, scroll_cb, pw);
}

static void pan_fullscreen_stop_func(FullScreenData *fs, gpointer data)
{
	PanWindow *pw = data;

	pw->fs = NULL;
}

static void pan_fullscreen_toggle(PanWindow *pw, gint force_off)
{
	if (force_off && !pw->fs) return;

	if (pw->fs)
		{
		fullscreen_stop(pw->fs);
		pw->imd = pw->imd_normal;
		}
	else
		{
		pw->fs = fullscreen_start(pw->window, pw->imd, pan_fullscreen_stop_func, pw);

		pan_image_set_buttons(pw, pw->fs->imd);
		g_signal_connect(G_OBJECT(pw->fs->window), "key_press_event",
				 G_CALLBACK(pan_window_key_press_cb), pw);

		pw->imd = pw->fs->imd;
		}
}

static void pan_overlay_toggle(PanWindow *pw)
{
	ImageWindow *imd;

	imd = pan_window_active_image(pw);
#if 0
	if (pw->overlay_id == -1)
		{
		pw->overlay_id = image_overlay_info_enable(imd);
		}
	else
		{
		image_overlay_info_disable(imd, pw->overlay_id);
		pw->overlay_id = -1;
		}
#endif
}

static void pan_window_image_update_cb(ImageWindow *imd, gpointer data)
{
	PanWindow *pw = data;
	gchar *text;

	text = image_zoom_get_as_text(imd);
	gtk_label_set_text(GTK_LABEL(pw->label_zoom), text);
	g_free(text);
}

static void pan_window_image_scroll_notify_cb(ImageWindow *imd, gint x, gint y,
					      gint width, gint height, gpointer data)
{
	PanWindow *pw = data;
	GtkAdjustment *adj;

	adj = gtk_range_get_adjustment(GTK_RANGE(pw->scrollbar_h));
	adj->page_size = (gdouble)imd->vis_width / imd->scale;
	adj->page_increment = adj->page_size / 2.0;
	adj->step_increment = 48.0 / imd->scale;
	adj->lower = 0.0;
	adj->upper = MAX((gdouble)width + adj->page_size, 1.0);
	adj->value = (gdouble)x;

	pref_signal_block_data(pw->scrollbar_h, pw);
	gtk_adjustment_changed(adj);
	pref_signal_unblock_data(pw->scrollbar_h, pw);

	adj = gtk_range_get_adjustment(GTK_RANGE(pw->scrollbar_v));
	adj->page_size = (gdouble)imd->vis_height / imd->scale;
	adj->page_increment = adj->page_size / 2.0;
	adj->step_increment = 48.0 / imd->scale;
	adj->lower = 0.0;
	adj->upper = MAX((gdouble)height + adj->page_size, 1.0);
	adj->value = (gdouble)y;

	pref_signal_block_data(pw->scrollbar_v, pw);
	gtk_adjustment_changed(adj);
	pref_signal_unblock_data(pw->scrollbar_v, pw);

//	printf("scrolled to %d,%d @ %d x %d\n", x, y, width, height);
}

static void pan_window_scrollbar_h_value_cb(GtkRange *range, gpointer data)
{
	PanWindow *pw = data;
	gint x;

	if (!pw->imd->scale) return;

	x = (gint)gtk_range_get_value(range);

	image_scroll_to_point(pw->imd, x, (gint)((gdouble)pw->imd->y_scroll / pw->imd->scale), 0.0, 0.0);
}

static void pan_window_scrollbar_v_value_cb(GtkRange *range, gpointer data)
{
	PanWindow *pw = data;
	gint y;

	if (!pw->imd->scale) return;

	y = (gint)gtk_range_get_value(range);

	image_scroll_to_point(pw->imd, (gint)((gdouble)pw->imd->x_scroll / pw->imd->scale), y, 0.0, 0.0);
}

static void pan_window_layout_change_cb(GtkWidget *combo, gpointer data)
{
	PanWindow *pw = data;

	pw->layout = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	pan_window_layout_update_idle(pw);
}

static void pan_window_layout_size_cb(GtkWidget *combo, gpointer data)
{
	PanWindow *pw = data;

	pw->size = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	pan_window_layout_update_idle(pw);
}

static void pan_window_entry_activate_cb(const gchar *new_text, gpointer data)
{
	PanWindow *pw = data;
	gchar *path;

	path = remove_trailing_slash(new_text);
	parse_out_relatives(path);

	if (!isdir(path))
		{
		warning_dialog(_("Folder not found"),
			       _("The entered path is not a folder"),
			       GTK_STOCK_DIALOG_WARNING, pw->path_entry);
		return;
		}

	tab_completion_append_to_history(pw->path_entry, path);

	g_free(pw->path);
	pw->path = g_strdup(path);

	pan_window_layout_update_idle(pw);
}

static void pan_window_entry_change_cb(GtkWidget *combo, gpointer data)
{
	PanWindow *pw = data;
	gchar *text;

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) < 0) return;

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(pw->path_entry)));
	pan_window_entry_activate_cb(text, pw);
	g_free(text);
}

static void pan_window_close(PanWindow *pw)
{
	pan_window_list = g_list_remove(pan_window_list, pw);

	if (pw->idle_id != -1)
		{
		g_source_remove(pw->idle_id);
		}

	pan_fullscreen_toggle(pw, TRUE);
	gtk_widget_destroy(pw->window);

	pan_window_items_free(pw);

	g_free(pw->path);

	g_free(pw);
}

static gint pan_window_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	PanWindow *pw = data;

	pan_window_close(pw);
	return TRUE;
}

static void pan_window_new_real(const gchar *path)
{
	PanWindow *pw;
	GtkWidget *vbox;
	GtkWidget *box;
	GtkWidget *combo;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *table;
	GdkGeometry geometry;

	pw = g_new0(PanWindow, 1);

	pw->path = g_strdup(path);
	pw->layout = LAYOUT_TIMELINE;
	pw->size = LAYOUT_SIZE_THUMB_NORMAL;
	pw->thumb_size = PAN_THUMB_SIZE_NORMAL;
	pw->thumb_gap = PAN_THUMB_GAP_NORMAL;
	pw->list = NULL;

	pw->fs = NULL;
	pw->overlay_id = -1;
	pw->idle_id = -1;

	pw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	geometry.min_width = 8;
	geometry.min_height = 8;
	gtk_window_set_geometry_hints(GTK_WINDOW(pw->window), NULL, &geometry, GDK_HINT_MIN_SIZE);

	gtk_window_set_resizable(GTK_WINDOW(pw->window), TRUE);
	gtk_window_set_title (GTK_WINDOW(pw->window), "Pan View - GQview");
        gtk_window_set_wmclass(GTK_WINDOW(pw->window), "view", "GQview");
        gtk_container_set_border_width(GTK_CONTAINER(pw->window), 0);

	window_set_icon(pw->window, NULL, NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pw->window), vbox);
	gtk_widget_show(vbox);

	box = pref_box_new(vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);

	pref_spacer(box, 0);
	pref_label_new(box, _("Location:"));
	combo = tab_completion_new_with_history(&pw->path_entry, path, "pan_view", -1,
						pan_window_entry_activate_cb, pw);
	g_signal_connect(G_OBJECT(pw->path_entry->parent), "changed",
			 G_CALLBACK(pan_window_entry_change_cb), pw);
	gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	combo = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Timeline"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Calendar"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Folders"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Folders (flower)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Grid"));

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), pw->layout);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(pan_window_layout_change_cb), pw);
	gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);
	gtk_widget_show(combo);

	combo = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Dots"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("No Images"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Small Thumbnails"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Normal Thumbnails"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Large Thumbnails"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("1:10 (10%)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("1:4 (25%)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("1:3 (33%)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("1:2 (50%)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("1:1 (100%)"));

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), pw->size);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(pan_window_layout_size_cb), pw);
	gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);
	gtk_widget_show(combo);

	table = pref_table_new(vbox, 2, 2, FALSE, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 2);

	pw->imd = image_new(TRUE);
	pw->imd_normal = pw->imd;

	if (black_window_background) image_background_set_black(pw->imd, TRUE);
	image_set_update_func(pw->imd, pan_window_image_update_cb, pw);

	image_set_scroll_notify_func(pw->imd, pan_window_image_scroll_notify_cb, pw);

#if 0
	gtk_box_pack_start(GTK_BOX(vbox), pw->imd->widget, TRUE, TRUE, 0);
#endif
	gtk_table_attach(GTK_TABLE(table), pw->imd->widget, 0, 1, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(pw->imd->widget);

	pan_window_dnd_init(pw);

	pan_image_set_buttons(pw, pw->imd);

	pw->scrollbar_h = gtk_hscrollbar_new(NULL);
	g_signal_connect(G_OBJECT(pw->scrollbar_h), "value_changed",
			 G_CALLBACK(pan_window_scrollbar_h_value_cb), pw);
	gtk_table_attach(GTK_TABLE(table), pw->scrollbar_h, 0, 1, 1, 2,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(pw->scrollbar_h);

	pw->scrollbar_v = gtk_vscrollbar_new(NULL);
	g_signal_connect(G_OBJECT(pw->scrollbar_v), "value_changed",
			 G_CALLBACK(pan_window_scrollbar_v_value_cb), pw);
	gtk_table_attach(GTK_TABLE(table), pw->scrollbar_v, 1, 2, 0, 1,
			 0, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(pw->scrollbar_v);

	/* find bar */

	pw->search_box = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), pw->search_box, FALSE, FALSE, 2);

	pref_spacer(pw->search_box, 0);
	pref_label_new(pw->search_box, _("Find:"));

	hbox = gtk_hbox_new(TRUE, PREF_PAD_SPACE);
	gtk_box_pack_start(GTK_BOX(pw->search_box), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	combo = tab_completion_new_with_history(&pw->search_entry, "", "pan_view_search", -1,
						pan_search_activate_cb, pw);
	gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	pw->search_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), pw->search_label, TRUE, TRUE, 0);
	gtk_widget_show(pw->search_label);

	/* status bar */

	box = pref_box_new(vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, 0);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_widget_set_size_request(frame, ZOOM_LABEL_WIDTH, -1);
	gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	pref_spacer(hbox, 0);
	pw->label_message = pref_label_new(hbox, "");

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_widget_set_size_request(frame, ZOOM_LABEL_WIDTH, -1);
	gtk_box_pack_end(GTK_BOX(box), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	pw->label_zoom = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(frame), pw->label_zoom);
	gtk_widget_show(pw->label_zoom);

	pw->search_button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(pw->search_button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click(GTK_BUTTON(pw->search_button), FALSE);
	hbox = gtk_hbox_new(FALSE, PREF_PAD_GAP);
	gtk_container_add(GTK_CONTAINER(pw->search_button), hbox);
	gtk_widget_show(hbox);
	pw->search_button_arrow = gtk_arrow_new(GTK_ARROW_UP, GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(hbox), pw->search_button_arrow, FALSE, FALSE, 0);
	gtk_widget_show(pw->search_button_arrow);
	pref_label_new(hbox, _("Find"));

	gtk_box_pack_end(GTK_BOX(box), pw->search_button, FALSE, FALSE, 0);
	gtk_widget_show(pw->search_button);
	g_signal_connect(G_OBJECT(pw->search_button), "clicked",
			 G_CALLBACK(pan_search_toggle_cb), pw);

	g_signal_connect(G_OBJECT(pw->window), "delete_event",
			 G_CALLBACK(pan_window_delete_cb), pw);
	g_signal_connect(G_OBJECT(pw->window), "key_press_event",
			 G_CALLBACK(pan_window_key_press_cb), pw);

	gtk_window_set_default_size(GTK_WINDOW(pw->window), PAN_WINDOW_DEFAULT_WIDTH, PAN_WINDOW_DEFAULT_HEIGHT);

	pan_window_layout_update_idle(pw);

	gtk_widget_grab_focus(pw->imd->widget);
	gtk_widget_show(pw->window);

	pan_window_list = g_list_append(pan_window_list, pw);
}

/*
 *-----------------------------------------------------------------------------
 * peformance warnings
 *-----------------------------------------------------------------------------
 */

static void pan_warning_ok_cb(GenericDialog *gd, gpointer data)
{
	gchar *path = data;

	generic_dialog_close(gd);

	pan_window_new_real(path);
	g_free(path);
}

static void pan_warning_hide_cb(GtkWidget *button, gpointer data)
{
	gint hide_dlg;

	hide_dlg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	pref_list_int_set(PAN_PREF_GROUP, PAN_PREF_HIDE_WARNING, hide_dlg);
}

static gint pan_warning(const gchar *path)
{
	GenericDialog *gd;
	GtkWidget *box;
	GtkWidget *group;
	GtkWidget *button;
	GtkWidget *ct_button;
	gint hide_dlg;

	if (enable_thumb_caching &&
	    thumbnail_spec_standard) return FALSE;

	if (!pref_list_int_get(PAN_PREF_GROUP, PAN_PREF_HIDE_WARNING, &hide_dlg)) hide_dlg = FALSE;
	if (hide_dlg) return FALSE;

	gd = generic_dialog_new(_("Pan View Performance"), "GQview", "pan_view_warning", NULL, FALSE,
				NULL, NULL);
	gd->data = g_strdup(path);
	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL,
				  pan_warning_ok_cb, TRUE);

	box = generic_dialog_add_message(gd, GTK_STOCK_DIALOG_INFO,
					 _("Pan view performance may be poor."),
					 _("To improve performance of thumbnails in the pan view the"
					   " following options can be enabled. Note that both options"
					   " must be enabled to notice a change in performance."));

	group = pref_box_new(box, FALSE, GTK_ORIENTATION_HORIZONTAL, 0);
	pref_spacer(group, PREF_PAD_INDENT);
	group = pref_box_new(group, TRUE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);

	ct_button = pref_checkbox_new_int(group, _("Cache thumbnails"),
			  		  enable_thumb_caching, &enable_thumb_caching);
	button = pref_checkbox_new_int(group, _("Use shared thumbnail cache"),
				       thumbnail_spec_standard, &thumbnail_spec_standard);
	pref_checkbox_link_sensitivity(ct_button, button);

	pref_line(box, 0);

	pref_checkbox_new(box, _("Do not show this dialog again"), hide_dlg,
			  G_CALLBACK(pan_warning_hide_cb), NULL);

	gtk_widget_show(gd->dialog);

	return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 * public
 *-----------------------------------------------------------------------------
 */

void pan_window_new(const gchar *path)
{
	if (pan_warning(path)) return;

	pan_window_new_real(path);
}

/*
 *-----------------------------------------------------------------------------
 * menus
 *-----------------------------------------------------------------------------
 */

static void pan_new_window_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path)
		{
		pan_fullscreen_toggle(pw, TRUE);
		view_window_new(path);
		}
}

static void pan_edit_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw;
	const gchar *path;
	gint n;

	pw = submenu_item_get_data(widget);
	n = GPOINTER_TO_INT(data);
	if (!pw) return;

	path = pan_menu_click_path(pw);
	if (path)
		{
		pan_fullscreen_toggle(pw, TRUE);
		start_editor_from_file(n, path);
		}
}

static void pan_info_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path) info_window_new(path, NULL);
}

static void pan_zoom_in_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	image_zoom_adjust(pan_window_active_image(pw), ZOOM_INCREMENT);
}

static void pan_zoom_out_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	image_zoom_adjust(pan_window_active_image(pw), -ZOOM_INCREMENT);
}

static void pan_zoom_1_1_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	image_zoom_set(pan_window_active_image(pw), 1.0);
}

static void pan_copy_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path) file_util_copy(path, NULL, NULL, pw->imd->widget);
}

static void pan_move_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path) file_util_move(path, NULL, NULL, pw->imd->widget);
}

static void pan_rename_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path) file_util_rename(path, NULL, pw->imd->widget);
}

static void pan_delete_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path) file_util_delete(path, NULL, pw->imd->widget);
}

static void pan_fullscreen_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	pan_fullscreen_toggle(pw, FALSE);
}

static void pan_close_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	pan_window_close(pw);
}

static GtkWidget *pan_popup_menu(PanWindow *pw)
{
	GtkWidget *menu;
	GtkWidget *item;
	gint active;

	active = (pw->click_pi != NULL);

	menu = popup_menu_short_lived();

	menu_item_add_stock(menu, _("Zoom _in"), GTK_STOCK_ZOOM_IN,
			    G_CALLBACK(pan_zoom_in_cb), pw);
	menu_item_add_stock(menu, _("Zoom _out"), GTK_STOCK_ZOOM_OUT,
			    G_CALLBACK(pan_zoom_out_cb), pw);
	menu_item_add_stock(menu, _("Zoom _1:1"), GTK_STOCK_ZOOM_100,
			    G_CALLBACK(pan_zoom_1_1_cb), pw);
	menu_item_add_divider(menu);

	submenu_add_edit(menu, &item, G_CALLBACK(pan_edit_cb), pw);
	gtk_widget_set_sensitive(item, active);

	menu_item_add_stock_sensitive(menu, _("_Properties"), GTK_STOCK_PROPERTIES, active,
				      G_CALLBACK(pan_info_cb), pw);

	menu_item_add_stock_sensitive(menu, _("View in _new window"), GTK_STOCK_NEW, active,
				      G_CALLBACK(pan_new_window_cb), pw);

	menu_item_add_divider(menu);
	menu_item_add_stock_sensitive(menu, _("_Copy..."), GTK_STOCK_COPY, active,
				      G_CALLBACK(pan_copy_cb), pw);
	menu_item_add_sensitive(menu, _("_Move..."), active,
				G_CALLBACK(pan_move_cb), pw);
	menu_item_add_sensitive(menu, _("_Rename..."), active,
				G_CALLBACK(pan_rename_cb), pw);
	menu_item_add_stock_sensitive(menu, _("_Delete..."), GTK_STOCK_DELETE, active,
				      G_CALLBACK(pan_delete_cb), pw);

	menu_item_add_divider(menu);

	if (pw->fs)
		{
		menu_item_add(menu, _("Exit _full screen"), G_CALLBACK(pan_fullscreen_cb), pw);
		}
	else
		{
		menu_item_add(menu, _("_Full screen"), G_CALLBACK(pan_fullscreen_cb), pw);
		}

	menu_item_add_divider(menu);
	menu_item_add_stock(menu, _("C_lose window"), GTK_STOCK_CLOSE, G_CALLBACK(pan_close_cb), pw);

	return menu;
}

/*
 *-----------------------------------------------------------------------------
 * drag and drop
 *-----------------------------------------------------------------------------
 */

static void pan_window_get_dnd_data(GtkWidget *widget, GdkDragContext *context,
				    gint x, gint y,
				    GtkSelectionData *selection_data, guint info,
				    guint time, gpointer data)
{
	PanWindow *pw = data;
	ImageWindow *imd;

	if (gtk_drag_get_source_widget(context) == pw->imd->image) return;

	imd = pw->imd;

	if (info == TARGET_URI_LIST)
		{
		GList *list;

		list = uri_list_from_text(selection_data->data, TRUE);
		if (list && isdir((gchar *)list->data))
			{
			gchar *path = list->data;

			g_free(pw->path);
			pw->path = g_strdup(path);

			pan_window_layout_update_idle(pw);
			}

		path_list_free(list);
		}
}

static void pan_window_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
				    GtkSelectionData *selection_data, guint info,
				    guint time, gpointer data)
{
	PanWindow *pw = data;
	const gchar *path;

	path = pan_menu_click_path(pw);
	if (path)
		{
		gchar *text = NULL;
		gint len;
		gint plain_text;
		GList *list;

		switch (info)
			{
			case TARGET_URI_LIST:
				plain_text = FALSE;
				break;
			case TARGET_TEXT_PLAIN:
			default:
				plain_text = TRUE;
				break;
			}
		list = g_list_append(NULL, (gchar *)path);
		text = uri_text_from_list(list, &len, plain_text);
		g_list_free(list);
		if (text)
			{
			gtk_selection_data_set (selection_data, selection_data->target,
						8, text, len);
			g_free(text);
			}
		}
	else
		{
		gtk_selection_data_set (selection_data, selection_data->target,
					8, NULL, 0);
		}
}

static void pan_window_dnd_init(PanWindow *pw)
{
	ImageWindow *imd;

	imd = pw->imd;

	gtk_drag_source_set(imd->image, GDK_BUTTON2_MASK,
			    dnd_file_drag_types, dnd_file_drag_types_count,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(imd->image), "drag_data_get",
			 G_CALLBACK(pan_window_set_dnd_data), pw);

	gtk_drag_dest_set(imd->image,
			  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			  dnd_file_drop_types, dnd_file_drop_types_count,
                          GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(imd->image), "drag_data_received",
			 G_CALLBACK(pan_window_get_dnd_data), pw);
}

/*
 *-----------------------------------------------------------------------------
 * maintenance (for rename, move, remove)
 *-----------------------------------------------------------------------------
 */

