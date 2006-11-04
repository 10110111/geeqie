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
#include "pan-view.h"

#include "bar_exif.h"
#include "cache.h"
#include "cache-loader.h"
#include "dnd.h"
#include "editors.h"
#include "exif.h"
#include "filelist.h"
#include "fullscreen.h"
#include "image.h"
#include "image-load.h"
#include "img-view.h"
#include "info.h"
#include "menu.h"
#include "pixbuf-renderer.h"
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

#define PAN_BACKGROUND_COLOR 150, 150, 150

#define PAN_GRID_SIZE 60
#define PAN_GRID_COLOR 0, 0, 0
#define PAN_GRID_ALPHA 20

#define PAN_FOLDER_BOX_COLOR 255, 255, 255
#define PAN_FOLDER_BOX_ALPHA 100
#define PAN_FOLDER_BOX_BORDER 20

#define PAN_FOLDER_BOX_OUTLINE_THICKNESS 4
#define PAN_FOLDER_BOX_OUTLINE_COLOR 0, 0, 0
#define PAN_FOLDER_BOX_OUTLINE_ALPHA 128

#define PAN_TEXT_BORDER_SIZE 4
#define PAN_TEXT_COLOR 0, 0, 0

#define PAN_POPUP_COLOR 255, 255, 225
#define PAN_POPUP_ALPHA 255
#define PAN_POPUP_BORDER 1
#define PAN_POPUP_BORDER_COLOR 0, 0, 0
#define PAN_POPUP_TEXT_COLOR 0, 0, 0

#define PAN_CAL_POPUP_COLOR 220, 220, 220
#define PAN_CAL_POPUP_ALPHA 255
#define PAN_CAL_POPUP_BORDER 1
#define PAN_CAL_POPUP_BORDER_COLOR 0, 0, 0
#define PAN_CAL_POPUP_TEXT_COLOR 0, 0, 0

#define PAN_CAL_DAY_WIDTH 100
#define PAN_CAL_DAY_HEIGHT 80

#define PAN_CAL_DAY_COLOR 255, 255, 255
#define PAN_CAL_DAY_ALPHA 220
#define PAN_CAL_DAY_BORDER 2
#define PAN_CAL_DAY_BORDER_COLOR 0, 0, 0
#define PAN_CAL_DAY_TEXT_COLOR 0, 0, 0

#define PAN_CAL_MONTH_COLOR 255, 255, 255
#define PAN_CAL_MONTH_ALPHA 200
#define PAN_CAL_MONTH_BORDER 4
#define PAN_CAL_MONTH_BORDER_COLOR 0, 0, 0
#define PAN_CAL_MONTH_TEXT_COLOR 0, 0, 0

#define PAN_CAL_DOT_SIZE 3
#define PAN_CAL_DOT_GAP 2
#define PAN_CAL_DOT_COLOR 128, 128, 128
#define PAN_CAL_DOT_ALPHA 128


#define PAN_GROUP_MAX 16

#define ZOOM_INCREMENT 1.0
#define ZOOM_LABEL_WIDTH 64


#define PAN_PREF_GROUP		"pan_view_options"
#define PAN_PREF_HIDE_WARNING	"hide_performance_warning"
#define PAN_PREF_EXIF_DATE	"use_exif_date"
#define PAN_PREF_INFO_IMAGE	"info_includes_image"
#define PAN_PREF_INFO_EXIF	"info_includes_exif"


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

	GtkWidget *date_button;

	GtkWidget *scrollbar_h;
	GtkWidget *scrollbar_v;

	gint overlay_id;

	gchar *path;
	LayoutType layout;
	LayoutSize size;
	gint thumb_size;
	gint thumb_gap;
	gint image_size;
	gint exif_date_enable;

	gint info_includes_image;
	gint info_includes_exif;

	gint ignore_symlinks;

	GList *list;
	GList *list_static;
	GList *list_grid;

	GList *cache_list;
	GList *cache_todo;
	gint cache_count;
	gint cache_total;
	gint cache_tick;
	CacheLoader *cache_cl;

	ImageLoader *il;
	ThumbLoader *tl;
	PanItem *queue_pi;
	GList *queue;

	PanItem *click_pi;
	PanItem *search_pi;

	gint idle_id;
};

typedef struct _PanGrid PanGrid;
struct _PanGrid {
	gint x;
	gint y;
	gint w;
	gint h;
	GList *list;
};

typedef struct _PanCacheData PanCacheData;
struct _PanCacheData {
	FileData fd;
	CacheData *cd;
};


static GList *pan_window_list = NULL;


static GList *pan_window_layout_list(const gchar *path, SortType sort, gint ascend,
				     gint ignore_symlinks);

static GList *pan_layout_intersect(PanWindow *pw, gint x, gint y, gint width, gint height);

static void pan_layout_resize(PanWindow *pw);

static void pan_window_layout_update_idle(PanWindow *pw);

static GtkWidget *pan_popup_menu(PanWindow *pw);
static void pan_fullscreen_toggle(PanWindow *pw, gint force_off);

static void pan_search_toggle_visible(PanWindow *pw, gint enable);
static void pan_search_activate(PanWindow *pw);

static void pan_window_close(PanWindow *pw);

static void pan_window_dnd_init(PanWindow *pw);


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

	cache_loader_free(pw->cache_cl);
	pw->cache_cl = NULL;
}

static void pan_cache_fill(PanWindow *pw, const gchar *path)
{
	GList *list;

	pan_cache_free(pw);

	list = pan_window_layout_list(path, SORT_NAME, TRUE, pw->ignore_symlinks);
	pw->cache_todo = g_list_reverse(list);

	pw->cache_total = g_list_length(pw->cache_todo);
}

static void pan_cache_step_done_cb(CacheLoader *cl, gint error, gpointer data)
{
	PanWindow *pw = data;

	if (pw->cache_list)
		{
		PanCacheData *pc;
		pc = pw->cache_list->data;

		if (!pc->cd)
			{
			pc->cd = cl->cd;
			cl->cd = NULL;
			}
		}

	cache_loader_free(cl);
	pw->cache_cl = NULL;

	pan_window_layout_update_idle(pw);
}

static gint pan_cache_step(PanWindow *pw)
{
	FileData *fd;
	PanCacheData *pc;
	CacheDataType load_mask;

	if (!pw->cache_todo) return TRUE;

	fd = pw->cache_todo->data;
	pw->cache_todo = g_list_remove(pw->cache_todo, fd);

#if 0
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
#endif
	pc = g_new0(PanCacheData, 1);
	memcpy(pc, fd, sizeof(FileData));
	g_free(fd);

	pc->cd = NULL;

	pw->cache_list = g_list_prepend(pw->cache_list, pc);

	cache_loader_free(pw->cache_cl);

	load_mask = CACHE_LOADER_NONE;
	if (pw->size > LAYOUT_SIZE_THUMB_LARGE) load_mask |= CACHE_LOADER_DIMENSIONS;
	if (pw->exif_date_enable) load_mask |= CACHE_LOADER_DATE;
	pw->cache_cl = cache_loader_new(((FileData *)pc)->path, load_mask,
					pan_cache_step_done_cb, pw);
	return (pw->cache_cl == NULL);
}

/* This sync date function is optimized for lists with a common sort */
static void pan_cache_sync_date(PanWindow *pw, GList *list)
{
	GList *haystack;
	GList *work;

	haystack = g_list_copy(pw->cache_list);

	work = list;
	while (work)
		{
		FileData *fd;
		GList *needle;

		fd = work->data;
		work = work->next;

		needle = haystack;
		while (needle)
			{
			PanCacheData *pc;
			gchar *path;

			pc = needle->data;
			path = ((FileData *)pc)->path;
			if (path && strcmp(path, fd->path) == 0)
				{
				if (pc->cd && pc->cd->have_date && pc->cd->date >= 0)
					{
					fd->date = pc->cd->date;
					}

				haystack = g_list_delete_link(haystack, needle);
				needle = NULL;
				}
			else
				{
				needle = needle->next;
				}
			}
		}

	g_list_free(haystack);
}

/*
 *-----------------------------------------------------------------------------
 * item grid
 *-----------------------------------------------------------------------------
 */

static void pan_grid_clear(PanWindow *pw)
{
	GList *work;

	work = pw->list_grid;
	while (work)
		{
		PanGrid *pg;

		pg = work->data;
		work = work->next;

		g_list_free(pg->list);
		g_free(pg);
		}

	g_list_free(pw->list_grid);
	pw->list_grid = NULL;

	pw->list = g_list_concat(pw->list, pw->list_static);
	pw->list_static = NULL;
}

static void pan_grid_build(PanWindow *pw, gint width, gint height, gint grid_size)
{
	GList *work;
	gint col, row;
	gint cw, ch;
	gint l;
	gdouble total;
	gdouble s;
	gdouble aw, ah;
	gint i, j;

	pan_grid_clear(pw);

	l = g_list_length(pw->list);

	if (l < 1) return;

	total = (gdouble)width * (gdouble)height / (gdouble)l;
	s = sqrt(total);

	aw = (gdouble)width / s;
	ah = (gdouble)height / s;

	col = (gint)(sqrt((gdouble)l / grid_size) * width / height + 0.999);
	col = CLAMP(col, 1, l / grid_size + 1);
	row = (gint)((gdouble)l / grid_size / col);
	if (row < 1) row = 1;

	/* limit minimum size of grid so that a tile will always fit regardless of position */
	cw = MAX((gint)ceil((gdouble)width / col), PAN_TILE_SIZE * 2);
	ch = MAX((gint)ceil((gdouble)height / row), PAN_TILE_SIZE * 2);

	row = row * 2 - 1;
	col = col * 2 - 1;

	printf("intersect speedup grid is %dx%d, based on %d average per grid\n", col, row, grid_size);

	for (j = 0; j < row; j++)
	    for (i = 0; i < col; i++)
		{
		if ((i + 1) * cw / 2 < width && (j + 1) * ch / 2 < height)
			{
			PanGrid *pg;

			pg = g_new0(PanGrid, 1);
			pg->x = i * cw / 2;
			pg->y = j * ch / 2;
			pg->w = cw;
			pg->h = ch;
			pg->list = NULL;

			pw->list_grid = g_list_prepend(pw->list_grid, pg);

			if (debug) printf("grid section: %d,%d (%dx%d)\n", pg->x, pg->y, pg->w, pg->h);
			}
		}

	work = pw->list;
	while (work)
		{
		PanItem *pi;
		GList *grid;

		pi = work->data;
		work = work->next;

		grid = pw->list_grid;
		while (grid)
			{
			PanGrid *pg;
			gint rx, ry, rw, rh;

			pg = grid->data;
			grid = grid->next;

			if (util_clip_region(pi->x, pi->y, pi->width, pi->height,
					     pg->x, pg->y, pg->w, pg->h,
					     &rx, &ry, &rw, &rh))
				{
				pg->list = g_list_prepend(pg->list, pi);
				}
			}
		}

	work = pw->list_grid;
	while (work)
		{
		PanGrid *pg;

		pg = work->data;
		work = work->next;

		pg->list = g_list_reverse(pg->list);
		}

	pw->list_static = pw->list;
	pw->list = NULL;
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

	pan_grid_clear(pw);

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
	pw->search_pi = NULL;
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

	pi->width += pi->border * 2;
	pi->height += pi->border * 2;
}

static PanItem *pan_item_new_text(PanWindow *pw, gint x, gint y, const gchar *text, TextAttrType attr,
				  gint border,
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

	pi->border = border;

	pan_item_text_compute_size(pi, pw->imd->pr);

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
	if (pw->search_pi == pi) pw->search_pi = NULL;
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

	pi->color_a = 255;

	pi->color2_r = 0;
	pi->color2_g = 0;
	pi->color2_b = 0;
	pi->color2_a = PAN_SHADOW_ALPHA / 2;

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
	work = g_list_last(pw->list_static);
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
static GList *pan_item_find_by_path_l(GList *list, GList *search_list,
				      ItemType type, const gchar *path,
				      gint ignore_case, gint partial)
{
	GList *work;

	work = g_list_last(search_list);
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

	return list;
}

/* when ignore_case and partial are TRUE, path should be converted to lower case */
static GList *pan_item_find_by_path(PanWindow *pw, ItemType type, const gchar *path,
				    gint ignore_case, gint partial)
{
	GList *list = NULL;

	if (!path) return NULL;
	if (partial && path[0] == '/') return NULL;

	list = pan_item_find_by_path_l(list, pw->list_static, type, path, ignore_case, partial);
	list = pan_item_find_by_path_l(list, pw->list, type, path, ignore_case, partial);

	return g_list_reverse(list);
}

static PanItem *pan_item_find_by_coord_l(GList *list, ItemType type, gint x, gint y, const gchar *key)
{
	GList *work;

	work = list;
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

static PanItem *pan_item_find_by_coord(PanWindow *pw, ItemType type, gint x, gint y, const gchar *key)
{
	PanItem *pi;

	pi = pan_item_find_by_coord_l(pw->list, type, x, y, key);
	if (pi) return pi;

	return pan_item_find_by_coord_l(pw->list_static, type, x, y, key);
}

/*
 *-----------------------------------------------------------------------------
 * layout generation
 *-----------------------------------------------------------------------------
 */

static gint islink_loop(const gchar *s)
{
	gchar *sl;
	struct stat st;
	gint ret = FALSE;

	sl = path_from_utf8(s);

	if (lstat(sl, &st) == 0 && S_ISLNK(st.st_mode))
		{
		gchar *buf;
		gint l;

		buf = g_malloc(st.st_size + 1);
		l = readlink(sl, buf, st.st_size);
		if (l == st.st_size)
			{
			buf[l] = '\0';

			parse_out_relatives(buf);
			l = strlen(buf);

			parse_out_relatives(sl);

			if (buf[0] == '/')
				{
				if (strncmp(sl, buf, l) == 0 &&
				    (sl[l] == '\0' || sl[l] == '/' || l == 1)) ret = TRUE;
				}
			else
				{
				gchar *link_path;

				link_path = concat_dir_and_file(sl, buf);
				parse_out_relatives(link_path);

				if (strncmp(sl, link_path, l) == 0 &&
				    (sl[l] == '\0' || sl[l] == '/' || l == 1)) ret = TRUE;

				g_free(link_path);
				}
			}

		g_free(buf);
		}

	g_free(sl);

	return ret;
}

static gint is_ignored(const gchar *s, gint ignore_symlinks)
{
	struct stat st;
	const gchar *n;

	if (!lstat_utf8(s, &st)) return TRUE;

#if 0
	/* normal filesystems have directories with some size or block allocation,
	 * special filesystems (like linux /proc) set both to zero.
	 * enable this check if you enable listing the root "/" folder
	 */
	if (st.st_size == 0 && st.st_blocks == 0) return TRUE;
#endif

	if (S_ISLNK(st.st_mode) && (ignore_symlinks || islink_loop(s))) return TRUE;

	n = filename_from_path(s);
	if (n && strcmp(n, GQVIEW_RC_DIR) == 0) return TRUE;

	return FALSE;
}

static GList *pan_window_layout_list(const gchar *path, SortType sort, gint ascend,
				     gint ignore_symlinks)
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

		if (!is_ignored(fd->path, ignore_symlinks) &&
		    filelist_read(fd->path, &flist, &dlist))
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

	list = pan_window_layout_list(path, SORT_NAME, TRUE, pw->ignore_symlinks);

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
				   PAN_TEXT_BORDER_SIZE,
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

		if (!is_ignored(fd->path, pw->ignore_symlinks))
			{
			child = pan_window_layout_compute_folders_flower_path(pw, fd->path, 0, 0);
			if (child) group->children = g_list_prepend(group->children, child);
			}
		}

	if (!f && !group->children)
		{
		work = group->items;
		while (work)
			{
			PanItem *pi;

			pi = work->data;
			work = work->next;

			pan_item_free(pi);
			}

		g_list_free(group->items);
		g_free(group);
		group = NULL;
		}

	g_list_free(f);
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
				   PAN_TEXT_BORDER_SIZE,
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

		if (!is_ignored(fd->path, pw->ignore_symlinks))
			{
			*level = *level + 1;
			pan_window_layout_compute_folders_linear_path(pw, fd->path, x, y, level,
								      pi_box, width, height);
			*level = *level - 1;
			}
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

#if 0
	if (!list) return;
#endif

	grid = (gint)(sqrt(g_list_length(list)) + 0.5);

	x = pi_day->x + pi_day->width + 4;
	y = pi_day->y;

#if 0
	if (y + grid * (PAN_THUMB_SIZE + PAN_THUMB_GAP) + PAN_FOLDER_BOX_BORDER * 4 > pw->pr->image_height)
		{
		y = pw->pr->image_height - (grid * (PAN_THUMB_SIZE + PAN_THUMB_GAP) + PAN_FOLDER_BOX_BORDER * 4);
		}
#endif

	pbox = pan_item_new_box(pw, NULL, x, y, PAN_FOLDER_BOX_BORDER, PAN_FOLDER_BOX_BORDER,
				PAN_CAL_POPUP_BORDER,
				PAN_CAL_POPUP_COLOR, PAN_CAL_POPUP_ALPHA,
				PAN_CAL_POPUP_BORDER_COLOR, PAN_CAL_POPUP_ALPHA);
	pan_item_set_key(pbox, "day_bubble");

	if (pi_day->fd)
		{
		PanItem *plabel;
		gchar *buf;

		buf = date_value_string(pi_day->fd->date, DATE_LENGTH_WEEK);
		plabel = pan_item_new_text(pw, x, y, buf, TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
					   PAN_TEXT_BORDER_SIZE,
					   PAN_CAL_POPUP_TEXT_COLOR, 255);
		pan_item_set_key(plabel, "day_bubble");
		g_free(buf);

		pan_item_size_by_item(pbox, plabel, 0);

		y += plabel->height;
		}

	if (list)
		{
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
					x += PAN_THUMB_SIZE + PAN_THUMB_GAP;
					}
				else
					{
					column = 0;
					x = pbox->x + PAN_FOLDER_BOX_BORDER;
					y += PAN_THUMB_SIZE + PAN_THUMB_GAP;
					}
				}
			}
		}

	x1 = pi_day->x + pi_day->width - 8;
	y1 = pi_day->y + 8;
	x2 = pbox->x + 1;
	y2 = pbox->y + MIN(42, pbox->height);
	x3 = pbox->x + 1;
	y3 = MAX(pbox->y, y2 - 30);
	util_clip_triangle(x1, y1, x2, y2, x3, y3,
			   &x, &y, &w, &h);

	pi = pan_item_new_tri(pw, NULL, x, y, w, h,
			      x1, y1, x2, y2, x3, y3,
			      PAN_CAL_POPUP_COLOR, PAN_CAL_POPUP_ALPHA);
	pan_item_tri_border(pi, BORDER_1 | BORDER_3, PAN_CAL_POPUP_BORDER_COLOR, PAN_CAL_POPUP_ALPHA);
	pan_item_set_key(pi, "day_bubble");
	pan_item_added(pw, pi);

	pan_item_box_shadow(pbox, PAN_SHADOW_OFFSET * 2, PAN_SHADOW_FADE * 2);
	pan_item_added(pw, pbox);

	pan_layout_resize(pw);
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

	list = pan_window_layout_list(path, SORT_NONE, TRUE, pw->ignore_symlinks);

	if (pw->cache_list && pw->exif_date_enable)
		{
		pw->cache_list = filelist_sort(pw->cache_list, SORT_NAME, TRUE);
		list = filelist_sort(list, SORT_NAME, TRUE);
		pan_cache_sync_date(pw, list);
		}

	pw->cache_list = filelist_sort(pw->cache_list, SORT_TIME, TRUE);
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
					    PAN_CAL_MONTH_BORDER,
					    PAN_CAL_MONTH_COLOR, PAN_CAL_MONTH_ALPHA,
					    PAN_CAL_MONTH_BORDER_COLOR, PAN_CAL_MONTH_ALPHA);
		buf = date_value_string(dt, DATE_LENGTH_MONTH);
		pi_text = pan_item_new_text(pw, x, y, buf,
					    TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
					    PAN_TEXT_BORDER_SIZE,
					    PAN_CAL_MONTH_TEXT_COLOR, 255);
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

			fd = g_new0(FileData, 1);
			/* path and name must be non NULL, so make them an invalid filename */
			fd->path = g_strdup("//");
			fd->name = path;
			fd->date = dt;
			pi_day = pan_item_new_box(pw, fd, x, y, PAN_CAL_DAY_WIDTH, PAN_CAL_DAY_HEIGHT,
						  PAN_CAL_DAY_BORDER,
						  PAN_CAL_DAY_COLOR, PAN_CAL_DAY_ALPHA,
						  PAN_CAL_DAY_BORDER_COLOR, PAN_CAL_DAY_ALPHA);
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

				n++;

				work = work->next;
				fd = (work) ? work->data : NULL;
				}

			if (n > 0)
				{
				PanItem *pi;

				pi_day->color_r = MAX(pi_day->color_r - 61 - n * 3, 80);
				pi_day->color_g = pi_day->color_r;

				buf = g_strdup_printf("( %d )", n);
				pi = pan_item_new_text(pw, x, y, buf, TEXT_ATTR_NONE,
						       PAN_TEXT_BORDER_SIZE,
						       PAN_CAL_DAY_TEXT_COLOR, 255);
				g_free(buf);

				pi->x = pi_day->x + (pi_day->width - pi->width) / 2;
				pi->y = pi_day->y + (pi_day->height - pi->height) / 2;
				}

			buf = g_strdup_printf("%d", day);
			pan_item_new_text(pw, x + 4, y + 4, buf, TEXT_ATTR_BOLD | TEXT_ATTR_HEADING,
					  PAN_TEXT_BORDER_SIZE,
					  PAN_CAL_DAY_TEXT_COLOR, 255);
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

	list = pan_window_layout_list(path, SORT_NONE, TRUE, pw->ignore_symlinks);

	if (pw->cache_list && pw->exif_date_enable)
		{
		pw->cache_list = filelist_sort(pw->cache_list, SORT_NAME, TRUE);
		list = filelist_sort(list, SORT_NAME, TRUE);
		pan_cache_sync_date(pw, list);
		}

	pw->cache_list = filelist_sort(pw->cache_list, SORT_TIME, TRUE);
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
						       PAN_TEXT_BORDER_SIZE,
						       PAN_TEXT_COLOR, 255);
				g_free(buf);
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
					       PAN_TEXT_BORDER_SIZE,
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

static GList *pan_layout_intersect_l(GList *list, GList *item_list,
				     gint x, gint y, gint width, gint height)
{
	GList *work;

	work = item_list;
	while (work)
		{
		PanItem *pi;
		gint rx, ry, rw, rh;

		pi = work->data;
		work = work->next;

		if (util_clip_region(x, y, width, height,
				     pi->x, pi->y, pi->width, pi->height,
				     &rx, &ry, &rw, &rh))
			{
			list = g_list_prepend(list, pi);
			}
		}

	return list;
}

static GList *pan_layout_intersect(PanWindow *pw, gint x, gint y, gint width, gint height)
{
	GList *list = NULL;
	GList *grid;
	PanGrid *pg = NULL;

	grid = pw->list_grid;
	while (grid && !pg)
		{
		pg = grid->data;
		grid = grid->next;

		if (x < pg->x || x + width > pg->x + pg->w ||
		    y < pg->y || y + height > pg->y + pg->h)
			{
			pg = NULL;
			}
		}

	list = pan_layout_intersect_l(list, pw->list, x, y, width, height);

	if (pg)
		{
		list = pan_layout_intersect_l(list, pg->list, x, y, width, height);
		}
	else
		{
		list = pan_layout_intersect_l(list, pw->list_static, x, y, width, height);
		}

	return list;
}

static void pan_layout_resize(PanWindow *pw)
{
	gint width = 0;
	gint height = 0;
	GList *work;
	PixbufRenderer *pr;

	work = pw->list;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		if (width < pi->x + pi->width) width = pi->x + pi->width;
		if (height < pi->y + pi->height) height = pi->y + pi->height;
		}
	work = pw->list_static;
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->next;

		if (width < pi->x + pi->width) width = pi->x + pi->width;
		if (height < pi->y + pi->height) height = pi->y + pi->height;
		}

	width += PAN_FOLDER_BOX_BORDER * 2;
	height += PAN_FOLDER_BOX_BORDER * 2;

	pr = PIXBUF_RENDERER(pw->imd->pr);
	if (width < pr->window_width) width = pr->window_width;
	if (height < pr->window_width) height = pr->window_height;

	pixbuf_renderer_set_tiles_size(PIXBUF_RENDERER(pw->imd->pr), width, height);
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
	if (pw->size <= LAYOUT_SIZE_THUMB_NONE &&
	    (!pi->key || strcmp(pi->key, "info") != 0) )
		{
		return;
		}

	pi->queued = TRUE;
	pw->queue = g_list_prepend(pw->queue, pi);

	if (!pw->tl && !pw->il) while(pan_layout_queue_step(pw));
}

static gint pan_window_request_tile_cb(PixbufRenderer *pr, gint x, gint y,
				       gint width, gint height, GdkPixbuf *pixbuf, gpointer data)
{
	PanWindow *pw = data;
	GList *list;
	GList *work;
	gint i;

	pixbuf_set_rect_fill(pixbuf,
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
							     pi->color_a);
					}
				else
					{
					pixbuf_draw_rect_fill(pixbuf,
							      rx - x, ry - y, rw, rh,
							      pi->color2_r, pi->color2_g, pi->color2_b, pi->color2_a);
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

			layout = pan_item_text_layout(pi, (GtkWidget *)pr);
			pixbuf_draw_layout(pixbuf, layout, (GtkWidget *)pr,
					   pi->x - x + pi->border, pi->y - y + pi->border,
					   pi->color_r, pi->color_g, pi->color_b, pi->color_a);
			g_object_unref(G_OBJECT(layout));
			}
		}
	g_list_free(list);

#if 0
	if (x%512 == 0 && y%512 == 0)
		{
		PangoLayout *layout;
		gchar *buf;

		layout = gtk_widget_create_pango_layout((GtkWidget *)pr, NULL);

		buf = g_strdup_printf("%d,%d\n(#%d)", x, y,
				      (x / pr->source_tile_width) +
				      (y / pr->source_tile_height * (pr->image_width/pr->source_tile_width + 1)));
		pango_layout_set_text(layout, buf, -1);
		g_free(buf);

		pixbuf_draw_layout(pixbuf, layout, (GtkWidget *)pr, 0, 0, 0, 0, 0, 255);

		g_object_unref(G_OBJECT(layout));
		}
#endif

	return TRUE;
}

static void pan_window_dispose_tile_cb(PixbufRenderer *pr, gint x, gint y,
				       gint width, gint height, GdkPixbuf *pixbuf, gpointer data)
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

	work = pw->list_static;
	if (pw->layout == LAYOUT_CALENDAR)
		{
		while (work)
			{
			PanItem *pi;

			pi = work->data;
			work = work->next;

			if (pi->fd &&
			    pi->type == ITEM_BOX &&
			    pi->key && strcmp(pi->key, "dot") == 0)
				{
				size += pi->fd->size;
				count++;
				}
			}
		}
	else
		{
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
		}

	ss = text_from_size_abrev(size);
	buf = g_strdup_printf(_("%d images, %s"), count, ss);
	g_free(ss);
	gtk_label_set_text(GTK_LABEL(pw->label_message), buf);
	g_free(buf);
}

static void pan_warning_folder(const gchar *path, GtkWidget *parent)
{
	gchar *message;

	message = g_strdup_printf(_("The pan view does not support the folder \"%s\"."), path);
	warning_dialog(_("Folder not supported"), message,
		      GTK_STOCK_DIALOG_INFO, parent);
	g_free(message);
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

	if (pw->size > LAYOUT_SIZE_THUMB_LARGE ||
	    (pw->exif_date_enable && (pw->layout == LAYOUT_TIMELINE || pw->layout == LAYOUT_CALENDAR)))
		{
		if (!pw->cache_list && !pw->cache_todo)
			{
			pan_cache_fill(pw, pw->path);
			if (pw->cache_todo)
				{
				pan_window_message(pw, _("Reading image data..."));
				return TRUE;
				}
			}
		if (pw->cache_todo)
			{
			pw->cache_count++;
			pw->cache_tick++;
			if (pw->cache_count == pw->cache_total)
				{
				pan_window_message(pw, _("Sorting..."));
				}
			else if (pw->cache_tick > 9)
				{
				gchar *buf;

				buf = g_strdup_printf("%s %d", _("Reading image data..."),
						      pw->cache_total - pw->cache_count);
				pan_window_message(pw, buf);
				g_free(buf);

				pw->cache_tick = 0;
				}

			if (pan_cache_step(pw)) return TRUE;

			pw->idle_id = -1;
			return FALSE;
			}
		}

	pan_window_layout_compute(pw, pw->path, &width, &height, &scroll_x, &scroll_y);

	pan_window_zoom_limit(pw);

	if (width > 0 && height > 0)
		{
		gdouble align;

		printf("Canvas size is %d x %d\n", width, height);

		pan_grid_build(pw, width, height, 1000);

		pixbuf_renderer_set_tiles(PIXBUF_RENDERER(pw->imd->pr), width, height,
					  PAN_TILE_SIZE, PAN_TILE_SIZE, 10,
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
		pixbuf_renderer_scroll_to_point(PIXBUF_RENDERER(pw->imd->pr), scroll_x, scroll_y, align, align);
		}

	pan_window_message(pw, NULL);

	pw->idle_id = -1;
	return FALSE;
}

static void pan_window_layout_update_idle(PanWindow *pw)
{
	if (pw->idle_id == -1)
		{
		pw->idle_id = g_idle_add(pan_window_layout_update_idle_cb, pw);
		}
}

static void pan_window_layout_update(PanWindow *pw)
{
	pan_window_message(pw, _("Sorting images..."));
	pan_window_layout_update_idle(pw);
}

static void pan_window_layout_set_path(PanWindow *pw, const gchar *path)
{
	if (!path) return;

	if (strcmp(path, "/") == 0)
		{
		pan_warning_folder(path, pw->window);
		return;
		}

	g_free(pw->path);
	pw->path = g_strdup(path);

	pan_window_layout_update(pw);
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

	gdk_window_get_origin(pw->imd->pr->window, x, y);
	popup_menu_position_clamp(menu, x, y, 0);
}

static gint pan_window_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	PanWindow *pw = data;
	PixbufRenderer *pr;
	const gchar *path;
	gint stop_signal = FALSE;
	GtkWidget *menu;
	gint x = 0;
	gint y = 0;
	gint focused;
	gint on_entry;

	pr = PIXBUF_RENDERER(pw->imd->pr);
	path = pan_menu_click_path(pw);

	focused = (pw->fs || GTK_WIDGET_HAS_FOCUS(GTK_WIDGET(pw->imd->widget)));
	on_entry = (GTK_WIDGET_HAS_FOCUS(pw->path_entry) ||
		    GTK_WIDGET_HAS_FOCUS(pw->search_entry));

	if (focused)
		{
		stop_signal = TRUE;
		switch (event->keyval)
			{
			case GDK_Left: case GDK_KP_Left:
				x -= 1;
				break;
			case GDK_Right: case GDK_KP_Right:
				x += 1;
				break;
			case GDK_Up: case GDK_KP_Up:
				y -= 1;
				break;
			case GDK_Down: case GDK_KP_Down:
				y += 1;
				break;
			case GDK_Page_Up: case GDK_KP_Page_Up:
				pixbuf_renderer_scroll(pr, 0, 0 - pr->vis_height / 2);
				break;
			case GDK_Page_Down: case GDK_KP_Page_Down:
				pixbuf_renderer_scroll(pr, 0, pr->vis_height / 2);
				break;
			case GDK_Home: case GDK_KP_Home:
				pixbuf_renderer_scroll(pr, 0 - pr->vis_width / 2, 0);
				break;
			case GDK_End: case GDK_KP_End:
				pixbuf_renderer_scroll(pr, pr->vis_width / 2, 0);
				break;
			default:
				stop_signal = FALSE;
				break;
			}

		if (x != 0 || y!= 0)
			{
			if (event->state & GDK_SHIFT_MASK)
				{
				x *= 3;
				y *= 3;
				}
			keyboard_scroll_calc(&x, &y, event);
			pixbuf_renderer_scroll(pr, x, y);
			}
		}

	if (stop_signal) return stop_signal;

	if (event->state & GDK_CONTROL_MASK)
		{
		gint n = -1;

		stop_signal = TRUE;
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
				if (path) file_util_copy(path, NULL, NULL, GTK_WIDGET(pr));
				break;
			case 'M': case 'm':
				if (path) file_util_move(path, NULL, NULL, GTK_WIDGET(pr));
				break;
			case 'R': case 'r':
				if (path) file_util_rename(path, NULL, GTK_WIDGET(pr));
				break;
			case 'D': case 'd':
				if (path) file_util_delete(path, NULL, GTK_WIDGET(pr));
				break;
			case 'P': case 'p':
				if (path) info_window_new(path, NULL);
				break;
			case 'F': case 'f':
				pan_search_toggle_visible(pw, TRUE);
				break;
			case 'G': case 'g':
				pan_search_activate(pw);
				break;
			case 'W': case 'w':
				pan_window_close(pw);
				break;
			default:
				stop_signal = FALSE;
				break;
			}

		if (n != -1 && path)
			{
			if (!editor_window_flag_set(n))
				{
				pan_fullscreen_toggle(pw, TRUE);
				}
			start_editor_from_file(n, path);
			}
		}
	else
		{
		stop_signal = TRUE;
		switch (event->keyval)
			{
			case GDK_Escape:
				if (pw->fs)
					{
					pan_fullscreen_toggle(pw, TRUE);
					}
				else
					{
					pan_search_toggle_visible(pw, FALSE);
					}
				break;
			default:
				stop_signal = FALSE;
				break;
			}

		if (stop_signal) return stop_signal;

		if (!on_entry)
			{
			stop_signal = TRUE;
			switch (event->keyval)
				{
				case '+': case '=': case GDK_KP_Add:
					pixbuf_renderer_zoom_adjust(pr, ZOOM_INCREMENT);
					break;
				case '-': case GDK_KP_Subtract:
					pixbuf_renderer_zoom_adjust(pr, -ZOOM_INCREMENT);
					break;
				case 'Z': case 'z': case GDK_KP_Divide: case '1':
					pixbuf_renderer_zoom_set(pr, 1.0);
					break;
				case '2':
					pixbuf_renderer_zoom_set(pr, 2.0);
					break;
				case '3':
					pixbuf_renderer_zoom_set(pr, 3.0);
					break;
				case '4':
					pixbuf_renderer_zoom_set(pr, 4.0);
					break;
				case '7':
					pixbuf_renderer_zoom_set(pr, -4.0);
					break;
				case '8':
					pixbuf_renderer_zoom_set(pr, -3.0);
					break;
				case '9':
					pixbuf_renderer_zoom_set(pr, -2.0);
					break;
				case 'F': case 'f':
				case 'V': case 'v':
				case GDK_F11:
					pan_fullscreen_toggle(pw, FALSE);
					break;
				case 'I': case 'i':
#if 0
					pan_overlay_toggle(pw);
#endif
					break;
				case GDK_Delete: case GDK_KP_Delete:
					break;
				case GDK_Menu:
				case GDK_F10:
					menu = pan_popup_menu(pw);
					gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
						       pan_window_menu_pos_cb, pw, 0, GDK_CURRENT_TIME);
					break;
				case '/':
					pan_search_toggle_visible(pw, TRUE);
					break;
				default:
					stop_signal = FALSE;
					break;
				}
			}
		}

	return stop_signal;
}

/*
 *-----------------------------------------------------------------------------
 * info popup
 *-----------------------------------------------------------------------------
 */

typedef struct _PanTextAlignment PanTextAlignment;
struct _PanTextAlignment {
	PanWindow *pw;

	GList *column1;
	GList *column2;

	gint x;
	gint y;
	gchar *key;
};


static PanTextAlignment *pan_text_alignment_new(PanWindow *pw, gint x, gint y, const gchar *key)
{
	PanTextAlignment *ta;

	ta = g_new0(PanTextAlignment, 1);

	ta->pw = pw;
	ta->column1 = NULL;
	ta->column2 = NULL;
	ta->x = x;
	ta->y = y;
	ta->key = g_strdup(key);

	return ta;
}

static PanItem *pan_text_alignment_add(PanTextAlignment *ta,
				       const gchar *label, const gchar *text)
{
	PanItem *item;

	if (label)
		{
		item = pan_item_new_text(ta->pw, ta->x, ta->y, label,
					 TEXT_ATTR_BOLD, 0,
					 PAN_POPUP_TEXT_COLOR, 255);
		pan_item_set_key(item, ta->key);
		}
	else
		{
		item = NULL;
		}
	ta->column1 = g_list_append(ta->column1, item);

	if (text)
		{
		item = pan_item_new_text(ta->pw, ta->x, ta->y, text,
					 TEXT_ATTR_NONE, 0,
					 PAN_POPUP_TEXT_COLOR, 255);
		pan_item_set_key(item, ta->key);
		}
	else
		{
		item = NULL;
		}
	ta->column2 = g_list_append(ta->column2, item);

	return item;
}

static void pan_text_alignment_calc(PanTextAlignment *ta, PanItem *box)
{
	gint cw1, cw2;
	gint x, y;
	GList *work1;
	GList *work2;

	cw1 = 0;
	cw2 = 0;

	work1 = ta->column1;
	while (work1)
		{
		PanItem *p;

		p = work1->data;
		work1 = work1->next;

		if (p && p->width > cw1) cw1 = p->width;
		}

	work2 = ta->column2;
	while (work2)
		{
		PanItem *p;

		p = work2->data;
		work2 = work2->next;

		if (p && p->width > cw2) cw2 = p->width;
		}

	x = ta->x;
	y = ta->y;
	work1 = ta->column1;
	work2 = ta->column2;
	while (work1 && work2)
		{
		PanItem *p1;
		PanItem *p2;
		gint height = 0;

		p1 = work1->data;
		p2 = work2->data;
		work1 = work1->next;
		work2 = work2->next;

		if (p1)
			{
			p1->x = x;
			p1->y = y;
			pan_item_size_by_item(box, p1, PREF_PAD_BORDER);
			height = p1->height;
			}
		if (p2)
			{
			p2->x = x + cw1 + PREF_PAD_SPACE;
			p2->y = y;
			pan_item_size_by_item(box, p2, PREF_PAD_BORDER);
			if (height < p2->height) height = p2->height;
			}

		if (!p1 && !p2) height = PREF_PAD_GROUP;

		y += height;
		}
}

static void pan_text_alignment_free(PanTextAlignment *ta)
{
	if (!ta) return;

	g_list_free(ta->column1);
	g_list_free(ta->column2);
	g_free(ta->key);
	g_free(ta);
}

static void pan_info_add_exif(PanTextAlignment *ta, FileData *fd)
{
	ExifData *exif;
	GList *work;
	gint i;

	if (!fd) return;
	exif = exif_read(fd->path);
	if (!exif) return;

	pan_text_alignment_add(ta, NULL, NULL);

	for (i = 0; i < bar_exif_key_count; i++)
		{
		gchar *label;
		gchar *text;

		label = g_strdup_printf("%s:", exif_get_description_by_key(bar_exif_key_list[i]));
		text = exif_get_data_as_text(exif, bar_exif_key_list[i]);
		text = bar_exif_validate_text(text);
		pan_text_alignment_add(ta, label, text);
		g_free(label);
		g_free(text);
		}

	work = g_list_last(history_list_get_by_key("exif_extras"));
	if (work) pan_text_alignment_add(ta, "---", NULL);
	while (work)
		{
		const gchar *name;
		gchar *label;
		gchar *text;

		name = work->data;
		work = work->prev;

		label = g_strdup_printf("%s:", name);
		text = exif_get_data_as_text(exif, name);
		text = bar_exif_validate_text(text);
		pan_text_alignment_add(ta, label, text);
		g_free(label);
		g_free(text);
		}

	exif_free(exif);
}

static void pan_info_update(PanWindow *pw, PanItem *pi)
{
	PanTextAlignment *ta;
	PanItem *pbox;
	PanItem *p;
	gchar *buf;
	gint x1, y1, x2, y2, x3, y3;
	gint x, y, w, h;

	if (pw->click_pi == pi) return;
	if (pi && !pi->fd) pi = NULL;

	while ((p = pan_item_find_by_key(pw, ITEM_NONE, "info"))) pan_item_remove(pw, p);
	pw->click_pi = pi;

	if (!pi) return;

	if (debug) printf("info set to %s\n", pi->fd->path);

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
	util_clip_triangle(x1, y1, x2, y2, x3, y3,
			   &x, &y, &w, &h);

	p = pan_item_new_tri(pw, NULL, x, y, w, h,
			     x1, y1, x2, y2, x3, y3,
			     PAN_POPUP_COLOR, PAN_POPUP_ALPHA);
	pan_item_tri_border(p, BORDER_1 | BORDER_3, PAN_POPUP_BORDER_COLOR, PAN_POPUP_ALPHA);
	pan_item_set_key(p, "info");
	pan_item_added(pw, p);

	ta = pan_text_alignment_new(pw, pbox->x + PREF_PAD_BORDER, pbox->y + PREF_PAD_BORDER, "info");

	pan_text_alignment_add(ta, _("Filename:"), pi->fd->name);
	buf = remove_level_from_path(pi->fd->path);
	pan_text_alignment_add(ta, _("Location:"), buf);
	g_free(buf);
	pan_text_alignment_add(ta, _("Date:"), text_from_time(pi->fd->date));
	buf = text_from_size(pi->fd->size);
	pan_text_alignment_add(ta, _("Size:"), buf);
	g_free(buf);

	if (pw->info_includes_exif)
		{
		pan_info_add_exif(ta, pi->fd);
		}

	pan_text_alignment_calc(ta, pbox);
	pan_text_alignment_free(ta);

	pan_item_box_shadow(pbox, PAN_SHADOW_OFFSET * 2, PAN_SHADOW_FADE * 2);
	pan_item_added(pw, pbox);

	if (pw->info_includes_image)
		{
		gint iw, ih;
		if (image_load_dimensions(pi->fd->path, &iw, &ih))
			{
			pbox = pan_item_new_box(pw, NULL, pbox->x, pbox->y + pbox->height + 8, 10, 10,
						PAN_POPUP_BORDER,
						PAN_POPUP_COLOR, PAN_POPUP_ALPHA,
						PAN_POPUP_BORDER_COLOR, PAN_POPUP_ALPHA);
			pan_item_set_key(pbox, "info");

			p = pan_item_new_image(pw, file_data_new_simple(pi->fd->path),
					       pbox->x + PREF_PAD_BORDER, pbox->y + PREF_PAD_BORDER, iw, ih);
			pan_item_set_key(p, "info");
			pan_item_size_by_item(pbox, p, PREF_PAD_BORDER);

			pan_item_box_shadow(pbox, PAN_SHADOW_OFFSET * 2, PAN_SHADOW_FADE * 2);
			pan_item_added(pw, pbox);

			pan_layout_resize(pw);
			}
		}
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

static GList *pan_search_by_date_val(PanWindow *pw, ItemType type,
				     gint year, gint month, gint day,
				     const gchar *key)
{
	GList *list = NULL;
	GList *work;

	work = g_list_last(pw->list_static);
	while (work)
		{
		PanItem *pi;

		pi = work->data;
		work = work->prev;

		if (pi->fd && (pi->type == type || type == ITEM_NONE) &&
		    ((!key && !pi->key) || (key && pi->key && strcmp(key, pi->key) == 0)))
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
	GList *list = NULL;
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

	if (pw->layout == LAYOUT_CALENDAR)
		{
		list = pan_search_by_date_val(pw, ITEM_BOX, year, month, day, "day");
		}
	else
		{
		ItemType type;

		type = (pw->size > LAYOUT_SIZE_THUMB_LARGE) ? ITEM_IMAGE : ITEM_THUMB;
		list = pan_search_by_date_val(pw, type, year, month, day, NULL);
		}

	if (list)
		{
		found = g_list_find(list, pw->search_pi);
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

	pw->search_pi = pi;

	if (pw->layout == LAYOUT_CALENDAR && pi && pi->type == ITEM_BOX)
		{
		pan_info_update(pw, NULL);
		pan_calendar_update(pw, pi);
		image_scroll_to_point(pw->imd,
				      pi->x + pi->width / 2,
				      pi->y + pi->height / 2, 0.5, 0.5);
		}
	else if (pi)
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

static void pan_search_activate(PanWindow *pw)
{
	gchar *text;

#if 0
	if (!GTK_WIDGET_VISIBLE(pw->search_box))
		{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw->search_button), TRUE);
		}
#endif

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(pw->search_entry)));
	pan_search_activate_cb(text, pw);
	g_free(text);
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

static void pan_search_toggle_visible(PanWindow *pw, gint enable)
{
	if (pw->fs) return;

	if (enable)
		{
		if (GTK_WIDGET_VISIBLE(pw->search_box))
			{
			gtk_widget_grab_focus(pw->search_entry);
			}
		else
			{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw->search_button), TRUE);
			}
		}
	else
		{
		if (GTK_WIDGET_VISIBLE(pw->search_entry))
			{
			if (GTK_WIDGET_HAS_FOCUS(pw->search_entry))
				{
				gtk_widget_grab_focus(GTK_WIDGET(pw->imd->widget));
				}
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw->search_button), FALSE);
			}
		}
}


/*
 *-----------------------------------------------------------------------------
 * view window main routines
 *-----------------------------------------------------------------------------
 */ 

static void button_cb(PixbufRenderer *pr, GdkEventButton *event, gpointer data)
{
	PanWindow *pw = data;
	PanItem *pi = NULL;
	GtkWidget *menu;
	gint rx, ry;

	rx = ry = 0;
	if (pr->scale)
		{
		rx = (double)(pr->x_scroll + event->x - pr->x_offset) / pr->scale;
		ry = (double)(pr->y_scroll + event->y - pr->y_offset) / pr->scale;
		}

	pi = pan_item_find_by_coord(pw, ITEM_BOX, rx, ry, "info");
	if (pi && event->button == 1)
		{
		pan_info_update(pw, NULL);
		return;
		}

	pi = pan_item_find_by_coord(pw, (pw->size > LAYOUT_SIZE_THUMB_LARGE) ? ITEM_IMAGE : ITEM_THUMB,
				    rx, ry, NULL);

	switch (event->button)
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
			gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);
			break;
		default:
			break;
		}
}

static void scroll_cb(PixbufRenderer *pr, GdkEventScroll *event, gpointer data)
{
#if 0
	PanWindow *pw = data;
#endif
	gint w, h;

	w = pr->vis_width;
	h = pr->vis_height;

	if (!(event->state & GDK_SHIFT_MASK))
		{
		w /= 3;
		h /= 3;
		}

	if (event->state & GDK_CONTROL_MASK)
		{
		switch (event->direction)
			{
			case GDK_SCROLL_UP:
				pixbuf_renderer_zoom_adjust_at_point(pr, ZOOM_INCREMENT,
								     (gint)event->x, (gint)event->y);
				break;
			case GDK_SCROLL_DOWN:
				pixbuf_renderer_zoom_adjust_at_point(pr, -ZOOM_INCREMENT,
								     (gint)event->x, (gint)event->y);
				break;
			default:
				break;
			}
		}
	else
		{
		switch (event->direction)
			{
			case GDK_SCROLL_UP:
				pixbuf_renderer_scroll(pr, 0, -h);
				break;
			case GDK_SCROLL_DOWN:
				pixbuf_renderer_scroll(pr, 0, h);
				break;
			case GDK_SCROLL_LEFT:
				pixbuf_renderer_scroll(pr, -w, 0);
				break;
			case GDK_SCROLL_RIGHT:
				pixbuf_renderer_scroll(pr, w, 0);
				break;
			default:
				break;
			}
		}
}

static void pan_image_set_buttons(PanWindow *pw, ImageWindow *imd)
{
	g_signal_connect(G_OBJECT(imd->pr), "clicked",
			 G_CALLBACK(button_cb), pw);
	g_signal_connect(G_OBJECT(imd->pr), "scroll_event",
			 G_CALLBACK(scroll_cb), pw);
}

static void pan_fullscreen_stop_func(FullScreenData *fs, gpointer data)
{
	PanWindow *pw = data;

	pw->fs = NULL;
	pw->imd = pw->imd_normal;
}

static void pan_fullscreen_toggle(PanWindow *pw, gint force_off)
{
	if (force_off && !pw->fs) return;

	if (pw->fs)
		{
		fullscreen_stop(pw->fs);
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

static void pan_window_image_zoom_cb(PixbufRenderer *pr, gdouble zoom, gpointer data)
{
	PanWindow *pw = data;
	gchar *text;

	text = image_zoom_get_as_text(pw->imd);
	gtk_label_set_text(GTK_LABEL(pw->label_zoom), text);
	g_free(text);
}

static void pan_window_image_scroll_notify_cb(PixbufRenderer *pr, gpointer data)
{
	PanWindow *pw = data;
	GtkAdjustment *adj;
	GdkRectangle rect;
	gint width, height;

	if (pr->scale == 0.0) return;

	pixbuf_renderer_get_visible_rect(pr, &rect);
	pixbuf_renderer_get_image_size(pr, &width, &height);

	adj = gtk_range_get_adjustment(GTK_RANGE(pw->scrollbar_h));
	adj->page_size = (gdouble)rect.width;
	adj->page_increment = adj->page_size / 2.0;
	adj->step_increment = 48.0 / pr->scale;
	adj->lower = 0.0;
	adj->upper = MAX((gdouble)width, 1.0);
	adj->value = (gdouble)rect.x;

	pref_signal_block_data(pw->scrollbar_h, pw);
	gtk_adjustment_changed(adj);
	gtk_adjustment_value_changed(adj);
	pref_signal_unblock_data(pw->scrollbar_h, pw);

	adj = gtk_range_get_adjustment(GTK_RANGE(pw->scrollbar_v));
	adj->page_size = (gdouble)rect.height;
	adj->page_increment = adj->page_size / 2.0;
	adj->step_increment = 48.0 / pr->scale;
	adj->lower = 0.0;
	adj->upper = MAX((gdouble)height, 1.0);
	adj->value = (gdouble)rect.y;

	pref_signal_block_data(pw->scrollbar_v, pw);
	gtk_adjustment_changed(adj);
	gtk_adjustment_value_changed(adj);
	pref_signal_unblock_data(pw->scrollbar_v, pw);
}

static void pan_window_scrollbar_h_value_cb(GtkRange *range, gpointer data)
{
	PanWindow *pw = data;
	PixbufRenderer *pr;
	gint x;

	pr = PIXBUF_RENDERER(pw->imd_normal->pr);

	if (!pr->scale) return;

	x = (gint)gtk_range_get_value(range);

	pixbuf_renderer_scroll_to_point(pr, x, (gint)((gdouble)pr->y_scroll / pr->scale), 0.0, 0.0);
}

static void pan_window_scrollbar_v_value_cb(GtkRange *range, gpointer data)
{
	PanWindow *pw = data;
	PixbufRenderer *pr;
	gint y;

	pr = PIXBUF_RENDERER(pw->imd_normal->pr);

	if (!pr->scale) return;

	y = (gint)gtk_range_get_value(range);

	pixbuf_renderer_scroll_to_point(pr, (gint)((gdouble)pr->x_scroll / pr->scale), y, 0.0, 0.0);
}

static void pan_window_layout_change_cb(GtkWidget *combo, gpointer data)
{
	PanWindow *pw = data;

	pw->layout = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	pan_window_layout_update(pw);
}

static void pan_window_layout_size_cb(GtkWidget *combo, gpointer data)
{
	PanWindow *pw = data;

	pw->size = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	pan_window_layout_update(pw);
}

#if 0
static void pan_window_date_toggle_cb(GtkWidget *button, gpointer data)
{
	PanWindow *pw = data;

	pw->exif_date_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	pan_window_layout_update(pw);
}
#endif

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
		}
	else
		{
		tab_completion_append_to_history(pw->path_entry, path);

		pan_window_layout_set_path(pw, path);
		}

	g_free(path);
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

	pref_list_int_set(PAN_PREF_GROUP, PAN_PREF_EXIF_DATE, pw->exif_date_enable);
	pref_list_int_set(PAN_PREF_GROUP, PAN_PREF_INFO_IMAGE, pw->info_includes_image);
	pref_list_int_set(PAN_PREF_GROUP, PAN_PREF_INFO_EXIF, pw->info_includes_exif);

	if (pw->idle_id != -1)
		{
		g_source_remove(pw->idle_id);
		}

	pan_fullscreen_toggle(pw, TRUE);
	gtk_widget_destroy(pw->window);

	pan_window_items_free(pw);
	pan_cache_free(pw);

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

	if (!pref_list_int_get(PAN_PREF_GROUP, PAN_PREF_EXIF_DATE, &pw->exif_date_enable))
		{
		pw->exif_date_enable = FALSE;
		}
	if (!pref_list_int_get(PAN_PREF_GROUP, PAN_PREF_INFO_IMAGE, &pw->info_includes_image))
		{
		pw->info_includes_image = FALSE;
		}
	if (!pref_list_int_get(PAN_PREF_GROUP, PAN_PREF_INFO_EXIF, &pw->info_includes_exif))
		{
		pw->info_includes_exif = TRUE;
		}

	pw->ignore_symlinks = TRUE;

	pw->list = NULL;
	pw->list_static = NULL;
	pw->list_grid = NULL;

	pw->fs = NULL;
	pw->overlay_id = -1;
	pw->idle_id = -1;

	pw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	geometry.min_width = 8;
	geometry.min_height = 8;
	gtk_window_set_geometry_hints(GTK_WINDOW(pw->window), NULL, &geometry, GDK_HINT_MIN_SIZE);

	gtk_window_set_resizable(GTK_WINDOW(pw->window), TRUE);
	gtk_window_set_title (GTK_WINDOW(pw->window), _("Pan View - GQview"));
        gtk_window_set_wmclass(GTK_WINDOW(pw->window), "view", "GQview");
        gtk_container_set_border_width(GTK_CONTAINER(pw->window), 0);

	window_set_icon(pw->window, NULL, NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pw->window), vbox);
	gtk_widget_show(vbox);

	box = pref_box_new(vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);

	pref_spacer(box, 0);
	pref_label_new(box, _("Location:"));
	combo = tab_completion_new_with_history(&pw->path_entry, path, "pan_view_path", -1,
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

	g_signal_connect(G_OBJECT(pw->imd->pr), "zoom",
			 G_CALLBACK(pan_window_image_zoom_cb), pw);
	g_signal_connect(G_OBJECT(pw->imd->pr), "scroll_notify",
			 G_CALLBACK(pan_window_image_scroll_notify_cb), pw);

	gtk_table_attach(GTK_TABLE(table), pw->imd->widget, 0, 1, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(GTK_WIDGET(pw->imd->widget));

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

#if 0
	pw->date_button = pref_checkbox_new(box, _("Use Exif date"), pw->exif_date_enable,
					    G_CALLBACK(pan_window_date_toggle_cb), pw);
#endif

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

	pan_window_layout_update(pw);

	gtk_widget_grab_focus(GTK_WIDGET(pw->imd->widget));
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

	if (path && strcmp(path, "/") == 0)
		{
		pan_warning_folder(path, NULL);
		return TRUE;
		}

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
		if (!editor_window_flag_set(n))
			{
			pan_fullscreen_toggle(pw, TRUE);
			}
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

	image_zoom_adjust(pw->imd, ZOOM_INCREMENT);
}

static void pan_zoom_out_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	image_zoom_adjust(pw->imd, -ZOOM_INCREMENT);
}

static void pan_zoom_1_1_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	image_zoom_set(pw->imd, 1.0);
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

static void pan_exif_date_toggle_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	pw->exif_date_enable = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	pan_window_layout_update(pw);
}

static void pan_info_toggle_exif_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	pw->info_includes_exif = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	/* fixme: sync info now */
}

static void pan_info_toggle_image_cb(GtkWidget *widget, gpointer data)
{
	PanWindow *pw = data;

	pw->info_includes_image = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	/* fixme: sync info now */
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
	item = menu_item_add_check(menu, _("Sort by E_xif date"), pw->exif_date_enable,
				   G_CALLBACK(pan_exif_date_toggle_cb), pw);
	gtk_widget_set_sensitive(item, (pw->layout == LAYOUT_TIMELINE || pw->layout == LAYOUT_CALENDAR));

	menu_item_add_divider(menu);
	menu_item_add_check(menu, _("Show EXIF information"), pw->info_includes_exif,
			    G_CALLBACK(pan_info_toggle_exif_cb), pw);
	menu_item_add_check(menu, _("Show full size image"), pw->info_includes_image,
			    G_CALLBACK(pan_info_toggle_image_cb), pw);

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

	if (gtk_drag_get_source_widget(context) == pw->imd->pr) return;

	if (info == TARGET_URI_LIST)
		{
		GList *list;

		list = uri_list_from_text((gchar *)selection_data->data, TRUE);
		if (list && isdir((gchar *)list->data))
			{
			gchar *path = list->data;

			pan_window_layout_set_path(pw, path);
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
						8, (guchar *)text, len);
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
	GtkWidget *widget;

	widget = pw->imd->pr;

	gtk_drag_source_set(widget, GDK_BUTTON2_MASK,
			    dnd_file_drag_types, dnd_file_drag_types_count,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(widget), "drag_data_get",
			 G_CALLBACK(pan_window_set_dnd_data), pw);

	gtk_drag_dest_set(widget,
			  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			  dnd_file_drop_types, dnd_file_drop_types_count,
                          GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(widget), "drag_data_received",
			 G_CALLBACK(pan_window_get_dnd_data), pw);
}

/*
 *-----------------------------------------------------------------------------
 * maintenance (for rename, move, remove)
 *-----------------------------------------------------------------------------
 */

