/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include "image.h"
#include <gdk/gdkkeysyms.h> /* for keyboard values */

/*
 *-----------------------------------------------------------------------------
 * view window keyboard
 *-----------------------------------------------------------------------------
 */

static gint view_window_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	ImageWindow *imd = data;
	gint stop_signal = FALSE;
	gint x = 0;
	gint y = 0;

	switch (event->keyval)
		{
		case GDK_Left:
			x -= 1;
			stop_signal = TRUE;
			break;
		case GDK_Right:
			x += 1;
			stop_signal = TRUE;
			break;
		case GDK_Up:
			y -= 1;
			stop_signal = TRUE;
			break;
		case GDK_Down:
			y += 1;
			stop_signal = TRUE;
			break;
		case '+': case '=':
			image_area_adjust_zoom(imd, 1);
			break;
		case '-':
			image_area_adjust_zoom(imd, -1);
			break;
		case 'x':
			image_area_set_zoom(imd, 0);
			break;
		case 'z':
			image_area_set_zoom(imd, 1);
			break;
		case GDK_Delete:
			file_util_delete(image_area_get_path(imd), NULL);
			stop_signal = TRUE;
			break;
		case GDK_Escape:
			gtk_widget_destroy(imd->top_window);
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
			}
		if (n != -1)
			{
			start_editor_from_file(n, image_area_get_path(imd));
			}
		}

	if (event->state & GDK_SHIFT_MASK)
		{
		x *= 3;
		y *= 3;
		}

	if (x != 0 || y!= 0)
		{
		keyboard_scroll_calc(&x, &y, event);
		image_area_scroll(imd, x, y);
		}

	return stop_signal;
}

/*
 *-----------------------------------------------------------------------------
 * view window main routines
 *-----------------------------------------------------------------------------
 */ 

static void button3_cb(ImageWindow *imd, GdkEventButton *bevent, gpointer data)
{
	gtk_object_set_data(GTK_OBJECT(menu_window_view), "view_active", imd);
	gtk_menu_popup (GTK_MENU(menu_window_view), NULL, NULL, NULL, NULL,
		bevent->button, bevent->time);
}

static gint view_window_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	gtk_widget_destroy(w);
	return TRUE;
}

static gint view_window_destroy_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	ImageWindow *imd = data;
	image_area_free(imd);
	return FALSE;
}

void view_window_new(gchar *path)
{
	GtkWidget *window;
	ImageWindow *imd;
	GtkAllocation req_size;
	gint w, h;
	if (!path) return;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_window_set_title (GTK_WINDOW (window), "GQview");
        gtk_window_set_wmclass(GTK_WINDOW (window), "view", "GQview");
        gtk_container_border_width (GTK_CONTAINER (window), 0);

	imd = image_area_new(NULL);
	image_area_set_topwindow(imd, window, NULL, TRUE);
	gtk_container_add(GTK_CONTAINER(window), imd->eventbox);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT(imd->viewport), GTK_SHADOW_NONE);
        gtk_container_border_width (GTK_CONTAINER (imd->viewport), 0);
	gtk_widget_show_all(imd->eventbox);

	image_dnd_init(imd);

	image_area_set_button(imd, 3, button3_cb, NULL);

	gtk_signal_connect(GTK_OBJECT(window), "delete_event", (GtkSignalFunc) view_window_delete_cb, NULL);
	gtk_signal_connect(GTK_OBJECT(window), "destroy_event", (GtkSignalFunc) view_window_destroy_cb, imd);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event", GTK_SIGNAL_FUNC(view_window_key_press_cb), imd);

	image_area_set_image(imd, path, get_default_zoom(NULL));

	w = imd->width;
	h = imd->height;
	if (limit_window_size)
		{
		gint mw = gdk_screen_width() * max_window_size / 100;
		gint mh = gdk_screen_height() * max_window_size / 100;

		if (w > mw) w = mw;
		if (h > mh) h = mh;
		}

	gtk_window_set_default_size (GTK_WINDOW(window), w, h);
	req_size.x = req_size.y = 0;
	req_size.width = w;
	req_size.height = h;
	gtk_widget_size_allocate(GTK_WIDGET(window), &req_size);

	gtk_widget_set_usize(imd->eventbox, w, h);

	gtk_widget_show(window);
}

/*
 *-----------------------------------------------------------------------------
 * view window menu routines and callbacks
 *-----------------------------------------------------------------------------
 */ 

static ImageWindow *view_window_get_active()
{
	return gtk_object_get_data(GTK_OBJECT(menu_window_view), "view_active");
}

void view_window_active_edit(gint n)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	start_editor_from_file(n, image_area_get_path(imd));
}

void view_window_active_to_root(gint n)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	image_area_to_root(imd, (image_area_get_zoom(imd) == 0));
}

static void view_zoom_in_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	image_area_adjust_zoom(imd, 1);
}

static void view_zoom_out_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	image_area_adjust_zoom(imd, -1);
}

static void view_zoom_1_1_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	image_area_set_zoom(imd, 1);
}

static void view_zoom_fit_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	image_area_set_zoom(imd, 0);
}

static void view_copy_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	file_util_copy(image_area_get_path(imd), NULL, current_path);
}

static void view_move_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	file_util_move(image_area_get_path(imd), NULL, current_path);
}

static void view_rename_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	file_util_rename(image_area_get_path(imd), NULL);
}

static void view_delete_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	file_util_delete(image_area_get_path(imd), NULL);
}

static void view_close_cb(GtkWidget *widget, gpointer data)
{
	ImageWindow *imd = view_window_get_active();
	if (!imd) return;
	gtk_widget_destroy(imd->top_window);
}

void create_menu_view_popup()
{
	menu_window_view = gtk_menu_new();
	add_menu_popup_item(menu_window_view, _("Zoom in"), view_zoom_in_cb, NULL);
	add_menu_popup_item(menu_window_view, _("Zoom out"), view_zoom_out_cb, NULL);
	add_menu_popup_item(menu_window_view, _("Zoom 1:1"), view_zoom_1_1_cb, NULL);
	add_menu_popup_item(menu_window_view, _("Fit image to window"), view_zoom_fit_cb, NULL);
	add_menu_divider(menu_window_view);

	menu_window_view_edit = gtk_menu_item_new_with_label(_("Edit"));
	gtk_menu_append(GTK_MENU(menu_window_view), menu_window_view_edit);
	gtk_widget_show(menu_window_view_edit);

	add_menu_divider(menu_window_view);
	add_menu_popup_item(menu_window_view, _("Copy..."), view_copy_cb, NULL);
	add_menu_popup_item(menu_window_view, _("Move..."), view_move_cb, NULL);
	add_menu_popup_item(menu_window_view, _("Rename..."), view_rename_cb, NULL);
	add_menu_popup_item(menu_window_view, _("Delete..."), view_delete_cb, NULL);

	add_menu_divider(menu_window_view);
	add_menu_popup_item(menu_window_view, _("Close window"), view_close_cb, NULL);
}


