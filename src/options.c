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

#include "main.h"
#include "options.h"

ConfOptions *init_options(ConfOptions *options)
{
	if (!options) options = g_new0(ConfOptions, 1);

	options->collections.rectangular_selection = FALSE;

	options->color_profile.enabled = FALSE;
	options->color_profile.input_type = 0;
	options->color_profile.screen_file = NULL;
	options->color_profile.screen_type = 0;
	options->color_profile.use_image = TRUE;

	options->dnd_icon_size = 48;
	options->duplicates_similarity_threshold = 99;
	options->enable_metadata_dirs = FALSE;

	options->file_filter.disable = FALSE;
	options->file_filter.show_dot_directory = FALSE;
	options->file_filter.show_hidden_files = FALSE;

	options->file_ops.confirm_delete = TRUE;
	options->file_ops.enable_delete_key = TRUE;
	options->file_ops.enable_in_place_rename = TRUE;
	options->file_ops.safe_delete_enable = FALSE;
	options->file_ops.safe_delete_folder_maxsize = 128;
	options->file_ops.safe_delete_path = NULL;

	options->file_sort.ascending = TRUE;
	options->file_sort.case_sensitive = FALSE;
	options->file_sort.method = SORT_NAME;

	options->fullscreen.above = FALSE;
	options->fullscreen.clean_flip = FALSE;
	options->fullscreen.disable_saver = TRUE;
	options->fullscreen.screen = -1;

	memset(&options->image.border_color, 0, sizeof(options->image.border_color));
	options->image.dither_quality = (gint)GDK_RGB_DITHER_NORMAL;
	options->image.enable_read_ahead = TRUE;
	options->image.exif_rotate_enable = TRUE;
	options->image.fit_window_to_image = FALSE;
	options->image.idle_read_loop_count = IMAGE_LOADER_IDLE_READ_LOOP_COUNT_DEFAULT;
	options->image.limit_autofit_size = FALSE;
	options->image.limit_window_size = FALSE;
	options->image.max_autofit_size = 100;
	options->image.max_window_size = 100;
	options->image.read_buffer_size = IMAGE_LOADER_READ_BUFFER_SIZE_DEFAULT;
	options->image.scroll_reset_method = SCROLL_RESET_TOPLEFT;
	options->image.tile_cache_max = 10;
	options->image.use_custom_border_color = FALSE;
	options->image.zoom_2pass = TRUE;
	options->image.zoom_increment = 5;
	options->image.zoom_mode = ZOOM_RESET_ORIGINAL;
	options->image.zoom_quality = (gint)GDK_INTERP_BILINEAR;
	options->image.zoom_to_fit_allow_expand = TRUE;

	options->image_overlay.common.enabled = FALSE;
	options->image_overlay.common.show_at_startup = FALSE;
	options->image_overlay.common.template_string = NULL;

	options->layout.dir_view_type = DIRVIEW_LIST;
	options->layout.float_window.h = 450;
	options->layout.float_window.vdivider_pos = -1;
	options->layout.float_window.w = 260;
	options->layout.float_window.x = 0;
	options->layout.float_window.y = 0;
	options->layout.main_window.h = 400;
	options->layout.main_window.hdivider_pos = -1;
	options->layout.main_window.maximized = FALSE;
	options->layout.main_window.vdivider_pos = 200;
	options->layout.main_window.w = 500;
	options->layout.main_window.x = 0;
	options->layout.main_window.y = 0;
	options->layout.order = NULL;
	options->layout.save_window_positions = FALSE;
	options->layout.show_marks = FALSE;
	options->layout.show_thumbnails = FALSE;
	options->layout.style = 0;
	options->layout.toolbar_hidden = FALSE;
	options->layout.tools_float = FALSE;
	options->layout.tools_hidden = FALSE;
	options->layout.tools_restore_state = FALSE;
	options->layout.view_as_icons = FALSE;

	options->lazy_image_sync = FALSE;
	options->mousewheel_scrolls = FALSE;
	options->open_recent_list_maxsize = 10;
	options->place_dialogs_under_mouse = FALSE;

	options->panels.exif.enabled = FALSE;
	options->panels.exif.width = PANEL_DEFAULT_WIDTH;
	options->panels.info.enabled = FALSE;
	options->panels.info.width = PANEL_DEFAULT_WIDTH;
	options->panels.sort.action_state = 0;
	options->panels.sort.enabled = FALSE;
	options->panels.sort.mode_state = 0;
	options->panels.sort.selection_state = 0;

	options->progressive_key_scrolling = FALSE;
	options->show_copy_path = FALSE;
	options->show_icon_names = TRUE;

	options->slideshow.delay = 150;
	options->slideshow.random = FALSE;
	options->slideshow.repeat = FALSE;

	options->startup_path_enable = FALSE;
	options->startup_path = NULL;

	options->thumbnails.cache_into_dirs = FALSE;
	options->thumbnails.enable_caching = TRUE;
	options->thumbnails.fast = TRUE;
	options->thumbnails.max_height = DEFAULT_THUMB_HEIGHT;
	options->thumbnails.max_width = DEFAULT_THUMB_WIDTH;
	options->thumbnails.quality = (gint)GDK_INTERP_TILES;
	options->thumbnails.spec_standard = TRUE;
	options->thumbnails.use_xvpics = TRUE;

	options->tree_descend_subdirs = FALSE;
	options->update_on_time_change = TRUE;

	return options;
}
