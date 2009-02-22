/*
 * Geeqie
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik, Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "options.h"

#include "bar_exif.h"
#include "editors.h"
#include "filefilter.h"
#include "histogram.h" /* HCHAN_RGB */
#include "image-overlay.h" /* OSD_SHOW_NOTHING */
#include "layout.h"
#include "layout_image.h"
#include "rcfile.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "window.h"

ConfOptions *init_options(ConfOptions *options)
{
	if (!options) options = g_new0(ConfOptions, 1);

	options->collections.rectangular_selection = FALSE;

	options->color_profile.enabled = TRUE;
	options->color_profile.input_type = 0;
	options->color_profile.screen_file = NULL;
	options->color_profile.screen_type = 0;
	options->color_profile.use_image = TRUE;

	options->dnd_icon_size = 48;
	options->duplicates_similarity_threshold = 99;
	
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

	options->histogram.last_channel_mode = HCHAN_RGB;
	options->histogram.last_log_mode = 1;
	
	memset(&options->image.border_color, 0, sizeof(options->image.border_color));
	options->image.dither_quality = GDK_RGB_DITHER_NORMAL;
	options->image.enable_read_ahead = TRUE;
	options->image.exif_rotate_enable = TRUE;
	options->image.fit_window_to_image = FALSE;
	options->image.idle_read_loop_count = IMAGE_LOADER_IDLE_READ_LOOP_COUNT_DEFAULT;
	options->image.limit_autofit_size = FALSE;
	options->image.limit_window_size = TRUE;
	options->image.max_autofit_size = 100;
	options->image.max_window_size = 90;
	options->image.read_buffer_size = IMAGE_LOADER_READ_BUFFER_SIZE_DEFAULT;
	options->image.scroll_reset_method = SCROLL_RESET_NOCHANGE;
	options->image.tile_cache_max = 10;
	options->image.image_cache_max = 128; /* 4 x 10MPix */
	options->image.use_custom_border_color = FALSE;
	options->image.zoom_2pass = TRUE;
	options->image.zoom_increment = 5;
	options->image.zoom_mode = ZOOM_RESET_NONE;
	options->image.zoom_quality = GDK_INTERP_BILINEAR;
	options->image.zoom_to_fit_allow_expand = FALSE;

	options->image_overlay.common.state = OSD_SHOW_NOTHING;
	options->image_overlay.common.show_at_startup = FALSE;
	options->image_overlay.common.template_string = NULL;
	options->image_overlay.common.x = 10;
	options->image_overlay.common.y = -10;

	options->layout.dir_view_type = DIRVIEW_LIST;
	options->layout.file_view_type = FILEVIEW_LIST;
	options->layout.float_window.h = 450;
	options->layout.float_window.vdivider_pos = -1;
	options->layout.float_window.w = 260;
	options->layout.float_window.x = 0;
	options->layout.float_window.y = 0;
	options->layout.home_path = NULL;
	options->layout.main_window.h = 540;
	options->layout.main_window.hdivider_pos = -1;
	options->layout.main_window.maximized = FALSE;
	options->layout.main_window.vdivider_pos = 200;
	options->layout.main_window.w = 720;
	options->layout.main_window.x = 0;
	options->layout.main_window.y = 0;
	options->layout.order = NULL;
	options->layout.save_window_positions = TRUE;
	options->layout.show_directory_date = FALSE;
	options->layout.show_marks = FALSE;
	options->layout.show_thumbnails = FALSE;
	options->layout.style = 0;
	options->layout.toolbar_hidden = FALSE;
	options->layout.tools_float = FALSE;
	options->layout.tools_hidden = FALSE;
	options->layout.tools_restore_state = TRUE;

	options->lazy_image_sync = FALSE;
	options->mousewheel_scrolls = FALSE;
	options->open_recent_list_maxsize = 10;
	options->place_dialogs_under_mouse = FALSE;

	options->layout.panels.exif.enabled = FALSE;
	options->layout.panels.exif.width = PANEL_DEFAULT_WIDTH;
	options->layout.panels.info.enabled = FALSE;
	options->layout.panels.info.width = PANEL_DEFAULT_WIDTH;
	options->layout.panels.sort.action_state = 0;
	options->layout.panels.sort.enabled = FALSE;
	options->layout.panels.sort.mode_state = 0;
	options->layout.panels.sort.selection_state = 0;
	options->layout.panels.sort.action_filter = NULL;
	
	options->progressive_key_scrolling = TRUE;
	
	options->metadata.enable_metadata_dirs = FALSE;
	options->metadata.save_in_image_file = FALSE;
	options->metadata.save_legacy_IPTC = FALSE;
	options->metadata.warn_on_write_problems = TRUE;
	options->metadata.save_legacy_format = FALSE;
	options->metadata.sync_grouped_files = TRUE;
	options->metadata.confirm_write = TRUE;
	options->metadata.confirm_after_timeout = FALSE;
	options->metadata.confirm_timeout = 10;
	options->metadata.confirm_on_image_change = FALSE;
	options->metadata.confirm_on_dir_change = TRUE;
	
	options->show_copy_path = TRUE;
	options->show_icon_names = TRUE;

	options->slideshow.delay = 50;
	options->slideshow.random = FALSE;
	options->slideshow.repeat = FALSE;

	options->startup.path = NULL;
	options->startup.restore_path = FALSE;
	options->startup.use_last_path = FALSE;

	options->thumbnails.cache_into_dirs = FALSE;
	options->thumbnails.enable_caching = TRUE;
	options->thumbnails.fast = TRUE;
	options->thumbnails.max_height = DEFAULT_THUMB_HEIGHT;
	options->thumbnails.max_width = DEFAULT_THUMB_WIDTH;
	options->thumbnails.quality = GDK_INTERP_TILES;
	options->thumbnails.spec_standard = TRUE;
	options->thumbnails.use_xvpics = TRUE;
	options->thumbnails.use_exif = FALSE;

	options->tree_descend_subdirs = FALSE;
	options->update_on_time_change = TRUE;

	return options;
}

void setup_default_options(ConfOptions *options)
{
	gchar *path;
	gint i;

	bookmark_add_default(_("Home"), homedir());
	path = g_build_filename(homedir(), "Desktop", NULL);
	bookmark_add_default(_("Desktop"), path);
	g_free(path);
	bookmark_add_default(_("Collections"), get_collections_dir());

	g_free(options->file_ops.safe_delete_path);
	options->file_ops.safe_delete_path = g_strdup(get_trash_dir());

	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		options->color_profile.input_file[i] = NULL;
		options->color_profile.input_name[i] = NULL;
		}

	set_default_image_overlay_template_string(&options->image_overlay.common.template_string);
	options->sidecar.ext = g_strdup(".jpg;%raw;.xmp");
	options->layout.order = g_strdup("123");

	options->shell.path = g_strdup(GQ_DEFAULT_SHELL_PATH);
	options->shell.options = g_strdup(GQ_DEFAULT_SHELL_OPTIONS);
	
	for (i = 0; ExifUIList[i].key; i++)
		ExifUIList[i].current = ExifUIList[i].default_value;
}

void copy_layout_options(LayoutOptions *dest, const LayoutOptions *src)
{
	free_layout_options_content(dest);
	
	*dest = *src;
	dest->order = g_strdup(src->order);
	dest->home_path = g_strdup(src->home_path);
	dest->panels.sort.action_filter = g_strdup(src->panels.sort.action_filter);
}

void free_layout_options_content(LayoutOptions *dest)
{
	if (dest->order) g_free(dest->order);
	if (dest->home_path) g_free(dest->home_path);
	if (dest->panels.sort.action_filter) g_free(dest->panels.sort.action_filter);
}

static void sync_options_with_current_state(ConfOptions *options)
{
	LayoutWindow *lw = NULL;

	if (layout_valid(&lw))
		{
		layout_sync_options_with_current_state(lw);
		copy_layout_options(&options->layout, &lw->options);
		options->image_overlay.common.state = image_osd_get(lw->image);
		layout_sort_get(lw, &options->file_sort.method, &options->file_sort.ascending);

	

		options->color_profile.enabled = layout_image_color_profile_get_use(lw);
		layout_image_color_profile_get(lw,
					       &options->color_profile.input_type,
					       &options->color_profile.screen_type,
					       &options->color_profile.use_image);

		if (options->startup.restore_path && options->startup.use_last_path)
			{
			g_free(options->startup.path);
			options->startup.path = g_strdup(layout_get_path(lw));
			}
		}

}

void save_options(ConfOptions *options)
{
	gchar *rc_path;

	sync_options_with_current_state(options);

	rc_path = g_build_filename(get_rc_dir(), RC_FILE_NAME, NULL);
	save_options_to(rc_path, options);
	g_free(rc_path);
}

void load_options(ConfOptions *options)
{
	gboolean success;
	gchar *rc_path;

	if (isdir(GQ_SYSTEM_WIDE_DIR))
		{
		rc_path = g_build_filename(GQ_SYSTEM_WIDE_DIR, RC_FILE_NAME, NULL);
		success = load_options_from(rc_path, options);
		DEBUG_1("Loading options from %s ... %s", rc_path, success ? "done" : "failed");
		g_free(rc_path);
		}
	
	rc_path = g_build_filename(get_rc_dir(), RC_FILE_NAME, NULL);
	success = load_options_from(rc_path, options);
	DEBUG_1("Loading options from %s ... %s", rc_path, success ? "done" : "failed");
	g_free(rc_path);
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
