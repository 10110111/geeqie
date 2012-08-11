/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 - 2012 The Geeqie Team
 *
 * Author: John Ellis
 * Author: Vladimir Nadvornik
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "pixbuf-renderer.h"
#include "renderer-clutter.h"

#include "intl.h"
#include "layout.h"

#include <gtk/gtk.h>
#include <clutter/clutter.h>

#include <clutter-gtk/clutter-gtk.h>



#define GQ_BUILD 1

#ifdef GQ_BUILD
#include "main.h"
#include "pixbuf_util.h"
#include "exif.h"
#else
typedef enum {
	EXIF_ORIENTATION_UNKNOWN	= 0,
	EXIF_ORIENTATION_TOP_LEFT	= 1,
	EXIF_ORIENTATION_TOP_RIGHT	= 2,
	EXIF_ORIENTATION_BOTTOM_RIGHT	= 3,
	EXIF_ORIENTATION_BOTTOM_LEFT	= 4,
	EXIF_ORIENTATION_LEFT_TOP	= 5,
	EXIF_ORIENTATION_RIGHT_TOP	= 6,
	EXIF_ORIENTATION_RIGHT_BOTTOM	= 7,
	EXIF_ORIENTATION_LEFT_BOTTOM	= 8
} ExifOrientationType;
#endif


typedef struct _OverlayData OverlayData;
struct _OverlayData
{
	gint id;

	GdkPixbuf *pixbuf;
	GdkWindow *window;

	gint x;
	gint y;

	OverlayRendererFlags flags;
};

typedef struct _RendererClutter RendererClutter;

struct _RendererClutter
{
	RendererFuncs f;
	PixbufRenderer *pr;

	
	gint stereo_mode;
	gint stereo_off_x;
	gint stereo_off_y;
	
	gint x_scroll;  /* allow local adjustment and mirroring */
	gint y_scroll;
	
	GtkWidget *widget; /* widget and stage may be shared with other renderers */
	ClutterActor *stage;
	ClutterActor *texture;
	ClutterActor *group;
};

static void rc_sync_scroll(RendererClutter *rc)
{
	PixbufRenderer *pr = rc->pr;
	
	rc->x_scroll = (rc->stereo_mode & PR_STEREO_MIRROR) ? 
	               pr->width - pr->vis_width - pr->x_scroll 
	               : pr->x_scroll;
	
	rc->y_scroll = (rc->stereo_mode & PR_STEREO_FLIP) ? 
	               pr->height - pr->vis_height - pr->y_scroll 
	               : pr->y_scroll;
}

static gint rc_get_orientation(RendererClutter *rc)
{
	PixbufRenderer *pr = rc->pr;

	gint orientation = pr->orientation;
	static const gint mirror[]       = {1,   2, 1, 4, 3, 6, 5, 8, 7};
	static const gint flip[]         = {1,   4, 3, 2, 1, 8, 7, 6, 5};

	if (rc->stereo_mode & PR_STEREO_MIRROR) orientation = mirror[orientation];
	if (rc->stereo_mode & PR_STEREO_FLIP) orientation = flip[orientation];
        return orientation;
}


static void rc_sync_actor(RendererClutter *rc)
{
	PixbufRenderer *pr = rc->pr;
	gint anchor_x = 0;
	gint anchor_y = 0;
	
	rc_sync_scroll(rc);
	
	clutter_actor_set_anchor_point(CLUTTER_ACTOR(rc->texture), 0, 0);

	printf("scale %d %d\n", rc->pr->width, rc->pr->height);
	printf("pos   %d %d        %d %d\n", rc->pr->x_offset, rc->pr->y_offset, rc->x_scroll, rc->y_scroll);
	
	switch (rc_get_orientation(rc))
		{
		case EXIF_ORIENTATION_TOP_LEFT:
			/* normal  */
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						0, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						0, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->width, pr->height);
			anchor_x = 0;
			anchor_y = 0;
			break;
		case EXIF_ORIENTATION_TOP_RIGHT:
			/* mirrored */
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						0, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						180, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->width, pr->height);
			anchor_x = pr->width;
			anchor_y = 0;
			break;
		case EXIF_ORIENTATION_BOTTOM_RIGHT:
			/* upside down */
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						180, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						0, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->width, pr->height);
			anchor_x = pr->width;
			anchor_y = pr->height;
			break;
		case EXIF_ORIENTATION_BOTTOM_LEFT:
			/* flipped */
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						180, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						180, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->width, pr->height);
			anchor_x = 0;
			anchor_y = pr->height;
			break;
		case EXIF_ORIENTATION_LEFT_TOP:
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						-90, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						180, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->height, pr->width);
			anchor_x = 0;
			anchor_y = 0;
			break;
		case EXIF_ORIENTATION_RIGHT_TOP:
			/* rotated -90 (270) */
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						-90, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						0, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->height, pr->width);
			anchor_x = 0;
			anchor_y = pr->height;
			break;
		case EXIF_ORIENTATION_RIGHT_BOTTOM:
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						90, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						180, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->height, pr->width);
			anchor_x = pr->width;
			anchor_y = pr->height;
			break;
		case EXIF_ORIENTATION_LEFT_BOTTOM:
			/* rotated 90 */
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Z_AXIS,
						90, 0, 0, 0);
			clutter_actor_set_rotation(CLUTTER_ACTOR(rc->texture),
						CLUTTER_Y_AXIS,
						0, 0, 0, 0);
			clutter_actor_set_size(CLUTTER_ACTOR(rc->texture), pr->height, pr->width);
			anchor_x = pr->width;
			anchor_y = 0;
			break;
		default:
			/* The other values are out of range */
			break;
		}
	
	clutter_actor_set_position(CLUTTER_ACTOR(rc->texture), 
				pr->x_offset - rc->x_scroll + anchor_x, 
				pr->y_offset - rc->y_scroll + anchor_y);

}


static void renderer_area_changed(void *renderer, gint src_x, gint src_y, gint src_w, gint src_h)
{
	RendererClutter *rc = (RendererClutter *)renderer;
	PixbufRenderer *pr = rc->pr;
	
	
	
	printf("renderer_area_changed %d %d %d %d\n", src_x, src_y, src_w, src_h);
	if (pr->pixbuf)
		{
		CoglHandle texture = clutter_texture_get_cogl_texture(CLUTTER_TEXTURE(rc->texture));
		
		cogl_texture_set_region(texture,
					src_x,
					src_y,
					src_x,
					src_y,
					src_w,
					src_h,
					src_w,
					src_h,
					gdk_pixbuf_get_has_alpha(pr->pixbuf) ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
					gdk_pixbuf_get_rowstride(pr->pixbuf),
					gdk_pixbuf_get_pixels(pr->pixbuf));
		clutter_actor_queue_redraw(CLUTTER_ACTOR(rc->texture));
		}

}

static void renderer_redraw(void *renderer, gint x, gint y, gint w, gint h,
                     gint clamp, ImageRenderType render, gboolean new_data, gboolean only_existing)
{
	RendererClutter *rc = (RendererClutter *)renderer;
	PixbufRenderer *pr = rc->pr;
}

static void renderer_update_pixbuf(void *renderer, gboolean lazy)
{
	RendererClutter *rc = (RendererClutter *)renderer;
	PixbufRenderer *pr = rc->pr;
	
	if (pr->pixbuf)
		{
		printf("renderer_update_pixbuf\n");
		
		/* FIXME use CoglMaterial with multiple textures for background, color management, anaglyph, ... */
		CoglHandle texture = cogl_texture_new_with_size(gdk_pixbuf_get_width(pr->pixbuf),
								gdk_pixbuf_get_height(pr->pixbuf),
								COGL_TEXTURE_NONE,
								gdk_pixbuf_get_has_alpha(pr->pixbuf) ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888);

		if (texture != COGL_INVALID_HANDLE)
			{
			clutter_texture_set_cogl_texture(CLUTTER_TEXTURE(rc->texture), texture);
			cogl_handle_unref(texture);
			}
		if (!lazy)
			{
			renderer_area_changed(renderer, 0, 0, gdk_pixbuf_get_width(pr->pixbuf), gdk_pixbuf_get_height(pr->pixbuf));
			}
		}


	printf("renderer_update_pixbuf\n");
	rc_sync_actor(rc);
}



static void renderer_update_zoom(void *renderer, gboolean lazy)
{
	RendererClutter *rc = (RendererClutter *)renderer;
	PixbufRenderer *pr = rc->pr;

	printf("renderer_update_zoom\n");
	rc_sync_actor(rc);
}

static void renderer_invalidate_region(void *renderer, gint x, gint y, gint w, gint h)
{
}

static void renderer_overlay_draw(void *renderer, gint x, gint y, gint w, gint h)
{
}

static void renderer_overlay_add(void *renderer, gint x, gint y, gint w, gint h)
{
}

static void renderer_overlay_set(void *renderer, gint x, gint y, gint w, gint h)
{
}

static void renderer_overlay_get(void *renderer, gint x, gint y, gint w, gint h)
{
}

static void renderer_update_sizes(void *renderer)
{
	RendererClutter *rc = (RendererClutter *)renderer;
	ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff }; 

	rc->stereo_off_x = 0;
	rc->stereo_off_y = 0;
	
	if (rc->stereo_mode & PR_STEREO_RIGHT) 
		{
		if (rc->stereo_mode & PR_STEREO_HORIZ) 
			{
			rc->stereo_off_x = rc->pr->viewport_width;
			}
		else if (rc->stereo_mode & PR_STEREO_VERT) 
			{
			rc->stereo_off_y = rc->pr->viewport_height;
			}
		else if (rc->stereo_mode & PR_STEREO_FIXED) 
			{
			rc->stereo_off_x = rc->pr->stereo_fixed_x_right;
			rc->stereo_off_y = rc->pr->stereo_fixed_y_right;
			}
		}
	else
		{
		if (rc->stereo_mode & PR_STEREO_FIXED) 
			{
			rc->stereo_off_x = rc->pr->stereo_fixed_x_left;
			rc->stereo_off_y = rc->pr->stereo_fixed_y_left;
			}
		}
        DEBUG_1("update size: %p  %d %d   %d %d", rc, rc->stereo_off_x, rc->stereo_off_y, rc->pr->viewport_width, rc->pr->viewport_height);

	printf("renderer_update_sizes  scale %d %d\n", rc->pr->width, rc->pr->height);

        clutter_stage_set_color(CLUTTER_STAGE(rc->stage), &stage_color);


	clutter_actor_set_size(rc->group, rc->pr->viewport_width, rc->pr->viewport_height);
	clutter_actor_set_position(rc->group, rc->stereo_off_x, rc->stereo_off_y);
	rc_sync_actor(rc);
}

static void renderer_scroll(void *renderer, gint x_off, gint y_off)
{
	printf("renderer_scroll\n");
	RendererClutter *rc = (RendererClutter *)renderer;
	PixbufRenderer *pr = rc->pr;

	rc_sync_actor(rc);
}

static void renderer_stereo_set(void *renderer, gint stereo_mode)
{
	RendererClutter *rc = (RendererClutter *)renderer;

	rc->stereo_mode = stereo_mode;
}

static void renderer_free(void *renderer)
{
	RendererClutter *rc = (RendererClutter *)renderer;
	GtkWidget *widget = gtk_bin_get_child(GTK_BIN(rc->pr));
	if (widget)
		{
		/* widget still exists */
		clutter_actor_destroy(rc->group);
		if (clutter_group_get_n_children(CLUTTER_GROUP(rc->stage)) == 0)
			{
			printf("destroy %p\n", rc->widget);
			/* this was the last user */
			gtk_widget_destroy(rc->widget);
			}
		else
			{
			printf("keep %p\n", rc->widget);
			g_object_unref(G_OBJECT(rc->widget));
			}
		}
	g_free(rc);
}

RendererFuncs *renderer_clutter_new(PixbufRenderer *pr)
{
	RendererClutter *rc = g_new0(RendererClutter, 1);
	
	rc->pr = pr;
	
	rc->f.redraw = renderer_redraw;
	rc->f.area_changed = renderer_area_changed;
	rc->f.update_pixbuf = renderer_update_pixbuf;
	rc->f.free = renderer_free;
	rc->f.update_zoom = renderer_update_zoom;
	rc->f.invalidate_region = renderer_invalidate_region;
	rc->f.scroll = renderer_scroll;
	rc->f.update_sizes = renderer_update_sizes;


	rc->f.overlay_add = renderer_overlay_add;
	rc->f.overlay_set = renderer_overlay_set;
	rc->f.overlay_get = renderer_overlay_get;
	rc->f.overlay_draw = renderer_overlay_draw;

	rc->f.stereo_set = renderer_stereo_set;
	
	
	rc->stereo_mode = 0;
	rc->stereo_off_x = 0;
	rc->stereo_off_y = 0;


  	rc->widget = gtk_bin_get_child(GTK_BIN(rc->pr));
  	
  	if (rc->widget)
  		{
  		if (!GTK_CLUTTER_IS_EMBED(rc->widget))
  			{
  			g_free(rc);
  			DEBUG_0("pixbuf renderer has a child of other type than gtk_clutter_embed");
  			return NULL;
  			}
  		}
  	else 
  		{
  		rc->widget = gtk_clutter_embed_new();
		gtk_container_add(GTK_CONTAINER(rc->pr), rc->widget);
  		}
  		
	gtk_event_box_set_above_child (GTK_EVENT_BOX(rc->pr), TRUE);
        rc->stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (rc->widget));
        
        rc->group = clutter_group_new();
  	clutter_container_add_actor(CLUTTER_CONTAINER(rc->stage), rc->group);
  	clutter_actor_set_clip_to_allocation(CLUTTER_ACTOR(rc->group), TRUE);
  
  	rc->texture = gtk_clutter_texture_new ();
  	clutter_container_add_actor(CLUTTER_CONTAINER(rc->group), rc->texture);
  	g_object_ref(G_OBJECT(rc->widget));
  
	gtk_widget_show(rc->widget);
	return (RendererFuncs *) rc;
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
