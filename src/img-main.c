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

#define BLACK_BACKGROUND 1

/*
 *-----------------------------------------------------------------------------
 * full screen keyboard
 *-----------------------------------------------------------------------------
 */

static gint full_screen_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
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
		case GDK_Page_Up:
		case GDK_BackSpace:
			file_prev_image();
			stop_signal = TRUE;
			break;
		case GDK_Page_Down:
		case GDK_space:
			file_next_image();
			stop_signal = TRUE;
			break;
		case GDK_Home:
			file_first_image();
			stop_signal = TRUE;
			break;
		case GDK_End:
			file_last_image();
			stop_signal = TRUE;
			break;
		case GDK_Delete:
			full_screen_stop();
			file_util_delete(image_area_get_path(imd), NULL);
			stop_signal = TRUE;
			break;
		case GDK_Escape:
			full_screen_stop();
			stop_signal = TRUE;
			break;
		case 'Q': case 'q':
			exit_gqview();
			return FALSE;
                        break;
		case 'S': case 's':
			slideshow_toggle();
			break;
		case 'V': case 'v':
			full_screen_stop();
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
			case 'C': case 'c':
				full_screen_stop();
				file_util_copy(image_area_get_path(imd), NULL, current_path);
				break;
			case 'M': case 'm':
				full_screen_stop();
				file_util_move(image_area_get_path(imd), NULL, current_path);
				break;
			case 'R': case 'r':
				full_screen_stop();
				file_util_rename(image_area_get_path(imd), NULL);
				break;
			case 'D': case 'd':
				full_screen_stop();
				file_util_delete(image_area_get_path(imd), NULL);
				break;
			}
		if (n != -1)
			{
			full_screen_stop();
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
 *----------------------------------------------------------------------------
 * mouse button callbacks
 *----------------------------------------------------------------------------
 */

static void button1_cb(ImageWindow *imd, GdkEventButton *bevent, gpointer data)
{
	file_next_image();
}

static void button2_cb(ImageWindow *imd, GdkEventButton *bevent, gpointer data)
{
	file_prev_image();
}

static void button3_cb(ImageWindow *imd, GdkEventButton *bevent, gpointer data)
{
	if (main_image == normal_image)
		{
		gtk_menu_popup (GTK_MENU(menu_image_popup), NULL, NULL, NULL, NULL,
			bevent->button, bevent->time);
		}
	else
		{
		gtk_menu_popup (GTK_MENU(menu_window_full), NULL, NULL, NULL, NULL,
			bevent->button, bevent->time);
		}
}

/*
 *----------------------------------------------------------------------------
 * full screen functions
 *----------------------------------------------------------------------------
 */

static gint full_screen_delete_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	full_screen_stop();
	return TRUE;
}

static gint full_screen_destroy_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	image_area_free(full_screen_image);
	full_screen_image = NULL;

	return FALSE;
}

void full_screen_start()
{
	GtkWidget *window;
	gint w;
	gint h;

	if (full_screen_window) return;

	w = gdk_screen_width();
	h = gdk_screen_height();

	window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_container_border_width(GTK_CONTAINER(window), 0);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", (GtkSignalFunc)full_screen_delete_cb, NULL);
	gtk_signal_connect(GTK_OBJECT(window), "destroy_event", (GtkSignalFunc)full_screen_destroy_cb, NULL);

	gtk_window_set_title(GTK_WINDOW(window), _("GQview full screen"));
	gtk_widget_set_usize(window, w, h);

	full_screen_image = image_area_new(NULL);

	gtk_viewport_set_shadow_type (GTK_VIEWPORT(full_screen_image->viewport), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(window), full_screen_image->eventbox);

	/* set background to black */
	if (BLACK_BACKGROUND)
		{
		GtkStyle *style;
		style = gtk_style_copy(gtk_widget_get_style(full_screen_image->eventbox));
		gtk_style_ref(style);
		style->bg[GTK_STATE_NORMAL] = style->black;
		gtk_widget_set_style(full_screen_image->viewport, style);
		gtk_style_unref(style);
		}

	gtk_widget_show_all(full_screen_image->eventbox);

	gtk_signal_connect(GTK_OBJECT(window), "key_press_event", GTK_SIGNAL_FUNC(full_screen_key_press_cb), full_screen_image);

	image_area_set_image(full_screen_image, image_get_path(), image_area_get_zoom(normal_image));

/*
	if (!GTK_WIDGET_REALIZED(window)) gtk_widget_realize(window);
	gdk_window_set_override_redirect(window->window, TRUE);
*/
	gtk_widget_show(window);
	gdk_keyboard_grab(window->window, TRUE, GDK_CURRENT_TIME);

	/* hide normal window */
	gtk_widget_hide(mainwindow);
	image_area_set_image(normal_image, NULL, image_area_get_zoom(normal_image));

	full_screen_window = window;

	image_area_set_button(full_screen_image, 1, button1_cb, NULL);
	image_area_set_button(full_screen_image, 2, button2_cb, NULL);
	image_area_set_button(full_screen_image, 3, button3_cb, NULL);

	main_image = full_screen_image;
}

void full_screen_stop()
{
	if (!full_screen_window) return;

	gdk_keyboard_ungrab (GDK_CURRENT_TIME);

	image_area_set_image(normal_image, image_get_path(), image_area_get_zoom(full_screen_image));
	main_image = normal_image;

	gtk_widget_destroy(full_screen_window);
	full_screen_window = NULL;

	image_area_free(full_screen_image);
	full_screen_image = NULL;

	gtk_widget_show(mainwindow);
}

void full_screen_toggle()
{
	if (full_screen_window)
		{
		full_screen_stop();
		}
	else
		{
		full_screen_start();
		}
}

/*
 *----------------------------------------------------------------------------
 * main image manipulation
 *----------------------------------------------------------------------------
 */

void image_scroll(gint x, gint y)
{
	image_area_scroll(main_image, x, y);
}

void image_adjust_zoom(gint increment)
{
	image_area_adjust_zoom(main_image, increment);
}

void image_set_zoom(gint zoom)
{
	image_area_set_zoom(main_image, zoom);
}

void image_set_path(gchar *path)
{
	image_area_set_path(main_image, path);
}

gchar *image_get_path()
{
	return image_area_get_path(main_image);
}

gchar *image_get_name()
{
	return image_area_get_name(main_image);
}

void image_change_to(gchar *path)
{
	image_area_set_image(main_image, path, get_default_zoom(main_image));
}

void image_set_labels(GtkWidget *info, GtkWidget *zoom)
{
	image_area_set_labels(normal_image, info, zoom);
}

GtkWidget *image_create()
{
	normal_image = image_area_new(mainwindow);

	main_image = normal_image;

	image_area_set_button(main_image, 1, button1_cb, NULL);
	image_area_set_button(main_image, 2, button2_cb, NULL);
	image_area_set_button(main_image, 3, button3_cb, NULL);

	return main_image->eventbox;
}

void image_to_root()
{
	image_area_to_root(main_image, (image_area_get_zoom(main_image) == 0));
}

