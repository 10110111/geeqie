/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "layout_util.h"

#include "bar_info.h"
#include "bar_exif.h"
#include "bar_sort.h"
#include "cache_maint.h"
#include "collect.h"
#include "collect-dlg.h"
#include "compat.h"
#include "dupe.h"
#include "editors.h"
#include "filedata.h"
#include "image-overlay.h"
#include "img-view.h"
#include "info.h"
#include "layout_image.h"
#include "logwindow.h"
#include "pan-view.h"
#include "pixbuf_util.h"
#include "preferences.h"
#include "print.h"
#include "search.h"
#include "utilops.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_misc.h"
#include "ui_tabcomp.h"
#include "view_dir.h"
#include "window.h"

#include <gdk/gdkkeysyms.h> /* for keyboard values */


#define MENU_EDIT_ACTION_OFFSET 16


/*
 *-----------------------------------------------------------------------------
 * keyboard handler
 *-----------------------------------------------------------------------------
 */

static guint tree_key_overrides[] = {
	GDK_Page_Up,	GDK_KP_Page_Up,
	GDK_Page_Down,	GDK_KP_Page_Down,
	GDK_Home,	GDK_KP_Home,
	GDK_End,	GDK_KP_End
};

static gboolean layout_key_match(guint keyval)
{
	guint i;

	for (i = 0; i < sizeof(tree_key_overrides) / sizeof(guint); i++)
		{
		if (keyval == tree_key_overrides[i]) return TRUE;
		}

	return FALSE;
}

gint layout_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	LayoutWindow *lw = data;
	gint stop_signal = FALSE;
	gint x = 0;
	gint y = 0;

	if (lw->path_entry && GTK_WIDGET_HAS_FOCUS(lw->path_entry))
		{
		if (event->keyval == GDK_Escape && lw->dir_fd)
			{
			gtk_entry_set_text(GTK_ENTRY(lw->path_entry), lw->dir_fd->path);
			}

		/* the gtkaccelgroup of the window is stealing presses before they get to the entry (and more),
		 * so when the some widgets have focus, give them priority (HACK)
		 */
		if (gtk_widget_event(lw->path_entry, (GdkEvent *)event))
			{
			return TRUE;
			}
		}
	if (lw->vd && lw->dir_view_type == DIRVIEW_TREE && GTK_WIDGET_HAS_FOCUS(lw->vd->view) &&
	    !layout_key_match(event->keyval) &&
	    gtk_widget_event(lw->vd->view, (GdkEvent *)event))
		{
		return TRUE;
		}
	if (lw->bar_info &&
	    bar_info_event(lw->bar_info, (GdkEvent *)event))
		{
		return TRUE;
		}

/*
	if (event->type == GDK_KEY_PRESS && lw->full_screen &&
    	    gtk_accel_groups_activate(G_OBJECT(lw->window), event->keyval, event->state))
		return TRUE;
*/

	if (lw->image &&
	    (GTK_WIDGET_HAS_FOCUS(lw->image->widget) || (lw->tools && widget == lw->window) || lw->full_screen) )
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
			default:
				stop_signal = FALSE;
				break;
			}

		if (!stop_signal &&
		    !(event->state & GDK_CONTROL_MASK))
			{
			stop_signal = TRUE;
			switch (event->keyval)
				{
				case GDK_Menu:
					layout_image_menu_popup(lw);
					break;
				default:
					stop_signal = FALSE;
					break;
				}
			}
		}

	if (x != 0 || y!= 0)
		{
		if (event->state & GDK_SHIFT_MASK)
			{
			x *= 3;
			y *= 3;
			}

		keyboard_scroll_calc(&x, &y, event);
		layout_image_scroll(lw, x, y);
		}

	return stop_signal;
}

void layout_keyboard_init(LayoutWindow *lw, GtkWidget *window)
{
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(layout_key_press_cb), lw);
}

/*
 *-----------------------------------------------------------------------------
 * menu callbacks
 *-----------------------------------------------------------------------------
 */


static GtkWidget *layout_window(LayoutWindow *lw)
{
	return lw->full_screen ? lw->full_screen->window : lw->window;
}

static void layout_exit_fullscreen(LayoutWindow *lw)
{
	if (!lw->full_screen) return;
	layout_image_full_screen_stop(lw);
}

static void layout_menu_new_window_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	LayoutWindow *nw;

	layout_exit_fullscreen(lw);

	nw = layout_new(NULL, FALSE, FALSE);
	layout_sort_set(nw, options->file_sort.method, options->file_sort.ascending);
	layout_set_fd(nw, lw->dir_fd);
}

static void layout_menu_new_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	collection_window_new(NULL);
}

static void layout_menu_open_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	collection_dialog_load(NULL);
}

static void layout_menu_search_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	search_new(lw->dir_fd, layout_image_get_fd(lw));
}

static void layout_menu_dupes_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	dupe_window_new(DUPE_MATCH_NAME);
}

static void layout_menu_pan_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	pan_window_new(lw->dir_fd);
}

static void layout_menu_print_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	print_window_new(layout_image_get_fd(lw), layout_selection_list(lw), layout_list(lw), layout_window(lw));
}

static void layout_menu_dir_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	file_util_create_dir(lw->dir_fd, layout_window(lw));
}

static void layout_menu_copy_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	file_util_copy(NULL, layout_selection_list(lw), NULL, layout_window(lw));
}

static void layout_menu_copy_path_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	file_util_copy_path_list_to_clipboard(layout_selection_list(lw));
}

static void layout_menu_move_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	file_util_move(NULL, layout_selection_list(lw), NULL, layout_window(lw));
}

static void layout_menu_rename_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	file_util_rename(NULL, layout_selection_list(lw), layout_window(lw));
}

static void layout_menu_delete_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	file_util_delete(NULL, layout_selection_list(lw), layout_window(lw));
}

static void layout_menu_close_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	layout_close(lw);
}

static void layout_menu_exit_cb(GtkAction *action, gpointer data)
{
	exit_program();
}

static void layout_menu_alter_90_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_ROTATE_90);
}

static void layout_menu_alter_90cc_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_ROTATE_90_CC);
}

static void layout_menu_alter_180_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_ROTATE_180);
}

static void layout_menu_alter_mirror_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_MIRROR);
}

static void layout_menu_alter_flip_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_FLIP);
}

static void layout_menu_alter_desaturate_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_DESATURATE);
}

static void layout_menu_alter_none_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_alter(lw, ALTER_NONE);
}

static void layout_menu_info_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	GList *list;
	FileData *fd = NULL;

	list = layout_selection_list(lw);
	if (!list) fd = layout_image_get_fd(lw);

	info_window_new(fd, list, NULL);
}


static void layout_menu_config_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	show_config_window();
}

static void layout_menu_remove_thumb_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	cache_manager_show();
}

static void layout_menu_wallpaper_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_to_root(lw);
}

static void layout_menu_zoom_in_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_adjust(lw, get_zoom_increment());
}

static void layout_menu_zoom_out_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_adjust(lw, -get_zoom_increment());
}

static void layout_menu_zoom_1_1_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 1.0);
}

static void layout_menu_zoom_fit_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 0.0);
}

static void layout_menu_zoom_fit_hor_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set_fill_geometry(lw, TRUE);
}

static void layout_menu_zoom_fit_vert_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set_fill_geometry(lw, FALSE);
}

static void layout_menu_zoom_2_1_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 2.0);
}

static void layout_menu_zoom_3_1_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 3.0);
}
static void layout_menu_zoom_4_1_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 4.0);
}

static void layout_menu_zoom_1_2_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, -2.0);
}

static void layout_menu_zoom_1_3_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, -3.0);
}

static void layout_menu_zoom_1_4_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, -4.0);
}


static void layout_menu_split_cb(GtkRadioAction *action, GtkRadioAction *current, gpointer data)
{
	LayoutWindow *lw = data;
	ImageSplitMode mode;

	layout_exit_fullscreen(lw);

	mode = gtk_radio_action_get_current_value(action);
	if (mode == lw->split_mode) mode = 0; /* toggle back */

	layout_split_change(lw, mode);
}

static void layout_menu_connect_scroll_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	lw->connect_scroll = gtk_toggle_action_get_active(action);
}

static void layout_menu_connect_zoom_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	lw->connect_zoom = gtk_toggle_action_get_active(action);
}


static void layout_menu_thumb_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_thumb_set(lw, gtk_toggle_action_get_active(action));
}


static void layout_menu_list_cb(GtkRadioAction *action, GtkRadioAction *current, gpointer data)
{
	LayoutWindow *lw = data;
	
	layout_exit_fullscreen(lw);
	layout_views_set(lw, lw->dir_view_type, (gtk_radio_action_get_current_value(action) == 1) ? FILEVIEW_ICON : FILEVIEW_LIST);
}

static void layout_menu_view_dir_as_cb(GtkRadioAction *action, GtkRadioAction *current, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	layout_views_set(lw, (DirViewType) gtk_radio_action_get_current_value(action), lw->file_view_type);
}

static void layout_menu_view_in_new_window_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	view_window_new(layout_image_get_fd(lw));
}

static void layout_menu_fullscreen_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_full_screen_toggle(lw);
}

static void layout_menu_escape_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);

	/* FIXME:interrupting thumbs no longer allowed */
#if 0
	interrupt_thumbs();
#endif
}

static void layout_menu_overlay_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	image_osd_toggle(lw->image);
}

static void layout_menu_histogram_chan_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	image_osd_histogram_chan_toggle(lw->image);
}

static void layout_menu_histogram_log_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	image_osd_histogram_log_toggle(lw->image);
}

static void layout_menu_refresh_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_refresh(lw);
}

static void layout_menu_float_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	
	layout_exit_fullscreen(lw);

	if (lw->tools_float == gtk_toggle_action_get_active(action)) return;
	layout_tools_float_toggle(lw);
}

static void layout_menu_hide_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	layout_tools_hide_toggle(lw);
}

static void layout_menu_toolbar_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);

	if (lw->toolbar_hidden == gtk_toggle_action_get_active(action)) return;
	layout_toolbar_toggle(lw);
}

static void layout_menu_bar_info_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);

	if (lw->bar_info_enabled == gtk_toggle_action_get_active(action)) return;
	layout_bar_info_toggle(lw);
}

static void layout_menu_bar_exif_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	
	layout_exit_fullscreen(lw);

	if (lw->bar_exif_enabled == gtk_toggle_action_get_active(action)) return;
	layout_bar_exif_toggle(lw);
}

static void layout_menu_bar_sort_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);

	if (lw->bar_sort_enabled == gtk_toggle_action_get_active(action)) return;
	layout_bar_sort_toggle(lw);
}

static void layout_menu_slideshow_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_slideshow_toggle(lw);
}

static void layout_menu_slideshow_pause_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_slideshow_pause_toggle(lw);
}

static void layout_menu_help_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	help_window_show("html_contents");
}

static void layout_menu_help_keys_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	help_window_show("documentation");
}

static void layout_menu_notes_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	
	layout_exit_fullscreen(lw);
	help_window_show("release_notes");
}

static void layout_menu_about_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	show_about_window();
}

static void layout_menu_log_window_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_exit_fullscreen(lw);
	log_window_new();
}


/*
 *-----------------------------------------------------------------------------
 * select menu
 *-----------------------------------------------------------------------------
 */

static void layout_menu_select_all_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_select_all(lw);
}

static void layout_menu_unselect_all_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_select_none(lw);
}

static void layout_menu_invert_selection_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_select_invert(lw);
}

static void layout_menu_marks_cb(GtkToggleAction *action, gpointer data)
{
	LayoutWindow *lw = data;

	layout_marks_set(lw, gtk_toggle_action_get_active(action));
}


static void layout_menu_set_mark_sel_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_selection_to_mark(lw, mark, STM_MODE_SET);
}

static void layout_menu_res_mark_sel_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_selection_to_mark(lw, mark, STM_MODE_RESET);
}

static void layout_menu_toggle_mark_sel_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_selection_to_mark(lw, mark, STM_MODE_TOGGLE);
}

static void layout_menu_sel_mark_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_mark_to_selection(lw, mark, MTS_MODE_SET);
}

static void layout_menu_sel_mark_or_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_mark_to_selection(lw, mark, MTS_MODE_OR);
}

static void layout_menu_sel_mark_and_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_mark_to_selection(lw, mark, MTS_MODE_AND);
}

static void layout_menu_sel_mark_minus_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	gint mark = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "mark_num"));
	g_assert(mark >= 1 && mark <= FILEDATA_MARKS_SIZE);

	layout_mark_to_selection(lw, mark, MTS_MODE_MINUS);
}


/*
 *-----------------------------------------------------------------------------
 * go menu
 *-----------------------------------------------------------------------------
 */

static void layout_menu_image_first_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	layout_image_first(lw);
}

static void layout_menu_image_prev_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	layout_image_prev(lw);
}

static void layout_menu_image_next_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	layout_image_next(lw);
}

static void layout_menu_image_last_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	layout_image_last(lw);
}


/*
 *-----------------------------------------------------------------------------
 * edit menu
 *-----------------------------------------------------------------------------
 */

static void layout_menu_edit_cb(GtkAction *action, gpointer data)
{
	LayoutWindow *lw = data;
	GList *list;
	gint n;

	n = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "edit_index"));

	if (!editor_window_flag_set(n))
		layout_exit_fullscreen(lw);

	list = layout_selection_list(lw);
	file_util_start_editor_from_filelist(n, list, lw->window);
	filelist_free(list);
}

static void layout_menu_edit_update(LayoutWindow *lw)
{
	gint i;

	/* main edit menu */

	if (!lw->action_group) return;

	for (i = 0; i < GQ_EDITOR_GENERIC_SLOTS; i++)
		{
		gchar *key;
		GtkAction *action;
		const gchar *name;
	
		key = g_strdup_printf("Editor%d", i);

		action = gtk_action_group_get_action(lw->action_group, key);
		g_object_set_data(G_OBJECT(action), "edit_index", GINT_TO_POINTER(i));

		name = editor_get_name(i);
		if (name)
			{
			gchar *text = g_strdup_printf(_("_%d %s..."), i, name);

			g_object_set(action, "label", text,
					     "sensitive", TRUE, NULL);
			g_free(text);
			}
		else
			{
			gchar *text;

			text = g_strdup_printf(_("_%d empty"), i);
			g_object_set(action, "label", text, "sensitive", FALSE, NULL);
			g_free(text);
			}

		g_free(key);
		}
}

void layout_edit_update_all(void)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		layout_menu_edit_update(lw);
		}
}

/*
 *-----------------------------------------------------------------------------
 * recent menu
 *-----------------------------------------------------------------------------
 */

static void layout_menu_recent_cb(GtkWidget *widget, gpointer data)
{
	gint n;
	gchar *path;

	n = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "recent_index"));

	path = g_list_nth_data(history_list_get_by_key("recent"), n);

	if (!path) return;

	/* make a copy of it */
	path = g_strdup(path);
	collection_window_new(path);
	g_free(path);
}

static void layout_menu_recent_update(LayoutWindow *lw)
{
	GtkWidget *menu;
	GtkWidget *recent;
	GtkWidget *item;
	GList *list;
	gint n;

	if (!lw->ui_manager) return;

	list = history_list_get_by_key("recent");
	n = 0;

	menu = gtk_menu_new();

	while (list)
		{
		item = menu_item_add_simple(menu, filename_from_path((gchar *)list->data),
					    G_CALLBACK(layout_menu_recent_cb), lw);
		g_object_set_data(G_OBJECT(item), "recent_index", GINT_TO_POINTER(n));
		list = list->next;
		n++;
		}

	if (n == 0)
		{
		menu_item_add(menu, _("Empty"), NULL, NULL);
		}

	recent = gtk_ui_manager_get_widget(lw->ui_manager, "/MainMenu/FileMenu/OpenRecent");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(recent), menu);
	gtk_widget_set_sensitive(recent, (n != 0));
}

void layout_recent_update_all(void)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		layout_menu_recent_update(lw);
		}
}

void layout_recent_add_path(const gchar *path)
{
	if (!path) return;

	history_list_add_to_key("recent", path, options->open_recent_list_maxsize);

	layout_recent_update_all();
}

/*
 *-----------------------------------------------------------------------------
 * copy path
 *-----------------------------------------------------------------------------
 */

static void layout_copy_path_update(LayoutWindow *lw)
{
	GtkWidget *item = gtk_ui_manager_get_widget(lw->ui_manager, "/MainMenu/FileMenu/CopyPath");

	if (!item) return;
	
	if (options->show_copy_path)
		gtk_widget_show(item);
	else
		gtk_widget_hide(item);
}

void layout_copy_path_update_all(void)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		layout_copy_path_update(lw);
		}
}

/*
 *-----------------------------------------------------------------------------
 * menu
 *-----------------------------------------------------------------------------
 */

#define CB G_CALLBACK

static GtkActionEntry menu_entries[] = {
  { "FileMenu",		NULL,		N_("_File"),			NULL, 		NULL, 	NULL },
  { "GoMenu",		NULL,		N_("_Go"),			NULL, 		NULL, 	NULL },
  { "EditMenu",		NULL,		N_("_Edit"),			NULL, 		NULL, 	NULL },
  { "SelectMenu",	NULL,		N_("_Select"),			NULL, 		NULL, 	NULL },
  { "AdjustMenu",	NULL,		N_("_Adjust"),			NULL, 		NULL, 	NULL },
  { "ViewMenu",		NULL,		N_("_View"),			NULL, 		NULL, 	NULL },
  { "DirMenu",          NULL,           N_("_View Directory as"),	NULL, 		NULL, 	NULL },
  { "ZoomMenu",		NULL,		N_("_Zoom"),			NULL, 		NULL, 	NULL },
  { "SplitMenu",	NULL,		N_("_Split"),			NULL, 		NULL, 	NULL },
  { "HelpMenu",		NULL,		N_("_Help"),			NULL, 		NULL, 	NULL },

  { "FirstImage",	GTK_STOCK_GOTO_TOP,	N_("_First Image"),	"Home",		NULL,	CB(layout_menu_image_first_cb) },
  { "PrevImage",	GTK_STOCK_GO_UP,   	N_("_Previous Image"),	"BackSpace",	NULL,	CB(layout_menu_image_prev_cb) },
  { "PrevImageAlt1",	GTK_STOCK_GO_UP,   	N_("_Previous Image"),	"Page_Up",	NULL,	CB(layout_menu_image_prev_cb) },
  { "PrevImageAlt2",	GTK_STOCK_GO_UP,   	N_("_Previous Image"),	"KP_Page_Up",	NULL,	CB(layout_menu_image_prev_cb) },
  { "NextImage",	GTK_STOCK_GO_DOWN,	N_("_Next Image"),	"space",	NULL,	CB(layout_menu_image_next_cb) },
  { "NextImageAlt1",	GTK_STOCK_GO_DOWN,	N_("_Next Image"),	"Page_Down",	NULL,	CB(layout_menu_image_next_cb) },
  { "NextImageAlt2",	GTK_STOCK_GO_DOWN,	N_("_Next Image"),	"KP_Page_Down",	NULL,	CB(layout_menu_image_next_cb) },
  { "LastImage",	GTK_STOCK_GOTO_BOTTOM,	N_("_Last Image"),	"End",		NULL,	CB(layout_menu_image_last_cb) },


  { "NewWindow",	GTK_STOCK_NEW,	N_("New _window"),	NULL,		NULL,	CB(layout_menu_new_window_cb) },
  { "NewCollection",	GTK_STOCK_INDEX,N_("_New collection"),	"C",		NULL,	CB(layout_menu_new_cb) },
  { "OpenCollection",	GTK_STOCK_OPEN,	N_("_Open collection..."),"O",		NULL,	CB(layout_menu_open_cb) },
  { "OpenRecent",	NULL,		N_("Open _recent"),	NULL,		NULL,	NULL },
  { "Search",		GTK_STOCK_FIND,	N_("_Search..."),	"F3",		NULL,	CB(layout_menu_search_cb) },
  { "FindDupes",	GTK_STOCK_FIND,	N_("_Find duplicates..."),"D",		NULL,	CB(layout_menu_dupes_cb) },
  { "PanView",		NULL,		N_("Pan _view"),	"<control>J",	NULL,	CB(layout_menu_pan_cb) },
  { "Print",		GTK_STOCK_PRINT,N_("_Print..."),	"<shift>P",	NULL,	CB(layout_menu_print_cb) },
  { "NewFolder",	NULL,		N_("N_ew folder..."),	"<control>F",	NULL,	CB(layout_menu_dir_cb) },
  { "Copy",		NULL,		N_("_Copy..."),		"<control>C",	NULL,	CB(layout_menu_copy_cb) },
  { "Move",		NULL,		N_("_Move..."),		"<control>M",	NULL,	CB(layout_menu_move_cb) },
  { "Rename",		NULL,		N_("_Rename..."),	"<control>R",	NULL,	CB(layout_menu_rename_cb) },
  { "Delete",	GTK_STOCK_DELETE,	N_("_Delete..."),	"<control>D",	NULL,	CB(layout_menu_delete_cb) },
  { "DeleteAlt1",GTK_STOCK_DELETE,	N_("_Delete..."),	"Delete",	NULL,	CB(layout_menu_delete_cb) },
  { "DeleteAlt2",GTK_STOCK_DELETE,	N_("_Delete..."),	"KP_Delete",	NULL,	CB(layout_menu_delete_cb) },
  { "CopyPath",		NULL,		N_("_Copy path"),	NULL,		NULL,	CB(layout_menu_copy_path_cb) },
  { "CloseWindow",	GTK_STOCK_CLOSE,N_("C_lose window"),	"<control>W",	NULL,	CB(layout_menu_close_cb) },
  { "Quit",		GTK_STOCK_QUIT, N_("_Quit"),		"<control>Q",	NULL,	CB(layout_menu_exit_cb) },

  { "Editor0",		NULL,		"editor0",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor1",		NULL,		"editor1",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor2",		NULL,		"editor2",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor3",		NULL,		"editor3",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor4",		NULL,		"editor4",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor5",		NULL,		"editor5",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor6",		NULL,		"editor6",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor7",		NULL,		"editor7",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor8",		NULL,		"editor8",		NULL,		NULL,	CB(layout_menu_edit_cb) },
  { "Editor9",		NULL,		"editor9",		NULL,		NULL,	CB(layout_menu_edit_cb) },

  { "RotateCW",		NULL,	N_("_Rotate clockwise"),	"bracketright",	NULL,	CB(layout_menu_alter_90_cb) },
  { "RotateCCW",	NULL,	N_("Rotate _counterclockwise"),	"bracketleft",	NULL,	CB(layout_menu_alter_90cc_cb) },
  { "Rotate180",	NULL,		N_("Rotate 1_80"),	"<shift>R",	NULL,	CB(layout_menu_alter_180_cb) },
  { "Mirror",		NULL,		N_("_Mirror"),		"<shift>M",	NULL,	CB(layout_menu_alter_mirror_cb) },
  { "Flip",		NULL,		N_("_Flip"),		"<shift>F",	NULL,	CB(layout_menu_alter_flip_cb) },
  { "Grayscale",	NULL,		N_("Toggle _grayscale"),"<shift>G",	NULL,	CB(layout_menu_alter_desaturate_cb) },
  { "AlterNone",	NULL,		N_("_Original state"),  "<shift>O",	NULL,	CB(layout_menu_alter_none_cb) },

  { "Properties",GTK_STOCK_PROPERTIES,	N_("_Properties"),	"<control>P",	NULL,	CB(layout_menu_info_cb) },
  { "SelectAll",	NULL,		N_("Select _all"),	"<control>A",	NULL,	CB(layout_menu_select_all_cb) },
  { "SelectNone",	NULL,		N_("Select _none"), "<control><shift>A",NULL,	CB(layout_menu_unselect_all_cb) },
  { "SelectInvert",	NULL,		N_("_Invert Selection"), "<control><shift>I",	NULL,	CB(layout_menu_invert_selection_cb) },

  { "Preferences",GTK_STOCK_PREFERENCES,N_("P_references..."),	"<control>O",	NULL,	CB(layout_menu_config_cb) },
  { "Maintenance",	NULL,		N_("_Thumbnail maintenance..."),NULL,	NULL,	CB(layout_menu_remove_thumb_cb) },
  { "Wallpaper",	NULL,		N_("Set as _wallpaper"),NULL,		NULL,	CB(layout_menu_wallpaper_cb) },

  { "ZoomIn",	GTK_STOCK_ZOOM_IN,	N_("Zoom _in"),		"equal",	NULL,	CB(layout_menu_zoom_in_cb) },
  { "ZoomInAlt1",GTK_STOCK_ZOOM_IN,	N_("Zoom _in"),		"plus",		NULL,	CB(layout_menu_zoom_in_cb) },
  { "ZoomInAlt2",GTK_STOCK_ZOOM_IN,	N_("Zoom _in"),		"KP_Add",	NULL,	CB(layout_menu_zoom_in_cb) },
  { "ZoomOut",	GTK_STOCK_ZOOM_OUT,	N_("Zoom _out"),	"minus",	NULL,	CB(layout_menu_zoom_out_cb) },
  { "ZoomOutAlt1",GTK_STOCK_ZOOM_OUT,	N_("Zoom _out"),	"KP_Subtract",	NULL,	CB(layout_menu_zoom_out_cb) },
  { "Zoom100",	GTK_STOCK_ZOOM_100,	N_("Zoom _1:1"),	"Z",		NULL,	CB(layout_menu_zoom_1_1_cb) },
  { "Zoom100Alt1",GTK_STOCK_ZOOM_100,	N_("Zoom _1:1"),	"KP_Divide",		NULL,	CB(layout_menu_zoom_1_1_cb) },
  { "ZoomFit",	GTK_STOCK_ZOOM_FIT,	N_("_Zoom to fit"),	"X",		NULL,	CB(layout_menu_zoom_fit_cb) },
  { "ZoomFitAlt1",GTK_STOCK_ZOOM_FIT,	N_("_Zoom to fit"),	"KP_Multiply",	NULL,	CB(layout_menu_zoom_fit_cb) },
  { "ZoomFillHor",	NULL,		N_("Fit _Horizontally"),"H",		NULL,	CB(layout_menu_zoom_fit_hor_cb) },
  { "ZoomFillVert",	NULL,		N_("Fit _Vorizontally"),"W",		NULL,	CB(layout_menu_zoom_fit_vert_cb) },
  { "Zoom200",	        NULL,		N_("Zoom _2:1"),	NULL,		NULL,	CB(layout_menu_zoom_2_1_cb) },
  { "Zoom300",	        NULL,		N_("Zoom _3:1"),	NULL,		NULL,	CB(layout_menu_zoom_3_1_cb) },
  { "Zoom400",		NULL,		N_("Zoom _4:1"),	NULL,		NULL,	CB(layout_menu_zoom_4_1_cb) },
  { "Zoom50",		NULL,		N_("Zoom 1:2"),		NULL,		NULL,	CB(layout_menu_zoom_1_2_cb) },
  { "Zoom33",		NULL,		N_("Zoom 1:3"),		NULL,		NULL,	CB(layout_menu_zoom_1_3_cb) },
  { "Zoom25",		NULL,		N_("Zoom 1:4"),		NULL,		NULL,	CB(layout_menu_zoom_1_4_cb) },


  { "ViewInNewWindow",	NULL,		N_("_View in new window"),	"<control>V",		NULL,	CB(layout_menu_view_in_new_window_cb) },

  { "FullScreen",	NULL,		N_("F_ull screen"),	"F",		NULL,	CB(layout_menu_fullscreen_cb) },
  { "FullScreenAlt1",	NULL,		N_("F_ull screen"),	"V",		NULL,	CB(layout_menu_fullscreen_cb) },
  { "FullScreenAlt2",	NULL,		N_("F_ull screen"),	"F11",		NULL,	CB(layout_menu_fullscreen_cb) },
  { "Escape",		NULL,		N_("Escape"),		"Escape",	NULL,	CB(layout_menu_escape_cb) },
  { "EscapeAlt1",	NULL,		N_("Escape"),		"Q",		NULL,	CB(layout_menu_escape_cb) },
  { "ImageOverlay",	NULL,		N_("_Image Overlay"),	"I",		NULL,	CB(layout_menu_overlay_cb) },
  { "HistogramChan",	NULL,	N_("Histogram _channels"),	"K",		NULL,	CB(layout_menu_histogram_chan_cb) },
  { "HistogramLog",	NULL,	N_("Histogram _log mode"),	"J",		NULL,	CB(layout_menu_histogram_log_cb) },
  { "HideTools",	NULL,		N_("_Hide file list"),	"<control>H",	NULL,	CB(layout_menu_hide_cb) },
  { "SlideShowPause",	NULL,		N_("_Pause slideshow"), "P",		NULL,	CB(layout_menu_slideshow_pause_cb) },
  { "Refresh",	GTK_STOCK_REFRESH,	N_("_Refresh"),		"R",		NULL,	CB(layout_menu_refresh_cb) },

  { "HelpContents",	GTK_STOCK_HELP,	N_("_Contents"),	"F1",		NULL,	CB(layout_menu_help_cb) },
  { "HelpShortcuts",	NULL,		N_("_Keyboard shortcuts"),NULL,		NULL,	CB(layout_menu_help_keys_cb) },
  { "HelpNotes",	NULL,		N_("_Release notes"),	NULL,		NULL,	CB(layout_menu_notes_cb) },
  { "About",		NULL,		N_("_About"),		NULL,		NULL,	CB(layout_menu_about_cb) },
  { "LogWindow",	NULL,		N_("_Log Window"),	NULL,		NULL,	CB(layout_menu_log_window_cb) }
};

static GtkToggleActionEntry menu_toggle_entries[] = {
  { "Thumbnails",	NULL,		N_("_Thumbnails"),	"T",		NULL,	CB(layout_menu_thumb_cb),	 FALSE },
  { "ShowMarks",        NULL,		N_("Show _Marks"),	"M",		NULL,	CB(layout_menu_marks_cb),	 FALSE  },
  { "FloatTools",	NULL,		N_("_Float file list"),	"L",		NULL,	CB(layout_menu_float_cb),	 FALSE  },
  { "HideToolbar",	NULL,		N_("Hide tool_bar"),	NULL,		NULL,	CB(layout_menu_toolbar_cb),	 FALSE  },
  { "SBarKeywords",	NULL,		N_("_Keywords"),	"<control>K",	NULL,	CB(layout_menu_bar_info_cb),	 FALSE  },
  { "SBarExif",		NULL,		N_("E_xif data"),	"<control>E",	NULL,	CB(layout_menu_bar_exif_cb),	 FALSE  },
  { "SBarSort",		NULL,		N_("Sort _manager"),	"<control>S",	NULL,	CB(layout_menu_bar_sort_cb),	 FALSE  },
  { "ConnectScroll",	NULL,		N_("Co_nnected scroll"),"<control>U",	NULL,	CB(layout_menu_connect_scroll_cb), FALSE  },
  { "ConnectZoom",	NULL,		N_("C_onnected zoom"),	"<control>Y",	NULL,	CB(layout_menu_connect_zoom_cb), FALSE  },
  { "SlideShow",	NULL,		N_("Toggle _slideshow"),"S",		NULL,	CB(layout_menu_slideshow_cb),	 FALSE  },
};

static GtkRadioActionEntry menu_radio_entries[] = {
  { "ViewList",		NULL,		N_("_List"),		"<control>L",	NULL,	0 },
  { "ViewIcons",	NULL,		N_("I_cons"),		"<control>I",	NULL,	1 }
};

static GtkRadioActionEntry menu_split_radio_entries[] = {
  { "SplitHorizontal",	NULL,		N_("Horizontal"),	"E",		NULL,	SPLIT_HOR },
  { "SplitVertical",	NULL,		N_("Vertical"),		"U",		NULL,	SPLIT_VERT },
  { "SplitQuad",	NULL,		N_("Quad"),		NULL,		NULL,	SPLIT_QUAD },
  { "SplitSingle",	NULL,		N_("Single"),		"Y",		NULL,	SPLIT_NONE }
};


#undef CB

static const char *menu_ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='NewWindow'/>"
"      <menuitem action='NewCollection'/>"
"      <menuitem action='OpenCollection'/>"
"      <menuitem action='OpenRecent'/>"
"      <separator/>"
"      <menuitem action='Search'/>"
"      <menuitem action='FindDupes'/>"
"      <menuitem action='PanView'/>"
"      <separator/>"
"      <menuitem action='Print'/>"
"      <menuitem action='NewFolder'/>"
"      <separator/>"
"      <menuitem action='Copy'/>"
"      <menuitem action='Move'/>"
"      <menuitem action='Rename'/>"
"      <menuitem action='Delete'/>"
"      <menuitem action='CopyPath'/>"
"      <separator/>"
"      <menuitem action='CloseWindow'/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='GoMenu'>"
"      <menuitem action='FirstImage'/>"
"      <menuitem action='PrevImage'/>"
"      <menuitem action='NextImage'/>"
"      <menuitem action='LastImage'/>"
"    </menu>"
"    <menu action='SelectMenu'>"
"      <menuitem action='SelectAll'/>"
"      <menuitem action='SelectNone'/>"
"      <menuitem action='SelectInvert'/>"
"      <separator/>"
"      <menuitem action='ShowMarks'/>"
"      <separator/>"
"    </menu>"
"    <menu action='EditMenu'>"
"      <menuitem action='Editor0'/>"
"      <menuitem action='Editor1'/>"
"      <menuitem action='Editor2'/>"
"      <menuitem action='Editor3'/>"
"      <menuitem action='Editor4'/>"
"      <menuitem action='Editor5'/>"
"      <menuitem action='Editor6'/>"
"      <menuitem action='Editor7'/>"
"      <menuitem action='Editor8'/>"
"      <menuitem action='Editor9'/>"
"      <separator/>"
"      <menu action='AdjustMenu'>"
"        <menuitem action='RotateCW'/>"
"        <menuitem action='RotateCCW'/>"
"        <menuitem action='Rotate180'/>"
"        <menuitem action='Mirror'/>"
"        <menuitem action='Flip'/>"
"        <menuitem action='Grayscale'/>"
"        <menuitem action='AlterNone'/>"
"      </menu>"
"      <menuitem action='Properties'/>"
"      <separator/>"
"      <menuitem action='Preferences'/>"
"      <menuitem action='Maintenance'/>"
"      <separator/>"
"      <menuitem action='Wallpaper'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menuitem action='ViewInNewWindow'/>"
"      <separator/>"
"      <menu action='ZoomMenu'>"
"        <menuitem action='ZoomIn'/>"
"        <menuitem action='ZoomOut'/>"
"        <menuitem action='ZoomFit'/>"
"        <menuitem action='ZoomFillHor'/>"
"        <menuitem action='ZoomFillVert'/>"
"        <menuitem action='Zoom100'/>"
"        <menuitem action='Zoom200'/>"
"        <menuitem action='Zoom300'/>"
"        <menuitem action='Zoom400'/>"
"        <menuitem action='Zoom50'/>"
"        <menuitem action='Zoom33'/>"
"        <menuitem action='Zoom25'/>"
"      </menu>"
"      <separator/>"
"      <menu action='SplitMenu'>"
"        <menuitem action='SplitHorizontal'/>"
"        <menuitem action='SplitVertical'/>"
"        <menuitem action='SplitQuad'/>"
"        <menuitem action='SplitSingle'/>"
"      </menu>"
"      <menuitem action='ConnectScroll'/>"
"      <menuitem action='ConnectZoom'/>"
"      <separator/>"
"      <menuitem action='Thumbnails'/>"
"      <menuitem action='ViewList'/>"
"      <menuitem action='ViewIcons'/>"
"      <separator/>"
"      <menu action='DirMenu'>"
"        <menuitem action='FolderList'/>"
"        <menuitem action='FolderTree'/>"
"      </menu>"
"      <separator/>"
"      <menuitem action='ImageOverlay'/>"
"      <menuitem action='HistogramChan'/>"
"      <menuitem action='HistogramLog'/>"
"      <menuitem action='FullScreen'/>"
"      <separator/>"
"      <menuitem action='FloatTools'/>"
"      <menuitem action='HideTools'/>"
"      <menuitem action='HideToolbar'/>"
"      <separator/>"
"      <menuitem action='SBarKeywords'/>"
"      <menuitem action='SBarExif'/>"
"      <menuitem action='SBarSort'/>"
"      <separator/>"
"      <menuitem action='SlideShow'/>"
"      <menuitem action='SlideShowPause'/>"
"      <menuitem action='Refresh'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <separator/>"
"      <menuitem action='HelpContents'/>"
"      <menuitem action='HelpShortcuts'/>"
"      <menuitem action='HelpNotes'/>"
"      <separator/>"
"      <menuitem action='About'/>"
"      <separator/>"
"      <menuitem action='LogWindow'/>"
"    </menu>"
"  </menubar>"
"<accelerator action='PrevImageAlt1'/>"
"<accelerator action='PrevImageAlt2'/>"
"<accelerator action='NextImageAlt1'/>"
"<accelerator action='NextImageAlt2'/>"
"<accelerator action='DeleteAlt1'/>"
"<accelerator action='DeleteAlt2'/>"
"<accelerator action='ZoomInAlt1'/>"
"<accelerator action='ZoomInAlt2'/>"
"<accelerator action='ZoomOutAlt1'/>"
"<accelerator action='Zoom100Alt1'/>"
"<accelerator action='ZoomFitAlt1'/>"
"<accelerator action='FullScreenAlt1'/>"
"<accelerator action='FullScreenAlt2'/>"
"<accelerator action='Escape'/>"
"<accelerator action='EscapeAlt1'/>"
"</ui>";


static gchar *menu_translate(const gchar *path, gpointer data)
{
	return _(path);
}

static void layout_actions_setup_mark(LayoutWindow *lw, gint mark, gchar *name_tmpl, gchar *label_tmpl, gchar *accel_tmpl,  GCallback cb)
{
	gchar name[50];
	gchar label[100];
	gchar accel[50];
	GtkActionEntry entry = { name, NULL, label, accel, NULL, cb };
	GtkAction *action;

	g_snprintf(name, sizeof(name), name_tmpl, mark);
	g_snprintf(label, sizeof(label), label_tmpl, mark);
	if (accel_tmpl)
		g_snprintf(accel, sizeof(accel), accel_tmpl, mark % 10);
	else
		accel[0] = 0;
	gtk_action_group_add_actions(lw->action_group, &entry, 1, lw);
	action = gtk_action_group_get_action(lw->action_group, name);
	g_object_set_data(G_OBJECT(action), "mark_num", GINT_TO_POINTER(mark));
}

static void layout_actions_setup_marks(LayoutWindow *lw)
{
	gint mark;
	GError *error;
	GString *desc = g_string_new(
				"<ui>"
				"  <menubar name='MainMenu'>"
				"    <menu action='SelectMenu'>");

	for (mark = 1; mark <= FILEDATA_MARKS_SIZE; mark++)
		{
		layout_actions_setup_mark(lw, mark, "Mark%d", 		_("Mark _%d"), NULL, NULL);
		layout_actions_setup_mark(lw, mark, "SetMark%d", 	_("_Set mark %d"), 			NULL, G_CALLBACK(layout_menu_set_mark_sel_cb));
		layout_actions_setup_mark(lw, mark, "ResetMark%d", 	_("_Reset mark %d"), 			NULL, G_CALLBACK(layout_menu_res_mark_sel_cb));
		layout_actions_setup_mark(lw, mark, "ToggleMark%d", 	_("_Toggle mark %d"), 			"%d", G_CALLBACK(layout_menu_toggle_mark_sel_cb));
		layout_actions_setup_mark(lw, mark, "ToggleMark%dAlt1",	_("_Toggle mark %d"), 			"KP_%d", G_CALLBACK(layout_menu_toggle_mark_sel_cb));
		layout_actions_setup_mark(lw, mark, "SelectMark%d", 	_("_Select mark %d"), 			"<control>%d", G_CALLBACK(layout_menu_sel_mark_cb));
		layout_actions_setup_mark(lw, mark, "SelectMark%dAlt1",	_("_Select mark %d"), 			"<control>KP_%d", G_CALLBACK(layout_menu_sel_mark_cb));
		layout_actions_setup_mark(lw, mark, "AddMark%d", 	_("_Add mark %d"), 			NULL, G_CALLBACK(layout_menu_sel_mark_or_cb));
		layout_actions_setup_mark(lw, mark, "IntMark%d", 	_("_Intersection with mark %d"), 	NULL, G_CALLBACK(layout_menu_sel_mark_and_cb));
		layout_actions_setup_mark(lw, mark, "UnselMark%d", 	_("_Unselect mark %d"), 		NULL, G_CALLBACK(layout_menu_sel_mark_minus_cb));

		g_string_append_printf(desc,
				"      <menu action='Mark%d'>"
				"        <menuitem action='ToggleMark%d'/>"
				"        <menuitem action='SetMark%d'/>"
				"        <menuitem action='ResetMark%d'/>"
				"        <separator/>"
				"        <menuitem action='SelectMark%d'/>"
				"        <menuitem action='AddMark%d'/>"
				"        <menuitem action='IntMark%d'/>"
				"        <menuitem action='UnselMark%d'/>"
				"      </menu>",
				mark, mark, mark, mark, mark, mark, mark, mark);
		}

	g_string_append(desc,
				"    </menu>"
				"  </menubar>");
	for (mark = 1; mark <= FILEDATA_MARKS_SIZE; mark++)
		{
		g_string_append_printf(desc,
				"<accelerator action='ToggleMark%dAlt1'/>"
				"<accelerator action='SelectMark%dAlt1'/>",
				mark, mark);
		}
	g_string_append(desc,   "</ui>" );

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string(lw->ui_manager, desc->str, -1, &error))
		{
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		exit(EXIT_FAILURE);
		}
	g_string_free(desc, TRUE);
}

void layout_actions_setup(LayoutWindow *lw)
{
	GError *error;

	if (lw->ui_manager) return;

	lw->action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_set_translate_func(lw->action_group, menu_translate, NULL, NULL);

	gtk_action_group_add_actions(lw->action_group,
				     menu_entries, G_N_ELEMENTS(menu_entries), lw);
	gtk_action_group_add_toggle_actions(lw->action_group,
					    menu_toggle_entries, G_N_ELEMENTS(menu_toggle_entries), lw);
	gtk_action_group_add_radio_actions(lw->action_group,
					   menu_radio_entries, G_N_ELEMENTS(menu_radio_entries),
					   0, G_CALLBACK(layout_menu_list_cb), lw);
	gtk_action_group_add_radio_actions(lw->action_group,
					   menu_split_radio_entries, G_N_ELEMENTS(menu_split_radio_entries),
					   0, G_CALLBACK(layout_menu_split_cb), lw);
	gtk_action_group_add_radio_actions(lw->action_group,
					   menu_view_dir_radio_entries, VIEW_DIR_TYPES_COUNT,
					   0, G_CALLBACK(layout_menu_view_dir_as_cb), lw);

	lw->ui_manager = gtk_ui_manager_new();
	gtk_ui_manager_set_add_tearoffs(lw->ui_manager, TRUE);
	gtk_ui_manager_insert_action_group(lw->ui_manager, lw->action_group, 0);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string(lw->ui_manager, menu_ui_description, -1, &error))
		{
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		exit(EXIT_FAILURE);
		}
	
	layout_actions_setup_marks(lw);
	layout_copy_path_update(lw);
}

void layout_actions_add_window(LayoutWindow *lw, GtkWidget *window)
{
	GtkAccelGroup *group;

	if (!lw->ui_manager) return;

	group = gtk_ui_manager_get_accel_group(lw->ui_manager);
	gtk_window_add_accel_group(GTK_WINDOW(window), group);
}

GtkWidget *layout_actions_menu_bar(LayoutWindow *lw)
{
	return gtk_ui_manager_get_widget(lw->ui_manager, "/MainMenu");
}


/*
 *-----------------------------------------------------------------------------
 * toolbar
 *-----------------------------------------------------------------------------
 */

static void layout_button_thumb_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_thumb_set(lw, gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)));
}

static void layout_button_home_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;
	const gchar *path = homedir();
	if (path)
		{
		FileData *dir_fd = file_data_new_simple(path);
		layout_set_fd(lw, dir_fd);
		file_data_unref(dir_fd);
		}
}

static void layout_button_refresh_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_refresh(lw);
}

static void layout_button_zoom_in_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_adjust(lw, get_zoom_increment());
}

static void layout_button_zoom_out_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_adjust(lw, -get_zoom_increment());
}

static void layout_button_zoom_fit_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 0.0);
}

static void layout_button_zoom_1_1_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_zoom_set(lw, 1.0);
}

static void layout_button_config_cb(GtkWidget *widget, gpointer data)
{
	show_config_window();
}

static void layout_button_float_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_tools_float_toggle(lw);
}

static void layout_button_custom_icon(GtkWidget *button, const gchar *key)
{
	GtkWidget *icon;
	GdkPixbuf *pixbuf;

	pixbuf = pixbuf_inline(key);
	if (!pixbuf) return;

	icon = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);

	pref_toolbar_button_set_icon(button, icon, NULL);
	gtk_widget_show(icon);
}

GtkWidget *layout_button_bar(LayoutWindow *lw)
{
	GtkWidget *box;
	GtkWidget *button;

	box =  pref_toolbar_new(NULL, GTK_TOOLBAR_ICONS);

	button = pref_toolbar_button(box, NULL, _("_Thumbnails"), TRUE,
				     _("Show thumbnails"), G_CALLBACK(layout_button_thumb_cb), lw);
	layout_button_custom_icon(button, PIXBUF_INLINE_ICON_THUMB);
	lw->thumb_button = button;

	pref_toolbar_button(box, GTK_STOCK_HOME, NULL, FALSE,
			    _("Change to home folder"), G_CALLBACK(layout_button_home_cb), lw);
	pref_toolbar_button(box, GTK_STOCK_REFRESH, NULL, FALSE,
			    _("Refresh file list"), G_CALLBACK(layout_button_refresh_cb), lw);
	pref_toolbar_button(box, GTK_STOCK_ZOOM_IN, NULL, FALSE,
			    _("Zoom in"), G_CALLBACK(layout_button_zoom_in_cb), lw);
	pref_toolbar_button(box, GTK_STOCK_ZOOM_OUT, NULL, FALSE,
			    _("Zoom out"), G_CALLBACK(layout_button_zoom_out_cb), lw);
	pref_toolbar_button(box, GTK_STOCK_ZOOM_FIT, NULL, FALSE,
			    _("Fit image to window"), G_CALLBACK(layout_button_zoom_fit_cb), lw);
	pref_toolbar_button(box, GTK_STOCK_ZOOM_100, NULL, FALSE,
			    _("Set zoom 1:1"), G_CALLBACK(layout_button_zoom_1_1_cb), lw);
	pref_toolbar_button(box, GTK_STOCK_PREFERENCES, NULL, FALSE,
			    _("Preferences"), G_CALLBACK(layout_button_config_cb), lw);
	button = pref_toolbar_button(box, NULL, _("_Float"), FALSE,
				     _("Float file list"), G_CALLBACK(layout_button_float_cb), lw);
	layout_button_custom_icon(button, PIXBUF_INLINE_ICON_FLOAT);

	return box;
}

/*
 *-----------------------------------------------------------------------------
 * misc
 *-----------------------------------------------------------------------------
 */

static void layout_util_sync_views(LayoutWindow *lw)
{
	GtkAction *action;

	if (!lw->action_group) return;

	action = gtk_action_group_get_action(lw->action_group, "FolderTree");
	radio_action_set_current_value(GTK_RADIO_ACTION(action), lw->dir_view_type);

	action = gtk_action_group_get_action(lw->action_group, "ViewIcons");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->file_view_type);

	action = gtk_action_group_get_action(lw->action_group, "FloatTools");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->tools_float);

	action = gtk_action_group_get_action(lw->action_group, "SBarKeywords");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->bar_info_enabled);

	action = gtk_action_group_get_action(lw->action_group, "SBarExif");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->bar_exif_enabled);

	action = gtk_action_group_get_action(lw->action_group, "SBarSort");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->bar_sort_enabled);

	action = gtk_action_group_get_action(lw->action_group, "HideToolbar");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->toolbar_hidden);

	action = gtk_action_group_get_action(lw->action_group, "ShowMarks");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->marks_enabled);

	action = gtk_action_group_get_action(lw->action_group, "SlideShow");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), layout_image_slideshow_active(lw));
}

void layout_util_sync_thumb(LayoutWindow *lw)
{
	GtkAction *action;

	if (!lw->action_group) return;

	action = gtk_action_group_get_action(lw->action_group, "Thumbnails");
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), lw->thumbs_enabled);
	g_object_set(action, "sensitive", (lw->file_view_type == FILEVIEW_LIST), NULL);

	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(lw->thumb_button), lw->thumbs_enabled);
	gtk_widget_set_sensitive(lw->thumb_button, (lw->file_view_type == FILEVIEW_LIST));
}

void layout_util_sync(LayoutWindow *lw)
{
	layout_util_sync_views(lw);
	layout_util_sync_thumb(lw);
	layout_menu_recent_update(lw);
	layout_menu_edit_update(lw);
}

/*
 *-----------------------------------------------------------------------------
 * icons (since all the toolbar icons are included here, best place as any)
 *-----------------------------------------------------------------------------
 */

PixmapFolders *folder_icons_new(void)
{
	PixmapFolders *pf;

	pf = g_new0(PixmapFolders, 1);

	pf->close = pixbuf_inline(PIXBUF_INLINE_FOLDER_CLOSED);
	pf->open = pixbuf_inline(PIXBUF_INLINE_FOLDER_OPEN);
	pf->deny = pixbuf_inline(PIXBUF_INLINE_FOLDER_LOCKED);
	pf->parent = pixbuf_inline(PIXBUF_INLINE_FOLDER_UP);

	return pf;
}

void folder_icons_free(PixmapFolders *pf)
{
	if (!pf) return;

	g_object_unref(pf->close);
	g_object_unref(pf->open);
	g_object_unref(pf->deny);
	g_object_unref(pf->parent);

	g_free(pf);
}

/*
 *-----------------------------------------------------------------------------
 * sidebars
 *-----------------------------------------------------------------------------
 */

static void layout_bar_info_destroyed(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	lw->bar_info = NULL;

	if (lw->utility_box)
		{
		/* destroyed from within itself */
		lw->bar_info_enabled = FALSE;
		layout_util_sync_views(lw);
		}
}

static GList *layout_bar_info_list_cb(gpointer data)
{
	LayoutWindow *lw = data;

	return layout_selection_list(lw);
}

static void layout_bar_info_sized(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	LayoutWindow *lw = data;

	if (!lw->bar_info) return;
	
	options->panels.info.width = lw->bar_info_width = allocation->width;
}

static void layout_bar_info_new(LayoutWindow *lw)
{
	if (!lw->utility_box) return;

	lw->bar_info = bar_info_new(layout_image_get_fd(lw), FALSE, lw->utility_box);
	bar_info_set_selection_func(lw->bar_info, layout_bar_info_list_cb, lw);
	bar_info_selection(lw->bar_info, layout_selection_count(lw, NULL) - 1);
	g_signal_connect(G_OBJECT(lw->bar_info), "destroy",
			 G_CALLBACK(layout_bar_info_destroyed), lw);
	g_signal_connect(G_OBJECT(lw->bar_info), "size_allocate",
			 G_CALLBACK(layout_bar_info_sized), lw);

	options->panels.info.enabled = lw->bar_info_enabled = TRUE;
	gtk_widget_set_size_request(lw->bar_info, lw->bar_info_width, -1);

	gtk_box_pack_start(GTK_BOX(lw->utility_box), lw->bar_info, FALSE, FALSE, 0);
	gtk_widget_show(lw->bar_info);
}

static void layout_bar_info_close(LayoutWindow *lw)
{
	if (lw->bar_info)
		{
		bar_info_close(lw->bar_info);
		lw->bar_info = NULL;
		}
	options->panels.info.enabled = lw->bar_info_enabled = FALSE;
}

void layout_bar_info_toggle(LayoutWindow *lw)
{
	if (lw->bar_info_enabled)
		{
		layout_bar_info_close(lw);
		}
	else
		{
		layout_bar_info_new(lw);
		}
}

static void layout_bar_info_new_image(LayoutWindow *lw)
{
	if (!lw->bar_info || !lw->bar_info_enabled) return;

	bar_info_set(lw->bar_info, layout_image_get_fd(lw));
}

static void layout_bar_info_new_selection(LayoutWindow *lw, gint count)
{
	if (!lw->bar_info || !lw->bar_info_enabled) return;

	bar_info_selection(lw->bar_info, count - 1);
}

static void layout_bar_info_maint_renamed(LayoutWindow *lw)
{
	if (!lw->bar_info || !lw->bar_info_enabled) return;

	bar_info_maint_renamed(lw->bar_info, layout_image_get_fd(lw));
}

static void layout_bar_exif_destroyed(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	if (lw->bar_exif)
		{
		lw->bar_exif_advanced = bar_exif_is_advanced(lw->bar_exif);
		}

	lw->bar_exif = NULL;
	if (lw->utility_box)
		{
		/* destroyed from within itself */
		lw->bar_exif_enabled = FALSE;
		layout_util_sync_views(lw);
		}
}

static void layout_bar_exif_sized(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	LayoutWindow *lw = data;

	if (!lw->bar_exif) return;
	
	options->panels.exif.width = lw->bar_exif_width = allocation->width;
}

static void layout_bar_exif_new(LayoutWindow *lw)
{
	if (!lw->utility_box) return;

	lw->bar_exif = bar_exif_new(TRUE, layout_image_get_fd(lw),
				    lw->bar_exif_advanced, lw->utility_box);
	g_signal_connect(G_OBJECT(lw->bar_exif), "destroy",
			 G_CALLBACK(layout_bar_exif_destroyed), lw);
	g_signal_connect(G_OBJECT(lw->bar_exif), "size_allocate",
			 G_CALLBACK(layout_bar_exif_sized), lw);

	options->panels.exif.enabled = lw->bar_exif_enabled = TRUE;
	gtk_widget_set_size_request(lw->bar_exif, lw->bar_exif_width, -1);

	gtk_box_pack_start(GTK_BOX(lw->utility_box), lw->bar_exif, FALSE, FALSE, 0);
	if (lw->bar_info) gtk_box_reorder_child(GTK_BOX(lw->utility_box), lw->bar_exif, 1);
	gtk_widget_show(lw->bar_exif);
}

static void layout_bar_exif_close(LayoutWindow *lw)
{
	if (lw->bar_exif)
		{
		bar_exif_close(lw->bar_exif);
		lw->bar_exif = NULL;
		}
	options->panels.exif.enabled = lw->bar_exif_enabled = FALSE;
}

void layout_bar_exif_toggle(LayoutWindow *lw)
{
	if (lw->bar_exif_enabled)
		{
		layout_bar_exif_close(lw);
		}
	else
		{
		layout_bar_exif_new(lw);
		}
}

static void layout_bar_exif_new_image(LayoutWindow *lw)
{
	if (!lw->bar_exif || !lw->bar_exif_enabled) return;

	bar_exif_set(lw->bar_exif, layout_image_get_fd(lw));
}

static void layout_bar_sort_destroyed(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	lw->bar_sort = NULL;

	if (lw->utility_box)
		{
		/* destroyed from within itself */
		lw->bar_sort_enabled = FALSE;

		layout_util_sync_views(lw);
		}
}

static void layout_bar_sort_new(LayoutWindow *lw)
{
	if (!lw->utility_box) return;

	lw->bar_sort = bar_sort_new(lw);
	g_signal_connect(G_OBJECT(lw->bar_sort), "destroy",
			 G_CALLBACK(layout_bar_sort_destroyed), lw);
	options->panels.sort.enabled = lw->bar_sort_enabled = TRUE;

	gtk_box_pack_end(GTK_BOX(lw->utility_box), lw->bar_sort, FALSE, FALSE, 0);
	gtk_widget_show(lw->bar_sort);
}

static void layout_bar_sort_close(LayoutWindow *lw)
{
	if (lw->bar_sort)
		{
		bar_sort_close(lw->bar_sort);
		lw->bar_sort = NULL;
		}
	options->panels.sort.enabled = lw->bar_sort_enabled = FALSE;
}

void layout_bar_sort_toggle(LayoutWindow *lw)
{
	if (lw->bar_sort_enabled)
		{
		layout_bar_sort_close(lw);
		}
	else
		{
		layout_bar_sort_new(lw);
		}
}

void layout_bars_new_image(LayoutWindow *lw)
{
	layout_bar_info_new_image(lw);
	layout_bar_exif_new_image(lw);
}

void layout_bars_new_selection(LayoutWindow *lw, gint count)
{
	layout_bar_info_new_selection(lw, count);
}

GtkWidget *layout_bars_prepare(LayoutWindow *lw, GtkWidget *image)
{
	lw->utility_box = gtk_hbox_new(FALSE, PREF_PAD_GAP);
	gtk_box_pack_start(GTK_BOX(lw->utility_box), image, TRUE, TRUE, 0);
	gtk_widget_show(image);

	if (lw->bar_sort_enabled)
		{
		layout_bar_sort_new(lw);
		}

	if (lw->bar_info_enabled)
		{
		layout_bar_info_new(lw);
		}

	if (lw->bar_exif_enabled)
		{
		layout_bar_exif_new(lw);
		}

	return lw->utility_box;
}

void layout_bars_close(LayoutWindow *lw)
{
	layout_bar_sort_close(lw);
	layout_bar_exif_close(lw);
	layout_bar_info_close(lw);
}

void layout_bars_maint_renamed(LayoutWindow *lw)
{
	layout_bar_info_maint_renamed(lw);
}
