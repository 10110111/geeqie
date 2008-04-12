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
	options->image.zoom_mode = ZOOM_RESET_ORIGINAL;
	options->image.zoom_2pass = TRUE;
	options->image.scroll_reset_method = SCROLL_RESET_TOPLEFT;
	options->image.fit_window_to_image = FALSE;
	options->image.limit_window_size = FALSE;
	options->image.zoom_to_fit_allow_expand = TRUE;
	options->image.max_window_size = 100;
	options->image.limit_autofit_size = FALSE;
	options->image.max_autofit_size = 100;
	options->thumbnails.max_width = DEFAULT_THUMB_WIDTH;
	options->thumbnails.max_height = DEFAULT_THUMB_HEIGHT;
	options->thumbnails.enable_caching = TRUE;
	options->thumbnails.cache_into_dirs = FALSE;
	options->thumbnails.use_xvpics = TRUE;
	options->thumbnails.fast = TRUE;
	options->thumbnails.spec_standard = TRUE;
	options->enable_metadata_dirs = FALSE;
	options->file_filter.show_dot_files = FALSE;
	options->file_filter.disable = FALSE;
	
	
	options->thumbnails.enabled = FALSE;
	options->file_sort.method = SORT_NAME;
	options->file_sort.ascending = TRUE;
	
	options->slideshow.delay = 150;
	options->slideshow.random = FALSE;
	options->slideshow.repeat = FALSE;
	
	options->mousewheel_scrolls = FALSE;
	options->enable_in_place_rename = TRUE;
	
	options->recent_list_max = 10;
	
	options->collections.rectangular_selection = FALSE;
	
	options->image.tile_cache_max = 10;
	options->thumbnails.quality = (gint)GDK_INTERP_TILES;
	options->image.zoom_quality = (gint)GDK_INTERP_BILINEAR;
	options->image.dither_quality = (gint)GDK_RGB_DITHER_NORMAL;
	
	options->image.zoom_increment = 5;
	
	options->image.enable_read_ahead = TRUE;
	
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

	options->file_sort.case_sensitive = FALSE;

	/* layout */
	options->layout.order = NULL;
	options->layout.style = 0;

	options->layout.view_as_icons = FALSE;
	options->layout.view_as_tree = FALSE;

	options->show_icon_names = TRUE;

	options->tree_descend_subdirs = FALSE;

	options->lazy_image_sync = FALSE;
	options->update_on_time_change = TRUE;
	options->image.exif_rotate_enable = TRUE;

	/* color profiles */
	options->color_profile.enabled = FALSE;
	options->color_profile.input_type = 0;
	options->color_profile.screen_type = 0;
	options->color_profile.screen_file = NULL;
	options->color_profile.use_image = TRUE;

	return options;
}
