/*
 * Geeqie
 * (C) 2006 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include <glib/gstdio.h>
#include <errno.h>

#include "main.h"
#include "rcfile.h"

#include "bar_exif.h"
#include "filelist.h"
#include "secure_save.h"
#include "slideshow.h"
#include "ui_fileops.h"


/*
 *-----------------------------------------------------------------------------
 * line write/parse routines (private)
 *-----------------------------------------------------------------------------
 */ 
 
/* 
   returns text without quotes or NULL for empty or broken string
   any text up to first '"' is skipped
   tail is set to point at the char after the second '"'
   or at the ending \0 
   
*/

gchar *quoted_value(const gchar *text, const gchar **tail)
{
	const gchar *ptr;
	gint c = 0;
	gint l = strlen(text);
	gchar *retval = NULL;
	
	if (tail) *tail = text;
	
	if (l == 0) return retval;

	while (c < l && text[c] !='"') c++;
	if (text[c] == '"')
		{
		gint e;
		c++;
		ptr = text + c;
		e = c;
		while (e < l)
			{
			if (text[e-1] != '\\' && text[e] == '"') break;
			e++;
			}
		if (text[e] == '"')
			{
			if (e - c > 0)
				{
				gchar *substring = g_strndup(ptr, e - c);
				
				if (substring)
					{
					retval = g_strcompress(substring);
					g_free(substring);
					}
				}
			}
		if (tail) *tail = text + e + 1;
		}
	else
		/* for compatibility with older formats (<0.3.7)
		 * read a line without quotes too */
		{
		c = 0;
		while (c < l && text[c] !=' ' && text[c] !=8 && text[c] != '\n') c++;
		if (c != 0)
			{
			retval = g_strndup(text, c);
			}
		if (tail) *tail = text + c;
		}

	return retval;
}

gchar *escquote_value(const gchar *text)
{
	gchar *e;
	
	if (!text) return g_strdup("\"\"");

	e = g_strescape(text, "");
	if (e)
		{
		gchar *retval = g_strdup_printf("\"%s\"", e);
		g_free(e);
		return retval;
		}
	return g_strdup("\"\"");
}

static void write_char_option(SecureSaveInfo *ssi, gchar *label, gchar *text)
{
	gchar *escval = escquote_value(text);

	secure_fprintf(ssi, "%s: %s\n", label, escval);
	g_free(escval);
}

static gchar *read_char_option(FILE *f, gchar *option, gchar *label, gchar *value, gchar *text)
{
	if (strcasecmp(option, label) == 0)
		{
		g_free(text);
		text = quoted_value(value, NULL);
		}
	return text;
}

/* Since gdk_color_to_string() is only available since gtk 2.12
 * here is an equivalent stub function. */
static gchar *color_to_string(GdkColor *color)
{
	return g_strdup_printf("#%04X%04X%04X", color->red, color->green, color->blue);
}

static void write_color_option(SecureSaveInfo *ssi, gchar *label, GdkColor *color)
{
	if (color)
		{
		gchar *colorstring = color_to_string(color);

		write_char_option(ssi, label, colorstring);
		g_free(colorstring);
		}
	else
		secure_fprintf(ssi, "%s: \n", label);
}

static GdkColor *read_color_option(FILE *f, gchar *option, gchar *label, gchar *value, GdkColor *color)
{
	if (strcasecmp(option, label) == 0)
		{
		gchar *colorstr = quoted_value(value, NULL);
		if (colorstr) gdk_color_parse(colorstr, color);
		g_free(colorstr);
		}
	return color;
}


static void write_int_option(SecureSaveInfo *ssi, gchar *label, gint n)
{
	secure_fprintf(ssi, "%s: %d\n", label, n);
}

static gint read_int_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n)
{
	if (strcasecmp(option, label) == 0)
		{
		n = strtol(value, NULL, 10);
		}
	return n;
}

static void write_int_unit_option(SecureSaveInfo *ssi, gchar *label, gint n, gint subunits)
{
	gint l, r;

	if (subunits > 0)
		{
		l = n / subunits;
		r = n % subunits;
		}
	else
		{
		l = n;
		r = 0;
		}

	secure_fprintf(ssi, "%s: %d.%d\n", label, l, r);
}

static gint read_int_unit_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n, gint subunits)
{
	if (strcasecmp(option, label) == 0)
		{
		gint l, r;
		gchar *ptr;

		ptr = value;
		while (*ptr != '\0' && *ptr != '.') ptr++;
		if (*ptr == '.')
			{
			*ptr = '\0';
			l = strtol(value, NULL, 10);
			*ptr = '.';
			ptr++;
			r = strtol(ptr, NULL, 10);
			}
		else
			{
			l = strtol(value, NULL, 10);
			r = 0;
			}

		n = l * subunits + r;
		}
	return n;
}

static void write_bool_option(SecureSaveInfo *ssi, gchar *label, gint n)
{
	secure_fprintf(ssi, "%s: ", label);
	if (n) secure_fprintf(ssi, "true\n"); else secure_fprintf(ssi, "false\n");
}

static gint read_bool_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n)
{
	if (strcasecmp(option, label) == 0)
		{
		if (strcasecmp(value, "true") == 0)
			n = TRUE;
		else
			n = FALSE;
		}
	return n;
}

/*
 *-----------------------------------------------------------------------------
 * save configuration (public)
 *-----------------------------------------------------------------------------
 */ 

void save_options(void)
{
	SecureSaveInfo *ssi;
	gchar *rc_path;
	gchar *rc_pathl;
	gint i;

	rc_path = g_strconcat(homedir(), "/", GQ_RC_DIR, "/", RC_FILE_NAME, NULL);

	rc_pathl = path_from_utf8(rc_path);
	ssi = secure_open(rc_pathl);
	g_free(rc_pathl);
	if (!ssi)
		{
		gchar *buf;

		buf = g_strdup_printf(_("error saving config file: %s\n"), rc_path);
		print_term(buf);
		g_free(buf);

		g_free(rc_path);
		return;
		}
	
	secure_fprintf(ssi, "######################################################################\n");
	secure_fprintf(ssi, "# %30s config file         version %7s #\n", GQ_APPNAME, VERSION);
	secure_fprintf(ssi, "######################################################################\n");
	secure_fputc(ssi, '\n');

	secure_fprintf(ssi, "# Note: This file is autogenerated. Options can be changed here,\n");
	secure_fprintf(ssi, "#       but user comments and formatting will be lost.\n");
	secure_fputc(ssi, '\n');

	secure_fprintf(ssi, "##### General Options #####\n\n");

	write_int_option(ssi, "layout_style", options->layout_style);
	write_char_option(ssi, "layout_order", options->layout_order);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "layout_view_as_icons", options->layout_view_icons);
	write_bool_option(ssi, "layout_view_as_tree", options->layout_view_tree);
	write_bool_option(ssi, "show_icon_names", options->show_icon_names);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "tree_descend_folders", options->tree_descend_subdirs);
	write_bool_option(ssi, "lazy_image_sync", options->lazy_image_sync);
	write_bool_option(ssi, "update_on_time_change", options->update_on_time_change);
	write_bool_option(ssi, "exif_auto_rotate", options->exif_rotate_enable);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "enable_startup_path", options->startup_path_enable);
	write_char_option(ssi, "startup_path", options->startup_path);
	secure_fputc(ssi, '\n');

	secure_fprintf(ssi, "zoom_mode: ");
	if (options->zoom_mode == ZOOM_RESET_ORIGINAL) secure_fprintf(ssi, "original\n");
	if (options->zoom_mode == ZOOM_RESET_FIT_WINDOW) secure_fprintf(ssi, "fit\n");
	if (options->zoom_mode == ZOOM_RESET_NONE) secure_fprintf(ssi, "dont_change\n");
	write_bool_option(ssi, "two_pass_scaling", options->two_pass_zoom);
	write_bool_option(ssi, "zoom_to_fit_allow_expand", options->zoom_to_fit_expands);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "fit_window_to_image", options->fit_window);
	write_bool_option(ssi, "limit_window_size", options->limit_window_size);
	write_int_option(ssi, "max_window_size", options->max_window_size);
	write_bool_option(ssi, "limit_autofit_size", options->limit_autofit_size);
	write_int_option(ssi, "max_autofit_size", options->max_autofit_size);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "progressive_keyboard_scrolling", options->progressive_key_scrolling);
	write_int_option(ssi, "scroll_reset_method", options->scroll_reset_method);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "enable_thumbnails", options->thumbnails_enabled);
	write_int_option(ssi, "thumbnail_width", options->thumb_max_width);
	write_int_option(ssi, "thumbnail_height", options->thumb_max_height);
	write_bool_option(ssi, "cache_thumbnails", options->enable_thumb_caching);
	write_bool_option(ssi, "cache_thumbnails_into_dirs", options->enable_thumb_dirs);
	write_bool_option(ssi, "thumbnail_fast", options->thumbnail_fast);
	write_bool_option(ssi, "use_xvpics_thumbnails", options->use_xvpics_thumbnails);
	write_bool_option(ssi, "thumbnail_spec_standard", options->thumbnail_spec_standard);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "local_metadata", options->enable_metadata_dirs);
	secure_fputc(ssi, '\n');

	write_int_option(ssi, "sort_method", (gint)options->file_sort_method);
	write_bool_option(ssi, "sort_ascending", options->file_sort_ascending);
	write_bool_option(ssi, "sort_case_sensitive", options->file_sort_case_sensitive);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "confirm_delete", options->confirm_delete);
	write_bool_option(ssi, "enable_delete_key", options->enable_delete_key);
	write_bool_option(ssi, "safe_delete", options->safe_delete_enable);
	write_char_option(ssi, "safe_delete_path", options->safe_delete_path);
	write_int_option(ssi, "safe_delete_size", options->safe_delete_size);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "tools_float", options->tools_float);
	write_bool_option(ssi, "tools_hidden", options->tools_hidden);
	write_bool_option(ssi, "restore_tool_state", options->restore_tool);
	write_bool_option(ssi, "toolbar_hidden", options->toolbar_hidden);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "mouse_wheel_scrolls", options->mousewheel_scrolls);
	write_bool_option(ssi, "in_place_rename", options->enable_in_place_rename);
	write_int_option(ssi, "open_recent_max", options->recent_list_max);
	write_int_option(ssi, "image_cache_size_max", options->tile_cache_max);
	write_int_option(ssi, "thumbnail_quality", options->thumbnail_quality);
	write_int_option(ssi, "zoom_quality", options->zoom_quality);
	write_int_option(ssi, "dither_quality", options->dither_quality);
	write_int_option(ssi, "zoom_increment", options->zoom_increment);
	write_bool_option(ssi, "enable_read_ahead", options->enable_read_ahead);
	write_bool_option(ssi, "display_dialogs_under_mouse", options->place_dialogs_under_mouse);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "user_specified_window_background", options->user_specified_window_background);
	write_color_option(ssi, "window_background_color", &options->window_background_color);
	secure_fputc(ssi, '\n');

	write_int_option(ssi, "fullscreen_screen", options->fullscreen_screen);
	write_bool_option(ssi, "fullscreen_clean_flip", options->fullscreen_clean_flip);
	write_bool_option(ssi, "fullscreen_disable_saver", options->fullscreen_disable_saver);
	write_bool_option(ssi, "fullscreen_above", options->fullscreen_above);
	write_bool_option(ssi, "show_fullscreen_info", options->show_fullscreen_info);
	write_char_option(ssi, "fullscreen_info", options->fullscreen_info);
	secure_fputc(ssi, '\n');

	write_int_option(ssi, "custom_similarity_threshold", options->dupe_custom_threshold);

	secure_fprintf(ssi, "\n##### Slideshow Options #####\n\n");

	write_int_unit_option(ssi, "slideshow_delay", options->slideshow_delay, SLIDESHOW_SUBSECOND_PRECISION);

	write_bool_option(ssi, "slideshow_random", options->slideshow_random);
	write_bool_option(ssi, "slideshow_repeat", options->slideshow_repeat);

	secure_fprintf(ssi, "\n##### Filtering Options #####\n\n");

	write_bool_option(ssi, "show_dotfiles", options->show_dot_files);
	write_bool_option(ssi, "disable_filtering", options->file_filter_disable);
	
	filter_write_list(ssi);
	
	sidecar_ext_write(ssi);

	secure_fprintf(ssi, "\n##### Color Profiles #####\n\n");

#ifndef HAVE_LCMS
	secure_fprintf(ssi, "# NOTICE: %s was not built with support for color profiles,\n"
		  	   "#         color profile options will have no effect.\n\n", GQ_APPNAME);
#endif

	write_bool_option(ssi, "color_profile_enabled", options->color_profile_enabled);
	write_bool_option(ssi, "color_profile_use_image", options->color_profile_use_image);
	write_int_option(ssi, "color_profile_input_type", options->color_profile_input_type);
	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		gchar *buf;

		buf = g_strdup_printf("color_profile_input_file_%d", i + 1);
		write_char_option(ssi, buf, options->color_profile_input_file[i]);
		g_free(buf);

		buf = g_strdup_printf("color_profile_input_name_%d", i + 1);
		write_char_option(ssi, buf, options->color_profile_input_name[i]);
		g_free(buf);
		}
	secure_fputc(ssi, '\n');
	write_int_option(ssi, "color_profile_screen_type", options->color_profile_screen_type);
	write_char_option(ssi, "color_profile_screen_file_1", options->color_profile_screen_file);

	secure_fprintf(ssi, "\n##### External Programs #####\n");
	secure_fprintf(ssi, "# Maximum of 10 programs (external_1 through external_10)\n");
	secure_fprintf(ssi, "# format: external_n: \"menu name\" \"command line\"\n\n");

	for (i = 0; i < GQ_EDITOR_SLOTS; i++)
		{
		gchar *qname = escquote_value(options->editor_name[i]);
		gchar *qcommand = escquote_value(options->editor_command[i]);
		secure_fprintf(ssi, "external_%d: %s %s\n", i+1, qname, qcommand);
		g_free(qname);
		g_free(qcommand);
		}

	secure_fprintf(ssi, "\n##### Collection Options #####\n\n");

	write_bool_option(ssi, "rectangular_selections", options->collection_rectangular_selection);

	secure_fprintf(ssi, "\n##### Window Positions #####\n\n");

	write_bool_option(ssi, "restore_window_positions", options->save_window_positions);
	secure_fputc(ssi, '\n');
	write_int_option(ssi, "main_window_x", options->main_window_x);
	write_int_option(ssi, "main_window_y", options->main_window_y);
	write_int_option(ssi, "main_window_width", options->main_window_w);
	write_int_option(ssi, "main_window_height", options->main_window_h);
	write_bool_option(ssi, "main_window_maximized", options->main_window_maximized);
	write_int_option(ssi, "float_window_x", options->float_window_x);
	write_int_option(ssi, "float_window_y", options->float_window_y);
	write_int_option(ssi, "float_window_width", options->float_window_w);
	write_int_option(ssi, "float_window_height", options->float_window_h);
	write_int_option(ssi, "float_window_divider", options->float_window_divider);
	write_int_option(ssi, "divider_position_h", options->window_hdivider_pos);
	write_int_option(ssi, "divider_position_v", options->window_vdivider_pos);

	secure_fprintf(ssi, "\n##### Exif #####\n# 0: never\n# 1: if set\n# 2: always\n\n");
	for (i = 0; ExifUIList[i].key; i++)
		{
		secure_fprintf(ssi, "exif_");
		write_int_option(ssi, (gchar *)ExifUIList[i].key, ExifUIList[i].current);
		}

	secure_fputc(ssi, '\n');
	secure_fprintf(ssi, "######################################################################\n");
	secure_fprintf(ssi, "#                         end of config file                         #\n");
	secure_fprintf(ssi, "######################################################################\n");

	
	if (secure_close(ssi))
		{
		gchar *buf;

		buf = g_strdup_printf(_("error saving config file: %s\nerror: %s\n"), rc_path,
				      secsave_strerror(secsave_errno));
		print_term(buf);
		g_free(buf);

		g_free(rc_path);
		return;
		}

	g_free(rc_path);
}

/*
 *-----------------------------------------------------------------------------
 * load configuration (public)
 *-----------------------------------------------------------------------------
 */ 

void load_options(void)
{
	FILE *f;
	gchar *rc_path;
	gchar *rc_pathl;
	gchar s_buf[1024];
	gchar *s_buf_ptr;
	gchar option[1024];
	gchar value[1024];
	gchar value_all[1024];
	gint c,l,i;

	for (i = 0; ExifUIList[i].key; i++)
		ExifUIList[i].current = ExifUIList[i].default_value;

	rc_path = g_strconcat(homedir(), "/", GQ_RC_DIR, "/", RC_FILE_NAME, NULL);

	rc_pathl = path_from_utf8(rc_path);
	f = fopen(rc_pathl,"r");
	g_free(rc_pathl);
	if (!f)
		{
		g_free(rc_path);
		return;
		}

	while (fgets(s_buf,1024,f))
		{
		if (s_buf[0]=='#') continue;
		if (s_buf[0]=='\n') continue;
		c = 0;
		l = strlen(s_buf);
		while (s_buf[c] != ':' && c < l) c++;
		if (c >= l) continue;
		s_buf[c] = '\0';
		c++;
		while ((s_buf[c] == ' ' || s_buf[c] == 8) && c < l) c++;
		s_buf_ptr = s_buf + c;
		strncpy(value_all, s_buf_ptr, sizeof(value_all));
		while (s_buf[c] != 8 && s_buf[c] != ' ' && s_buf[c] != '\n' && c < l) c++;
		s_buf[c] = '\0';
		strncpy(option, s_buf, sizeof(option));
		strncpy(value, s_buf_ptr, sizeof(value));

		/* general options */

		options->layout_style = read_int_option(f, option,
			"layout_style", value, options->layout_style);
		options->layout_order = read_char_option(f, option,
			"layout_order", value, options->layout_order);
		options->layout_view_icons = read_bool_option(f, option,
			"layout_view_as_icons", value, options->layout_view_icons);
		options->layout_view_tree = read_bool_option(f, option,
			"layout_view_as_tree", value, options->layout_view_tree);
		options->show_icon_names = read_bool_option(f, option,
			"show_icon_names", value, options->show_icon_names);

		options->tree_descend_subdirs = read_bool_option(f, option,
			"tree_descend_folders", value, options->tree_descend_subdirs);
		options->lazy_image_sync = read_bool_option(f, option,
			"lazy_image_sync", value, options->lazy_image_sync);
		options->update_on_time_change = read_bool_option(f, option,
			"update_on_time_change", value, options->update_on_time_change);
		options->exif_rotate_enable = read_bool_option(f, option,
			"exif_auto_rotate", value, options->exif_rotate_enable);

		options->startup_path_enable = read_bool_option(f, option,
			"enable_startup_path", value, options->startup_path_enable);
		options->startup_path = read_char_option(f, option,
			"startup_path", value_all, options->startup_path);

		if (strcasecmp(option, "zoom_mode") == 0)
                        {
                        if (strcasecmp(value, "original") == 0) options->zoom_mode = ZOOM_RESET_ORIGINAL;
                        if (strcasecmp(value, "fit") == 0) options->zoom_mode = ZOOM_RESET_FIT_WINDOW;
                        if (strcasecmp(value, "dont_change") == 0) options->zoom_mode = ZOOM_RESET_NONE;
                        }
		options->two_pass_zoom = read_bool_option(f, option,
			"two_pass_scaling", value, options->two_pass_zoom);
		options->zoom_to_fit_expands = read_bool_option(f, option,
			"zoom_to_fit_allow_expand", value, options->zoom_to_fit_expands);

		options->fit_window = read_bool_option(f, option,
			"fit_window_to_image", value, options->fit_window);
		options->limit_window_size = read_bool_option(f, option,
			"limit_window_size", value, options->limit_window_size);
		options->max_window_size = read_int_option(f, option,
			"max_window_size", value, options->max_window_size);
		options->limit_autofit_size = read_bool_option(f, option,
			"limit_autofit_size", value, options->limit_autofit_size);
		options->max_autofit_size = read_int_option(f, option,
			"max_autofit_size", value, options->max_autofit_size);
		options->progressive_key_scrolling = read_bool_option(f, option,
			"progressive_keyboard_scrolling", value, options->progressive_key_scrolling);
		options->scroll_reset_method = read_int_option(f, option,
			"scroll_reset_method", value, options->scroll_reset_method);

		options->thumbnails_enabled = read_bool_option(f, option,
			"enable_thumbnails", value, options->thumbnails_enabled);
		options->thumb_max_width = read_int_option(f, option,
			"thumbnail_width", value, options->thumb_max_width);
		if (options->thumb_max_width < 16) options->thumb_max_width = 16;
		options->thumb_max_height = read_int_option(f, option,
			"thumbnail_height", value, options->thumb_max_height);
		if (options->thumb_max_height < 16) options->thumb_max_height = 16;
		options->enable_thumb_caching = read_bool_option(f, option,
			"cache_thumbnails", value, options->enable_thumb_caching);
		options->enable_thumb_dirs = read_bool_option(f, option,
			"cache_thumbnails_into_dirs", value, options->enable_thumb_dirs);
		options->thumbnail_fast = read_bool_option(f, option,
			"thumbnail_fast", value, options->thumbnail_fast);
		options->use_xvpics_thumbnails = read_bool_option(f, option,
			"use_xvpics_thumbnails", value, options->use_xvpics_thumbnails);
		options->thumbnail_spec_standard = read_bool_option(f, option,
			"thumbnail_spec_standard", value, options->thumbnail_spec_standard);

		options->enable_metadata_dirs = read_bool_option(f, option,
			"local_metadata", value, options->enable_metadata_dirs);

		options->file_sort_method = (SortType)read_int_option(f, option,
			"sort_method", value, (gint)options->file_sort_method);
		options->file_sort_ascending = read_bool_option(f, option,
			"sort_ascending", value, options->file_sort_ascending);
		options->file_sort_case_sensitive = read_bool_option(f, option,
			"sort_case_sensitive", value, options->file_sort_case_sensitive);

		options->confirm_delete = read_bool_option(f, option,
			"confirm_delete", value, options->confirm_delete);
		options->enable_delete_key = read_bool_option(f, option,
			"enable_delete_key", value, options->enable_delete_key);
		options->safe_delete_enable = read_bool_option(f, option,
			"safe_delete",  value, options->safe_delete_enable);
		options->safe_delete_path = read_char_option(f, option,
			"safe_delete_path", value, options->safe_delete_path);
		options->safe_delete_size = read_int_option(f, option,
			"safe_delete_size", value,options->safe_delete_size);

		options->tools_float = read_bool_option(f, option,
			"tools_float", value, options->tools_float);
		options->tools_hidden = read_bool_option(f, option,
			"tools_hidden", value, options->tools_hidden);
		options->restore_tool = read_bool_option(f, option,
			"restore_tool_state", value, options->restore_tool);

		options->toolbar_hidden = read_bool_option(f, option,
			"toolbar_hidden", value, options->toolbar_hidden);

		options->mousewheel_scrolls = read_bool_option(f, option,
			"mouse_wheel_scrolls", value, options->mousewheel_scrolls);
		options->enable_in_place_rename = read_bool_option(f, option,
			"in_place_rename", value, options->enable_in_place_rename);

		options->recent_list_max = read_int_option(f, option,
			"open_recent_max", value, options->recent_list_max);

		options->tile_cache_max = read_int_option(f, option,
			"image_cache_size_max", value, options->tile_cache_max);

		options->thumbnail_quality = CLAMP(read_int_option(f, option,
			"thumbnail_quality", value, options->thumbnail_quality), GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		options->zoom_quality = CLAMP(read_int_option(f, option,
			"zoom_quality", value, options->zoom_quality), GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		options->dither_quality = CLAMP(read_int_option(f, option,
			"dither_quality", value, options->dither_quality), GDK_RGB_DITHER_NONE, GDK_RGB_DITHER_MAX);

		options->zoom_increment = read_int_option(f, option,
			"zoom_increment", value, options->zoom_increment);

		options->enable_read_ahead = read_bool_option(f, option,
			"enable_read_ahead", value, options->enable_read_ahead);

		options->place_dialogs_under_mouse = read_bool_option(f, option,
			"display_dialogs_under_mouse", value, options->place_dialogs_under_mouse);

		options->user_specified_window_background = read_bool_option(f, option,
			"user_specified_window_background", value, options->user_specified_window_background);
		read_color_option(f, option,
			"window_background_color", value, &options->window_background_color);

		options->fullscreen_screen = read_int_option(f, option,
			"fullscreen_screen", value, options->fullscreen_screen);
		options->fullscreen_clean_flip = read_bool_option(f, option,
			"fullscreen_clean_flip", value, options->fullscreen_clean_flip);
		options->fullscreen_disable_saver = read_bool_option(f, option,
			"fullscreen_disable_saver", value, options->fullscreen_disable_saver);
		options->fullscreen_above = read_bool_option(f, option,
			"fullscreen_above", value, options->fullscreen_above);
		options->show_fullscreen_info = read_bool_option(f, option,
			"show_fullscreen_info", value, options->show_fullscreen_info);
		options->fullscreen_info = read_char_option(f, option,
			"fullscreen_info", value_all, options->fullscreen_info);

		options->dupe_custom_threshold = read_int_option(f, option,
			"custom_similarity_threshold", value, options->dupe_custom_threshold);

		/* slideshow options */

		options->slideshow_delay = read_int_unit_option(f, option,
			"slideshow_delay", value, options->slideshow_delay, SLIDESHOW_SUBSECOND_PRECISION);
		options->slideshow_random = read_bool_option(f, option,
			"slideshow_random", value, options->slideshow_random);
		options->slideshow_repeat = read_bool_option(f, option,
			"slideshow_repeat", value, options->slideshow_repeat);

		/* filtering options */

		options->show_dot_files = read_bool_option(f, option,
			"show_dotfiles", value, options->show_dot_files);
		options->file_filter_disable = read_bool_option(f, option,
			"disable_filtering", value, options->file_filter_disable);

		if (strcasecmp(option, "filter_ext") == 0)
			{
			filter_parse(value_all);
			}

		if (strcasecmp(option, "sidecar_ext") == 0)
			{
			sidecar_ext_parse(value_all, TRUE);
			}
		
		/* Color Profiles */

		options->color_profile_enabled = read_bool_option(f, option,
			"color_profile_enabled", value, options->color_profile_enabled);
		options->color_profile_use_image = read_bool_option(f, option,
			"color_profile_use_image", value, options->color_profile_use_image);
		options->color_profile_input_type = read_int_option(f, option,
			"color_profile_input_type", value, options->color_profile_input_type);

		if (strncasecmp(option, "color_profile_input_file_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				options->color_profile_input_file[i] = read_char_option(f, option,
					option, value, options->color_profile_input_file[i]);
				}
			}
		if (strncasecmp(option, "color_profile_input_name_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				options->color_profile_input_name[i] = read_char_option(f, option,
					option, value, options->color_profile_input_name[i]);
				}
			}

		options->color_profile_screen_type = read_int_option(f, option,
			"color_profile_screen_type", value, options->color_profile_screen_type);
		options->color_profile_screen_file = read_char_option(f, option,
			"color_profile_screen_file_1", value, options->color_profile_screen_file);

		/* External Programs */

		if (strncasecmp(option, "external_", 9) == 0)
			{
			i = strtol(option + 9, NULL, 0);
			if (i > 0 && i <= GQ_EDITOR_SLOTS)
				{
				const gchar *ptr;
				i--;
				g_free(options->editor_name[i]);
				g_free(options->editor_command[i]);
				
				options->editor_name[i] = quoted_value(value_all, &ptr);
				options->editor_command[i] = quoted_value(ptr, NULL);
				}
			}

		/* colection options */

		options->collection_rectangular_selection = read_bool_option(f, option,
			"rectangular_selections", value, options->collection_rectangular_selection);

		/* window positions */

		options->save_window_positions = read_bool_option(f, option,
			"restore_window_positions", value, options->save_window_positions);

		options->main_window_x = read_int_option(f, option,
			"main_window_x", value, options->main_window_x);
		options->main_window_y = read_int_option(f, option,
			"main_window_y", value, options->main_window_y);
		options->main_window_w = read_int_option(f, option,
			"main_window_width", value, options->main_window_w);
		options->main_window_h = read_int_option(f, option,
			"main_window_height", value, options->main_window_h);
		options->main_window_maximized = read_bool_option(f, option,
			"main_window_maximized", value, options->main_window_maximized);
		options->float_window_x = read_int_option(f, option,
			"float_window_x", value, options->float_window_x);
		options->float_window_y = read_int_option(f, option,
			"float_window_y", value, options->float_window_y);
		options->float_window_w = read_int_option(f, option,
			"float_window_width", value, options->float_window_w);
		options->float_window_h = read_int_option(f, option,
			"float_window_height", value, options->float_window_h);
		options->float_window_divider = read_int_option(f, option,
			"float_window_divider", value, options->float_window_divider);
		options->window_hdivider_pos = read_int_option(f, option,
			"divider_position_h", value,options-> window_hdivider_pos);
		options->window_vdivider_pos = read_int_option(f, option,
			"divider_position_v", value, options->window_vdivider_pos);

		if (0 == strncasecmp(option, "exif_", 5))
			{
			for (i = 0; ExifUIList[i].key; i++)
				if (0 == strcasecmp(option+5, ExifUIList[i].key))
					ExifUIList[i].current = strtol(value, NULL, 10);
		  	}
		}

	fclose(f);
	g_free(rc_path);
}

