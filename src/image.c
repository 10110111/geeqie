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
#include "image.h"


#include "image-load.h"
#include "collect.h"
#include "exif.h"
#include "pixbuf_util.h"
#include "ui_fileops.h"

#include <math.h>


#define IMAGE_TILE_SIZE 512
#define IMAGE_ZOOM_MIN -32.0
#define IMAGE_ZOOM_MAX 32.0

/* size of the image loader buffer (512 bytes x defined number) */
#define IMAGE_LOAD_BUFFER_COUNT 8

/* define this so that more bytes are read per idle loop on larger images (> 1MB) */
#define IMAGE_THROTTLE_LARGER_IMAGES 1

/* throttle factor to increase read bytes by (2 is double, 3 is triple, etc.) */
#define IMAGE_THROTTLE_FACTOR 4

/* the file size at which throttling take place */
#define IMAGE_THROTTLE_THRESHOLD 1048576

/* distance to drag mouse to disable image flip */
#define IMAGE_DRAG_SCROLL_THRESHHOLD 4

/* alpha channel checkerboard background (same as gimp) */
#define IMAGE_ALPHA_CHECK1 0x00999999
#define IMAGE_ALPHA_CHECK2 0x00666666
#define IMAGE_ALPHA_CHECK_SIZE 16

#define IMAGE_AUTO_REFRESH_TIME 3000

/* when scaling image to below this size, use nearest pixel for scaling
 * (below about 4, the other scale types become slow generating their conversion tables)
 */
#define IMAGE_MIN_SCALE_SIZE 8


typedef enum {
	TILE_RENDER_NONE = 0,	/* do nothing */
	TILE_RENDER_AREA,	/* render an area of the tile */
	TILE_RENDER_ALL		/* render the whole tile */
} TileRenderType;

typedef struct _ImageTile ImageTile;
struct _ImageTile
{
	GdkPixmap *pixmap;	/* off screen buffer */
	GdkPixbuf *pixbuf;	/* pixbuf area for zooming */
	gint x;			/* x offset into image */
	gint y;			/* y offset into image */
	gint w;			/* width that is visible (may be less if at edge of image) */
	gint h;			/* height '' */

	gint blank;

/* render_todo: (explanation)
	NONE	do nothing
	AREA	render area of tile, usually only used when loading an image
		note: will jump to an ALL if render_done is not ALL.
	ALL	render entire tile, if never done before w/ ALL, for expose events *only*
*/

	TileRenderType render_todo;	/* what to do (see above) */
	TileRenderType render_done;	/* highest that has been done before on tile */
};

typedef struct _QueueData QueueData;
struct _QueueData
{
	ImageTile *it;
	gint x;
	gint y;
	gint w;
	gint h;
	gint new_data;
};

typedef struct _CacheData CacheData;
struct _CacheData
{
	GdkPixmap *pixmap;
	GdkPixbuf *pixbuf;
	ImageTile *it;
	guint size;
};

typedef struct _OverlayData OverlayData;
struct _OverlayData
{
	gint id;

	GdkPixbuf *pixbuf;

	gint x;
	gint y;
	gint relative;	/* x,y coordinates are relative, negative values start bottom right */

	gint visible;
	gint always;	/* hide temporarily when scrolling */
};

/* needed to be declared before the source_tile stuff */
static void image_pixbuf_sync(ImageWindow *imd, gdouble zoom, gint blank, gint new);
static void image_zoom_sync(ImageWindow *imd, gdouble zoom,
                            gint force, gint blank, gint new,
                            gint center_point, gint px, gint py);
static void image_queue(ImageWindow *imd, gint x, gint y, gint w, gint h,
			gint clamp, TileRenderType render, gint new_data);

static gint util_clip_region(gint x, gint y, gint w, gint h,
			     gint clip_x, gint clip_y, gint clip_w, gint clip_h,
			     gint *rx, gint *ry, gint *rw, gint *rh);

/*
 *-------------------------------------------------------------------
 * source tiles
 *-------------------------------------------------------------------
 */

typedef struct _SourceTile SourceTile;
struct _SourceTile
{
	gint x;
	gint y;
	GdkPixbuf *pixbuf;
	gint blank;
};

static void source_tile_free(SourceTile *st)
{
	if (!st) return;

	if (st->pixbuf) gdk_pixbuf_unref(st->pixbuf);
	g_free(st);
}

static void source_tile_free_all(ImageWindow *imd)
{
	GList *work;

	work = imd->source_tiles;
	while (work)
		{
		SourceTile *st = work->data;
		work = work->next;

		source_tile_free(st);
		}

	g_list_free(imd->source_tiles);
	imd->source_tiles = NULL;
}

static gint source_tile_visible(ImageWindow *imd, SourceTile *st)
{
	gint x, y, w, h;

	if (!st) return FALSE;

	x = (imd->x_scroll / IMAGE_TILE_SIZE) * IMAGE_TILE_SIZE;
	y = (imd->y_scroll / IMAGE_TILE_SIZE) * IMAGE_TILE_SIZE;
	w = ((imd->x_scroll + imd->vis_width) / IMAGE_TILE_SIZE) * IMAGE_TILE_SIZE + IMAGE_TILE_SIZE;
	h = ((imd->y_scroll + imd->vis_height) / IMAGE_TILE_SIZE) * IMAGE_TILE_SIZE + IMAGE_TILE_SIZE;

	return !((double)st->x * imd->scale < (double)x ||
		 (double)(st->x + imd->source_tile_width) * imd->scale > (double)w ||
		 (double)st->y * imd->scale < (double)y ||
		 (double)(st->y + imd->source_tile_height) * imd->scale > (double)h);
}

static SourceTile *source_tile_new(ImageWindow *imd, gint x, gint y)
{
	SourceTile *st = NULL;
	gint count;

	if (imd->source_tiles_cache_size < 4) imd->source_tiles_cache_size = 4;

	if (imd->source_tile_width < 1 || imd->source_tile_height < 1)
		{
		printf("warning: source tile size too small %d x %d\n", imd->source_tile_width, imd->source_tile_height);
		return NULL;
		}

	count = g_list_length(imd->source_tiles);
	if (count >= imd->source_tiles_cache_size)
		{
		GList *work;

		work = g_list_last(imd->source_tiles);
		while (work && count >= imd->source_tiles_cache_size)
			{
			SourceTile *needle;

			needle = work->data;
			work = work->prev;

			if (!source_tile_visible(imd, needle))
				{
				imd->source_tiles = g_list_remove(imd->source_tiles, needle);

				if (imd->func_tile_dispose)
					{
					if (debug) printf("tile dispose: %d x %d @ %d x %d\n",
							 needle->x, needle->y, imd->x_scroll, imd->y_scroll);
					imd->func_tile_dispose(imd, needle->x, needle->y,
							       imd->source_tile_width, imd->source_tile_height,
							       needle->pixbuf, imd->data_tile);
					}

				if (!st)
					{
					st = needle;
					}
				else
					{
					source_tile_free(needle);
					}

				count--;
				}
			else if (debug)
				{
				printf("we still think %d x %d is visble\n", needle->x, needle->y);
				}
			}

		if (debug)
			{
			printf("cache count %d, max is %d\n", count, imd->source_tiles_cache_size);
			}
		}

	if (!st)
		{
		st = g_new0(SourceTile, 1);
		st->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
					    imd->source_tile_width, imd->source_tile_height);
		}

	st->x = (x / imd->source_tile_width) * imd->source_tile_width;
	st->y = (y / imd->source_tile_height) * imd->source_tile_height;
	st->blank = TRUE;

	imd->source_tiles = g_list_prepend(imd->source_tiles, st);

	if (debug)
		{
		printf("tile request: %d x %d\n", st->x, st->y);
		if (!source_tile_visible(imd, st)) printf("tile request for invisible tile!\n");
		}

	return st;
}

static void image_tile_invalidate(ImageWindow *imd, gint x, gint y, gint w, gint h)
{
	gint i, j;
	gint x1, x2;
	gint y1, y2;
	GList *work;

	x1 = (gint)floor(x / imd->tile_width) * imd->tile_width;
	x2 = (gint)ceil((x + w) / imd->tile_width) * imd->tile_width;

	y1 = (gint)floor(y / imd->tile_height) * imd->tile_height;
	y2 = (gint)ceil((y + h) / imd->tile_height) * imd->tile_height;

	work = g_list_nth(imd->tiles, y1 / imd->tile_height * imd->tile_cols + (x1 / imd->tile_width));
	for (j = y1; j <= y2; j += imd->tile_height)
		{
		GList *tmp;
		tmp = work;
		for (i = x1; i <= x2; i += imd->tile_width)
			{
			if (tmp)
				{
				ImageTile *it = tmp->data;

				it->render_done = TILE_RENDER_NONE;
				it->render_todo = TILE_RENDER_ALL;

				tmp = tmp->next;
				}
			}
		work = g_list_nth(work, imd->tile_cols);        /* step 1 row */
		}
}

static SourceTile *source_tile_request(ImageWindow *imd, gint x, gint y)
{
	SourceTile *st;

	st = source_tile_new(imd, x, y);

	if (imd->func_tile_request &&
	    imd->func_tile_request(imd, st->x, st->y,
				   imd->source_tile_width, imd->source_tile_height, st->pixbuf, imd->data_tile))
		{
		st->blank = FALSE;
		}
#if 0
	/* fixme: somehow invalidate the new st region */
	image_queue(imd, st->x, st->y, imd->source_tile_width, imd->source_tile_height, FALSE, TILE_RENDER_AREA, TRUE);
#endif
	image_tile_invalidate(imd, st->x * imd->scale, st->y * imd->scale,
			      imd->source_tile_width * imd->scale, imd->source_tile_height * imd->scale);

	return st;
}

static SourceTile *source_tile_find(ImageWindow *imd, gint x, gint y)
{
	GList *work;

	work = imd->source_tiles;
	while (work)
		{
		SourceTile *st = work->data;

		if (x >= st->x && x < st->x + imd->source_tile_width &&
		    y >= st->y && y < st->y + imd->source_tile_height)
			{
			if (work != imd->source_tiles)
				{
				imd->source_tiles = g_list_remove_link(imd->source_tiles, work);
				imd->source_tiles = g_list_concat(work, imd->source_tiles);
				}
			return st;
			}

		work = work->next;
		}

	return NULL;
}

static GList *source_tile_compute_region(ImageWindow *imd, gint x, gint y, gint w, gint h, gint request)
{
	gint x1, y1;
	GList *list = NULL;
	gint sx, sy;

	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (w > imd->image_width) w = imd->image_width;
	if (h > imd->image_height) h = imd->image_height;

	sx = (x / imd->source_tile_width) * imd->source_tile_width;
	sy = (y / imd->source_tile_height) * imd->source_tile_height;

	for (x1 = sx; x1 < x + w; x1+= imd->source_tile_width)
		{
		for (y1 = sy; y1 < y + h; y1 += imd->source_tile_height)
			{
			SourceTile *st;

			st = source_tile_find(imd, x1, y1);
			if (!st && request) st = source_tile_request(imd, x1, y1);

			if (st) list = g_list_prepend(list, st);
			}
		}

	return g_list_reverse(list);
}

static void source_tile_changed(ImageWindow *imd, gint x, gint y, gint width, gint height)
{
	GList *work;

	work = imd->source_tiles;
	while (work)
		{
		SourceTile *st;
		gint rx, ry, rw, rh;

		st = work->data;
		work = work->next;

		if (util_clip_region(st->x, st->y, imd->source_tile_width, imd->source_tile_height,
				     x, y, width, height,
				     &rx, &ry, &rw, &rh))
			{
			GdkPixbuf *pixbuf;

			pixbuf = gdk_pixbuf_new_subpixbuf(st->pixbuf, rx - st->x, ry - st->y, rw, rh);
			if (imd->func_tile_request &&
			    imd->func_tile_request(imd, rx, ry, rw, rh, pixbuf, imd->data_tile))
				{
				image_tile_invalidate(imd, rx * imd->scale, ry * imd->scale, rw * imd->scale, rh * imd->scale);
				}
			g_object_unref(pixbuf);
			}
		}
}


static gint source_tile_render(ImageWindow *imd, ImageTile *it,
			       gint x, gint y, gint w, gint h,
			       gint new_data, gint fast)
{
	GList *list;
	GList *work;
	gint draw = FALSE;

	if (imd->zoom == 1.0 || imd->scale == 1.0)
		{
		list = source_tile_compute_region(imd, it->x + x, it->y + y, w, h, TRUE);
		work = list;
		while (work)
			{
			SourceTile *st;
			gint rx, ry, rw, rh;

			st = work->data;
			work = work->next;

			if (util_clip_region(st->x, st->y, imd->source_tile_width, imd->source_tile_height,
					     it->x + x, it->y + y, w, h,
					     &rx, &ry, &rw, &rh))
				{
				if (st->blank)
					{
					gdk_draw_rectangle(it->pixmap, imd->image->style->black_gc, TRUE,
							   rx - st->x, ry - st->y, rw, rh);
					}
				else /* (imd->zoom == 1.0 || imd->scale == 1.0) */
					{
					gdk_draw_pixbuf(it->pixmap,
							imd->image->style->fg_gc[GTK_WIDGET_STATE(imd->image)],
							st->pixbuf,
							rx - st->x, ry - st->y,
							rx - it->x, ry - it->y,
							rw, rh,
							(GdkRgbDither)dither_quality, rx, ry);
					}
				}
			}
		}
	else
		{
		double scale_x, scale_y;
		gint sx, sy, sw, sh;

		if (imd->image_width == 0 || imd->image_height == 0) return FALSE;
		scale_x = (double)imd->width / imd->image_width;
		scale_y = (double)imd->height / imd->image_height;

		sx = (double)(it->x + x) / scale_x;
		sy = (double)(it->y + y) / scale_y;
		sw = (double)w / scale_x;
		sh = (double)h / scale_y;

		if (imd->width < IMAGE_MIN_SCALE_SIZE || imd->height < IMAGE_MIN_SCALE_SIZE) fast = TRUE;

#if 0
		/* draws red over draw region, to check for leaks (regions not filled) */
		pixbuf_draw_rect(it->pixbuf, x, y, w, h, 255, 0, 0, 255, FALSE);
#endif

		list = source_tile_compute_region(imd, sx, sy, sw, sh, TRUE);
		work = list;
		while (work)
			{
			SourceTile *st;
			gint rx, ry, rw, rh;
			gint stx, sty, stw, sth;

			st = work->data;
			work = work->next;

			stx = floor((double)st->x * scale_x);
			sty = floor((double)st->y * scale_y);
			stw = ceil ((double)(st->x + imd->source_tile_width) * scale_x) - stx;
			sth = ceil ((double)(st->y + imd->source_tile_height) * scale_y) - sty;

			if (util_clip_region(stx, sty, stw, sth,
					     it->x + x, it->y + y, w, h,
					     &rx, &ry, &rw, &rh))
				{
				if (st->blank)
					{
					gdk_draw_rectangle(it->pixmap, imd->image->style->black_gc, TRUE,
					   rx - st->x, ry - st->y, rw, rh);
					}
				else
					{
					double offset_x;
					double offset_y;

					/* may need to use unfloored stx,sty values here */
					offset_x = ((double)stx < (double)it->x) ?
						    (double)stx - (double)it->x  : 0.0;
					offset_y = ((double)sty < (double)it->y) ?
						    (double)sty - (double)it->y  : 0.0;

					gdk_pixbuf_scale(st->pixbuf, it->pixbuf, rx - it->x, ry - it->y, rw, rh,
						 (double) 0.0 + rx - it->x + offset_x,
						 (double) 0.0 + ry - it->y + offset_y,
						 scale_x, scale_y,
						 (fast) ? GDK_INTERP_NEAREST : (GdkInterpType)zoom_quality);
					draw = TRUE;
					}
				}
			}
		}

	g_list_free(list);

	return draw;
}

static void image_source_tile_unset(ImageWindow *imd)
{
	source_tile_free_all(imd);

	imd->source_tiles_enabled = FALSE;
}

void image_set_image_as_tiles(ImageWindow *imd, gint width, gint height,
			      gint tile_width, gint tile_height, gint cache_size,
			      ImageTileRequestFunc func_tile_request,
			      ImageTileDisposeFunc func_tile_dispose,
			      gpointer data,
			      gdouble zoom)
{
	/* FIXME: unset any current image */
	image_source_tile_unset(imd);

	if (tile_width < 32 || tile_height < 32)
		{
		printf("warning: tile size too small %d x %d (min 32x32)\n", tile_width, tile_height);
		return;
		}
	if (width < 32 || height < 32)
		{
		printf("warning: tile canvas too small %d x %d (min 32x32)\n", width, height);
		return;
		}
	if (!func_tile_request)
		{
		printf("warning: tile request function is null\n");
		}

	printf("Setting source tiles to size %d x %d, grid is %d x %d\n", tile_width, tile_height, width, height);

	if (cache_size < 4) cache_size = 4;

	imd->source_tiles_enabled = TRUE;
	imd->source_tiles_cache_size = cache_size;
	imd->source_tile_width = tile_width;
	imd->source_tile_height = tile_height;

	imd->image_width = width;
	imd->image_height = height;

	imd->func_tile_request = func_tile_request;
	imd->func_tile_dispose = func_tile_dispose;
	imd->data_tile = data;

	image_zoom_sync(imd, zoom, TRUE, FALSE, TRUE, FALSE, 0, 0);
}


static void image_queue_clear(ImageWindow *imd);

static void image_update_title(ImageWindow *imd);
static void image_update_util(ImageWindow *imd);
static void image_complete_util(ImageWindow *imd, gint preload);

static void image_button_do(ImageWindow *imd, GdkEventButton *bevent);

static void image_overlay_draw(ImageWindow *imd, gint x, gint y, gint w, gint h);
static void image_overlay_queue_all(ImageWindow *imd);

static void image_scroller_timer_set(ImageWindow *imd, gint start);


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


/*
 *-------------------------------------------------------------------
 * tile cache
 *-------------------------------------------------------------------
 */

static gint pixmap_calc_size(GdkPixmap *pixmap)
{
	gint w, h, d;

	d = gdk_drawable_get_depth(pixmap);
	gdk_drawable_get_size(pixmap, &w, &h);
	return w * h * (d / 8);
}

static void image_tile_cache_remove(ImageWindow *imd, ImageTile *it)
{
	GList *work;

	work = imd->tile_cache;
	while(work)
		{
		CacheData *cd = work->data;
		work = work->next;

		if (cd->it == it)
			{
			imd->tile_cache = g_list_remove(imd->tile_cache, cd);
			imd->tile_cache_size -= cd->size;
			g_free(cd);
			}
		}
}

static void image_tile_cache_free(ImageWindow *imd, CacheData *cd)
{
	imd->tile_cache = g_list_remove(imd->tile_cache, cd);
	if (cd->pixmap)
		{
		g_object_unref(cd->it->pixmap);
		cd->it->pixmap = NULL;
		cd->it->render_done = TILE_RENDER_NONE;
		}
	if (cd->pixbuf)
		{
		gdk_pixbuf_unref(cd->it->pixbuf);
		cd->it->pixbuf = NULL;
		}
	imd->tile_cache_size -= cd->size;
	g_free(cd);
}

static void image_tile_cache_free_space(ImageWindow *imd, gint space, ImageTile *it)
{
	GList *work;
	gint tile_max;

	work = g_list_last(imd->tile_cache);

	if (imd->source_tiles_enabled && imd->scale < 1.0)
		{
		gint tiles;

		tiles = (imd->vis_width / IMAGE_TILE_SIZE + 1) * (imd->vis_width / IMAGE_TILE_SIZE + 1);
		tile_max = MAX(tiles * IMAGE_TILE_SIZE * IMAGE_TILE_SIZE * 3,
			       (gint)((double)tile_cache_max * 1048576.0 * imd->scale));
		}
	else
		{
		tile_max = tile_cache_max * 1048576;
		}

	while (work && imd->tile_cache_size > 0 && imd->tile_cache_size + space > tile_max)
		{
		CacheData *cd = work->data;
		work = work->prev;
		if (cd->it != it) image_tile_cache_free(imd, cd);
		}
}

static void image_tile_cache_add(ImageWindow *imd, ImageTile *it,
				 GdkPixmap *pixmap, GdkPixbuf *pixbuf, guint size)
{
	CacheData *cd;

	cd = g_new(CacheData, 1);
	cd->pixmap = pixmap;
	cd->pixbuf = pixbuf;
	cd->it = it;
	cd->size = size;

	imd->tile_cache = g_list_prepend(imd->tile_cache, cd);

	imd->tile_cache_size += cd->size;
}

static void image_tile_prepare(ImageWindow *imd, ImageTile *it)
{
	if (!it->pixmap)
		{
		GdkPixmap *pixmap;
		guint size;

		pixmap = gdk_pixmap_new(imd->image->window, imd->tile_width, imd->tile_height, -1);

		size = pixmap_calc_size(pixmap);
		image_tile_cache_free_space(imd, size, it);

		it->pixmap = pixmap;
		image_tile_cache_add(imd, it, pixmap, NULL, size);
		}
	
	if ((imd->zoom != 1.0 || imd->source_tiles_enabled || (imd->pixbuf && gdk_pixbuf_get_has_alpha(imd->pixbuf)) ) &&
	    !it->pixbuf)
		{
		GdkPixbuf *pixbuf;
		guint size;

		if (imd->pixbuf)
			{
			pixbuf = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(imd->pixbuf),
						gdk_pixbuf_get_has_alpha(imd->pixbuf),
						gdk_pixbuf_get_bits_per_sample(imd->pixbuf),
						imd->tile_width, imd->tile_height);
			}
		else
			{
			 pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, imd->tile_width, imd->tile_height);
			}

		size = gdk_pixbuf_get_rowstride(pixbuf) * imd->tile_height;
		image_tile_cache_free_space(imd, size, it);

		it->pixbuf = pixbuf;
		image_tile_cache_add(imd, it, NULL, pixbuf, size);
		}
}

/*
 *-------------------------------------------------------------------
 * tiles
 *-------------------------------------------------------------------
 */

static ImageTile *image_tile_new(gint w, gint h)
{
	ImageTile *it;

	it = g_new0(ImageTile, 1);
	it->w = w;
	it->h = h;
	it->pixmap = NULL;
	it->pixbuf = NULL;
	it->blank = TRUE;
	it->render_todo = TILE_RENDER_NONE;
	it->render_done = TILE_RENDER_NONE;

	return it;
}

static void image_tile_free(ImageTile *it)
{
	if (!it) return;

	if (it->pixbuf) gdk_pixbuf_unref(it->pixbuf);
	if (it->pixmap) g_object_unref(it->pixmap);

	g_free(it);
}

static void image_tile_sync_count(ImageWindow *imd, gint n)
{
	gint l;

	l = g_list_length(imd->tiles);

	if (l == n) return;

	if (l < n)
		{
		while (l < n)
			{
			imd->tiles = g_list_prepend(imd->tiles, image_tile_new(imd->tile_width, imd->tile_height));
			l++;
			}
		}
	else
		{
		/* This should remove from the tail of the GList, but with large images there are many tiles,
		 * making this significantly faster for those cases.
		 */
		while (l > n && imd->tiles)
			{
			ImageTile *it = imd->tiles->data;
			imd->tiles = g_list_remove(imd->tiles, it);
			image_tile_cache_remove(imd, it);
			image_tile_free(it);
			l--;
			}
		}
}

static void image_tile_sync(ImageWindow *imd, gint width, gint height, gint blank)
{
	gint rows;
	gint x, y;
	GList *work;

	imd->tile_cols = (width + imd->tile_width - 1) / imd->tile_width;

	rows = (height + imd->tile_height - 1) / imd->tile_height;

	image_tile_sync_count(imd, imd->tile_cols * rows);

	x = y = 0;
	work = imd->tiles;
	while(work)
		{
		ImageTile *it = work->data;
		work = work->next;

		it->x = x;
		it->y = y;
		if (x + imd->tile_width > width)
			{
			it->w = width - x;
			}	
		else
			{
			it->w = imd->tile_width;
			}
		if (y + imd->tile_height > height)
			{
			it->h = height - y;
			}
		else
			{
			it->h = imd->tile_height;
			}

		it->blank = blank;
		it->render_todo = TILE_RENDER_NONE;
		it->render_done = TILE_RENDER_NONE;

		x += imd->tile_width;
		if (x >= width)
			{
			x = 0;
			y += imd->tile_height;
			}
		}

	/* all it's are now useless in queue */
	image_queue_clear(imd);
}

static void image_tile_render(ImageWindow *imd, ImageTile *it,
			      gint x, gint y, gint w, gint h,
			      gint new_data, gint fast)
{
	gint has_alpha;
	gint draw = FALSE;

	if (it->render_todo == TILE_RENDER_NONE && it->pixmap && !new_data) return;

	if (it->render_done != TILE_RENDER_ALL)
		{
		x = 0;
		y = 0;
		w = it->w;
		h = it->h;
		if (!fast) it->render_done = TILE_RENDER_ALL;
		}
	else if (it->render_todo != TILE_RENDER_AREA)
		{
		if (!fast) it->render_todo = TILE_RENDER_NONE;
		return;
		}

	if (!fast) it->render_todo = TILE_RENDER_NONE;

	if (new_data) it->blank = FALSE;

	image_tile_prepare(imd, it);
	has_alpha = (imd->pixbuf && gdk_pixbuf_get_has_alpha(imd->pixbuf));

	/* FIXME checker colors for alpha should be configurable,
	 * also should be drawn for blank = TRUE
	 */

	if (it->blank)
		{
		/* no data, do fast rect fill */
		gdk_draw_rectangle(it->pixmap, imd->image->style->black_gc, TRUE,
				   0, 0, it->w, it->h);
		}
	else if (imd->source_tiles_enabled)
		{
		draw = source_tile_render(imd, it, x, y, w, h, new_data, fast);
		}
	else if (imd->zoom == 1.0 || imd->scale == 1.0)
		{
		if (has_alpha)
			{
			gdk_pixbuf_composite_color(imd->pixbuf, it->pixbuf, x, y, w, h,
					 (double) 0.0 - it->x,
					 (double) 0.0 - it->y,
					 1.0, 1.0, GDK_INTERP_NEAREST,
					 255, it->x + x, it->y + y,
					 IMAGE_ALPHA_CHECK_SIZE, IMAGE_ALPHA_CHECK1, IMAGE_ALPHA_CHECK2);
			draw = TRUE;
			}
		else
			{
			/* faster, simple */
			gdk_draw_pixbuf(it->pixmap,
					imd->image->style->fg_gc[GTK_WIDGET_STATE(imd->image)],
					imd->pixbuf,
					it->x + x, it->y + y,
					x, y,
					w, h,
					(GdkRgbDither)dither_quality, it->x + x, it->y + y);
			}
		}
	else
		{
		double scale_x, scale_y;

		if (imd->image_width == 0 || imd->image_height == 0) return;
		scale_x = (double)imd->width / imd->image_width;
		scale_y = (double)imd->height / imd->image_height;

		/* HACK: The pixbuf scalers get kinda buggy(crash) with extremely
		 * small sizes for anything but GDK_INTERP_NEAREST
		 */
		if (imd->width < IMAGE_MIN_SCALE_SIZE || imd->height < IMAGE_MIN_SCALE_SIZE) fast = TRUE;

		if (!has_alpha)
			{
			gdk_pixbuf_scale(imd->pixbuf, it->pixbuf, x, y, w, h,
					 (double) 0.0 - it->x,
					 (double) 0.0 - it->y,
					 scale_x, scale_y,
					 (fast) ? GDK_INTERP_NEAREST : (GdkInterpType)zoom_quality);
			}
		else
			{
			gdk_pixbuf_composite_color(imd->pixbuf, it->pixbuf, x, y, w, h,
					 (double) 0.0 - it->x,
					 (double) 0.0 - it->y,
					 scale_x, scale_y,
					 (fast) ? GDK_INTERP_NEAREST : (GdkInterpType)zoom_quality,
					 255, it->x + x, it->y + y,
					 IMAGE_ALPHA_CHECK_SIZE, IMAGE_ALPHA_CHECK1, IMAGE_ALPHA_CHECK2);
			}
		draw = TRUE;
		}

	if (draw && it->pixbuf && !it->blank)
		{
		gdk_draw_pixbuf(it->pixmap,
				imd->image->style->fg_gc[GTK_WIDGET_STATE(imd->image)],
				it->pixbuf,
				x, y,
				x, y,
				w, h,
				(GdkRgbDither)dither_quality, it->x + x, it->y + y);
		}
}

static void image_tile_expose(ImageWindow *imd, ImageTile *it,
			      gint x, gint y, gint w, gint h,
			      gint new_data, gint fast)
{
	image_tile_render(imd, it, x, y, w, h, new_data, fast);

	gdk_draw_drawable(imd->image->window, imd->image->style->fg_gc[GTK_WIDGET_STATE(imd->image)],
			  it->pixmap, x, y,
			  imd->x_offset + (it->x - imd->x_scroll) + x, imd->y_offset + (it->y - imd->y_scroll) + y, w, h);

	if (imd->overlay_list)
		{
		image_overlay_draw(imd, imd->x_offset + (it->x - imd->x_scroll) + x,
					imd->y_offset + (it->y - imd->y_scroll) + y,
					w, h);
		}
}

static gint image_tile_is_visible(ImageWindow *imd, ImageTile *it)
{
	return (it->x + it->w >= imd->x_scroll && it->x <= imd->x_scroll + imd->window_width - imd->x_offset * 2 &&
		it->y + it->h >= imd->y_scroll && it->y <= imd->y_scroll + imd->window_height - imd->y_offset * 2);
}

/*
 *-------------------------------------------------------------------
 * render queue
 *-------------------------------------------------------------------
 */


static gint image_queue_draw_idle_cb(gpointer data)
{
	ImageWindow *imd = data;
	QueueData *qd;
	gint fast;

	if ((!imd->pixbuf && !imd->source_tiles_enabled) || (!imd->draw_queue && !imd->draw_queue_2pass) || imd->draw_idle_id == -1)
		{
		if (!imd->completed) image_complete_util(imd, FALSE);

		imd->draw_idle_id = -1;
		return FALSE;
		}

	if (imd->draw_queue)
		{
		qd = imd->draw_queue->data;
		fast = (two_pass_zoom && (GdkInterpType)zoom_quality != GDK_INTERP_NEAREST && imd->scale != 1.0);
		}
	else
		{
		if (imd->il)
			{
			/* still loading, wait till done (also drops the higher priority) */

			imd->draw_idle_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
							    image_queue_draw_idle_cb, imd, NULL);
			imd->draw_idle_high = FALSE;
			return FALSE;
			}
		qd = imd->draw_queue_2pass->data;
		fast = FALSE;
		}

	if (GTK_WIDGET_REALIZED(imd->image))
		{
		if (image_tile_is_visible(imd, qd->it))
			{
			image_tile_expose(imd, qd->it, qd->x, qd->y, qd->w, qd->h, qd->new_data, fast);
			}
		else if (qd->new_data)
			{
			/* if new pixel data, and we already have a pixmap, update the tile */
			qd->it->blank = FALSE;
			if (qd->it->pixmap && qd->it->render_done == TILE_RENDER_ALL)
				{
				image_tile_render(imd, qd->it, qd->x, qd->y, qd->w, qd->h, qd->new_data, fast);
				}
			}
		}

	if (imd->draw_queue)
		{
		imd->draw_queue = g_list_remove(imd->draw_queue, qd);
		if (fast)
			{
			imd->draw_queue_2pass = g_list_append(imd->draw_queue_2pass, qd);
			}
		else
			{
			g_free(qd);
			}
		}
	else
		{
		imd->draw_queue_2pass = g_list_remove(imd->draw_queue_2pass, qd);
		g_free(qd);
		}

	if (!imd->draw_queue && !imd->draw_queue_2pass)
		{
		if (!imd->completed) image_complete_util(imd, FALSE);

		imd->draw_idle_id = -1;
		return FALSE;
		}

	return TRUE;
}

static QueueData *image_queue_combine(ImageWindow *imd, QueueData *qd)
{
	QueueData *found = NULL;
	GList *work;

	work = imd->draw_queue;
	while (work && !found)
		{
		found = work->data;
		work = work->next;

		if (found->it != qd->it) found = NULL;
		}

	if (found)
		{
		if (found->x + found->w < qd->x + qd->w) found->w += (qd->x + qd->w) - (found->x + found->w);
		if (found->x > qd->x)
			{
			found->w += found->x - qd->x;
			found->x = qd->x;
			}

		if (found->y + found->h < qd->y + qd->h) found->h += (qd->y + qd->h) - (found->y + found->h);
		if (found->y > qd->y)
			{
			found->h += found->y - qd->y;
			found->y = qd->y;
			}
		found->new_data |= qd->new_data;
		}

	return found;
}

static gint image_clamp_to_visible(ImageWindow *imd, gint *x, gint *y, gint *w, gint *h)
{
	gint nx, ny;
	gint nw, nh;
	gint vx, vy;
	gint vw, vh;

	vw = imd->vis_width;
	vh = imd->vis_height;

	vx = imd->x_scroll;
	vy = imd->y_scroll;

	if (*x + *w < vx || *x > vx + vw || *y + *h < vy || *y > vy + vh) return FALSE;

	/* now clamp it */
	nx = CLAMP(*x, vx, vx + vw);
	nw = CLAMP(*w - (nx - *x), 1, vw);

	ny = CLAMP(*y, vy, vy + vh);
	nh = CLAMP(*h - (ny - *y), 1, vh);

	*x = nx;
	*y = ny;
	*w = nw;
	*h = nh;

	return TRUE;
}

static gint image_queue_to_tiles(ImageWindow *imd, gint x, gint y, gint w, gint h,
				 gint clamp, TileRenderType render, gint new_data)
{
	gint i, j;
	gint x1, x2;
	gint y1, y2;
	GList *work;

	if (clamp && !image_clamp_to_visible(imd, &x, &y, &w, &h)) return FALSE;

	x1 = (gint)floor(x / imd->tile_width) * imd->tile_width;
	x2 = (gint)ceil((x + w) / imd->tile_width) * imd->tile_width;

	y1 = (gint)floor(y / imd->tile_height) * imd->tile_height;
	y2 = (gint)ceil((y + h) / imd->tile_height) * imd->tile_height;

	work = g_list_nth(imd->tiles, y1 / imd->tile_height * imd->tile_cols + (x1 / imd->tile_width));
	for (j = y1; j <= y2; j += imd->tile_height)
		{
		GList *tmp;
		tmp = work;
		for (i = x1; i <= x2; i += imd->tile_width)
			{
			if (tmp)
				{
				ImageTile *it = tmp->data;
				QueueData *qd;

				if ((render == TILE_RENDER_ALL && it->render_done != TILE_RENDER_ALL) ||
				    (render == TILE_RENDER_AREA && it->render_todo != TILE_RENDER_ALL))
					{
					it->render_todo = render;
					}

				qd = g_new(QueueData, 1);
				qd->it = it;
				qd->new_data = new_data;

				if (i < x)
					{
					qd->x = x - i;
					}
				else
					{
					qd->x = 0;
					}
				qd->w = x + w - i - qd->x;
				if (qd->x + qd->w > imd->tile_width) qd->w = imd->tile_width - qd->x;


				if (j < y)
					{
					qd->y = y - j;
					}
				else
					{
					qd->y = 0;
					}
				qd->h = y + h - j - qd->y;
				if (qd->y + qd->h > imd->tile_height) qd->h = imd->tile_height - qd->y;

				if (qd->w < 1 || qd->h < 1 || /* <--- sanity checks, rare cases cause this */
				    image_queue_combine(imd, qd))
					{
					g_free(qd);
					}
				else
					{
					imd->draw_queue = g_list_append(imd->draw_queue, qd);
					}

				tmp = tmp->next;
				}
			}
		work = g_list_nth(work, imd->tile_cols);	/* step 1 row */
		}

	return TRUE;
}

static void image_queue(ImageWindow *imd, gint x, gint y, gint w, gint h,
			gint clamp, TileRenderType render, gint new_data)
{
	gint nx, ny;

	nx = CLAMP(x, 0, imd->width - 1);
	ny = CLAMP(y, 0, imd->height - 1);
	w -= (nx - x);
	h -= (ny - y);
	w = CLAMP(w, 0, imd->width - nx);
	h = CLAMP(h, 0, imd->height - ny);
	if (w < 1 || h < 1) return;

	if (image_queue_to_tiles(imd, nx, ny, w, h, clamp, render, new_data) &&
	    ((!imd->draw_queue && !imd->draw_queue_2pass) || imd->draw_idle_id == -1 || !imd->draw_idle_high))
		{
		if (imd->draw_idle_id != -1) g_source_remove(imd->draw_idle_id);
		imd->draw_idle_id = g_idle_add_full(GDK_PRIORITY_REDRAW,
						    image_queue_draw_idle_cb, imd, NULL);
		imd->draw_idle_high = TRUE;
		}
}

static void image_queue_list_free(GList *list)
{
	GList *work;

	work = list;
	while (work)
		{
		QueueData *qd;

		qd = work->data;
		work = work->next;
		g_free(qd);
		}

	g_list_free(list);
}

static void image_queue_clear(ImageWindow *imd)
{
	image_queue_list_free(imd->draw_queue);
	imd->draw_queue = NULL;

	image_queue_list_free(imd->draw_queue_2pass);
	imd->draw_queue_2pass = NULL;

	if (imd->draw_idle_id != -1) g_source_remove(imd->draw_idle_id);
	imd->draw_idle_id = -1;
}

/*
 *-------------------------------------------------------------------
 * core calculations
 *-------------------------------------------------------------------
 */

static gint image_top_window_sizable(ImageWindow *imd)
{
	if (!imd->top_window) return FALSE;
	if (!fit_window) return FALSE;
	if (!imd->top_window_sync) return FALSE;
	if (!imd->image->window) return FALSE;
	if (window_maximized(imd->top_window)) return FALSE;

	return TRUE;
}

static gint image_size_top_window(ImageWindow *imd, gint w, gint h)
{
	gint ww, wh;

	if (!image_top_window_sizable(imd)) return FALSE;

	if (limit_window_size)
		{
		gint sw = gdk_screen_width() * max_window_size / 100;
		gint sh = gdk_screen_height() * max_window_size / 100;

		if (w > sw) w = sw;
		if (h > sh) h = sh;
		}

	w += (imd->top_window->allocation.width - imd->image->allocation.width);
	h += (imd->top_window->allocation.height - imd->image->allocation.height);

	gdk_drawable_get_size(imd->top_window->window, &ww, &wh);
	if (w == ww && h == wh) return FALSE;

	gdk_window_resize(imd->top_window->window, w, h);

	return TRUE;
}

static void image_redraw(ImageWindow *imd, gint new_data)
{
	image_queue_clear(imd);
	image_queue(imd, 0, 0, imd->width, imd->height, TRUE, TILE_RENDER_ALL, new_data);
}

static void image_border_draw(ImageWindow *imd, gint x, gint y, gint w, gint h)
{
	gint rx, ry, rw, rh;

	if (!imd->image->window) return;

	if (!imd->pixbuf && !imd->source_tiles_enabled)
		{
		if (util_clip_region(x, y, w, h,
			0, 0,
			imd->window_width, imd->window_height,
			&rx, &ry, &rw, &rh))
			{
			gdk_window_clear_area(imd->image->window, rx, ry, rw, rh);
			image_overlay_draw(imd, rx, ry, rw, rh);
			}
		return;
		}

	if (imd->vis_width < imd->window_width)
		{
		if (imd->x_offset > 0 &&
		    util_clip_region(x, y, w, h,
			0, 0,
			imd->x_offset, imd->window_height,
			&rx, &ry, &rw, &rh))
			{
			gdk_window_clear_area(imd->image->window, rx, ry, rw, rh);
			image_overlay_draw(imd, rx, ry, rw, rh);
			}
		if (imd->window_width - imd->vis_width - imd->x_offset > 0 &&
		    util_clip_region(x, y, w, h,
			imd->x_offset + imd->vis_width, 0,
			imd->window_width - imd->vis_width - imd->x_offset, imd->window_height,
			&rx, &ry, &rw, &rh))
			{
			gdk_window_clear_area(imd->image->window, rx, ry, rw, rh);
			image_overlay_draw(imd, rx, ry, rw, rh);
			}
		}
	if (imd->vis_height < imd->window_height)
		{
		if (imd->y_offset > 0 &&
		    util_clip_region(x, y, w, h,
			imd->x_offset, 0,
			imd->vis_width, imd->y_offset,
			&rx, &ry, &rw, &rh))
			{
			gdk_window_clear_area(imd->image->window, rx, ry, rw, rh);
			image_overlay_draw(imd, rx, ry, rw, rh);
			}
		if (imd->window_height - imd->vis_height - imd->y_offset > 0 &&
		    util_clip_region(x, y, w, h,
			imd->x_offset, imd->y_offset + imd->vis_height,
			imd->vis_width, imd->window_height - imd->vis_height - imd->y_offset,
			&rx, &ry, &rw, &rh))
			{
			gdk_window_clear_area(imd->image->window, rx, ry, rw, rh);
			image_overlay_draw(imd, rx, ry, rw, rh);
			}
		}
}

static void image_border_clear(ImageWindow *imd)
{
	image_border_draw(imd, 0, 0, imd->window_width, imd->window_height);
}

static void image_scroll_notify(ImageWindow *imd)
{
	if (imd->func_scroll_notify && imd->scale)
		{
		imd->func_scroll_notify(imd,
					(gint)((gdouble)imd->x_scroll / imd->scale),
					(gint)((gdouble)imd->y_scroll / imd->scale),
					(gint)((gdouble)imd->image_width - imd->vis_width / imd->scale),
					(gint)((gdouble)imd->image_height - imd->vis_height / imd->scale),
					imd->data_scroll_notify);
		}
}

static gint image_scroll_clamp(ImageWindow *imd)
{
	gint old_xs;
	gint old_ys;

	if (imd->zoom == 0.0)
		{
		imd->x_scroll = 0;
		imd->y_scroll = 0;

		image_scroll_notify(imd);
		return FALSE;
		}

	old_xs = imd->x_scroll;
	old_ys = imd->y_scroll;

	if (imd->x_offset > 0)
		{
		imd->x_scroll = 0;
		}
	else
		{
		imd->x_scroll = CLAMP(imd->x_scroll, 0, imd->width - imd->vis_width);
		}

	if (imd->y_offset > 0)
		{
		imd->y_scroll = 0;
		}
	else
		{
		imd->y_scroll = CLAMP(imd->y_scroll, 0, imd->height - imd->vis_height);
		}

	image_scroll_notify(imd);

	return (old_xs != imd->x_scroll || old_ys != imd->y_scroll);
}

static gint image_zoom_clamp(ImageWindow *imd, gdouble zoom, gint force, gint new)
{
	gint w, h;
	gdouble scale;

	zoom = CLAMP(zoom, imd->zoom_min, imd->zoom_max);

	if (imd->zoom == zoom && !force) return FALSE;

	w = imd->image_width;
	h = imd->image_height;

	if (zoom == 0.0 && !imd->pixbuf)
		{
		scale = 1.0;
		}
	else if (zoom == 0.0)
		{
		gint max_w;
		gint max_h;
		gint sizeable;

		sizeable = (new && image_top_window_sizable(imd));

		if (sizeable)
			{
			max_w = gdk_screen_width();
			max_h = gdk_screen_height();

			if (limit_window_size)
				{
				max_w = max_w * max_window_size / 100;
				max_h = max_h * max_window_size / 100;
				}
			}
		else
			{
			max_w = imd->window_width;
			max_h = imd->window_height;
			}

		if ((zoom_to_fit_expands && !sizeable) || w > max_w || h > max_h)
			{
			if ((gdouble)max_w / w > (gdouble)max_h / h)
				{
				scale = (gdouble)max_h / h;
				h = max_h;
				w = w * scale + 0.5;
				if (w > max_w) w = max_w;
				}
			else
				{
				scale = (gdouble)max_w / w;
				w = max_w;
				h = h * scale + 0.5;
				if (h > max_h) h = max_h;
				}
			if (w < 1) w = 1;
			if (h < 1) h = 1;
			}
		else
			{
			scale = 1.0;
			}
		}
	else if (zoom > 0.0) /* zoom orig, in */
		{
		scale = zoom;
		w = w * scale;
		h = h * scale;
		}
	else /* zoom out */
		{
		scale = 1.0 / (0.0 - zoom);
		w = w * scale;
		h = h * scale;
		}

	imd->zoom = zoom;
	imd->width = w;
	imd->height = h;
	imd->scale = scale;

	return TRUE;
}

static gint image_size_clamp(ImageWindow *imd)
{
	gint old_vw, old_vh;

	old_vw = imd->vis_width;
	old_vh = imd->vis_height;

	if (imd->width < imd->window_width)
		{
		imd->vis_width = imd->width;
		imd->x_offset = (imd->window_width - imd->width) / 2;
		}
	else
		{
		imd->vis_width = imd->window_width;
		imd->x_offset = 0;
		}

	if (imd->height < imd->window_height)
		{
		imd->vis_height = imd->height;
		imd->y_offset = (imd->window_height - imd->height) / 2;
		}
	else
		{
		imd->vis_height = imd->window_height;
		imd->y_offset = 0;
		}

	return (old_vw != imd->vis_width || old_vh != imd->vis_height);
}

static void image_size_sync(ImageWindow *imd, gint new_width, gint new_height)
{
	if (imd->window_width == new_width && imd->window_height == new_height) return;

	imd->window_width = new_width;
	imd->window_height = new_height;

	if (imd->zoom == 0.0) image_zoom_clamp(imd, 0.0, TRUE, FALSE);

	image_size_clamp(imd);
	image_scroll_clamp(imd);

#if 0
	gtk_widget_set_size_request(imd->image, imd->window_width, imd->window_height);
#endif

	/* ensure scroller remains visible */
	if (imd->scroller_overlay != -1)
		{
		gint update = FALSE;

		if (imd->scroller_x > new_width)
			{
			imd->scroller_x = new_width;
			imd->scroller_xpos = new_width;
			update = TRUE;
			}
		if (imd->scroller_y > new_height)
			{
			imd->scroller_y = new_height;
			imd->scroller_ypos = new_height;
			update = TRUE;
			}

		if (update)
			{
			GdkPixbuf *pixbuf;

			if (image_overlay_get(imd, imd->scroller_overlay, &pixbuf, NULL, NULL))
				{
				gint w, h;

				w = gdk_pixbuf_get_width(pixbuf);
				h = gdk_pixbuf_get_height(pixbuf);
				image_overlay_set(imd, imd->scroller_overlay, pixbuf,
						  imd->scroller_x - w / 2, imd->scroller_y - h / 2);
				}
			}
		}

	/* clear any borders */
	image_border_clear(imd);
	
	image_tile_sync(imd, imd->width, imd->height, FALSE);
#if 0
	/* no longer needed? (expose event should be doing this for us) */
	image_redraw(imd, FALSE);
#endif

	if (imd->title_show_zoom) image_update_title(imd);
	image_update_util(imd);
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
		const gchar *name;
		name = imd->collection->name;
		if (!name) name = _("Untitled");
		collection = g_strdup_printf(" (Collection %s)", name);
		}

	if (imd->title_show_zoom)
		{
		gchar *buf = image_zoom_get_as_text(imd);
		zoom = g_strconcat(" [", buf, "]", NULL);
		g_free(buf);
		}

	title = g_strdup_printf("%s%s%s%s%s%s",
		imd->title ? imd->title : "",
		imd->image_name ? imd->image_name : "",
		zoom ? zoom : "",
		collection ? collection : "",
		imd->image_name ? " - " : "",
		imd->title_right ? imd->title_right : "");

	gtk_window_set_title(GTK_WINDOW(imd->top_window), title);

	g_free(title);
	g_free(zoom);
	g_free(collection);
}

static void image_update_util(ImageWindow *imd)
{
	if (imd->func_update) imd->func_update(imd, imd->data_update);
}

static void image_complete_util(ImageWindow *imd, gint preload)
{
	if (imd->il && imd->pixbuf != image_loader_get_pixbuf(imd->il)) return;

	if (debug) printf("image load completed \"%s\" (%s)\n",
			  (preload) ? imd->read_ahead_path : imd->image_path,
			  (preload) ? "preload" : "current");

	if (!preload) imd->completed = TRUE;
	if (imd->func_complete) imd->func_complete(imd, preload, imd->data_complete);
}

static void image_new_util(ImageWindow *imd)
{
	if (imd->func_new) imd->func_new(imd, imd->data_new);
}

static void image_scroll_real(ImageWindow *imd, gint x, gint y)
{
	gint old_x, old_y;
	gint x_off, y_off;
	gint w, h;

	if (!imd->pixbuf && !imd->source_tiles_enabled) return;

	old_x = imd->x_scroll;
	old_y = imd->y_scroll;

	imd->x_scroll += x;
	imd->y_scroll += y;

	image_scroll_clamp(imd);
	if (imd->x_scroll == old_x && imd->y_scroll == old_y) return;

	if (imd->overlay_list)
		{
		gint new_x, new_y;

		new_x = imd->x_scroll;
		new_y = imd->y_scroll;
		imd->x_scroll = old_x;
		imd->y_scroll = old_y;
		image_overlay_queue_all(imd);
		imd->x_scroll = new_x;
		imd->y_scroll = new_y;
		}

	x_off = imd->x_scroll - old_x;
	y_off = imd->y_scroll - old_y;

	w = imd->vis_width - abs(x_off);
	h = imd->vis_height - abs(y_off);

	if (w < 1 || h < 1)
		{
		/* scrolled completely to new material */
		image_queue(imd, 0, 0, imd->width, imd->height, TRUE, TILE_RENDER_ALL, FALSE);
		return;
		}
	else
		{
		gint x1, y1;
		gint x2, y2;
		GdkGC *gc;

		if (x_off < 0)
			{
			x1 = abs(x_off);
			x2 = 0;
			}
		else
			{
			x1 = 0;
			x2 = abs(x_off);
			}

		if (y_off < 0)
			{
			y1 = abs(y_off);
			y2 = 0;
			}
		else
			{
			y1 = 0;
			y2 = abs(y_off);
			}

		gc = gdk_gc_new(imd->image->window);
		gdk_gc_set_exposures(gc, TRUE);
		gdk_draw_drawable(imd->image->window, gc,
				  imd->image->window,
				  x2 + imd->x_offset, y2 + imd->y_offset,
				  x1 + imd->x_offset, y1 + imd->y_offset, w, h);
		g_object_unref(gc);

		if (imd->overlay_list)
			{
			image_overlay_queue_all(imd);
			}

		w = imd->vis_width - w;
		h = imd->vis_height - h;

		if (w > 0)
			{
			image_queue(imd,
				    x_off > 0 ? imd->x_scroll + (imd->vis_width - w) : imd->x_scroll, imd->y_scroll,
				    w, imd->vis_height, TRUE, TILE_RENDER_ALL, FALSE);
			}
		if (h > 0)
			{
			/* FIXME, to optimize this, remove overlap */
			image_queue(imd,
				    imd->x_scroll, y_off > 0 ? imd->y_scroll + (imd->vis_height - h) : imd->y_scroll,
				    imd->vis_width, h, TRUE, TILE_RENDER_ALL, FALSE);
			}
		}
}

static void widget_set_cursor(GtkWidget *widget, gint icon)
{
	GdkCursor *cursor;

	if (!widget->window) return;

	if (icon == -1)
		{
		cursor = NULL;
		}
	else
		{
		cursor = gdk_cursor_new (icon);
		}

	gdk_window_set_cursor(widget->window, cursor);

	if (cursor) gdk_cursor_unref(cursor);
}

/*
 *-------------------------------------------------------------------
 * image pixbuf handling
 *-------------------------------------------------------------------
 */

static void image_zoom_sync(ImageWindow *imd, gdouble zoom,
			    gint force, gint blank, gint new,
			    gint center_point, gint px, gint py)
{
	gdouble old_scale;
	gint old_cx, old_cy;
	gint clamped;
	gint sized;

	old_scale = imd->scale;
	if (center_point)
		{
		px = CLAMP(px, 0, imd->width);
		py = CLAMP(py, 0, imd->height);
		old_cx = imd->x_scroll + (px - imd->x_offset);
		old_cy = imd->y_scroll + (py - imd->y_offset);
		}
	else
		{
		px = py = 0;
		old_cx = imd->x_scroll + imd->vis_width / 2;
		old_cy = imd->y_scroll + imd->vis_height / 2;
		}

	if (!image_zoom_clamp(imd, zoom, force, new)) return;

	clamped = image_size_clamp(imd);
	sized = image_size_top_window(imd, imd->width, imd->height);

	if (force)
		{
		/* force means new image, so update scroll offset per options */
		switch (scroll_reset_method)
			{
			case SCROLL_RESET_NOCHANGE:
				/* maintain old scroll position, do nothing */
				break;
			case SCROLL_RESET_CENTER:
				/* center new image */
				imd->x_scroll = ((double)imd->image_width / 2.0 * imd->scale) - imd->vis_width / 2;
				imd->y_scroll = ((double)imd->image_height / 2.0 * imd->scale) - imd->vis_height / 2;
				break;
			case SCROLL_RESET_TOPLEFT:
			default:
				/* reset to upper left */
				imd->x_scroll = 0;
				imd->y_scroll = 0;
				break;
			}
		}
	else
		{
		/* user zoom does not force, so keep visible center point */
		if (center_point)
			{
			imd->x_scroll = old_cx / old_scale * imd->scale - (px - imd->x_offset);
			imd->y_scroll = old_cy / old_scale * imd->scale - (py - imd->y_offset);
			}
		else
			{
			imd->x_scroll = old_cx / old_scale * imd->scale - (imd->vis_width / 2);
			imd->y_scroll = old_cy / old_scale * imd->scale - (imd->vis_height / 2);
			}
		}
	image_scroll_clamp(imd);

	image_tile_sync(imd, imd->width, imd->height, blank);

	/* If the window was not sized, redraw the image - we know there will be no size/expose signal.
	 * But even if a size is claimed, there is no guarantee that the window manager will allow it,
	 * so redraw the window anyway :/
	 */
	if (sized || clamped) image_border_clear(imd);
	image_redraw(imd, FALSE);

	if (imd->title_show_zoom) image_update_title(imd);
	image_update_util(imd);
}

static void image_pixbuf_sync(ImageWindow *imd, gdouble zoom, gint blank, gint new)
{
	if (!imd->pixbuf)
		{
		/* no pixbuf so just clear the window */
		imd->image_width = 0;
		imd->image_height = 0;
		imd->scale = 1.0;

		if (imd->image->window)
			{
			gdk_window_clear(imd->image->window);
			image_overlay_draw(imd, 0, 0, imd->window_width, imd->window_height);
			}

		image_update_util(imd);
		
		return;
		}

	imd->image_width = gdk_pixbuf_get_width(imd->pixbuf);
	imd->image_height = gdk_pixbuf_get_height(imd->pixbuf);

#if 0
	/* reset scrolling */
	imd->x_scroll = 0;
	imd->y_scroll = 0;
#endif

	image_zoom_sync(imd, zoom, TRUE, blank, new, FALSE, 0, 0);
}

static void image_set_pixbuf(ImageWindow *imd, GdkPixbuf *pixbuf, gdouble zoom, gint new)
{
	if (pixbuf) g_object_ref(pixbuf);
	if (imd->pixbuf) g_object_unref(imd->pixbuf);
	imd->pixbuf = pixbuf;

	image_pixbuf_sync(imd, zoom, FALSE, new);
}

static void image_alter_real(ImageWindow *imd, AlterType type, gint clamp)
{
	GdkPixbuf *new = NULL;
	gint x, y;
	gint t;

	imd->delay_alter_type = ALTER_NONE;

	if (!imd->pixbuf) return;

	x = imd->x_scroll + (imd->vis_width / 2);
	y = imd->y_scroll + (imd->vis_height / 2);

	switch (type)
		{
		case ALTER_ROTATE_90:
			new = pixbuf_copy_rotate_90(imd->pixbuf, FALSE);
			t = x;
			x = imd->height - y;
			y = t;
			break;
		case ALTER_ROTATE_90_CC:
			new = pixbuf_copy_rotate_90(imd->pixbuf, TRUE);
			t = x;
			x = y;
			y = imd->width - t;
			break;
		case ALTER_ROTATE_180:
			new = pixbuf_copy_mirror(imd->pixbuf, TRUE, TRUE);
			x = imd->width - x;
			y = imd->height - y;
			break;
		case ALTER_MIRROR:
			new = pixbuf_copy_mirror(imd->pixbuf, TRUE, FALSE);
			x = imd->width - x;
			break;
		case ALTER_FLIP:
			new = pixbuf_copy_mirror(imd->pixbuf, FALSE, TRUE);
			y = imd->height - y;
			break;
		case ALTER_NONE:
		default:
			return;
			break;
		}

	if (!new) return;

	if (clamp)
		{
		image_set_pixbuf(imd, new, imd->zoom, TRUE);
		g_object_unref(new);

		if (imd->zoom != 0.0)
			{
			image_scroll(imd, x - (imd->vis_width / 2), y - (imd->vis_height / 2));
			}
		}
	else
		{
		g_object_unref(imd->pixbuf);
		imd->pixbuf = new;
		}
}

static void image_post_process(ImageWindow *imd, gint clamp)
{
	if (exif_rotate_enable && imd->pixbuf)
		{
		ExifData *ed;
		gint orientation;

		ed = exif_read(imd->image_path);
		if (ed && exif_get_integer(ed, "Orientation", &orientation))
			{
			/* see http://jpegclub.org/exif_orientation.html 
			  1        2       3      4         5            6           7          8

			888888  888888      88  88      8888888888  88                  88  8888888888
			88          88      88  88      88  88      88  88          88  88      88  88
			8888      8888    8888  8888    88          8888888888  8888888888          88
			88          88      88  88
			88          88  888888  888888
			*/

			switch (orientation)
				{
				case EXIF_ORIENTATION_TOP_LEFT:
					/* normal -- nothing to do */
					break;
				case EXIF_ORIENTATION_TOP_RIGHT:
					/* mirrored */
					imd->delay_alter_type = ALTER_MIRROR;
					break;
				case EXIF_ORIENTATION_BOTTOM_RIGHT:
					/* upside down */
					imd->delay_alter_type = ALTER_ROTATE_180;
					break;
				case EXIF_ORIENTATION_BOTTOM_LEFT:
					/* flipped */
					imd->delay_alter_type = ALTER_FLIP;
					break;
				case EXIF_ORIENTATION_LEFT_TOP:
					/* not implemented -- too wacky to fix in one step */
					break;
				case EXIF_ORIENTATION_RIGHT_TOP:
					/* rotated -90 (270) */
					imd->delay_alter_type = ALTER_ROTATE_90;
					break;
				case EXIF_ORIENTATION_RIGHT_BOTTOM:
					/* not implemented -- too wacky to fix in one step */
					break;
				case EXIF_ORIENTATION_LEFT_BOTTOM:
					/* rotated 90 */
					imd->delay_alter_type = ALTER_ROTATE_90_CC;
					break;
				default:
					/* The other values are out of range */
					break;
				}
			}
		exif_free(ed);
		}

	if (imd->delay_alter_type != ALTER_NONE)
		{
		image_alter_real(imd, imd->delay_alter_type, clamp);
		}
}

/*
 *-------------------------------------------------------------------
 * read ahead (prebuffer)
 *-------------------------------------------------------------------
 */

static void image_read_ahead_cancel(ImageWindow *imd)
{
	if (debug) printf("read ahead cancelled for :%s\n", imd->read_ahead_path);

	image_loader_free(imd->read_ahead_il);
	imd->read_ahead_il = NULL;

	if (imd->read_ahead_pixbuf) g_object_unref(imd->read_ahead_pixbuf);
	imd->read_ahead_pixbuf = NULL;

	g_free(imd->read_ahead_path);
	imd->read_ahead_path = NULL;
}

static void image_read_ahead_done_cb(ImageLoader *il, gpointer data)
{
	ImageWindow *imd = data;

	if (debug) printf("read ahead done for :%s\n", imd->read_ahead_path);

	imd->read_ahead_pixbuf = image_loader_get_pixbuf(imd->read_ahead_il);
	if (imd->read_ahead_pixbuf)
		{
		g_object_ref(imd->read_ahead_pixbuf);
		}
	else
		{
		imd->read_ahead_pixbuf = pixbuf_inline(PIXBUF_INLINE_BROKEN);
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
	if (!imd->read_ahead_path || imd->read_ahead_il || imd->read_ahead_pixbuf) return;

	/* still loading ?, do later */
	if (imd->il) return;

	if (debug) printf("read ahead started for :%s\n", imd->read_ahead_path);

	imd->read_ahead_il = image_loader_new(imd->read_ahead_path);

	image_loader_set_error_func(imd->read_ahead_il, image_read_ahead_error_cb, imd);
	if (!image_loader_start(imd->read_ahead_il, image_read_ahead_done_cb, imd))
		{
		image_read_ahead_cancel(imd);
		image_complete_util(imd, TRUE);
		}
}

static void image_read_ahead_set(ImageWindow *imd, const gchar *path)
{
	if (imd->read_ahead_path && path && strcmp(imd->read_ahead_path, path) == 0) return;

	image_read_ahead_cancel(imd);

	imd->read_ahead_path = g_strdup(path);

	if (debug) printf("read ahead set to :%s\n", imd->read_ahead_path);

	image_read_ahead_start(imd);
}

/*
 *-------------------------------------------------------------------
 * post buffering
 *-------------------------------------------------------------------
 */

static void image_post_buffer_set(ImageWindow *imd, const gchar *path, GdkPixbuf *pixbuf)
{
	g_free(imd->prev_path);
	if (imd->prev_pixbuf) g_object_unref(imd->prev_pixbuf);

	if (path && pixbuf)
		{
		imd->prev_path = g_strdup(path);
			
		g_object_ref(pixbuf);
		imd->prev_pixbuf = pixbuf;
		}
	else
		{
		imd->prev_path = NULL;
		imd->prev_pixbuf = NULL;
		}

	if (debug) printf("post buffer set: %s\n", path);
}

static gint image_post_buffer_get(ImageWindow *imd)
{
	gint success;

	if (imd->prev_pixbuf &&
	    imd->image_path && imd->prev_path && strcmp(imd->image_path, imd->prev_path) == 0)
		{
		if (imd->pixbuf) g_object_unref(imd->pixbuf);
		imd->pixbuf = imd->prev_pixbuf;
		success = TRUE;
		}
	else
		{
		if (imd->prev_pixbuf) g_object_unref(imd->prev_pixbuf);
		success = FALSE;
		}

	imd->prev_pixbuf = NULL;

	g_free(imd->prev_path);
	imd->prev_path = NULL;

	return success;
}

/*
 *-------------------------------------------------------------------
 * loading
 *-------------------------------------------------------------------
 */

static void image_load_pixbuf_ready(ImageWindow *imd)
{
	if (imd->pixbuf || !imd->il) return;

	imd->pixbuf = image_loader_get_pixbuf(imd->il);

	if (imd->pixbuf) g_object_ref(imd->pixbuf);

	image_pixbuf_sync(imd, imd->zoom, TRUE, TRUE);
}

static void image_load_area_cb(ImageLoader *il, guint x, guint y, guint w, guint h, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->delay_flip &&
	    imd->pixbuf != image_loader_get_pixbuf(il))
		{
		return;
		}

	if (!imd->pixbuf) image_load_pixbuf_ready(imd);

	if (imd->scale != 1.0)
		{
		x = (guint) floor((double)x * imd->scale);
		y = (guint) floor((double)y * imd->scale);
		w = (guint) ceil((double)w * imd->scale);
		h = (guint) ceil((double)h * imd->scale);

		if (w == 0) w = 1;
		if (h == 0) h = 1;

		if ((GdkInterpType)zoom_quality != GDK_INTERP_NEAREST)
			{
			/* some scaling types use surrounding pixels to smooth the image,
			 * this will expand the new area to cover up for faint black
			 * lines caused by previous renders with non-complete image
			 */
			y -= 1;
			h += 2;
			}

		}

	image_queue(imd, (gint) x, (gint) y, (gint) w, (gint) h, FALSE, TILE_RENDER_AREA, TRUE);
}

static void image_load_done_cb(ImageLoader *il, gpointer data)
{
	ImageWindow *imd = data;

	if (debug) printf ("image done\n");

	if (imd->delay_flip &&
	    imd->pixbuf != image_loader_get_pixbuf(imd->il))
		{
		if (imd->pixbuf) g_object_unref(imd->pixbuf);
		imd->pixbuf = image_loader_get_pixbuf(imd->il);
		if (imd->pixbuf) g_object_ref(imd->pixbuf);
		image_pixbuf_sync(imd, imd->zoom, FALSE, TRUE);
		}

	image_loader_free(imd->il);
	imd->il = NULL;

	image_post_process(imd, TRUE);

	image_read_ahead_start(imd);
}

static void image_load_error_cb(ImageLoader *il, gpointer data)
{
	if (debug) printf ("image error\n");

	/* even on error handle it like it was done,
	 * since we have a pixbuf with _something_ */

	image_load_done_cb(il, data);
}

#ifdef IMAGE_THROTTLE_LARGER_IMAGES
static void image_load_buffer_throttle(ImageLoader *il)
{
	if (!il || il->bytes_total < IMAGE_THROTTLE_THRESHOLD) return;

	/* Larger image files usually have larger chunks of data per pixel...
	 * So increase the buffer read size so that the rendering chunks called
	 * are also larger.
	 */

	image_loader_set_buffer_size(il, IMAGE_LOAD_BUFFER_COUNT * IMAGE_THROTTLE_FACTOR);
}
#endif

/* this read ahead is located here merely for the callbacks, above */

static gint image_read_ahead_check(ImageWindow *imd)
{
	if (!imd->read_ahead_path) return FALSE;
	if (imd->il) return FALSE;

	if (!imd->image_path || strcmp(imd->read_ahead_path, imd->image_path) != 0)
		{
		image_read_ahead_cancel(imd);
		return FALSE;
		}

	if (imd->read_ahead_il)
		{
		imd->il = imd->read_ahead_il;
		imd->read_ahead_il = NULL;

		/* override the old signals */
		image_loader_set_area_ready_func(imd->il, image_load_area_cb, imd);
		image_loader_set_error_func(imd->il, image_load_error_cb, imd);
		image_loader_set_buffer_size(imd->il, IMAGE_LOAD_BUFFER_COUNT);

#ifdef IMAGE_THROTTLE_LARGER_IMAGES
		image_load_buffer_throttle(imd->il);
#endif

		/* do this one directly (probably should add a set func) */
		imd->il->func_done = image_load_done_cb;

		if (!imd->delay_flip)
			{
			if (imd->pixbuf) g_object_unref(imd->pixbuf);
			imd->pixbuf = image_loader_get_pixbuf(imd->il);
			if (imd->pixbuf) g_object_ref(imd->pixbuf);
			}

		image_read_ahead_cancel(imd);
		return TRUE;
		}
	else if (imd->read_ahead_pixbuf)
		{
		if (imd->pixbuf) g_object_unref(imd->pixbuf);
		imd->pixbuf = imd->read_ahead_pixbuf;
		imd->read_ahead_pixbuf = NULL;

		image_read_ahead_cancel(imd);

		image_post_process(imd, FALSE);
		return TRUE;
		}

	image_read_ahead_cancel(imd);
	return FALSE;
}

static gint image_load_begin(ImageWindow *imd, const gchar *path)
{
	if (debug) printf ("image begin \n");

	if (imd->il) return FALSE;

	imd->completed = FALSE;

	if (image_post_buffer_get(imd))
		{
		if (debug) printf("from post buffer: %s\n", imd->image_path);

		image_pixbuf_sync(imd, imd->zoom, FALSE, TRUE);
		return TRUE;
		}

	if (image_read_ahead_check(imd))
		{
		if (debug) printf("from read ahead buffer: %s\n", imd->image_path);

		if (!imd->delay_flip || !imd->il) image_pixbuf_sync(imd, imd->zoom, FALSE, TRUE);
		return TRUE;
		}

	if (!imd->delay_flip && imd->pixbuf)
		{
		g_object_unref(imd->pixbuf);
		imd->pixbuf = NULL;
		}

	imd->il = image_loader_new(path);

	image_loader_set_area_ready_func(imd->il, image_load_area_cb, imd);
	image_loader_set_error_func(imd->il, image_load_error_cb, imd);
	image_loader_set_buffer_size(imd->il, IMAGE_LOAD_BUFFER_COUNT);

	if (!image_loader_start(imd->il, image_load_done_cb, imd))
		{
		if (debug) printf("image start error\n");

		image_loader_free(imd->il);
		imd->il = NULL;

		image_complete_util(imd, FALSE);

		return FALSE;
		}

#ifdef IMAGE_THROTTLE_LARGER_IMAGES
	image_load_buffer_throttle(imd->il);
#endif

	if (!imd->delay_flip && !imd->pixbuf) image_load_pixbuf_ready(imd);

	return TRUE;
}

static void image_reset(ImageWindow *imd)
{
	/* stops anything currently being done */

	if (debug) printf("image reset\n");

	image_loader_free(imd->il);
	imd->il = NULL;

	image_queue_clear(imd);
	imd->delay_alter_type = ALTER_NONE;
}

/*
 *-------------------------------------------------------------------
 * image changer
 *-------------------------------------------------------------------
 */

static void image_change_complete(ImageWindow *imd, gdouble zoom, gint new)
{
	gint sync = TRUE;

	image_source_tile_unset(imd);

	imd->zoom = zoom;	/* store the zoom, needed by the loader */

	image_reset(imd);

	if (imd->image_path && isfile(imd->image_path))
		{
		if (image_load_begin(imd, imd->image_path))
			{
			imd->unknown = FALSE;
			sync = FALSE;
			}
		else
			{
			if (imd->pixbuf) g_object_unref(imd->pixbuf);
			imd->pixbuf = pixbuf_inline(PIXBUF_INLINE_BROKEN);
			imd->unknown = TRUE;
			}
		imd->size = filesize(imd->image_path);
		imd->mtime = filetime(imd->image_path);
		}
	else
		{
		if (imd->pixbuf) g_object_unref(imd->pixbuf);
		imd->pixbuf = NULL;

		if (imd->image_path)
			{
			imd->pixbuf = pixbuf_inline(PIXBUF_INLINE_BROKEN);
			imd->mtime = filetime(imd->image_path);
			}
		else
			{
			imd->pixbuf = NULL;
			imd->mtime = 0;
			}
		imd->unknown = TRUE;
		imd->size = 0;
		}

	if (sync)
		{
		image_pixbuf_sync(imd, zoom, FALSE, new);
		}
	else
		{
		image_update_util(imd);
		}
}

static void image_change_real(ImageWindow *imd, const gchar *path,
			      CollectionData *cd, CollectInfo *info, gdouble zoom)
{
	GdkPixbuf *prev_pixbuf = NULL;
	gchar *prev_path = NULL;
	gint prev_clear = FALSE;

	imd->collection = cd;
	imd->collection_info = info;

	if (enable_read_ahead && imd->image_path && imd->pixbuf)
		{
		if (imd->il)
			{
			/* current image is not finished */
			prev_clear = TRUE;
			}
		else
			{
			prev_path = g_strdup(imd->image_path);
			prev_pixbuf = imd->pixbuf;
			g_object_ref(prev_pixbuf);
			}
		}

	g_free(imd->image_path);
	imd->image_path = g_strdup(path);
	imd->image_name = filename_from_path(imd->image_path);

	image_change_complete(imd, zoom, TRUE);

	if (prev_pixbuf)
		{
		image_post_buffer_set(imd, prev_path, prev_pixbuf);
		g_free(prev_path);
		g_object_unref(prev_pixbuf);
		}
	else if (prev_clear)
		{
		image_post_buffer_set(imd, NULL, NULL);
		}

	image_update_title(imd);
	image_new_util(imd);
}

/*
 *-------------------------------------------------------------------
 * callbacks
 *-------------------------------------------------------------------
 */

static gint image_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	gint x, y;

	ImageWindow *imd = data;

	image_border_draw(imd, event->area.x, event->area.y,
			  event->area.width, event->area.height);

	/* image */
	x = MAX(0, (gint)event->area.x - imd->x_offset + imd->x_scroll);
	y = MAX(0, (gint)event->area.y - imd->y_offset + imd->y_scroll);

	image_queue(imd, x, y,
		    MIN((gint)event->area.width, imd->width - x),
		    MIN((gint)event->area.height, imd->height - y),
		    FALSE, TILE_RENDER_ALL, FALSE);

	return TRUE;
}

static void image_size_cb(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	ImageWindow *imd = data;

	image_size_sync(imd, allocation->width, allocation->height);
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
		gtk_paint_focus (widget->style, widget->window, GTK_STATE_ACTIVE,
				 area, widget, "image_window",
				 widget->allocation.x, widget->allocation.y,
				 widget->allocation.width - 1, widget->allocation.height - 1);	
		}
	else
		{
		gtk_paint_shadow (widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_IN,
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


/*
 *-------------------------------------------------------------------
 * overlays
 *-------------------------------------------------------------------
 */

static void image_overlay_draw(ImageWindow *imd, gint x, gint y, gint w, gint h)
{
	GList *work;

	work = imd->overlay_list;
	while (work)
		{
		OverlayData *od;
		gint px, py, pw, ph;
		gint rx, ry, rw, rh;

		od = work->data;
		work = work->next;

		if (!od->visible) continue;

		pw = gdk_pixbuf_get_width(od->pixbuf);
		ph = gdk_pixbuf_get_height(od->pixbuf);
		px = od->x;
		py = od->y;

	        if (od->relative)
			{
			if (px < 0) px = imd->window_width - pw + px;
			if (py < 0) py = imd->window_height - ph + py;
			}

		if (util_clip_region(x, y, w, h, px, py, pw, ph, &rx, &ry, &rw, &rh))
			{
			gdk_draw_pixbuf(imd->image->window,
					imd->image->style->fg_gc[GTK_WIDGET_STATE(imd->image)],
					od->pixbuf,
					rx - px, ry - py,
					rx, ry, rw, rh,
					(GdkRgbDither)dither_quality, rx, ry);
			}
		}
}

static void image_overlay_queue_draw(ImageWindow *imd, OverlayData *od, gint hidden)
{
	gint x, y, w, h;
	gint old_vis;

	w = gdk_pixbuf_get_width(od->pixbuf);
	h = gdk_pixbuf_get_height(od->pixbuf);
	x = od->x;
	y = od->y;

	if (od->relative)
		{
		if (x < 0) x = imd->window_width - w + x;
		if (y < 0) y = imd->window_height - h + y;
		}

	image_queue(imd, imd->x_scroll - imd->x_offset + x,
			 imd->y_scroll - imd->y_offset + y,
			 w, h,
			 FALSE, TILE_RENDER_ALL, FALSE);

	old_vis = od->visible;
	if (hidden) od->visible = FALSE;
	image_border_draw(imd, x, y, w, h);
	od->visible = old_vis;
}

static void image_overlay_queue_all(ImageWindow *imd)
{
	GList *work;

	work = imd->overlay_list;
	while (work)
		{
		OverlayData *od = work->data;
		work = work->next;

		image_overlay_queue_draw(imd, od, FALSE);
		}
}

static OverlayData *image_overlay_find(ImageWindow *imd, gint id)
{
	GList *work;

	work = imd->overlay_list;
	while (work)
		{
		OverlayData *od = work->data;
		work = work->next;

		if (od->id == id) return od;
		}

	return NULL;
}

gint image_overlay_add(ImageWindow *imd, GdkPixbuf *pixbuf, gint x, gint y,
		       gint relative, gint always)
{
	OverlayData *od;
	gint id;

	if (!imd || !pixbuf) return -1;

	id = 1;
	while (image_overlay_find(imd, id)) id++;

	od = g_new0(OverlayData, 1);
	od->id = id;
	od->pixbuf = pixbuf;
	g_object_ref(G_OBJECT(od->pixbuf));
	od->x = x;
	od->y = y;
	od->relative = relative;
	od->visible = TRUE;
	od->always = always;

	imd->overlay_list = g_list_append(imd->overlay_list, od);

	image_overlay_queue_draw(imd, od, FALSE);

	return od->id;
}

static void image_overlay_free(ImageWindow *imd, OverlayData *od)
{
	imd->overlay_list = g_list_remove(imd->overlay_list, od);

	if (od->pixbuf) g_object_unref(G_OBJECT(od->pixbuf));
	g_free(od);
}

static void image_overlay_list_clear(ImageWindow *imd)
{
	while (imd->overlay_list)
		{
		OverlayData *od;

		od = imd->overlay_list->data;
		image_overlay_free(imd, od);
		}
}

void image_overlay_set(ImageWindow *imd, gint id, GdkPixbuf *pixbuf, gint x, gint y)
{
	OverlayData *od;

	if (!imd) return;

	od = image_overlay_find(imd, id);
	if (!od) return;

	if (pixbuf)
		{
		image_overlay_queue_draw(imd, od, TRUE);

		g_object_ref(G_OBJECT(pixbuf));
		g_object_unref(G_OBJECT(od->pixbuf));
		od->pixbuf = pixbuf;

		od->x = x;
		od->y = y;

		image_overlay_queue_draw(imd, od, FALSE);
		}
	else
		{
		image_overlay_queue_draw(imd, od, TRUE);
		image_overlay_free(imd, od);
		}
}

gint image_overlay_get(ImageWindow *imd, gint id, GdkPixbuf **pixbuf, gint *x, gint *y)
{
	OverlayData *od;

	if (!imd) return FALSE;

	od = image_overlay_find(imd, id);
	if (!od) return FALSE;

	if (pixbuf) *pixbuf = od->pixbuf;
	if (x) *x = od->x;
	if (y) *y = od->y;

	return TRUE;
}

void image_overlay_remove(ImageWindow *imd, gint id)
{
	image_overlay_set(imd, id, NULL, 0, 0);
}

/*
 *-------------------------------------------------------------------
 * scroller
 *-------------------------------------------------------------------
 */

#define SCROLLER_UPDATES_PER_SEC 30
#define SCROLLER_DEAD_ZONE 6


static gboolean image_scroller_update_cb(gpointer data)
{
	ImageWindow *imd = data;
	gint x, y;
	gint xinc, yinc;

	/* this was a simple scroll by difference between scroller and mouse position,
	 * but all this math results in a smoother result and accounts for a dead zone.
	 */

	if (abs(imd->scroller_xpos - imd->scroller_x) < SCROLLER_DEAD_ZONE)
		{
		x = 0;
		}
	else
		{
		gint shift = SCROLLER_DEAD_ZONE / 2 * SCROLLER_UPDATES_PER_SEC;
		x = (imd->scroller_xpos - imd->scroller_x) / 2 * SCROLLER_UPDATES_PER_SEC;
		x += (x > 0) ? -shift : shift;
		}

	if (abs(imd->scroller_ypos - imd->scroller_y) < SCROLLER_DEAD_ZONE)
		{
		y = 0;
		}
	else
		{
		gint shift = SCROLLER_DEAD_ZONE / 2 * SCROLLER_UPDATES_PER_SEC;
		y = (imd->scroller_ypos - imd->scroller_y) / 2 * SCROLLER_UPDATES_PER_SEC;
		y += (y > 0) ? -shift : shift;
		}

	if (abs(x) < SCROLLER_DEAD_ZONE * SCROLLER_UPDATES_PER_SEC)
		{
		xinc = x;
		}
	else
		{
		xinc = imd->scroller_xinc;

		if (x >= 0)
			{
			if (xinc < 0) xinc = 0;
			if (x < xinc) xinc = x;
			if (x > xinc) xinc = MIN(xinc + x / SCROLLER_UPDATES_PER_SEC, x);
			}
		else
			{
			if (xinc > 0) xinc = 0;
			if (x > xinc) xinc = x;
			if (x < xinc) xinc = MAX(xinc + x / SCROLLER_UPDATES_PER_SEC, x);
			}
		}

	if (abs(y) < SCROLLER_DEAD_ZONE * SCROLLER_UPDATES_PER_SEC)
		{
		yinc = y;
		}
	else
		{
		yinc = imd->scroller_yinc;

		if (y >= 0)
			{
			if (yinc < 0) yinc = 0;
			if (y < yinc) yinc = y;
			if (y > yinc) yinc = MIN(yinc + y / SCROLLER_UPDATES_PER_SEC, y);
			}
		else
			{
			if (yinc > 0) yinc = 0;
			if (y > yinc) yinc = y;
			if (y < yinc) yinc = MAX(yinc + y / SCROLLER_UPDATES_PER_SEC, y);
			}
		}

	imd->scroller_xinc = xinc;
	imd->scroller_yinc = yinc;

	xinc = xinc / SCROLLER_UPDATES_PER_SEC;
	yinc = yinc / SCROLLER_UPDATES_PER_SEC;

	image_scroll(imd, xinc, yinc);

	return TRUE;
}

static void image_scroller_timer_set(ImageWindow *imd, gint start)
{
	if (imd->scroller_id != -1)
		{
		g_source_remove(imd->scroller_id);
		imd->scroller_id = -1;
		}

	if (start)
		{
		imd->scroller_id = g_timeout_add(1000 / SCROLLER_UPDATES_PER_SEC,
						   image_scroller_update_cb, imd);
		}
}

static void image_scroller_start(ImageWindow *imd, gint x, gint y)
{
	if (imd->scroller_overlay == -1)
		{
		GdkPixbuf *pixbuf;
		gint w, h;

		pixbuf = pixbuf_inline(PIXBUF_INLINE_SCROLLER);
		w = gdk_pixbuf_get_width(pixbuf);
		h = gdk_pixbuf_get_height(pixbuf);

		imd->scroller_overlay = image_overlay_add(imd, pixbuf, x - w / 2, y - h / 2, FALSE, TRUE);
		}

	imd->scroller_x = x;
	imd->scroller_y = y;
	imd->scroller_xpos = x;
	imd->scroller_ypos = y;

	image_scroller_timer_set(imd, TRUE);
}

static void image_scroller_stop(ImageWindow *imd)
{
	if (imd->scroller_id == -1) return;

	image_overlay_remove(imd, imd->scroller_overlay);
	imd->scroller_overlay = -1;

	image_scroller_timer_set(imd, FALSE);
}

/*
 *-------------------------------------------------------------------
 * mouse stuff
 *-------------------------------------------------------------------
 */

static gint image_mouse_motion_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->scroller_id != -1)
		{
		imd->scroller_xpos = bevent->x;
		imd->scroller_ypos = bevent->y;
		}

	if (!imd->in_drag || !gdk_pointer_is_grabbed()) return FALSE;

	if (imd->drag_moved < IMAGE_DRAG_SCROLL_THRESHHOLD)
		{
		imd->drag_moved++;
		}
	else
		{
		widget_set_cursor (imd->image, GDK_FLEUR);
		}

	/* do the scroll */
	image_scroll_real(imd, imd->drag_last_x - bevent->x, imd->drag_last_y - bevent->y);

	imd->drag_last_x = bevent->x;
	imd->drag_last_y = bevent->y;

	return FALSE;
}

static gint image_mouse_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->scroller_id != -1) return TRUE;

	switch (bevent->button)
		{
		case 1:
			imd->in_drag = TRUE;
			imd->drag_last_x = bevent->x;
			imd->drag_last_y = bevent->y;
			imd->drag_moved = 0;
			gdk_pointer_grab(imd->image->window, FALSE,
                                GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                NULL, NULL, bevent->time);
			gtk_grab_add(imd->image);
			break;
		case 2:
			imd->drag_moved = 0;
			break;
		case 3:
			image_button_do(imd, bevent);
			break;
		default:
			break;
		}

	gtk_widget_grab_focus(imd->widget);

	return FALSE;
}

static gint image_mouse_release_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->scroller_id != -1)
		{
		image_scroller_stop(imd);
		return TRUE;
		}

	if (gdk_pointer_is_grabbed() && GTK_WIDGET_HAS_GRAB(imd->image))
		{
		gtk_grab_remove(imd->image);
		gdk_pointer_ungrab(bevent->time);
		widget_set_cursor(imd->image, -1);
		}

	if (bevent->button == 1 && (bevent->state & GDK_SHIFT_MASK))
		{
		image_scroller_start(imd, bevent->x, bevent->y);
		}
	else if (bevent->button == 1 || bevent->button == 2)
		{
		if (imd->drag_moved < IMAGE_DRAG_SCROLL_THRESHHOLD) image_button_do(imd, bevent);
		}

	imd->in_drag = FALSE;

	return FALSE;
}

static gint image_mouse_leave_cb(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->scroller_id != -1)
		{
		imd->scroller_xpos = imd->scroller_x;
		imd->scroller_ypos = imd->scroller_y;
		imd->scroller_xinc = 0;
		imd->scroller_yinc = 0;
		}

	return FALSE;
}

static gint image_scroll_cb(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->func_scroll &&
	    event && event->type == GDK_SCROLL)
	        {
		imd->func_scroll(imd, event->direction, event->time,
				 event->x, event->y, event->state, imd->data_scroll);
		return TRUE;
		}

	return FALSE;
}

static void image_mouse_drag_cb(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	ImageWindow *imd = data;

	imd->drag_moved = IMAGE_DRAG_SCROLL_THRESHHOLD;
}

/*
 *-------------------------------------------------------------------
 * drag and drop
 *-------------------------------------------------------------------
 */

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
			     void (*func)(ImageWindow *, gint preload, gpointer),
			     gpointer data)
{
	imd->func_complete = func;
	imd->data_complete = data;
}

void image_set_new_func(ImageWindow *imd,
			void (*func)(ImageWindow *, gpointer),
			gpointer data)
{
	imd->func_new = func;
	imd->data_new = data;
}


static void image_button_do(ImageWindow *imd, GdkEventButton *bevent)
{
	if (imd->func_button &&
	    bevent &&
	    (bevent->type == GDK_BUTTON_PRESS || bevent->type == GDK_BUTTON_RELEASE))
		{
		imd->func_button(imd, bevent->button, bevent->time,
				 bevent->x, bevent->y, bevent->state, imd->data_button);
		}
}

void image_set_button_func(ImageWindow *imd,
			   void (*func)(ImageWindow *, gint button, guint32 time, gdouble x, gdouble y, guint state, gpointer),
			   gpointer data)
{
	imd->func_button = func;
	imd->data_button = data;
}

void image_set_scroll_func(ImageWindow *imd,
			   void (*func)(ImageWindow *, GdkScrollDirection direction, guint32 time, gdouble x, gdouble y, guint state, gpointer),
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
	return imd->image_path;
}

const gchar *image_get_name(ImageWindow *imd)
{
	return imd->image_name;
}

/* merely changes path string, does not change the image! */
void image_set_path(ImageWindow *imd, const gchar *newpath)
{
	g_free(imd->image_path);
	imd->image_path = g_strdup(newpath);
	imd->image_name = filename_from_path(imd->image_path);

	image_update_title(imd);
	image_new_util(imd);
}

/* load a new image */

void image_change_path(ImageWindow *imd, const gchar *path, gdouble zoom)
{
	if (imd->image_path == path ||
	    (path && imd->image_path && !strcmp(path, imd->image_path)) ) return;

	image_source_tile_unset(imd);

	image_change_real(imd, path, NULL, NULL, zoom);
}

void image_change_pixbuf(ImageWindow *imd, GdkPixbuf *pixbuf, gdouble zoom)
{
	image_source_tile_unset(imd);

	image_set_pixbuf(imd, pixbuf, zoom, TRUE);
	image_new_util(imd);
}

void image_change_from_collection(ImageWindow *imd, CollectionData *cd, CollectInfo *info, gdouble zoom)
{
	if (!cd || !info || !g_list_find(cd->list, info)) return;

	image_source_tile_unset(imd);

	image_change_real(imd, info->path, cd, info, zoom);
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

static void image_loader_sync_data(ImageLoader *il, gpointer data)
{
	/* change data for the callbacks directly */

	il->data_area_ready = data;
	il->data_error = data;
	il->data_done = data;
	il->data_percent = data;
}

/* this is more like a move function
 * it moves most data from source to imd, source does keep a ref on the pixbuf
 */
void image_change_from_image(ImageWindow *imd, ImageWindow *source)
{
	if (imd == source) return;

	imd->zoom_min = source->zoom_min;
	imd->zoom_max = source->zoom_max;

	imd->unknown = source->unknown;

	image_set_pixbuf(imd, source->pixbuf, image_zoom_get(source), TRUE);

	imd->collection = source->collection;
	imd->collection_info = source->collection_info;
	imd->size = source->size;
	imd->mtime = source->mtime;

	image_set_path(imd, image_get_path(source));

	image_loader_free(imd->il);
	imd->il = NULL;

	if (imd->pixbuf && source->il)
		{
		imd->il = source->il;
		source->il = NULL;

		image_loader_sync_data(imd->il, imd);

		imd->delay_alter_type = source->delay_alter_type;
		source->delay_alter_type = ALTER_NONE;
		}

	image_loader_free(imd->read_ahead_il);
	imd->read_ahead_il = source->read_ahead_il;
	source->read_ahead_il = NULL;
	if (imd->read_ahead_il) image_loader_sync_data(imd->read_ahead_il, imd);

	if (imd->read_ahead_pixbuf) g_object_unref(imd->read_ahead_pixbuf);
	imd->read_ahead_pixbuf = source->read_ahead_pixbuf;
	source->read_ahead_pixbuf = NULL;

	g_free(imd->read_ahead_path);
	imd->read_ahead_path = source->read_ahead_path;
	source->read_ahead_path = NULL;

	if (imd->prev_pixbuf) g_object_unref(imd->prev_pixbuf);
	imd->prev_pixbuf = source->prev_pixbuf;
	source->prev_pixbuf = NULL;

	g_free(imd->prev_path);
	imd->prev_path = source->prev_path;
	source->prev_path = NULL;

	imd->completed = source->completed;

	imd->x_scroll = source->x_scroll;
	imd->y_scroll = source->y_scroll;

	if (imd->source_tiles_enabled)
		{
		image_source_tile_unset(imd);
		}

	if (source->source_tiles_enabled)
		{
		imd->source_tiles_enabled = source->source_tiles_enabled;
		imd->source_tiles_cache_size = source->source_tiles_cache_size;
		imd->source_tiles = source->source_tiles;
		imd->source_tile_width = source->source_tile_width;
		imd->source_tile_height = source->source_tile_height;

		source->source_tiles_enabled = FALSE;
		source->source_tiles = NULL;

		imd->func_tile_request = source->func_tile_request;
		imd->func_tile_dispose = source->func_tile_dispose;
		imd->data_tile = source->data_tile;

		source->func_tile_request = NULL;
		source->func_tile_dispose = NULL;
		source->data_tile = NULL;

		imd->image_width = source->image_width;
		imd->image_height = source->image_height;

		if (image_zoom_clamp(imd, source->zoom, TRUE, TRUE))
			{
			image_size_clamp(imd);
			image_scroll_clamp(imd);
			image_tile_sync(imd, imd->width, imd->height, FALSE);
			image_redraw(imd, FALSE);
			}
		return;
		}

	image_scroll_clamp(imd);
}

/* manipulation */

void image_area_changed(ImageWindow *imd, gint x, gint y, gint width, gint height)
{
	gint sx, sy, sw, sh;

	sx = (gint)floor((double)x * imd->scale);
	sy = (gint)floor((double)y * imd->scale);
	sw = (gint)ceil((double)width * imd->scale);
	sh = (gint)ceil((double)height * imd->scale);

	if (imd->source_tiles_enabled)
		{
		source_tile_changed(imd, x, y, width, height);
		}

	image_queue(imd, sx, sy, sw, sh, FALSE, TILE_RENDER_AREA, TRUE);
}

void image_reload(ImageWindow *imd)
{
	if (imd->source_tiles_enabled) return;

	image_change_complete(imd, imd->zoom, FALSE);
}

void image_scroll(ImageWindow *imd, gint x, gint y)
{
	image_scroll_real(imd, x, y);
}

void image_scroll_to_point(ImageWindow *imd, gint x, gint y,
			   gdouble x_align, gdouble y_align)
{
	gint px, py;
	gint ax, ay;

	x_align = CLAMP(x_align, 0.0, 1.0);
	y_align = CLAMP(y_align, 0.0, 1.0);

	ax = (gdouble)imd->vis_width * x_align;
	ay = (gdouble)imd->vis_height * y_align;

	px = (gdouble)x * imd->scale - (imd->x_scroll + ax);
	py = (gdouble)y * imd->scale - (imd->y_scroll + ay);

	image_scroll(imd, px, py);
}

void image_alter(ImageWindow *imd, AlterType type)
{
	if (imd->source_tiles_enabled) return;

	if (imd->il)
		{
		/* still loading, wait till done */
		imd->delay_alter_type = type;
		return;
		}

	image_alter_real(imd, type, TRUE);
}

/* zoom */

static void image_zoom_adjust_real(ImageWindow *imd, gdouble increment,
				   gint center_point, gint x, gint y)
{
	gdouble zoom = imd->zoom;

	if (increment == 0.0) return; /* avoid possible div by zero, a no-op anyway... */

	if (zoom == 0.0)
		{
		if (imd->scale < 1.0)
			{
			zoom = 0.0 - 1.0 / imd->scale;
			}
		else
			{
			zoom = imd->scale;
			}
		}

	if (increment < 0.0)
		{
		if (zoom >= 1.0 && zoom + increment < 1.0)
			{
			zoom = zoom + increment - 2.0;
			}
		else
			{
			zoom = zoom + increment;
			}
		}
	else
		{
		if (zoom <= -1.0 && zoom + increment > -1.0)
			{
			zoom = zoom + increment + 2.0;
			}
		else
			{
			zoom = zoom + increment;
			}
		}

	image_zoom_sync(imd, zoom, FALSE, FALSE, FALSE, center_point, x, y);
}

void image_zoom_adjust(ImageWindow *imd, gdouble increment)
{
	image_zoom_adjust_real(imd, increment, FALSE, 0, 0);
}

void image_zoom_adjust_at_point(ImageWindow *imd, gdouble increment, gint x, gint y)
{
	image_zoom_adjust_real(imd, increment, TRUE, x, y);
}

void image_zoom_set_limits(ImageWindow *imd, gdouble min, gdouble max)
{
	if (min > 1.0 || max < 1.0) return;
	if (min < 1.0 && min > -1.0) return;
	if (min < -200.0 || max > 200.0) return;

	imd->zoom_min = min;
	imd->zoom_max = max;
}

void image_zoom_set(ImageWindow *imd, gdouble zoom)
{
	image_zoom_sync(imd, zoom, FALSE, FALSE, FALSE, FALSE, 0, 0);
}

void image_zoom_set_fill_geometry(ImageWindow *imd, gint vertical)
{
	gdouble zoom;

	if (!imd->pixbuf || imd->image_width < 1 || imd->image_height < 1) return;

	if (vertical)
		{
		zoom = (gdouble)imd->window_height / imd->image_height;
		}
	else
		{
		zoom = (gdouble)imd->window_width / imd->image_width;
		}

	if (zoom < 1.0)
		{
		zoom = 0.0 - 1.0 / zoom;
		}

	image_zoom_set(imd, zoom);
}

gdouble image_zoom_get(ImageWindow *imd)
{
	return imd->zoom;
}

gdouble image_zoom_get_real(ImageWindow *imd)
{
	return imd->scale;
}

gchar *image_zoom_get_as_text(ImageWindow *imd)
{
	gdouble l = 1.0;
	gdouble r = 1.0;
	gint pl = 0;
	gint pr = 0;
	gchar *approx = " ";

	if (imd->zoom > 0.0)
		{
		l = imd->zoom;
		}
	else if (imd->zoom < 0.0)
		{
		r = 0.0 - imd->zoom;
		}
	else if (imd->zoom == 0.0 && imd->scale != 0.0)
		{
		if (imd->scale >= 1.0)
			{
			l = imd->scale;
			}
		else
			{
			r = 1.0 / imd->scale;
			}
		approx = " ~";
		}

	if (rint(l) != l) pl = 1;
	if (rint(r) != r) pr = 1;

	return g_strdup_printf("%.*f :%s%.*f", pl, l, approx, pr, r);
}

gdouble image_zoom_get_default(ImageWindow *imd, gint mode)
{
	gdouble zoom;

	if (mode == ZOOM_RESET_ORIGINAL)
		{
		zoom = 1.0;
		}
	else if (mode == ZOOM_RESET_FIT_WINDOW)
		{
		zoom = 0.0;
		}
	else
		{
		if (imd)
			{
			zoom = image_zoom_get(imd);
			}
		else
			{
			zoom = 1.0;
			}
		}

	return zoom;
}

/* read ahead */

void image_prebuffer_set(ImageWindow *imd, const gchar *path)
{
	if (imd->source_tiles_enabled) return;

	if (path)
		{
		image_read_ahead_set(imd, path);
		}
	else
		{
		image_read_ahead_cancel(imd);
		}
}

static gint image_auto_refresh_cb(gpointer data)
{
	ImageWindow *imd = data;
	time_t newtime;
	
	if (!imd || !imd->pixbuf ||
	    imd->il || !imd->image_path ||
	    !update_on_time_change) return TRUE;

	newtime = filetime(imd->image_path);
	if (newtime > 0 && newtime != imd->mtime)
		{
		imd->mtime = newtime;
		image_reload(imd);
		}

	return TRUE;
}

/* image auto refresh on time stamp change, in 1/1000's second, -1 disables */

void image_auto_refresh(ImageWindow *imd, gint interval)
{
	if (!imd) return;
	if (imd->source_tiles_enabled) return;

	if (imd->auto_refresh_id > -1)
		{
		g_source_remove(imd->auto_refresh_id);
		imd->auto_refresh_id = -1;
		imd->auto_refresh_interval = -1;
		}

	if (interval < 0) return;

	if (interval == 0) interval = IMAGE_AUTO_REFRESH_TIME;

	imd->auto_refresh_id = g_timeout_add((guint32)interval, image_auto_refresh_cb, imd);
	imd->auto_refresh_interval = interval;
}

/* allow top window to be resized ? */

void image_top_window_set_sync(ImageWindow *imd, gint allow_sync)
{
	imd->top_window_sync = allow_sync;
}

/* background colors */

void image_background_set_black(ImageWindow *imd, gint black)
{
	GtkStyle *style;

	if (!imd) return;

	style = gtk_style_copy(gtk_widget_get_style(imd->widget));
	g_object_ref(G_OBJECT(style));

	if (black)
		{
		style->bg[GTK_STATE_NORMAL] = style->black;
		}

	gtk_widget_set_style(imd->image, style);
	g_object_unref(G_OBJECT(style));

	if (GTK_WIDGET_VISIBLE(imd->widget)) image_border_clear(imd);
}

void image_background_set_color(ImageWindow *imd, GdkColor *color)
{
	GtkStyle *style;

	if (!imd) return;

	style = gtk_style_copy(gtk_widget_get_style(imd->widget));
	g_object_ref(G_OBJECT(style));

	if (color)
		{
		GdkColor *slot;

		slot = &style->bg[GTK_STATE_NORMAL];

		slot->red = color->red;
		slot->green = color->green;
		slot->blue = color->blue;
		}

	gtk_widget_set_style(imd->image, style);
	g_object_unref(G_OBJECT(style));

	if (GTK_WIDGET_VISIBLE(imd->widget)) image_border_clear(imd);
}

void image_set_delay_flip(ImageWindow *imd, gint delay)
{
	if (!imd ||
	    imd->delay_flip == delay) return;

	imd->delay_flip = delay;
	if (!imd->delay_flip && imd->il)
		{
		if (imd->pixbuf) g_object_unref(imd->pixbuf);
		imd->pixbuf = NULL;
		image_load_pixbuf_ready(imd);

		image_queue_clear(imd);
		image_queue(imd, 0, 0, imd->width, imd->height, FALSE, TILE_RENDER_AREA, TRUE);
		}
}

/* wallpaper util */

void image_to_root_window(ImageWindow *imd, gint scaled)
{
	GdkScreen *screen;
	GdkWindow *rootwindow;
	GdkPixmap *pixmap;
	GdkPixbuf *pb;

	if (!imd || !imd->pixbuf) return;

	screen = gtk_widget_get_screen(imd->image);
	rootwindow = gdk_screen_get_root_window(screen);
	if (gdk_drawable_get_visual(rootwindow) != gdk_visual_get_system()) return;

	if (scaled)
		{
		pb = gdk_pixbuf_scale_simple(imd->pixbuf, gdk_screen_width(), gdk_screen_height(), (GdkInterpType)zoom_quality);
		}
	else
		{
		pb = gdk_pixbuf_scale_simple(imd->pixbuf, imd->width, imd->height, (GdkInterpType)zoom_quality);
		}

	gdk_pixbuf_render_pixmap_and_mask (pb, &pixmap, NULL, 128);
	gdk_window_set_back_pixmap(rootwindow, pixmap, FALSE);
	gdk_window_clear(rootwindow);
	g_object_unref(pb);
	g_object_unref(pixmap);

	gdk_flush();
}


/*
 *-------------------------------------------------------------------
 * init / destroy
 *-------------------------------------------------------------------
 */

static void image_free(ImageWindow *imd)
{
	image_read_ahead_cancel(imd);
	image_post_buffer_set(imd, NULL, NULL);
	image_auto_refresh(imd, -1);

	g_free(imd->image_path);
	g_free(imd->title);
	g_free(imd->title_right);

	image_reset(imd);
	image_tile_sync_count(imd, 0);
	if (imd->pixbuf) g_object_unref(imd->pixbuf);

	image_scroller_timer_set(imd, FALSE);

	image_overlay_list_clear(imd);

	source_tile_free_all(imd);

	g_free(imd);
}

static void image_destroy_cb(GtkObject *widget, gpointer data)
{
	ImageWindow *imd = data;
	image_free(imd);
}

ImageWindow *image_new(gint frame)
{
	ImageWindow *imd;

	imd = g_new0(ImageWindow, 1);

	imd->zoom_min = IMAGE_ZOOM_MIN;
	imd->zoom_max = IMAGE_ZOOM_MAX;
	imd->zoom = 1.0;
	imd->scale = 1.0;

	imd->draw_idle_id = -1;

	imd->tile_width = IMAGE_TILE_SIZE;
	imd->tile_height = IMAGE_TILE_SIZE;

	imd->top_window = NULL;
	imd->title = NULL;
	imd->title_right = NULL;
	imd->title_show_zoom = FALSE;

	imd->unknown = TRUE;

	imd->pixbuf = NULL;

	imd->has_frame = frame;
	imd->top_window_sync = FALSE;

	imd->tile_cache = NULL;
	imd->tile_cache_size = 0;

	imd->delay_alter_type = ALTER_NONE;

	imd->read_ahead_il = NULL;
	imd->read_ahead_pixbuf = NULL;
	imd->read_ahead_path = NULL;

	imd->completed = FALSE;

	imd->auto_refresh_id = -1;
	imd->auto_refresh_interval = -1;

	imd->delay_flip = FALSE;

	imd->func_update = NULL;
	imd->func_complete = NULL;
	imd->func_tile_request = NULL;
	imd->func_tile_dispose = NULL;

	imd->func_button = NULL;
	imd->func_scroll = NULL;

	imd->scroller_id = -1;
	imd->scroller_overlay = -1;

	imd->source_tiles_enabled = FALSE;
	imd->source_tiles = NULL;

	imd->image = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(imd->image, FALSE);

	if (imd->has_frame)
		{
		imd->widget = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(imd->widget), GTK_SHADOW_IN);
		gtk_container_add(GTK_CONTAINER(imd->widget), imd->image);
		gtk_widget_show(imd->image);

		GTK_WIDGET_SET_FLAGS(imd->widget, GTK_CAN_FOCUS);
		g_signal_connect(G_OBJECT(imd->widget), "focus_in_event",
				 G_CALLBACK(image_focus_in_cb), imd);
		g_signal_connect(G_OBJECT(imd->widget), "focus_out_event",
				 G_CALLBACK(image_focus_out_cb), imd);

		g_signal_connect_after(G_OBJECT(imd->widget), "expose_event",
				       G_CALLBACK(image_focus_expose), imd);
		}
	else
		{
		imd->widget = imd->image;
		}

	g_signal_connect(G_OBJECT(imd->image), "motion_notify_event",
			 G_CALLBACK(image_mouse_motion_cb), imd);
	g_signal_connect(G_OBJECT(imd->image), "button_press_event",
			 G_CALLBACK(image_mouse_press_cb), imd);
	g_signal_connect(G_OBJECT(imd->image), "button_release_event",
			 G_CALLBACK(image_mouse_release_cb), imd);
	g_signal_connect(G_OBJECT(imd->image), "leave_notify_event",
			 G_CALLBACK(image_mouse_leave_cb), imd);
	gtk_widget_set_events(imd->image, GDK_POINTER_MOTION_MASK |
					  GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK |
					  GDK_LEAVE_NOTIFY_MASK);

	g_signal_connect(G_OBJECT(imd->image), "expose_event",
			 G_CALLBACK(image_expose_cb), imd);
	g_signal_connect_after(G_OBJECT(imd->image), "size_allocate",
			 G_CALLBACK(image_size_cb), imd);

	g_signal_connect(G_OBJECT(imd->image), "drag_begin",
			 G_CALLBACK(image_mouse_drag_cb), imd);
	g_signal_connect(G_OBJECT(imd->image), "scroll_event",
			 G_CALLBACK(image_scroll_cb), imd);

	g_signal_connect(G_OBJECT(imd->widget), "destroy",
			 G_CALLBACK(image_destroy_cb), imd);

	return imd;
}

