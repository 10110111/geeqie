/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include "image.h"
#include "icons/img_unknown.xpm"
#include <gdk/gdkx.h>

static gchar *zoom_as_text(gint zoom, gfloat scale);
static void set_zoom_label(GtkWidget *label, gint zoom, gfloat scale);
static void set_info_label(GtkWidget *label, gint width, gint height, gint size, gint unknown);
static void set_window_title(ImageWindow *imd, gchar *text);

static gint image_area_size_top_window(ImageWindow *imd, gint w, gint h);

static void image_area_recalc_size(ImageWindow *imd, GtkAllocation *allocation);

static void image_area_redraw(ImageWindow *imd);
static gint image_area_size_cb(GtkWidget *widget, GtkAllocation *allocation, gpointer data);
static gint image_area_update_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data);

static void set_mouse_cursor (GdkWindow *window, gint icon);
static void image_area_mouse_moved(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
static void image_area_mouse_pressed(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
static void image_area_mouse_released(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
static void image_area_mouse_drag(GtkWidget *widget, GdkDragContext *context, gpointer data);

/*
 *-----------------------------------------------------------------------------
 * image status widget update routines (private)
 *-----------------------------------------------------------------------------
 */

static gchar *zoom_as_text(gint zoom, gfloat scale)
{
	gint l = 1;
	gint r = 1;
	gchar *approx = " ";
	if (zoom > 1) l = zoom;
	if (zoom < -1) r = -zoom;
	if (zoom == 0 && scale != 0)
		{
		if (scale < 1) r = 1 / scale + 0.5;
		approx = " ~";
		}
	return g_strdup_printf("%d :%s%d", l, approx, r);
}

static void set_zoom_label(GtkWidget *label, gint zoom, gfloat scale)
{
	gchar *buf;
	buf = zoom_as_text(zoom, scale);
	gtk_label_set(GTK_LABEL(label), buf);
	g_free(buf);
}

static void set_info_label(GtkWidget *label, gint width, gint height, gint size, gint unknown)
{
	gchar buf[64];
	if (unknown)
		sprintf(buf, _("( ? x ? ) %d bytes"), size);
	else
		sprintf(buf, _("( %d x %d ) %d bytes"), width, height, size);
	gtk_label_set(GTK_LABEL(label), buf);
}

static void set_window_title(ImageWindow *imd, gchar *text)
{
	gchar *title = NULL;
	if (!imd->top_window) return;

	if (imd->title)
		{
		title = g_strconcat(imd->title, imd->image_name, text, NULL);
		}
	else
		{
		title = g_strconcat(imd->image_name, text, NULL);
		}

	gtk_window_set_title(GTK_WINDOW(imd->top_window), title);
	g_free(title);
}

/*
 *-----------------------------------------------------------------------------
 * fit window to image utility (private)
 *-----------------------------------------------------------------------------
 */ 

static gint image_area_size_top_window(ImageWindow *imd, gint w, gint h)
{
	if (!imd->top_window) return FALSE;
	if (imd == full_screen_image) return FALSE;
	if (imd == normal_image && !toolwindow) return FALSE;
	if (!fit_window) return FALSE;

	if (imd == normal_image)
		{
		/* account for border frame */
		w += 4;
		h += 4;
		}

	if (limit_window_size)
		{
		gint sw = gdk_screen_width() * max_window_size / 100;
		gint sh = gdk_screen_height() * max_window_size / 100;

		if (w > sw) w = sw;
		if (h > sh) h = sh;
		}

	/* to cheat on a prob a little, don't resize if within 1 either way...
	   ...dumb off by 1 errors! ;) */

/*	if (w >= (imd->top_window)->allocation.width - 1 &&
	    w <= (imd->top_window)->allocation.width + 1 &&
	    h >= (imd->top_window)->allocation.height - 1 &&
	    h <= (imd->top_window)->allocation.height + 1)
		return FALSE;
*/
	if (debug) printf("auto sized to %d x %d\n", w, h);

	gdk_window_resize(imd->top_window->window, w, h);
	gtk_widget_set_usize(imd->top_window, w, h);

	return TRUE;
}

/*
 *-----------------------------------------------------------------------------
 * image widget zoom/recalc routines
 *-----------------------------------------------------------------------------
 */ 

void image_area_scroll(ImageWindow *imd, gint x, gint y)
{
	if (x != 0)
		{
		GtkAdjustment *h = gtk_viewport_get_hadjustment(GTK_VIEWPORT(imd->viewport));
		gfloat val = h->value + x;
		if (val < h->lower) val = h->lower;
		if (val > h->upper - h->page_size) val = h->upper - h->page_size;
		gtk_adjustment_set_value(GTK_ADJUSTMENT(h), val);
		}

	if (y != 0)
		{
		GtkAdjustment *v = gtk_viewport_get_vadjustment(GTK_VIEWPORT(imd->viewport));
		gfloat val = v->value + y;
		if (val < v->lower) val = v->lower;
		if (val > v->upper - v->page_size) val = v->upper - v->page_size;
		gtk_adjustment_set_value(GTK_ADJUSTMENT(v), val);
		}
}

gint image_area_get_zoom(ImageWindow *imd)
{
	return imd->zoom;
}

void image_area_adjust_zoom(ImageWindow *imd, gint increment)
{
	gint zoom = imd->zoom;
	if (increment < 0)
		{
		while (increment < 0)
			{
			zoom--;
			if (zoom == 0 || zoom == -1) zoom = -2;
			increment++;
			}
		if (zoom < -8) zoom = -8;
		}
	else
		{
		while (increment > 0)
			{
			zoom++;
			if (zoom == -1) zoom = 1;
			increment--;
			}
		if (zoom > 3) zoom = 3;
		}
	if (zoom != imd->zoom)
		image_area_set_zoom(imd, zoom);
}

void image_area_set_zoom(ImageWindow *imd, gint zoom)
{
	if (zoom == imd->zoom && imd->width > 0 && imd->height > 0) return;

	imd->zoom = zoom;
	image_area_recalc_size(imd, NULL);

	gtk_widget_set_usize (imd->table, imd->width, imd->height);
	gtk_drawing_area_size(GTK_DRAWING_AREA(imd->image), imd->width, imd->height);
}

static void image_area_recalc_size(ImageWindow *imd, GtkAllocation *allocation)
{
	gint w, h, ww, wh;
	gfloat scale_factor = 1;

	w = imd->image_data->rgb_width;
	h = imd->image_data->rgb_height;
	if (allocation)
		{
		ww = allocation->width;
        	wh = allocation->height;
		}
	else
		{
		ww = (imd->eventbox)->allocation.width;
        	wh = (imd->eventbox)->allocation.height;
		}

	if (imd == normal_image)
		{
		/* account for frame */
		ww -= 4;
		wh -= 4;
		}

	if (imd->zoom == 0) /* zoom to fit */
		{
		if (imd == normal_image && imd->width == 0 && imd->height == 0 &&
		    fit_window && toolwindow)
			{
			if (limit_window_size)
				{
				ww = (gdk_screen_width() * max_window_size / 100) - 4;
				wh = (gdk_screen_height() * max_window_size / 100) - 4;
				}
			else
				{
				ww = w;
				wh = h;
				}
			}
		if (w > ww || h > wh)
			{
			if ((gfloat)ww / w > (gfloat)wh / h)
				{
				scale_factor = (gfloat) wh / h;
				h = wh;
				w = w * scale_factor + 0.5;
				if (w > ww) w = ww;
				}
			else
				{
				scale_factor = (gfloat)ww / w;
				w = ww;
				h = h * scale_factor + 0.5;
				if (h > wh) h = wh;
				}
			if (w < 1) w = 1;
			if (h < 1) h = 1;
			}
		}
	else if (imd->zoom > 0) /* zoom orig, in */
		{
		scale_factor = imd->zoom;
		w = w * scale_factor;
		h = h * scale_factor;
		}
	else if (imd->zoom < -1) /* zoom out */
		{
		scale_factor = (- imd->zoom);
		w = w / scale_factor;
		h = h / scale_factor;
		}

	imd->width = w;
	imd->height = h;

	if (debug) printf("recalc %d x %d @ %f\n", w, h, scale_factor);

	if (imd->zoom_label)
		{
		set_zoom_label(imd->zoom_label, imd->zoom, scale_factor);
		}

/* this is causing problems with resizing
	if (imd->top_window && imd->show_title_zoom)
		{
		gchar *buf = zoom_as_text(imd->zoom, scale_factor);
		gchar *zbuf = g_strconcat(" [ ", buf, "]", NULL);
		g_free(buf);
		set_window_title(imd, zbuf);
		g_free(zbuf);
		}
*/

	if (image_area_size_top_window(imd, w, h))
		{
		/* this is hacky */
		imd->artificial_size = TRUE;
		gtk_grab_add (info_zoom);
		while(gtk_events_pending()) gtk_main_iteration();
		gtk_grab_remove(info_zoom);
		imd->artificial_size = FALSE;
		}
}

/*
 *-----------------------------------------------------------------------------
 * image widget set/get image information
 *-----------------------------------------------------------------------------
 */ 

void image_area_set_path(ImageWindow *imd, gchar *newpath)
{
	if (!imd->image_path || !newpath) return;

	g_free(imd->image_path);
	imd->image_path = g_strdup(newpath);
	imd->image_name = filename_from_path(imd->image_path);

	if (imd->top_window)
		{
		set_window_title(imd, NULL);
		}
}

gchar *image_area_get_path(ImageWindow *imd)
{
	return imd->image_path;
}

gchar *image_area_get_name(ImageWindow *imd)
{
	return imd->image_name;
}

void image_area_set_image(ImageWindow *imd, gchar *path, gint zoom)
{
	if (path && imd->image_path && !strcmp(path, imd->image_path)) return;

	g_free(imd->image_path);
	if (path)
		{
		imd->image_path = g_strdup(path);
		imd->image_name = filename_from_path(imd->image_path);
		}
	else
		{
		imd->image_path = NULL;
		imd->image_name = " ";
		zoom = 1;
		}

	if (imd->image_data) gdk_imlib_destroy_image(imd->image_data);
	if (path && isfile(path))
		{
		imd->image_data = gdk_imlib_load_image(path);
		if (!imd->image_data)
			{
			imd->image_data = gdk_imlib_create_image_from_xpm_data((gchar **)img_unknown_xpm);
			imd->unknown = TRUE;
			}
		else
			{
			imd->unknown = FALSE;
			}
		imd->size = filesize(path);
		}
	else
		{
		if (path)
			imd->image_data = gdk_imlib_create_image_from_xpm_data((gchar **)img_unknown_xpm);
		else
			imd->image_data = gdk_imlib_create_image_from_data((char *)logo, NULL, logo_width, logo_height);
		imd->unknown = TRUE;
		imd->size = 0;
		}

	imd->width = imd->old_width = 0;
	imd->height = imd->old_height = 0;

	if (imd->top_window)
		{
		set_window_title(imd, NULL);
		}
	if (imd->info_label)
		{
		set_info_label(imd->info_label, imd->image_data->rgb_width, imd->image_data->rgb_height, imd->size, imd->unknown);
		}

	/* do info area updates here */

	imd->new_img = TRUE;
	image_area_set_zoom(imd, zoom);
}

/*
 *-----------------------------------------------------------------------------
 * image widget redraw/callbacks (private)
 *-----------------------------------------------------------------------------
 */ 

static void image_area_redraw(ImageWindow *imd)
{
	GdkBitmap *mask = NULL;

	if (debug) printf("redrawn %d x %d\n", imd->width, imd->height);

	if (!imd->image_data) return;

	if (imd->width == imd->old_width && imd->height == imd->old_height)
		{
		if (debug) printf("redraw cancelled\n");
		return;
		}

	if (imd->image_pixmap) gdk_imlib_free_pixmap(imd->image_pixmap);
	imd->image_pixmap = NULL;

	gdk_imlib_render(imd->image_data, imd->width, imd->height);
	imd->image_pixmap = gdk_imlib_move_image(imd->image_data);
	mask = gdk_imlib_move_mask(imd->image_data);

	gdk_window_set_back_pixmap(imd->image->window, imd->image_pixmap, FALSE);
	gdk_window_shape_combine_mask (imd->image->window, mask, 0, 0);
	gdk_window_clear(imd->image->window);
	gdk_flush();

	imd->old_width = imd->width;
	imd->old_height = imd->height;
}

static gint image_area_size_cb(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	ImageWindow *imd = data;
	gint old_w, old_h;
	GtkAdjustment *h;
	GtkAdjustment *v;
	gfloat h_pos;
	gfloat v_pos;
	gfloat h_max;
	gfloat v_max;

	if (imd->artificial_size) return FALSE;

	h = gtk_viewport_get_hadjustment(GTK_VIEWPORT(imd->viewport));
	v = gtk_viewport_get_vadjustment(GTK_VIEWPORT(imd->viewport));

	h_pos = h->value;
	h_max = allocation->width;
	v_pos = v->value;
	v_max = allocation->height;

	if (imd == normal_image)
		{
		h_max -= 4.0;
		v_max -= 4.0;
		}

	if (h_pos > h->upper - h_max) h_pos = h->upper - h_max;
	if (v_pos > v->upper - v_max) v_pos = v->upper - v_max;

	if (imd->new_img)
		{
		imd->new_img = FALSE;
		gtk_adjustment_clamp_page(h, 0.0, h_max);
	        gtk_adjustment_clamp_page(v, 0.0, v_max);
		}
	else
		{
		gtk_adjustment_clamp_page(h, h_pos, h_max);
	        gtk_adjustment_clamp_page(v, v_pos, v_max);
		}

        gtk_adjustment_changed(h);
        gtk_adjustment_changed(v);

	if (!imd->image_data || imd->zoom != 0) return FALSE;

	old_w = imd->width;
	old_h = imd->height;
	image_area_recalc_size(imd, allocation);
	if (old_w != imd->width || old_h != imd->height)
		{
		gtk_widget_set_usize (imd->table, imd->width, imd->height);
		gtk_drawing_area_size(GTK_DRAWING_AREA(imd->image), imd->width, imd->height);
		}

	if (debug) printf("sized %d x %d (%d x %d)\n", allocation->width, allocation->height, imd->width, imd->height);

	return FALSE;
}

static gint image_area_update_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	ImageWindow *imd = data;

	if (imd->artificial_size) return FALSE;

	image_area_redraw(imd);

	return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 * image widget mouse routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void set_mouse_cursor (GdkWindow *window, gint icon)
{
	GdkCursor *cursor;

	if (icon == -1)
		{
		cursor = NULL;
		}
	else
		{
		cursor = gdk_cursor_new (icon);
		}

	gdk_window_set_cursor (window, cursor);

	if (cursor) gdk_cursor_destroy (cursor);
}

static void image_area_mouse_moved(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ImageWindow *imd = data;
	GtkAdjustment* h;
	GtkAdjustment* v;
	gfloat x, y;
	gfloat val;

	if (!imd->in_drag || !gdk_pointer_is_grabbed()) return;

	if (imd->drag_moved < 4)
		{
		imd->drag_moved++;
		}
	else
		{
		set_mouse_cursor (imd->eventbox->window, GDK_FLEUR);
		}

	h = gtk_viewport_get_hadjustment(GTK_VIEWPORT(imd->viewport));
	v = gtk_viewport_get_vadjustment(GTK_VIEWPORT(imd->viewport));

	x = imd->drag_last_x - bevent->x;
	y = imd->drag_last_y - bevent->y;

	/* x */
	if (h->upper - h->page_size > 0)
		{
		val = (float)h->value + x;
		if (val < 0 ) val = 0;
		if (val > h->upper - h->page_size) val = h->upper - h->page_size;
		h->value = val;
		gtk_adjustment_set_value (GTK_ADJUSTMENT(h), val);
		}

	/* y */
	if (v->upper - v->page_size > 0)
		{
		val = v->value + y;
		if (val < 0 ) val = 0;
		if (val > v->upper - v->page_size) val = v->upper - v->page_size;
		v->value = val;
		gtk_adjustment_set_value (GTK_ADJUSTMENT(v), val);
		}

	gtk_adjustment_value_changed(h);
	gtk_adjustment_value_changed(v);

	imd->drag_last_x = bevent->x;
	imd->drag_last_y = bevent->y;
}

static void image_area_mouse_pressed(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ImageWindow *imd = data;
	switch (bevent->button)
		{
		case 1:
			imd->in_drag = TRUE;
			imd->drag_last_x = bevent->x;
			imd->drag_last_y = bevent->y;
			imd->drag_moved = 0;
			gdk_pointer_grab (imd->eventbox->window, FALSE,
                                GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON_RELEASE_MASK,
                                NULL, NULL, bevent->time);
			gtk_grab_add (imd->eventbox);
			break;
		case 2:
			imd->drag_moved = 0;
			break;
		case 3:
			if (imd->func_btn3)
				imd->func_btn3(imd, bevent, imd->data_btn3);
			break;
		default:
			break;
		}
	gtk_widget_grab_focus(imd->viewport);
}

static void image_area_mouse_released(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ImageWindow *imd = data;
	if (gdk_pointer_is_grabbed() && GTK_WIDGET_HAS_GRAB (imd->eventbox))
		{
		gtk_grab_remove (imd->eventbox);
		gdk_pointer_ungrab (bevent->time);
		set_mouse_cursor (imd->eventbox->window, -1);
		}

	if (bevent->button == 1)
		{
		if (imd->drag_moved < 4 && imd->func_btn1)
			imd->func_btn1(imd, bevent, imd->data_btn1);
		}

	if (bevent->button == 2)
		{
		if (imd->drag_moved < 4 && imd->func_btn2)
			imd->func_btn2(imd, bevent, imd->data_btn2);
		}

	imd->in_drag = FALSE;
}

static void image_area_mouse_drag(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	ImageWindow *imd = data;
	imd->drag_moved = 4;
}

/*
 *-----------------------------------------------------------------------------
 * image widget setup routines
 *-----------------------------------------------------------------------------
 */ 

void image_area_set_topwindow(ImageWindow *imd, GtkWidget *window, gchar *title, gint show_zoom)
{
	imd->top_window = window;
	imd->show_title_zoom = show_zoom;

	g_free(imd->title);
	if (title)
		imd->title = g_strdup(title);
	else
		imd->title = NULL;
}

void image_area_set_labels(ImageWindow *imd, GtkWidget *info, GtkWidget *zoom)
{
	imd->info_label = info;
	imd->zoom_label = zoom;
}

void image_area_set_button(ImageWindow *imd, gint button,
	void (*func)(ImageWindow *, GdkEventButton *, gpointer), gpointer data)
{
	switch (button)
		{
		case 1:
			imd->func_btn1 = func;
			imd->data_btn1 = data;
			break;
		case 2:
			imd->func_btn2 = func;
			imd->data_btn2 = data;
			break;
		case 3:
			imd->func_btn3 = func;
			imd->data_btn3 = data;
			break;
		}
}

ImageWindow *image_area_new(GtkWidget *top_window)
{
        GtkObject *h_adj;
        GtkObject *v_adj;
	ImageWindow *imd;

	imd = g_new0(ImageWindow, 1);
	imd->zoom = 0;

	imd->top_window = top_window;
	imd->title = g_strdup("GQview - ");
	imd->show_title_zoom = FALSE;
	imd->new_img = FALSE;

	imd->eventbox = gtk_event_box_new();

	gtk_signal_connect(GTK_OBJECT(imd->eventbox),"motion_notify_event",
			   GTK_SIGNAL_FUNC(image_area_mouse_moved), imd);
	gtk_signal_connect(GTK_OBJECT(imd->eventbox),"button_press_event",
			   GTK_SIGNAL_FUNC(image_area_mouse_pressed), imd);
	gtk_signal_connect(GTK_OBJECT(imd->eventbox),"button_release_event",
			   GTK_SIGNAL_FUNC(image_area_mouse_released), imd);
	gtk_widget_set_events(imd->eventbox, GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK);

	/* viewer */
	h_adj = gtk_adjustment_new(0.0,0.0,0.0,1.0,1.0,1.0);
	v_adj = gtk_adjustment_new(0.0,0.0,0.0,1.0,1.0,1.0);

	imd->viewport = gtk_viewport_new (GTK_ADJUSTMENT(h_adj), GTK_ADJUSTMENT(v_adj));
	gtk_container_add(GTK_CONTAINER(imd->eventbox), imd->viewport);

	/* table for resize */
	imd->table = gtk_table_new (1,1,TRUE);
	gtk_container_add(GTK_CONTAINER (imd->viewport), imd->table);

	/* imagewindow */
	imd->image = gtk_drawing_area_new();
	gtk_table_attach(GTK_TABLE (imd->table),imd->image,0,1,0,1,GTK_EXPAND,GTK_EXPAND,0,0);

	gtk_signal_connect(GTK_OBJECT(imd->eventbox),"size_allocate",GTK_SIGNAL_FUNC(image_area_size_cb), imd);
	gtk_signal_connect(GTK_OBJECT(imd->image),"configure_event",GTK_SIGNAL_FUNC(image_area_update_cb), imd);

	gtk_signal_connect(GTK_OBJECT(imd->viewport),"drag_begin",
			   GTK_SIGNAL_FUNC(image_area_mouse_drag), imd);

	return imd;
}

void image_area_free(ImageWindow *imd)
{
	g_free(imd->image_path);
	g_free(imd->title);

	if (imd->image_pixmap) gdk_imlib_free_pixmap(imd->image_pixmap);
	if (imd->image_data) gdk_imlib_destroy_image(imd->image_data);

	g_free(imd);
}

gint get_default_zoom(ImageWindow *imd)
{
	gint zoom;

	if (zoom_mode == ZOOM_RESET_ORIGINAL)
		{
		zoom = 1;
		}
	else if (zoom_mode == ZOOM_RESET_FIT_WINDOW)
		{
		zoom = 0;
		}
	else
		{
		if (imd)
			{
			zoom = image_area_get_zoom(imd);
			}
		else
			{
			zoom = 1;
			}
		}

	return zoom;
}

/*
 *-----------------------------------------------------------------------------
 * image widget misc utils
 *-----------------------------------------------------------------------------
 */ 

void image_area_to_root(ImageWindow *imd, gint scaled)
{                                                                               
	GdkVisual *gdkvisual;
	GdkWindow *rootwindow;
	GdkPixmap *pixmap;

	if (!imd || !imd->image_data) return;


	rootwindow = (GdkWindow *) &gdk_root_parent;	/* hmm, don't know, correct? */
	gdkvisual = gdk_window_get_visual(rootwindow);
	if (gdkvisual != gdk_imlib_get_visual()) return;

	if (scaled)
		{
		gdk_imlib_render(imd->image_data, gdk_screen_width(), gdk_screen_height());
		}
	else
		{
		gdk_imlib_render(imd->image_data, imd->width, imd->height);
		}

	pixmap = gdk_imlib_move_image(imd->image_data);
	gdk_window_set_back_pixmap(rootwindow, pixmap, FALSE);
	gdk_window_clear(rootwindow);
	gdk_imlib_free_pixmap(pixmap);

	gdk_flush();
}


