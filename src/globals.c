/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include "logo.h"

/* image */
ImageWindow *main_image = NULL;

/* main window */
GtkWidget *mainwindow;
GtkWidget *mainwindow_hbox;
GtkWidget *mainwindow_vbox;
GtkAccelGroup *mainwindow_accel_grp;

GtkWidget *info_box = NULL;
GtkWidget *info_progress_bar;
GtkWidget *info_status;
GtkWidget *info_details;
GtkWidget *info_zoom;

/* full screen */
ImageWindow *normal_image = NULL;
ImageWindow *full_screen_image = NULL;
GtkWidget *full_screen_window = NULL;

/* tools floating window */
GtkWidget *toolwindow = NULL;

/* tools */
GtkWidget *tool_vbox;

GtkWidget *path_entry;
GtkWidget *history_menu;

GtkWidget *dir_clist;
GtkWidget *file_clist;

GtkWidget *menu_file;
GtkWidget *menu_edit;
GtkWidget *menu_view;
GtkWidget *menu_help;
GtkWidget *menu_file_popup;
GtkWidget *menu_filelist_edit;
GtkWidget *menu_image_popup;
GtkWidget *menu_image_edit;
GtkWidget *menu_window_full;
GtkWidget *menu_window_full_edit;
GtkWidget *menu_window_view;
GtkWidget *menu_window_view_edit;

GtkWidget *thumb_button;
GtkWidget *thumb_menu_item;

/* lists */
GList *dir_list = NULL;
GList *file_list = NULL;
gchar *current_path = NULL;

GList *filename_filter = NULL;

/* -- options -- */
gint main_window_w = 400;
gint main_window_h = 350;
gint main_window_x = 0;
gint main_window_y = 0;

gint float_window_w = 150;
gint float_window_h = 350;
gint float_window_x = 0;
gint float_window_y = 0;

gint save_window_positions = FALSE;
gint tools_float = FALSE;
gint tools_hidden = FALSE;
gint progressive_key_scrolling = FALSE;

gint startup_path_enable = FALSE;
gchar *startup_path = NULL;
gint confirm_delete = TRUE;
gint restore_tool = FALSE;
gint zoom_mode = ZOOM_RESET_ORIGINAL;
gint fit_window = FALSE;
gint limit_window_size = FALSE;
gint max_window_size = 100;
gint thumb_max_width = 64;
gint thumb_max_height = 64;
gint enable_thumb_caching = FALSE;
gint use_xvpics_thumbnails = TRUE;
gint show_dot_files = FALSE;
gint file_filter_disable = FALSE;
gint filter_include_jpg = TRUE;
gint filter_include_xpm = TRUE;
gint filter_include_tif = TRUE;
gint filter_include_gif = TRUE;
gint filter_include_png = TRUE;
gint filter_include_ppm = TRUE;
gint filter_include_pgm = TRUE;
gint filter_include_pcx = TRUE;
gint filter_include_bmp = TRUE;
gchar *custom_filter = NULL;
gchar *editor_name[8];
gchar *editor_command[8];

gint thumbnails_enabled = FALSE;

gint slideshow_delay = 15;
gint slideshow_random = FALSE;
gint slideshow_repeat = FALSE;

gint debug = FALSE;

/* logo & misc images */


