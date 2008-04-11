/*
 * Geeqie
 * (C) 2004 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"

#ifdef DEBUG
gint debug = FALSE;
#endif


ConfOptions *init_options(ConfOptions *options)
{
	if (!options) options = g_new0(ConfOptions, 1);

	options->main_window_w = 500;
	options->main_window_h = 400;
	options->main_window_x = 0;
	options->main_window_y = 0;
	options->main_window_maximized = FALSE;
	
	options->float_window_w = 260;
	options->float_window_h = 450;
	options->float_window_x = 0;
	options->float_window_y = 0;
	options->float_window_divider = -1;
	
	options->window_hdivider_pos = -1;
	options->window_vdivider_pos = 200;
	
	options->save_window_positions = FALSE;
	options->tools_float = FALSE;
	options->tools_hidden = FALSE;
	options->toolbar_hidden = FALSE;
	options->progressive_key_scrolling = FALSE;
	
	options->startup_path_enable = FALSE;
	options->startup_path = NULL;
	options->confirm_delete = TRUE;
	options->enable_delete_key = TRUE;
	options->safe_delete_enable = FALSE;
	options->safe_delete_path = NULL;
	options->safe_delete_size = 128;
	options->restore_tool = FALSE;
	options->zoom_mode = ZOOM_RESET_ORIGINAL;
	options->two_pass_zoom = TRUE;
	options->scroll_reset_method = SCROLL_RESET_TOPLEFT;
	options->fit_window = FALSE;
	options->limit_window_size = FALSE;
	options->zoom_to_fit_expands = TRUE;
	options->max_window_size = 100;
	options->limit_autofit_size = FALSE;
	options->max_autofit_size = 100;
	options->thumb_max_width = DEFAULT_THUMB_WIDTH;
	options->thumb_max_height = DEFAULT_THUMB_HEIGHT;
	options->enable_thumb_caching = TRUE;
	options->enable_thumb_dirs = FALSE;
	options->use_xvpics_thumbnails = TRUE;
	options->thumbnail_fast = TRUE;
	options->thumbnail_spec_standard = TRUE;
	options->enable_metadata_dirs = FALSE;
	options->show_dot_files = FALSE;
	options->file_filter_disable = FALSE;
	
	
	options->thumbnails_enabled = FALSE;
	options->file_sort_method = SORT_NAME;
	options->file_sort_ascending = TRUE;
	
	options->slideshow_delay = 150;
	options->slideshow_random = FALSE;
	options->slideshow_repeat = FALSE;
	
	options->mousewheel_scrolls = FALSE;
	options->enable_in_place_rename = TRUE;
	
	options->recent_list_max = 10;
	
	options->collection_rectangular_selection = FALSE;
	
	options->tile_cache_max = 10;
	options->thumbnail_quality = (gint)GDK_INTERP_TILES;
	options->zoom_quality = (gint)GDK_INTERP_BILINEAR;
	options->dither_quality = (gint)GDK_RGB_DITHER_NORMAL;
	
	options->zoom_increment = 5;
	
	options->enable_read_ahead = TRUE;
	
	options->place_dialogs_under_mouse = FALSE;
	
	options->user_specified_window_background = FALSE;
	memset(&options->window_background_color, 0, sizeof(options->window_background_color));
	
	options->fullscreen.screen = -1;
	options->fullscreen.clean_flip = FALSE;
	options->fullscreen.disable_saver = TRUE;
	options->fullscreen.above = FALSE;
	options->fullscreen.show_info = TRUE;
	options->fullscreen.info = NULL;
	
	options->dupe_custom_threshold = 99;

	options->file_sort_case_sensitive = FALSE;

	/* layout */
	options->layout_order = NULL;
	options->layout_style = 0;

	options->layout_view_icons = FALSE;
	options->layout_view_tree = FALSE;

	options->show_icon_names = TRUE;

	options->tree_descend_subdirs = FALSE;

	options->lazy_image_sync = FALSE;
	options->update_on_time_change = TRUE;
	options->exif_rotate_enable = TRUE;

	/* color profiles */
	options->color_profile_enabled = FALSE;
	options->color_profile_input_type = 0;
	options->color_profile_screen_type = 0;
	options->color_profile_screen_file = NULL;
	options->color_profile_use_image = TRUE;

	options->filename_filter = NULL;

	return options;
}
