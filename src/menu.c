/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

#include "icons/icon_thumb.xpm"
#include "icons/icon_home.xpm"
#include "icons/icon_reload.xpm"
#include "icons/icon_zoom_in.xpm"
#include "icons/icon_zoom_out.xpm"
#include "icons/icon_zoom_fit.xpm"
#include "icons/icon_zoom_norm.xpm"
#include "icons/icon_config.xpm"
#include "icons/icon_float.xpm"


static void add_menu_item(GtkWidget *menu, gchar *label, GtkAccelGroup *accel_group,
				guint accel_key, guint accel_mods, GtkSignalFunc func, gpointer data);

static void add_edit_items(GtkWidget *menu, GtkSignalFunc func, GtkAccelGroup *accel_grp);

static void add_button_to_bar(GtkWidget *hbox, gchar **pixmap_data,
			      GtkTooltips *tooltips, gchar *tip_text,
			      GtkSignalFunc func, gpointer data);

static void set_thumbnails(gint mode)
{
	if (thumbnails_enabled == mode) return;
	thumbnails_enabled = mode;
	gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(thumb_menu_item), thumbnails_enabled);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thumb_button), thumbnails_enabled);
	filelist_populate_clist();
}

/*
 *-----------------------------------------------------------------------------
 * menu and button callbacks (private)
 *-----------------------------------------------------------------------------
 */ 

static void exit_cb(GtkWidget *widget, gpointer data)
{
	exit_gqview();
}

static void about_cb(GtkWidget *widget, gpointer data)
{
	show_about_window();
}

static void create_dir_cb(GtkWidget *widget, gpointer data)
{
	file_util_create_dir(current_path);
}

static void zoom_in_cb(GtkWidget *widget, gpointer data)
{
	image_adjust_zoom(1);
}

static void zoom_out_cb(GtkWidget *widget, gpointer data)
{
	image_adjust_zoom(-1);
}

static void zoom_1_1_cb(GtkWidget *widget, gpointer data)
{
	image_set_zoom(1);
}

static void zoom_fit_cb(GtkWidget *widget, gpointer data)
{
	image_set_zoom(0);
}

static void thumb_menu_cb(GtkWidget *widget, gpointer data)
{
	set_thumbnails(GTK_CHECK_MENU_ITEM(widget)->active);
}

static void thumb_button_cb(GtkWidget *widget, gpointer data)
{
	set_thumbnails(GTK_TOGGLE_BUTTON(widget)->active);
}

static void refresh_cb(GtkWidget *widget, gpointer data)
{
	gchar *buf = g_strdup(current_path);
	filelist_change_to(buf);
	g_free(buf);
}

static void float_cb(GtkWidget *widget, gpointer data)
{
	toolwindow_float();
}

static void hide_cb(GtkWidget *widget, gpointer data)
{
	toolwindow_hide();
}

static void slideshow_cb(GtkWidget *widget, gpointer data)
{
	slideshow_toggle();
}

static void home_dir_cb(GtkWidget *widget, gpointer data)
{
	gchar *path = homedir();
	if (path) filelist_change_to(path);
}

static void config_cb(GtkWidget *widget, gpointer data)
{
	show_config_window();
}

static void remove_thumb_cb(GtkWidget *widget, gpointer data)
{
	maintain_thumbnail_dir("/", TRUE);
}

static void full_screen_cb(GtkWidget *widget, gpointer data)
{
	full_screen_toggle();
}

static void wallpaper_image_cb(GtkWidget *widget, gpointer data)
{
	image_to_root();
}

/*
 *-----------------------------------------------------------------------------
 * image menu callbacks (private)
 *-----------------------------------------------------------------------------
 */ 

static void copy_image_cb(GtkWidget *widget, gpointer data)
{
	full_screen_stop();
	file_util_copy(image_get_path(), NULL, current_path);
}

static void move_image_cb(GtkWidget *widget, gpointer data)
{
	full_screen_stop();
	file_util_move(image_get_path(), NULL, current_path);
}

static void rename_image_cb(GtkWidget *widget, gpointer data)
{
	full_screen_stop();
	file_util_rename(image_get_path(), NULL);
}

static void delete_image_cb(GtkWidget *widget, gpointer data)
{
	full_screen_stop();
	file_util_delete(image_get_path(), NULL);
}

/*
 *-----------------------------------------------------------------------------
 * file menu callbacks (private)
 *-----------------------------------------------------------------------------
 */ 

static void copy_file_cb(GtkWidget *widget, gpointer data)
{
	file_util_copy(NULL, file_get_selected_list(), current_path);
}

static void move_file_cb(GtkWidget *widget, gpointer data)
{
	file_util_move(NULL, file_get_selected_list(), current_path);
}

static void rename_file_cb(GtkWidget *widget, gpointer data)
{
	file_util_rename(NULL, file_get_selected_list());
}

static void delete_file_cb(GtkWidget *widget, gpointer data)
{
	file_util_delete(NULL, file_get_selected_list());
}

/*
 *-----------------------------------------------------------------------------
 * filelist popup menu callbacks (private)
 *-----------------------------------------------------------------------------
 */ 

static void popup_copy_file_cb(GtkWidget *widget, gpointer data)
{
	if (file_clicked_is_selected())
		{
		file_util_copy(NULL, file_get_selected_list(), current_path);
		}
	else
		{
		gchar *path = file_clicked_get_path();
		file_util_copy(path, NULL, current_path);
		g_free(path);
		}
}

static void popup_move_file_cb(GtkWidget *widget, gpointer data)
{
	if (file_clicked_is_selected())
		{
		file_util_move(NULL, file_get_selected_list(), current_path);
		}
	else
		{
		gchar *path = file_clicked_get_path();
		file_util_move(path, NULL, current_path);
		g_free(path);
		}
}

static void popup_rename_file_cb(GtkWidget *widget, gpointer data)
{
	if (file_clicked_is_selected())
		{
		file_util_rename(NULL, file_get_selected_list());
		}
	else
		{
		gchar *path = file_clicked_get_path();
		file_util_rename(path, NULL);
		g_free(path);
		}
}

static void popup_delete_file_cb(GtkWidget *widget, gpointer data)
{
	if (file_clicked_is_selected())
		{
		file_util_delete(NULL, file_get_selected_list());
		}
	else
		{
		gchar *path = file_clicked_get_path();
		file_util_delete(path, NULL);
		g_free(path);
		}
}

static void edit_image_cb(GtkWidget *widget, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);
	start_editor_from_image(n);
}

static void edit_list_cb(GtkWidget *widget, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);
	start_editor_from_list(n);
}

static void edit_full_cb(GtkWidget *widget, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);
	full_screen_stop();
	start_editor_from_image(n);
}

static void edit_view_cb(GtkWidget *widget, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);
	view_window_active_edit(n);
}

static void wallpaper_view_cb(GtkWidget *widget, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);
	view_window_active_to_root(n);
}

static void popup_edit_list_cb(GtkWidget *widget, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);

	if (file_clicked_is_selected())
		{
		start_editor_from_list(n);
		}
	else
		{
		gchar *path;
		start_editor_from_file(n, path);
		g_free(path);
		}
}

static void new_window_file_cb(GtkWidget *widget, gpointer data)
{
	gchar *path = file_clicked_get_path();
	view_window_new(path);
	g_free(path);
}

static void new_window_image_cb(GtkWidget *widget, gpointer data)
{
	view_window_new(image_get_path());
}

static void menu_file_popup_hide_cb(GtkWidget *widget, gpointer data)
{
	file_clist_highlight_unset();
}


/*
 *-----------------------------------------------------------------------------
 * menu addition utilities (private)
 *-----------------------------------------------------------------------------
 */ 

static void add_menu_item(GtkWidget *menu, gchar *label, GtkAccelGroup *accel_group,
				guint accel_key, guint accel_mods, GtkSignalFunc func, gpointer data)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label(label);
	gtk_widget_add_accelerator (item, "activate", accel_group, accel_key, accel_mods, GTK_ACCEL_VISIBLE);
	gtk_signal_connect (GTK_OBJECT (item), "activate",(GtkSignalFunc) func, data);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_widget_show(item);
}

void add_menu_popup_item(GtkWidget *menu, gchar *label,
			 GtkSignalFunc func, gpointer data)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label(label);
	gtk_signal_connect (GTK_OBJECT (item), "activate",(GtkSignalFunc) func, data);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_widget_show(item);
}

void add_menu_divider(GtkWidget *menu)
{
	GtkWidget *item = gtk_menu_item_new();
        gtk_menu_append(GTK_MENU(menu),item);
        gtk_widget_show(item);
}

/*
 *-----------------------------------------------------------------------------
 * edit menu routines
 *-----------------------------------------------------------------------------
 */ 

static void add_edit_items(GtkWidget *menu, GtkSignalFunc func, GtkAccelGroup *accel_grp)
{
	gint i;
	for (i = 0; i < 8; i++)
		{
		if (editor_command[i] && strlen(editor_command[i]) > 0)
			{
			gchar *text;
			if (editor_name[i] && strlen(editor_name[i]) > 0)
				text = g_strdup_printf(_("in %s..."), editor_name[i]);
			else
				text = g_strdup(_("in (unknown)..."));
			if (accel_grp)
				add_menu_item(menu, text, accel_grp, i + 49, GDK_CONTROL_MASK, func, GINT_TO_POINTER(i));
			else
				add_menu_popup_item(menu, text, func, GINT_TO_POINTER(i));
			g_free(text);
			}
		}
}

void update_edit_menus(GtkAccelGroup *accel_grp)
{
	GtkWidget *menu;

	/* main edit menu */

	menu = gtk_menu_new();
	add_edit_items(menu, edit_list_cb, accel_grp);
	add_menu_divider(menu);
	add_menu_item(menu, _("Options..."), accel_grp, 'O', GDK_CONTROL_MASK, config_cb, NULL);
	add_menu_divider(menu);
	add_menu_item(menu, _("Remove old thumbnails"), accel_grp, 'T', GDK_CONTROL_MASK, remove_thumb_cb, NULL);
	add_menu_divider(menu);
	add_menu_item(menu, _("Set as wallpaper"), accel_grp, 'W', GDK_CONTROL_MASK, wallpaper_image_cb, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_edit), menu);

	/* file edit popup */
	menu = gtk_menu_new();
	add_edit_items(menu, popup_edit_list_cb, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_filelist_edit), menu);

	/* image edit popup */
	menu = gtk_menu_new();
	add_edit_items(menu, edit_image_cb, NULL);
	add_menu_divider(menu);
	add_menu_popup_item(menu, _("Set as wallpaper"), wallpaper_image_cb, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_image_edit), menu);

	/* full screen edit popup */
	menu = gtk_menu_new();
	add_edit_items(menu, edit_full_cb, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_window_full_edit), menu);

	/* view edit popup */
	menu = gtk_menu_new();
	add_edit_items(menu, edit_view_cb, NULL);
	add_menu_divider(menu);
	add_menu_popup_item(menu, _("Set as wallpaper"), wallpaper_view_cb, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_window_view_edit), menu);
}

/*
 *-----------------------------------------------------------------------------
 * menu bar setup routines
 *-----------------------------------------------------------------------------
 */ 

GtkWidget *create_menu_bar(GtkAccelGroup *accel_grp)
{
	GtkWidget *bar;
	GtkWidget *menu;

	bar = gtk_menu_bar_new();

	/* file menu */
	menu_file = gtk_menu_item_new_with_label(_("File"));
	gtk_widget_show(menu_file);

	menu = gtk_menu_new();
	add_menu_item(menu, _("Create Dir..."), accel_grp, 'N', GDK_CONTROL_MASK, create_dir_cb, NULL);
	add_menu_divider(menu);
	add_menu_item(menu, _("Copy..."), accel_grp, 'C', GDK_CONTROL_MASK, copy_file_cb, NULL);
	add_menu_item(menu, _("Move..."), accel_grp, 'M', GDK_CONTROL_MASK, move_file_cb, NULL);
	add_menu_item(menu, _("Rename..."), accel_grp, 'R', GDK_CONTROL_MASK, rename_file_cb, NULL);
	add_menu_item(menu, _("Delete..."), accel_grp, 'D', GDK_CONTROL_MASK, delete_file_cb, NULL);
	add_menu_divider(menu);
	add_menu_item(menu, _("Exit"), accel_grp, 'Q', GDK_CONTROL_MASK, exit_cb, NULL);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_file),menu);
	gtk_menu_bar_append(GTK_MENU_BAR(bar),menu_file);

	/* edit menu */
	menu_edit = gtk_menu_item_new_with_label(_("Edit"));
	gtk_widget_show(menu_edit);

	menu = gtk_menu_new();

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_edit), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(bar), menu_edit);

	/* view menu */
	menu_view = gtk_menu_item_new_with_label(_("View"));
	gtk_widget_show(menu_view);

	menu = gtk_menu_new();
	add_menu_item(menu, _("Zoom in"), accel_grp, '=', FALSE, zoom_in_cb, NULL);
	add_menu_item(menu, _("Zoom out"), accel_grp, '-', FALSE, zoom_out_cb, NULL);
	add_menu_item(menu, _("Zoom 1:1"), accel_grp, 'Z', FALSE, zoom_1_1_cb, NULL);
	add_menu_item(menu, _("Fit image to window"), accel_grp, 'X', FALSE, zoom_fit_cb, NULL);
	add_menu_divider(menu);

	add_menu_item(menu, _("Full screen"), accel_grp, 'V', FALSE, full_screen_cb, NULL);
	thumb_menu_item = gtk_check_menu_item_new_with_label(_("Thumbnails"));
	gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(thumb_menu_item), thumbnails_enabled);
	gtk_widget_add_accelerator (thumb_menu_item, "activate", accel_grp, 'T', FALSE, GTK_ACCEL_VISIBLE);
	gtk_signal_connect (GTK_OBJECT (thumb_menu_item), "activate",(GtkSignalFunc) thumb_menu_cb, thumb_menu_item);
	gtk_menu_append(GTK_MENU(menu), thumb_menu_item);
	gtk_widget_show(thumb_menu_item);

	add_menu_divider(menu);
	add_menu_item(menu, _("Refresh Lists"), accel_grp, 'R', FALSE, refresh_cb, NULL);
	add_menu_item(menu, _("(Un)Float file list"), accel_grp, 'F', FALSE, float_cb, NULL);
	add_menu_item(menu, _("(Un)Hide file list"), accel_grp, 'H', FALSE, hide_cb, NULL);

	add_menu_divider(menu);
	add_menu_item(menu, _("Toggle slideshow"), accel_grp, 'S', FALSE, slideshow_cb, NULL);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_view), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(bar), menu_view);

	/* help menu */
	menu_help = gtk_menu_item_new_with_label(_("Help"));
	gtk_widget_show(menu_help);

	menu = gtk_menu_new();
	add_menu_item(menu, _("About"), accel_grp, 'A', GDK_CONTROL_MASK, about_cb, NULL);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_help), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(bar), menu_help);

	return bar;
}

/*
 *-----------------------------------------------------------------------------
 * menu file list routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void create_menu_file_list_popup()
{
	menu_file_popup = gtk_menu_new();
	gtk_signal_connect(GTK_OBJECT(menu_file_popup), "hide", (GtkSignalFunc) menu_file_popup_hide_cb, NULL);

	menu_filelist_edit = gtk_menu_item_new_with_label(_("Edit"));
	gtk_menu_append(GTK_MENU(menu_file_popup), menu_filelist_edit);
	gtk_widget_show(menu_filelist_edit);

	add_menu_popup_item(menu_file_popup, _("View in new window"), new_window_file_cb, NULL);

	add_menu_divider(menu_file_popup);
	add_menu_popup_item(menu_file_popup, _("Copy..."), popup_copy_file_cb, NULL);
	add_menu_popup_item(menu_file_popup, _("Move..."), popup_move_file_cb, NULL);
	add_menu_popup_item(menu_file_popup, _("Rename..."), popup_rename_file_cb, NULL);
	add_menu_popup_item(menu_file_popup, _("Delete..."), popup_delete_file_cb, NULL);
	add_menu_divider(menu_file_popup);
	add_menu_popup_item(menu_file_popup, _("Refresh"), refresh_cb, NULL);
}

/*
 *-----------------------------------------------------------------------------
 * menu image routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void create_menu_image_popup()
{
	menu_image_popup = gtk_menu_new();

	add_menu_popup_item(menu_image_popup, _("Zoom in"), zoom_in_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Zoom out"), zoom_out_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Zoom 1:1"), zoom_1_1_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Fit image to window"), zoom_fit_cb, NULL);
	add_menu_divider(menu_image_popup);

	menu_image_edit = gtk_menu_item_new_with_label(_("Edit"));
	gtk_menu_append(GTK_MENU(menu_image_popup), menu_image_edit);
	gtk_widget_show(menu_image_edit);

	add_menu_popup_item(menu_image_popup, _("View in new window"), new_window_image_cb, NULL);

	add_menu_divider(menu_image_popup);
	add_menu_popup_item(menu_image_popup, _("Copy..."), copy_image_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Move..."), move_image_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Rename..."), rename_image_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Delete..."), delete_image_cb, NULL);
	add_menu_divider(menu_image_popup);
	add_menu_popup_item(menu_image_popup, _("(Un)Hide file list"), hide_cb, NULL);
	add_menu_popup_item(menu_image_popup, _("Full screen"), full_screen_cb, NULL);
}

/*
 *-----------------------------------------------------------------------------
 * menu full screen routines (private)
 *-----------------------------------------------------------------------------
 */ 

/* this re-grabs the keyboard when the menu closes, needed for override redirect */
static void menu_full_popup_hide_cb(GtkWidget *widget, gpointer data)
{
	if (full_screen_window)
		{
		gdk_keyboard_grab(full_screen_window->window, TRUE, GDK_CURRENT_TIME);
		}
}

static void create_menu_full_screen_popup()
{
	menu_window_full = gtk_menu_new();
	add_menu_popup_item(menu_window_full, _("Zoom in"), zoom_in_cb, NULL);
	add_menu_popup_item(menu_window_full, _("Zoom out"), zoom_out_cb, NULL);
	add_menu_popup_item(menu_window_full, _("Zoom 1:1"), zoom_1_1_cb, NULL);
	add_menu_popup_item(menu_window_full, _("Fit image to window"), zoom_fit_cb, NULL);
	add_menu_divider(menu_window_full);

	menu_window_full_edit = gtk_menu_item_new_with_label(_("Edit"));
	gtk_menu_append(GTK_MENU(menu_window_full), menu_window_full_edit);
	gtk_widget_show(menu_window_full_edit);

	add_menu_divider(menu_window_full);
	add_menu_popup_item(menu_window_full, _("Copy..."), copy_image_cb, NULL);
	add_menu_popup_item(menu_window_full, _("Move..."), move_image_cb, NULL);
	add_menu_popup_item(menu_window_full, _("Rename..."), rename_image_cb, NULL);
	add_menu_popup_item(menu_window_full, _("Delete..."), delete_image_cb, NULL);

	add_menu_divider(menu_window_full);
	add_menu_popup_item(menu_window_full, _("Exit full screen"), full_screen_cb, NULL);

	gtk_signal_connect(GTK_OBJECT(menu_window_full), "hide", (GtkSignalFunc) menu_full_popup_hide_cb, NULL);
}

void create_menu_popups()
{
	create_menu_file_list_popup();
	create_menu_image_popup();
	create_menu_full_screen_popup();
	create_menu_view_popup();
}

/*
 *-----------------------------------------------------------------------------
 * toolbar routines
 *-----------------------------------------------------------------------------
 */ 

static void add_button_to_bar(GtkWidget *hbox, gchar **pixmap_data,
			      GtkTooltips *tooltips, gchar *tip_text,
			      GtkSignalFunc func, gpointer data)
{
	GtkWidget *button;
	GtkStyle *style;
	GtkWidget *pixmapwid;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	style = gtk_widget_get_style(mainwindow);

	button = gtk_button_new ();
	gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) func, thumb_button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show (button);
	gtk_tooltips_set_tip (tooltips, button, tip_text, NULL);

	pixmap = gdk_pixmap_create_from_xpm_d(mainwindow->window,  &mask,
                 &style->bg[GTK_STATE_NORMAL], (gchar **)pixmap_data);
	pixmapwid = gtk_pixmap_new(pixmap, mask);
	gtk_widget_show(pixmapwid);
	gtk_container_add(GTK_CONTAINER(button), pixmapwid);
}

GtkWidget *create_button_bar(GtkTooltips *tooltips)
{
	GtkWidget *hbox;
	GtkStyle *style;
	GtkWidget *pixmapwid;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	style = gtk_widget_get_style(mainwindow);
	hbox = gtk_hbox_new(FALSE, 0);

	thumb_button = gtk_toggle_button_new ();
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thumb_button), thumbnails_enabled);
	gtk_signal_connect (GTK_OBJECT (thumb_button), "clicked",(GtkSignalFunc) thumb_button_cb, thumb_button);
	gtk_box_pack_start (GTK_BOX (hbox), thumb_button, FALSE, FALSE, 0);
	gtk_widget_show (thumb_button);
	gtk_tooltips_set_tip (tooltips, thumb_button, _("Create thumbnails"), NULL);

	pixmap = gdk_pixmap_create_from_xpm_d(mainwindow->window,  &mask,
                 &style->bg[GTK_STATE_NORMAL], (gchar **)icon_thumb_xpm );
	pixmapwid = gtk_pixmap_new(pixmap, mask);
	gtk_widget_show(pixmapwid);
	gtk_container_add(GTK_CONTAINER(thumb_button), pixmapwid);

	add_button_to_bar(hbox, (gchar **)icon_home_xpm,
			 tooltips, _("Change to home directory"), home_dir_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_reload_xpm,
			 tooltips, _("Refresh file list"), refresh_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_zoom_in_xpm,
			 tooltips, _("Zoom in"), zoom_in_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_zoom_out_xpm,
			 tooltips, _("Zoom out"), zoom_out_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_zoom_fit_xpm,
			 tooltips, _("Fit image to window"), zoom_fit_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_zoom_norm_xpm,
			 tooltips, _("Set zoom 1:1"), zoom_1_1_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_config_xpm,
			 tooltips, _("Configure options"), config_cb, NULL);
	add_button_to_bar(hbox, (gchar **)icon_float_xpm,
			 tooltips, _("Float Controls"), float_cb, NULL);

	return hbox;
}

