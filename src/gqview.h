/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "intl.h"

/*
 *-------------------------------------
 * Standard library includes
 *-------------------------------------
 */

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

/*
 *-------------------------------------
 * includes for glib / gtk / imlib
 *-------------------------------------
 */

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk_imlib.h>

/*
 *----------------------------------------------------------------------------
 * defines
 *----------------------------------------------------------------------------
 */

#define RC_FILE_NAME ".gqviewrc"
#define RC_THUMB_DIR ".gqview_thmb"

#define ZOOM_RESET_ORIGINAL 0
#define ZOOM_RESET_FIT_WINDOW 1
#define ZOOM_RESET_NONE 2

typedef struct _ImageWindow ImageWindow;
struct _ImageWindow
{
	GtkWidget *eventbox;
	GtkWidget *table;
	GtkWidget *viewport;
	GtkWidget *image;

	gchar *image_path;
	gchar *image_name;

	gint width;
	gint height;
	gint size;

	gint old_width;
	gint old_height;

	gint unknown;
	gint zoom;

	GdkPixmap *image_pixmap;
	GdkImlibImage *image_data;

	gint in_drag;
	gint drag_last_x;
	gint drag_last_y;
	gint drag_moved;

	gint artificial_size;
	gint new_img;

	/* info, zoom labels & windows */

	GtkWidget *top_window; /* window that gets title set to image filename */
	GtkWidget *info_label; /* label set to show image h x w , size */
	GtkWidget *zoom_label; /* label to display zoom */
	gchar *title;	       /* window title to display left of file name */
	gint show_title_zoom;  /* option to include zoom in window title */

	/* button functions */
	void (*func_btn1)(ImageWindow *, GdkEventButton *, gpointer);
	void (*func_btn2)(ImageWindow *, GdkEventButton *, gpointer);
	void (*func_btn3)(ImageWindow *, GdkEventButton *, gpointer);

	gpointer data_btn1;
	gpointer data_btn2;
	gpointer data_btn3;
};

/* image */
extern ImageWindow *main_image;

/* main window */
extern GtkWidget *mainwindow;
extern GtkWidget *mainwindow_hbox;
extern GtkWidget *mainwindow_vbox;
extern GtkAccelGroup *mainwindow_accel_grp;

extern GtkWidget *info_box;
extern GtkWidget *info_progress_bar;
extern GtkWidget *info_status;
extern GtkWidget *info_details;
extern GtkWidget *info_zoom;

/* full screen */
extern ImageWindow *normal_image;
extern ImageWindow *full_screen_image;
extern GtkWidget *full_screen_window;

/* tools floating window */
extern GtkWidget *toolwindow;

/* tools */
extern GtkWidget *tool_vbox;

extern GtkWidget *path_entry;
extern GtkWidget *history_menu;

extern GtkWidget *dir_clist;
extern GtkWidget *file_clist;

extern GtkWidget *menu_file;
extern GtkWidget *menu_edit;
extern GtkWidget *menu_view;
extern GtkWidget *menu_help;
extern GtkWidget *menu_file_popup;
extern GtkWidget *menu_filelist_edit;
extern GtkWidget *menu_image_popup;
extern GtkWidget *menu_image_edit;
extern GtkWidget *menu_window_full;
extern GtkWidget *menu_window_full_edit;
extern GtkWidget *menu_window_view;
extern GtkWidget *menu_window_view_edit;

extern GtkWidget *thumb_button;
extern GtkWidget *thumb_menu_item;

/* lists */
extern GList *dir_list;
extern GList *file_list;
extern gchar *current_path;

extern GList *filename_filter;

/* -- options -- */
extern gint main_window_w;
extern gint main_window_h;
extern gint main_window_x;
extern gint main_window_y;

extern gint float_window_w;
extern gint float_window_h;
extern gint float_window_x;
extern gint float_window_y;

extern gint save_window_positions;
extern gint tools_float;
extern gint tools_hidden;
extern gint progressive_key_scrolling;

extern gint startup_path_enable;
extern gchar *startup_path;
extern gint confirm_delete;
extern gint restore_tool;
extern gint zoom_mode;
extern gint fit_window;
extern gint limit_window_size;
extern gint max_window_size;
extern gint thumb_max_width;
extern gint thumb_max_height;
extern gint enable_thumb_caching;
extern gint use_xvpics_thumbnails;
extern gint show_dot_files;
extern gint file_filter_disable;
extern gint filter_include_jpg;
extern gint filter_include_xpm;
extern gint filter_include_tif;
extern gint filter_include_gif;
extern gint filter_include_png;
extern gint filter_include_ppm;
extern gint filter_include_pgm;
extern gint filter_include_pcx;
extern gint filter_include_bmp;
extern gchar *custom_filter;
extern gchar *editor_name[];
extern gchar *editor_command[];

extern gint thumbnails_enabled;

extern gint slideshow_delay;	/* in seconds */
extern gint slideshow_random;
extern gint slideshow_repeat;

extern gint debug;

/* logo & misc images */
extern const int logo_width;
extern const int logo_height;
extern const unsigned char logo[];

/* -- functions -- */

/* main.c */
gchar *filename_from_path(char *t);
gchar *remove_level_from_path(gchar *path);
void parse_out_relatives(gchar *path);
void start_editor_from_file(gint n, gchar *path);
void start_editor_from_image(gint n);
void start_editor_from_list(gint n);
void keyboard_scroll_calc(gint *x, gint *y, GdkEventKey *event);
gint key_press_cb(GtkWidget *widget, GdkEventKey *event);
void exit_gqview();

/* window.c */
void toolwindow_float();
void toolwindow_hide();
void create_main_window();

/* menu.c */
void add_menu_popup_item(GtkWidget *menu, gchar *label,
			 GtkSignalFunc func, gpointer data);
void add_menu_divider(GtkWidget *menu);
void update_edit_menus(GtkAccelGroup *accel_grp);
GtkWidget *create_menu_bar(GtkAccelGroup *accel_grp);
void create_menu_popups();
GtkWidget *create_button_bar(GtkTooltips *tooltips);

/* img-main.c */
void full_screen_start();
void full_screen_stop();
void full_screen_toggle();
void image_scroll(gint x, gint y);
void image_adjust_zoom(gint increment);
void image_set_zoom(gint zoom);
void image_set_path(gchar *path);
gchar *image_get_path();
gchar *image_get_name();
void image_change_to(gchar *path);
void image_set_labels(GtkWidget *info, GtkWidget *zoom);
GtkWidget *image_create();
void image_to_root();

/* filelist.c */
void update_status_label(gchar *text);
void rebuild_file_filter();
gint find_file_in_list(gchar *path);
GList *file_get_selected_list();
void free_selected_list(GList *list);
gint file_clicked_is_selected();
gchar *file_clicked_get_path();
gint file_count();
gint file_selection_count();
gchar *file_get_path(gint row);
gint file_is_selected(gint row);
void file_image_change_to(gint row);
void file_next_image();
void file_prev_image();
void file_first_image();
void file_last_image();
void file_is_gone(gchar *path, GList *ignore_list);
void file_is_renamed(gchar *source, gchar *dest);
void dir_select_cb(GtkWidget *widget, gint row, gint col,
		   GdkEvent *event, gpointer data);
void dir_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
void file_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
void file_select_cb(GtkWidget *widget, gint row, gint col,
		   GdkEvent *event, gpointer data);
void file_unselect_cb(GtkWidget *widget, gint row, gint col,
		   GdkEvent *event, gpointer data);
void file_clist_highlight_set();
void file_clist_highlight_unset();
void path_entry_tab_cb(gchar *newpath, gpointer data);
void path_entry_cb(gchar *newdir, gpointer data);
void interrupt_thumbs();
void filelist_populate_clist();
void filelist_refresh();
void filelist_change_to(gchar *path);

/* config.c */
void show_config_window();
void show_about_window();

/* rcfile.c */
void save_options();
void load_options();

/* tabcomp.c */
GtkWidget *tab_completion_new_with_history(GtkWidget **entry, GtkWidget *window, gchar *text,
					   const gchar *history_key, gint max_levels,
					   void (*enter_func)(gchar *, gpointer), gpointer data);
gchar *tab_completion_set_to_last_history(GtkWidget *entry);
void tab_completion_append_to_history(GtkWidget *entry, gchar *path);

GtkWidget *tab_completion_new(GtkWidget **entry, GtkWidget *window, gchar *text,
			      void (*enter_func)(gchar *, gpointer), gpointer data);
void tab_completion_add_to_entry(GtkWidget *entry, void (*enter_func)(gchar *, gpointer), gpointer data);
void tab_completion_add_tab_func(GtkWidget *entry, void (*tab_func)(gchar *, gpointer), gpointer data);
gchar *remove_trailing_slash(gchar *path);

/* fileops.c */
gchar *homedir();
int isfile(char *s);
int isdir(char *s);
int filesize(char *s);
time_t filetime(gchar *s);
int copy_file(char *s, char *t);
int move_file(char *s, char *t);
gchar *get_current_dir();

/* dnd.c */
void image_dnd_init(ImageWindow *imd);
void init_dnd();

/* pathsel.c */
GtkWidget *destination_widget_new(gchar *path, GtkWidget *entry);
void destination_widget_sync_to_entry(GtkWidget *entry);

#include "utildlg.h"

/* utilops.c */
void file_util_delete(gchar *source_path, GList *source_list);
void file_util_move(gchar *source_path, GList *source_list, gchar *dest_path);
void file_util_copy(gchar *source_path, GList *source_list, gchar *dest_path);
void file_util_rename(gchar *source_path, GList *source_list);
void file_util_create_dir(gchar *path);

/* thumb.c */
gint create_thumbnail(gchar *path, GdkPixmap **thumb_pixmap, GdkBitmap **thumb_mask);
gint maintain_thumbnail_dir(gchar *dir, gint recursive);

/* slideshow.c */
void slideshow_start();
void slideshow_stop();
void slideshow_next();
void slideshow_prev();
void slideshow_toggle();
gint slideshow_is_running();

/* img-view.c */
void view_window_new(gchar *path);
void view_window_active_edit(gint n);
void view_window_active_to_root(gint n);
void create_menu_view_popup();



