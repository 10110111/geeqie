/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik, Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct _ConfOptions ConfOptions;

struct _ConfOptions
{
	/* ui */
	gboolean progressive_key_scrolling;
	gboolean place_dialogs_under_mouse;
	gboolean mousewheel_scrolls;
	gboolean show_icon_names;
	gboolean show_copy_path;

	/* various */
	gboolean enable_metadata_dirs;

	gboolean tree_descend_subdirs;

	gboolean lazy_image_sync;
	gboolean update_on_time_change;

	guint duplicates_similarity_threshold;

	gint open_recent_list_maxsize;
	gint dnd_icon_size;

	gboolean save_metadata_in_image_file;

	/* start up */
	struct {
		gboolean restore_path;
		gboolean use_last_path;
		gchar *path;
	} startup;

	/* file ops */
	struct {
		gboolean enable_in_place_rename;

		gboolean confirm_delete;
		gboolean enable_delete_key;
		gboolean safe_delete_enable;
		gchar *safe_delete_path;
		gint safe_delete_folder_maxsize;
	} file_ops;

	/* image */
	struct {
		gboolean exif_rotate_enable;
		guint scroll_reset_method;
		gboolean fit_window_to_image;
		gboolean limit_window_size;
		gint max_window_size;
		gboolean limit_autofit_size;
		gint max_autofit_size;

		gint tile_cache_max;	/* in megabytes */
		gint image_cache_max;   /* in megabytes */
		guint dither_quality;
		gboolean enable_read_ahead;

		ZoomMode zoom_mode;
		gboolean zoom_2pass;
		gboolean zoom_to_fit_allow_expand;
		guint zoom_quality;
		gint zoom_increment;	/* 10 is 1.0, 5 is 0.05, 20 is 2.0, etc. */

		gint use_custom_border_color;
		GdkColor border_color;

		gint read_buffer_size; /* bytes to read from file per read() */
		gint idle_read_loop_count; /* the number of bytes to read per idle call (define x image.read_buffer_size) */
	} image;

	/* thumbnails */
	struct {
		gint max_width;
		gint max_height;
		gboolean enable_caching;
		gboolean cache_into_dirs;
		gboolean fast;
		gboolean use_xvpics;
		gboolean spec_standard;
		guint quality;
	} thumbnails;

	/* file filtering */
	struct {
		gboolean show_hidden_files;
		gboolean show_dot_directory;
		gboolean disable;
	} file_filter;

	/* collections */
	struct {
		gboolean rectangular_selection;
	} collections;

	/* editors */
	Editor editor[GQ_EDITOR_SLOTS];

	/* shell */
	struct {
		gchar *path;
		gchar *options;
	} shell;
	
	/* file sorting */
	struct {
		SortType method;
		gboolean ascending;
		gboolean case_sensitive; /* file sorting method (case) */
	} file_sort;

	/* slideshow */
	struct {
		gint delay;	/* in tenths of a second */
		gboolean random;
		gboolean repeat;
	} slideshow;

	/* fullscreen */
	struct {
		gint screen;
		gboolean clean_flip;
		gboolean disable_saver;
		gboolean above;
	} fullscreen;

	/* histogram */
	struct {
		guint last_channel_mode;
		guint last_log_mode;
	} histogram;
	
	/* image overlay */
	struct {
		struct {
			guint state;
			gboolean show_at_startup;
			gchar *template_string;
			gint x;
			gint y;
		} common;
	} image_overlay;

	/* layout */
	struct {
		gchar *order;
		gint style;

		DirViewType dir_view_type;
		FileViewType file_view_type;

		gboolean show_thumbnails;
		gboolean show_marks;
		gboolean show_directory_date;

		struct {
			gint w;
			gint h;
			gint x;
			gint y;
			gboolean maximized;
			gint hdivider_pos;
			gint vdivider_pos;
		} main_window;

		struct {
			gint w;
			gint h;
			gint x;
			gint y;
			gint vdivider_pos;
		} float_window;

		struct {
			gint w;
			gint h;
		} properties_window;

		gboolean save_window_positions;

		gboolean tools_float;
		gboolean tools_hidden;
		gboolean tools_restore_state;

		gboolean toolbar_hidden;
		
		gchar *home_path;
	} layout;

	/* panels */
	struct {
		struct {
			gboolean enabled;
			gint width;
		} info;

		struct {
			gboolean enabled;
			gint width;
		} exif;

		struct {
			gboolean enabled;
			gint mode_state;
			gint action_state;
			gint selection_state;
		} sort;
	} panels;

	/* properties dialog */
	struct {
		gchar *tabs_order;
	} properties;

	/* color profiles */
	struct {
		gboolean enabled;
		gint input_type;
		gchar *input_file[COLOR_PROFILE_INPUTS];
		gchar *input_name[COLOR_PROFILE_INPUTS];
		gint screen_type;
		gchar *screen_file;
		gboolean use_image;

	} color_profile;

	/* Helpers programs */
	struct {
		struct {
			gchar *command_name;
			gchar *command_line;
		} html_browser;
	} helpers;
};

ConfOptions *options;

ConfOptions *init_options(ConfOptions *options);
void setup_default_options(ConfOptions *options);
void sync_options_with_current_state(ConfOptions *options);

#endif /* OPTIONS_H */
