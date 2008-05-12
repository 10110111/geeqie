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
	gint progressive_key_scrolling;
	gint place_dialogs_under_mouse;
	gint mousewheel_scrolls;
	gint show_icon_names;
	gint show_copy_path;

	/* various */
	gint enable_metadata_dirs;

	gint tree_descend_subdirs;

	gint lazy_image_sync;
	gint update_on_time_change;

	gint duplicates_similarity_threshold;

	gint open_recent_list_maxsize;
	gint dnd_icon_size;

	gint save_metadata_in_image_file;

	struct {
		gboolean restore_path;
		gboolean use_last_path;
		gchar *path;
	} startup;

	/* file ops */
	struct {
		gint enable_in_place_rename;

		gint confirm_delete;
		gint enable_delete_key;
		gint safe_delete_enable;
		gchar *safe_delete_path;
		gint safe_delete_folder_maxsize;
	} file_ops;

	/* image */
	struct {
		gint exif_rotate_enable;
		gint scroll_reset_method;
		gint fit_window_to_image;
		gint limit_window_size;
		gint max_window_size;
		gint limit_autofit_size;
		gint max_autofit_size;

		gint tile_cache_max;	/* in megabytes */
		gint dither_quality;
		gint enable_read_ahead;

		gint zoom_mode;
		gint zoom_2pass;
		gint zoom_to_fit_allow_expand;
		gint zoom_quality;
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
		gint enable_caching;
		gint cache_into_dirs;
		gint fast;
		gint use_xvpics;
		gint spec_standard;
		gint quality;
	} thumbnails;

	/* file filtering */
	struct {
		gint show_hidden_files;
		gint show_dot_directory;
		gint disable;
	} file_filter;

	/* collections */
	struct {
		gint rectangular_selection;
	} collections;

	/* editors */
	gchar *editor_name[GQ_EDITOR_SLOTS];
	gchar *editor_command[GQ_EDITOR_SLOTS];

	/* file sorting */
	struct {
		SortType method;
		gint ascending;
		gint case_sensitive; /* file sorting method (case) */
	} file_sort;

	/* slideshow */
	struct {
		gint delay;	/* in tenths of a second */
		gint random;
		gint repeat;
	} slideshow;

	/* fullscreen */
	struct {
		gint screen;
		gint clean_flip;
		gint disable_saver;
		gint above;
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
			gint show_at_startup;
			gchar *template_string;
		} common;
	} image_overlay;

	/* layout */
	struct {
		gchar *order;
		gint style;

		DirViewType dir_view_type;
		FileViewType file_view_type;

		gint show_thumbnails;
		gint show_marks;

		struct {
			gint w;
			gint h;
			gint x;
			gint y;
			gint maximized;
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

		gint save_window_positions;

		gint tools_float;
		gint tools_hidden;
		gint tools_restore_state;

		gint toolbar_hidden;

	} layout;

	/* panels */
	struct {
		struct {
			gint enabled;
			gint width;
		} info;

		struct {
			gint enabled;
			gint width;
		} exif;

		struct {
			gint enabled;
			gint mode_state;
			gint action_state;
			gint selection_state;
		} sort;
	} panels;


	/* color profiles */
	struct {
		gint enabled;
		gint input_type;
		gchar *input_file[COLOR_PROFILE_INPUTS];
		gchar *input_name[COLOR_PROFILE_INPUTS];
		gint screen_type;
		gchar *screen_file;
		gint use_image;

	} color_profile;

};

ConfOptions *options;

ConfOptions *init_options(ConfOptions *options);

#endif /* OPTIONS_H */
