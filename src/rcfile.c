/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
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
#include "editors.h"
#include "filefilter.h"
#include "misc.h"
#include "pixbuf-renderer.h"
#include "secure_save.h"
#include "slideshow.h"
#include "ui_fileops.h"

/*
 *-----------------------------------------------------------------------------
 * line write/parse routines (private)
 *-----------------------------------------------------------------------------
 */


static void write_char_option(SecureSaveInfo *ssi, gchar *label, gchar *text)
{
	gchar *escval = escquote_value(text);

	secure_fprintf(ssi, "%s: %s\n", label, escval);
	g_free(escval);
}

static gboolean read_char_option(FILE *f, gchar *option, gchar *label, gchar *value, gchar **text)
{
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!text) return FALSE;

	g_free(*text);
	*text = quoted_value(value, NULL);
	return TRUE;
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

static gboolean read_color_option(FILE *f, gchar *option, gchar *label, gchar *value, GdkColor *color)
{
	gchar *colorstr;
	
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!color) return FALSE;

	colorstr = quoted_value(value, NULL);
	if (!colorstr) return FALSE;
	gdk_color_parse(colorstr, color);
	g_free(colorstr);
	return TRUE;
}

static void write_int_option(SecureSaveInfo *ssi, gchar *label, gint n)
{
	secure_fprintf(ssi, "%s: %d\n", label, n);
}

static gboolean read_int_option(FILE *f, gchar *option, gchar *label, gchar *value, gint *n)
{
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!n) return FALSE;

	if (g_ascii_isdigit(value[0]) || (value[0] == '-' && g_ascii_isdigit(value[1])))
		{
		*n = strtol(value, NULL, 10);
		}
	else
		{
		if (g_ascii_strcasecmp(value, "true") == 0)
			*n = 1;
		else
			*n = 0;
		}

	return TRUE;
}

static void write_uint_option(SecureSaveInfo *ssi, gchar *label, guint n)
{
	secure_fprintf(ssi, "%s: %u\n", label, n);
}

static gboolean read_uint_option(FILE *f, gchar *option, gchar *label, gchar *value, guint *n)
{
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!n) return FALSE;

	if (g_ascii_isdigit(value[0]))
		{
		*n = strtoul(value, NULL, 10);
		}
	else
		{
		if (g_ascii_strcasecmp(value, "true") == 0)
			*n = 1;
		else
			*n = 0;
		}
	
	return TRUE;
}

static gboolean read_uint_option_clamp(FILE *f, gchar *option, gchar *label, gchar *value, guint *n, guint min, guint max)
{
	gboolean ret;

	ret = read_uint_option(f, option, label, value, n);
	if (ret) *n = CLAMP(*n, min, max);

	return ret;
}


static gboolean read_int_option_clamp(FILE *f, gchar *option, gchar *label, gchar *value, gint *n, gint min, gint max)
{
	gboolean ret;

	ret = read_int_option(f, option, label, value, n);
	if (ret) *n = CLAMP(*n, min, max);

	return ret;
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

static gboolean read_int_unit_option(FILE *f, gchar *option, gchar *label, gchar *value, gint *n, gint subunits)
{
	gint l, r;
	gchar *ptr;

	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!n) return FALSE;

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

	*n = l * subunits + r;

	return TRUE;
}

static void write_bool_option(SecureSaveInfo *ssi, gchar *label, gint n)
{
	secure_fprintf(ssi, "%s: ", label);
	if (n) secure_fprintf(ssi, "true\n"); else secure_fprintf(ssi, "false\n");
}

static gboolean read_bool_option(FILE *f, gchar *option, gchar *label, gchar *value, gint *n)
{
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!n) return FALSE;

	if (g_ascii_strcasecmp(value, "true") == 0 || atoi(value) != 0)
		*n = TRUE;
	else
		*n = FALSE;

	return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 * save configuration (public)
 *-----------------------------------------------------------------------------
 */

gboolean save_options_to(const gchar *utf8_path, ConfOptions *options)
{
	SecureSaveInfo *ssi;
	gchar *rc_pathl;
	gint i;

	rc_pathl = path_from_utf8(utf8_path);
	ssi = secure_open(rc_pathl);
	g_free(rc_pathl);
	if (!ssi)
		{
		log_printf(_("error saving config file: %s\n"), utf8_path);
		return FALSE;
		}

#define WRITE_BOOL(_name_) write_bool_option(ssi, #_name_, options->_name_)
#define WRITE_INT(_name_) write_int_option(ssi, #_name_, options->_name_)
#define WRITE_UINT(_name_) write_uint_option(ssi, #_name_, options->_name_)
#define WRITE_INT_UNIT(_name_, _unit_) write_int_unit_option(ssi, #_name_, options->_name_, _unit_)
#define WRITE_CHAR(_name_) write_char_option(ssi, #_name_, options->_name_)
#define WRITE_COLOR(_name_) write_color_option(ssi, #_name_, &options->_name_)

#define WRITE_SEPARATOR() secure_fputc(ssi, '\n')
#define WRITE_SUBTITLE(_title_) secure_fprintf(ssi, "\n\n##### "_title_" #####\n\n")

	secure_fprintf(ssi, "######################################################################\n");
	secure_fprintf(ssi, "# %30s config file      version %-10s #\n", GQ_APPNAME, VERSION);
	secure_fprintf(ssi, "######################################################################\n");
	WRITE_SEPARATOR();

	secure_fprintf(ssi, "# Note: This file is autogenerated. Options can be changed here,\n");
	secure_fprintf(ssi, "#       but user comments and formatting will be lost.\n");
	WRITE_SEPARATOR();

	WRITE_SUBTITLE("General Options");

	WRITE_BOOL(show_icon_names);
	WRITE_BOOL(show_copy_path);
	WRITE_SEPARATOR();

	WRITE_BOOL(tree_descend_subdirs);
	WRITE_BOOL(lazy_image_sync);
	WRITE_BOOL(update_on_time_change);
	WRITE_SEPARATOR();

	WRITE_BOOL(progressive_key_scrolling);

	WRITE_UINT(duplicates_similarity_threshold);
	WRITE_SEPARATOR();

	WRITE_BOOL(mousewheel_scrolls);
	WRITE_INT(open_recent_list_maxsize);
	WRITE_INT(dnd_icon_size);
	WRITE_BOOL(place_dialogs_under_mouse);


	WRITE_SUBTITLE("Startup Options");

	WRITE_BOOL(startup.restore_path);
	WRITE_BOOL(startup.use_last_path);
	WRITE_CHAR(startup.path);


	WRITE_SUBTITLE("File operations Options");

	WRITE_BOOL(file_ops.enable_in_place_rename);
	WRITE_BOOL(file_ops.confirm_delete);
	WRITE_BOOL(file_ops.enable_delete_key);
	WRITE_BOOL(file_ops.safe_delete_enable);
	WRITE_CHAR(file_ops.safe_delete_path);
	WRITE_INT(file_ops.safe_delete_folder_maxsize);


	WRITE_SUBTITLE("Layout Options");

	WRITE_INT(layout.style);
	WRITE_CHAR(layout.order);
	WRITE_UINT(layout.dir_view_type);
	WRITE_UINT(layout.file_view_type);
	WRITE_BOOL(layout.show_marks);
	WRITE_BOOL(layout.show_thumbnails);
	WRITE_BOOL(layout.show_directory_date);
	WRITE_CHAR(layout.home_path);
	WRITE_SEPARATOR();

	WRITE_BOOL(layout.save_window_positions);
	WRITE_SEPARATOR();

	WRITE_INT(layout.main_window.x);
	WRITE_INT(layout.main_window.y);
	WRITE_INT(layout.main_window.w);
	WRITE_INT(layout.main_window.h);
	WRITE_BOOL(layout.main_window.maximized);
	WRITE_INT(layout.main_window.hdivider_pos);
	WRITE_INT(layout.main_window.vdivider_pos);
	WRITE_SEPARATOR();

	WRITE_INT(layout.float_window.x);
	WRITE_INT(layout.float_window.y);
	WRITE_INT(layout.float_window.w);
	WRITE_INT(layout.float_window.h);
	WRITE_INT(layout.float_window.vdivider_pos);
	WRITE_SEPARATOR();

	WRITE_INT(layout.properties_window.w);
	WRITE_INT(layout.properties_window.h);
	WRITE_SEPARATOR();

	WRITE_BOOL(layout.tools_float);
	WRITE_BOOL(layout.tools_hidden);
	WRITE_BOOL(layout.tools_restore_state);
	WRITE_SEPARATOR();

	WRITE_BOOL(layout.toolbar_hidden);

	WRITE_SUBTITLE("Panels Options");

	WRITE_BOOL(panels.exif.enabled);
	WRITE_INT(panels.exif.width);
	WRITE_BOOL(panels.info.enabled);
	WRITE_INT(panels.info.width);
	WRITE_BOOL(panels.sort.enabled);
	WRITE_INT(panels.sort.action_state);
	WRITE_INT(panels.sort.mode_state);
	WRITE_INT(panels.sort.selection_state);

	WRITE_SUBTITLE("Properties dialog Options");
	WRITE_CHAR(properties.tabs_order);

	WRITE_SUBTITLE("Image Options");

	secure_fprintf(ssi, "# image.zoom_mode possible values are:\n"
			    "#   original\n"
			    "#   fit\n"
			    "#   dont_change\n");
	secure_fprintf(ssi, "image.zoom_mode: ");
	switch (options->image.zoom_mode)
	{
	case ZOOM_RESET_ORIGINAL: secure_fprintf(ssi, "original\n"); break;
	case ZOOM_RESET_FIT_WINDOW: secure_fprintf(ssi, "fit\n"); break;
	case ZOOM_RESET_NONE: secure_fprintf(ssi, "dont_change\n"); break;
	}
	WRITE_SEPARATOR();
	WRITE_BOOL(image.zoom_2pass);
	WRITE_BOOL(image.zoom_to_fit_allow_expand);
	WRITE_UINT(image.zoom_quality);
	WRITE_INT(image.zoom_increment);
	WRITE_BOOL(image.fit_window_to_image);
	WRITE_BOOL(image.limit_window_size);
	WRITE_INT(image.max_window_size);
	WRITE_BOOL(image.limit_autofit_size);
	WRITE_INT(image.max_autofit_size);
	WRITE_UINT(image.scroll_reset_method);
	WRITE_INT(image.tile_cache_max);
	WRITE_INT(image.image_cache_max);
	WRITE_UINT(image.dither_quality);
	WRITE_BOOL(image.enable_read_ahead);
	WRITE_BOOL(image.exif_rotate_enable);
	WRITE_BOOL(image.use_custom_border_color);
	WRITE_COLOR(image.border_color);
	WRITE_INT(image.read_buffer_size);
	WRITE_INT(image.idle_read_loop_count);

	WRITE_SUBTITLE("Thumbnails Options");

	WRITE_INT(thumbnails.max_width);
	WRITE_INT(thumbnails.max_height);
	WRITE_BOOL(thumbnails.enable_caching);
	WRITE_BOOL(thumbnails.cache_into_dirs);
	WRITE_BOOL(thumbnails.fast);
	WRITE_BOOL(thumbnails.use_xvpics);
	WRITE_BOOL(thumbnails.spec_standard);
	WRITE_UINT(thumbnails.quality);
	WRITE_BOOL(thumbnails.use_exif);


	WRITE_SUBTITLE("File sorting Options");

	WRITE_INT(file_sort.method);
	WRITE_BOOL(file_sort.ascending);
	WRITE_BOOL(file_sort.case_sensitive);


	WRITE_SUBTITLE("Fullscreen Options");

	WRITE_INT(fullscreen.screen);
	WRITE_BOOL(fullscreen.clean_flip);
	WRITE_BOOL(fullscreen.disable_saver);
	WRITE_BOOL(fullscreen.above);


	WRITE_SUBTITLE("Histogram Options");
	WRITE_UINT(histogram.last_channel_mode);
	WRITE_UINT(histogram.last_log_mode);


	WRITE_SUBTITLE("Image Overlay Options");
	WRITE_UINT(image_overlay.common.state);
	WRITE_BOOL(image_overlay.common.show_at_startup);
	WRITE_CHAR(image_overlay.common.template_string);
	WRITE_SEPARATOR();

	secure_fprintf(ssi, "# these are relative positions:\n");
	secure_fprintf(ssi, "# x >= 0: |x| pixels from left border\n");
	secure_fprintf(ssi, "# x < 0 : |x| pixels from right border\n");
	secure_fprintf(ssi, "# y >= 0: |y| pixels from top border\n");
	secure_fprintf(ssi, "# y < 0 : |y| pixels from bottom border\n");
	WRITE_INT(image_overlay.common.x);
	WRITE_INT(image_overlay.common.y);


	WRITE_SUBTITLE("Slideshow Options");

	WRITE_INT_UNIT(slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
	WRITE_BOOL(slideshow.random);
	WRITE_BOOL(slideshow.repeat);


	WRITE_SUBTITLE("Collection Options");

	WRITE_BOOL(collections.rectangular_selection);


	WRITE_SUBTITLE("Filtering Options");

	WRITE_BOOL(file_filter.show_hidden_files);
	WRITE_BOOL(file_filter.show_dot_directory);
	WRITE_BOOL(file_filter.disable);
	WRITE_SEPARATOR();

	filter_write_list(ssi);


	WRITE_SUBTITLE("Sidecars Options");

	WRITE_CHAR(sidecar.ext);

	WRITE_SUBTITLE("Color Profiles");

#ifndef HAVE_LCMS
	secure_fprintf(ssi, "# NOTICE: %s was not built with support for color profiles,\n"
			    "#         color profile options will have no effect.\n\n", GQ_APPNAME);
#endif

	WRITE_BOOL(color_profile.enabled);
	WRITE_BOOL(color_profile.use_image);
	WRITE_INT(color_profile.input_type);
	WRITE_SEPARATOR();

	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		gchar *buf;

		buf = g_strdup_printf("color_profile.input_file_%d", i + 1);
		write_char_option(ssi, buf, options->color_profile.input_file[i]);
		g_free(buf);

		buf = g_strdup_printf("color_profile.input_name_%d", i + 1);
		write_char_option(ssi, buf, options->color_profile.input_name[i]);
		g_free(buf);
		}

	WRITE_SEPARATOR();
	WRITE_INT(color_profile.screen_type);
	WRITE_CHAR(color_profile.screen_file);


	WRITE_SUBTITLE("Shell command");
	WRITE_CHAR(shell.path);
	WRITE_CHAR(shell.options);


	WRITE_SUBTITLE("Helpers");
	secure_fprintf(ssi, "# Html browser\n");
	secure_fprintf(ssi, "# command_name is: the binary's name to look for in the path\n");
	secure_fprintf(ssi, "# If command_name is empty, the program will try various common html browsers\n");
	secure_fprintf(ssi, "# command_line is:\n");
	secure_fprintf(ssi, "# \"\" (empty string)  = execute binary with html file path as command line\n");
	secure_fprintf(ssi, "# \"string\"           = execute string and use results for command line\n");
	secure_fprintf(ssi, "# \"!string\"          = use text following ! as command line, replacing optional %%s with html file path\n");
	WRITE_CHAR(helpers.html_browser.command_name);
	WRITE_CHAR(helpers.html_browser.command_line);


	WRITE_SUBTITLE("External Programs");
	secure_fprintf(ssi, "# Maximum of %d programs (external_1 through external_%d)\n", GQ_EDITOR_GENERIC_SLOTS, GQ_EDITOR_GENERIC_SLOTS);
	secure_fprintf(ssi, "# external_%d through external_%d are used for file ops\n", GQ_EDITOR_GENERIC_SLOTS + 1, GQ_EDITOR_SLOTS);
	secure_fprintf(ssi, "# format: external_n: \"menu name\" \"command line\"\n\n");

	for (i = 0; i < GQ_EDITOR_SLOTS; i++)
		{
		if (i == GQ_EDITOR_GENERIC_SLOTS) secure_fputc(ssi, '\n');
		gchar *qname = escquote_value(options->editor[i].name);
		gchar *qcommand = escquote_value(options->editor[i].command);
		secure_fprintf(ssi, "external_%d: %s %s\n", i+1, qname, qcommand);
		g_free(qname);
		g_free(qcommand);
		}


	WRITE_SUBTITLE("Exif Options");
	secure_fprintf(ssi, "# Display: 0: never\n"
			    "#          1: if set\n"
			    "#          2: always\n\n");
	for (i = 0; ExifUIList[i].key; i++)
		{
		secure_fprintf(ssi, "exif.display.");
		write_int_option(ssi, (gchar *)ExifUIList[i].key, ExifUIList[i].current);
		}

	WRITE_SUBTITLE("Metadata Options");
	WRITE_BOOL(metadata.enable_metadata_dirs);
	WRITE_BOOL(metadata.save_in_image_file); 
	WRITE_BOOL(metadata.save_legacy_IPTC);
	WRITE_BOOL(metadata.warn_on_write_problems);
	WRITE_BOOL(metadata.save_legacy_format);
	WRITE_BOOL(metadata.sync_grouped_files);
	WRITE_BOOL(metadata.confirm_write);
	WRITE_INT(metadata.confirm_timeout);
	WRITE_BOOL(metadata.confirm_after_timeout);
	WRITE_BOOL(metadata.confirm_on_image_change);
	WRITE_BOOL(metadata.confirm_on_dir_change);

	WRITE_SUBTITLE("Documentation Options");
	WRITE_CHAR(documentation.helpdir);
	WRITE_CHAR(documentation.htmldir);

	WRITE_SEPARATOR();
	WRITE_SEPARATOR();

	secure_fprintf(ssi, "######################################################################\n");
	secure_fprintf(ssi, "#                         end of config file                         #\n");
	secure_fprintf(ssi, "######################################################################\n");


	if (secure_close(ssi))
		{
		log_printf(_("error saving config file: %s\nerror: %s\n"), utf8_path,
			   secsave_strerror(secsave_errno));
		return FALSE;
		}

	return TRUE;
}




/*
 *-----------------------------------------------------------------------------
 * load configuration (public)
 *-----------------------------------------------------------------------------
 */

static gboolean is_numbered_option(const gchar *option, const gchar *prefix, gint *number)
{
	gsize n;
	gsize option_len = strlen(option);
	gsize prefix_len = strlen(prefix);
	
	if (option_len <= prefix_len) return FALSE;
	if (g_ascii_strncasecmp(option, prefix, prefix_len) != 0) return FALSE;

	n = prefix_len;
	while (g_ascii_isdigit(option[n])) n++;
	if (n < option_len) return FALSE;
	
	if (number) *number = atoi(option + prefix_len);
	return TRUE;
}

#define OPTION_READ_BUFFER_SIZE 1024

gboolean load_options_from(const gchar *utf8_path, ConfOptions *options)
{
	FILE *f;
	gchar *rc_pathl;
	gchar s_buf[OPTION_READ_BUFFER_SIZE];
	gchar value_all[OPTION_READ_BUFFER_SIZE];
	gchar *option;
	gchar *value;
	gint i;

	rc_pathl = path_from_utf8(utf8_path);
	f = fopen(rc_pathl,"r");
	g_free(rc_pathl);
	if (!f) return FALSE;

	while (fgets(s_buf, sizeof(s_buf), f))
		{
		gchar *value_end;
		gchar *p = s_buf;

		/* skip empty lines and comments */
		while (g_ascii_isspace(*p)) p++;
		if (!*p || *p == '\n' || *p == '#') continue;

		/* parse option name */
		option = p;
		while (g_ascii_isalnum(*p) || *p == '_' || *p == '.') p++;
		if (!*p) continue;
		*p = '\0';
		p++;

		/* search for value start, name and value are normally separated by ': '
		 * but we allow relaxed syntax here, so '=', ':=' or just a tab will work too */
		while (*p == ':' || g_ascii_isspace(*p) || *p == '=') p++;
		value = p;

		while (*p && !g_ascii_isspace(*p) && *p != '\n') p++;
		value_end = p; /* value part up to the first whitespace or end of line */
		while (*p != '\0') p++;
		memcpy(value_all, value, 1 + p - value);

		*value_end = '\0';

#define READ_BOOL(_name_) if (read_bool_option(f, option, #_name_, value, &options->_name_)) continue;
#define READ_INT(_name_) if (read_int_option(f, option, #_name_, value, &options->_name_)) continue;
#define READ_UINT(_name_) if (read_uint_option(f, option, #_name_, value, &options->_name_)) continue;
#define READ_INT_CLAMP(_name_, _min_, _max_) if (read_int_option_clamp(f, option, #_name_, value, &options->_name_, _min_, _max_)) continue;
#define READ_UINT_CLAMP(_name_, _min_, _max_) if (read_uint_option_clamp(f, option, #_name_, value, &options->_name_, _min_, _max_)) continue;
#define READ_INT_UNIT(_name_, _unit_) if (read_int_unit_option(f, option, #_name_, value, &options->_name_, _unit_)) continue;
#define READ_CHAR(_name_) if (read_char_option(f, option, #_name_, value_all, &options->_name_)) continue;
#define READ_COLOR(_name_) if (read_color_option(f, option, #_name_, value, &options->_name_)) continue;

#define COMPAT_READ_BOOL(_oldname_, _name_) if (read_bool_option(f, option, #_oldname_, value, &options->_name_)) continue;
#define COMPAT_READ_INT(_oldname_, _name_) if (read_int_option(f, option, #_oldname_, value, &options->_name_)) continue;
#define COMPAT_READ_UINT(_oldname_, _name_) if (read_uint_option(f, option, #_oldname_, value, &options->_name_)) continue;
#define COMPAT_READ_INT_CLAMP(_oldname_, _name_, _min_, _max_) if (read_int_option_clamp(f, option, #_oldname_, value, &options->_name_, _min_, _max_)) continue;
#define COMPAT_READ_INT_UNIT(_oldname_, _name_, _unit_) if (read_int_unit_option(f, option, #_oldname_, value, &options->_name_, _unit_)) continue;
#define COMPAT_READ_CHAR(_oldname_, _name_) if (read_char_option(f, option, #_oldname_, value_all, &options->_name_)) continue;
#define COMPAT_READ_COLOR(_oldname_, _name_) if (read_color_option(f, option, #_oldname_, value, &options->_name_)) continue;

		/* general options */
		READ_BOOL(show_icon_names);
		READ_BOOL(show_copy_path);

		READ_BOOL(tree_descend_subdirs);
		READ_BOOL(lazy_image_sync);
		READ_BOOL(update_on_time_change);

		READ_UINT_CLAMP(duplicates_similarity_threshold, 0, 100);

		READ_BOOL(progressive_key_scrolling);

		READ_BOOL(mousewheel_scrolls);

		READ_INT(open_recent_list_maxsize);
		READ_INT(dnd_icon_size);
		READ_BOOL(place_dialogs_under_mouse);

		/* startup options */
		
		COMPAT_READ_BOOL(startup_path_enable, startup.restore_path); /* 2008/05/11 */
		READ_BOOL(startup.restore_path);

		READ_BOOL(startup.use_last_path);

		COMPAT_READ_CHAR(startup_path, startup.path); /* 2008/05/11 */
		READ_CHAR(startup.path);
	
		/* layout options */

		READ_INT(layout.style);
		READ_CHAR(layout.order);
		
		COMPAT_READ_UINT(layout.view_as_icons, layout.file_view_type); /* 2008/05/03 */

		READ_UINT(layout.dir_view_type);
		READ_UINT(layout.file_view_type);
		READ_BOOL(layout.show_marks);
		READ_BOOL(layout.show_thumbnails);
		READ_BOOL(layout.show_directory_date);
		READ_CHAR(layout.home_path);

		/* window positions */

		READ_BOOL(layout.save_window_positions);

		READ_INT(layout.main_window.x);
		READ_INT(layout.main_window.y);
		READ_INT(layout.main_window.w);
		READ_INT(layout.main_window.h);
		READ_BOOL(layout.main_window.maximized);
		READ_INT(layout.main_window.hdivider_pos);
		READ_INT(layout.main_window.vdivider_pos);

		READ_INT(layout.float_window.x);
		READ_INT(layout.float_window.y);
		READ_INT(layout.float_window.w);
		READ_INT(layout.float_window.h);
		READ_INT(layout.float_window.vdivider_pos);
	
		READ_INT(layout.properties_window.w);
		READ_INT(layout.properties_window.h);

		READ_BOOL(layout.tools_float);
		READ_BOOL(layout.tools_hidden);
		READ_BOOL(layout.tools_restore_state);
		READ_BOOL(layout.toolbar_hidden);

		/* panels */
		READ_BOOL(panels.exif.enabled);
		READ_INT_CLAMP(panels.exif.width, PANEL_MIN_WIDTH, PANEL_MAX_WIDTH);
		READ_BOOL(panels.info.enabled);
		READ_INT_CLAMP(panels.info.width, PANEL_MIN_WIDTH, PANEL_MAX_WIDTH);
		READ_BOOL(panels.sort.enabled);
		READ_INT(panels.sort.action_state);
		READ_INT(panels.sort.mode_state);
		READ_INT(panels.sort.selection_state);

		/* properties dialog options */
		READ_CHAR(properties.tabs_order);

		/* image options */
		if (g_ascii_strcasecmp(option, "image.zoom_mode") == 0)
			{
			if (g_ascii_strcasecmp(value, "original") == 0)
				options->image.zoom_mode = ZOOM_RESET_ORIGINAL;
			else if (g_ascii_strcasecmp(value, "fit") == 0)
				options->image.zoom_mode = ZOOM_RESET_FIT_WINDOW;
			else if (g_ascii_strcasecmp(value, "dont_change") == 0)
				options->image.zoom_mode = ZOOM_RESET_NONE;
			continue;
			}
		READ_BOOL(image.zoom_2pass);
		READ_BOOL(image.zoom_to_fit_allow_expand);
		READ_BOOL(image.fit_window_to_image);
		READ_BOOL(image.limit_window_size);
		READ_INT(image.max_window_size);
		READ_BOOL(image.limit_autofit_size);
		READ_INT(image.max_autofit_size);
		READ_UINT_CLAMP(image.scroll_reset_method, 0, PR_SCROLL_RESET_COUNT - 1);
		READ_INT(image.tile_cache_max);
		READ_INT(image.image_cache_max);
		READ_UINT_CLAMP(image.zoom_quality, GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		READ_UINT_CLAMP(image.dither_quality, GDK_RGB_DITHER_NONE, GDK_RGB_DITHER_MAX);
		READ_INT(image.zoom_increment);
		READ_BOOL(image.enable_read_ahead);
		READ_BOOL(image.exif_rotate_enable);
		READ_BOOL(image.use_custom_border_color);
		READ_COLOR(image.border_color);
		READ_INT_CLAMP(image.read_buffer_size, IMAGE_LOADER_READ_BUFFER_SIZE_MIN, IMAGE_LOADER_READ_BUFFER_SIZE_MAX);
		READ_INT_CLAMP(image.idle_read_loop_count, IMAGE_LOADER_IDLE_READ_LOOP_COUNT_MIN, IMAGE_LOADER_IDLE_READ_LOOP_COUNT_MAX);


		/* thumbnails options */
		READ_INT_CLAMP(thumbnails.max_width, 16, 512);
		READ_INT_CLAMP(thumbnails.max_height, 16, 512);

		READ_BOOL(thumbnails.enable_caching);
		READ_BOOL(thumbnails.cache_into_dirs);
		READ_BOOL(thumbnails.fast);
		READ_BOOL(thumbnails.use_xvpics);
		READ_BOOL(thumbnails.spec_standard);
		READ_UINT_CLAMP(thumbnails.quality, GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		READ_BOOL(thumbnails.use_exif);

		/* file sorting options */
		READ_UINT(file_sort.method);
		READ_BOOL(file_sort.ascending);
		READ_BOOL(file_sort.case_sensitive);

		/* file operations options */
		READ_BOOL(file_ops.enable_in_place_rename);
		READ_BOOL(file_ops.confirm_delete);
		READ_BOOL(file_ops.enable_delete_key);
		READ_BOOL(file_ops.safe_delete_enable);
		READ_CHAR(file_ops.safe_delete_path);
		READ_INT(file_ops.safe_delete_folder_maxsize);

		/* fullscreen options */
		READ_INT(fullscreen.screen);
		READ_BOOL(fullscreen.clean_flip);
		READ_BOOL(fullscreen.disable_saver);
		READ_BOOL(fullscreen.above);

		/* histogram */
		READ_UINT(histogram.last_channel_mode);
		READ_UINT(histogram.last_log_mode);

		/* image overlay */
		COMPAT_READ_UINT(image_overlay.common.enabled, image_overlay.common.state); /* 2008-05-12 */
		READ_UINT(image_overlay.common.state);
		COMPAT_READ_BOOL(fullscreen.show_info, image_overlay.common.show_at_startup); /* 2008-04-21 */
		READ_BOOL(image_overlay.common.show_at_startup);
		COMPAT_READ_CHAR(fullscreen.info, image_overlay.common.template_string); /* 2008-04-21 */
		READ_CHAR(image_overlay.common.template_string);

		READ_INT(image_overlay.common.x);
		READ_INT(image_overlay.common.y);


		/* slideshow options */
		READ_INT_UNIT(slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
		READ_BOOL(slideshow.random);
		READ_BOOL(slideshow.repeat);

		/* collection options */

		READ_BOOL(collections.rectangular_selection);

		/* filtering options */

		READ_BOOL(file_filter.show_hidden_files);
		READ_BOOL(file_filter.show_dot_directory);
		READ_BOOL(file_filter.disable);

		if (g_ascii_strcasecmp(option, "file_filter.ext") == 0)
			{
			filter_parse(value_all);
			continue;
			}

		READ_CHAR(sidecar.ext);

		/* Color Profiles */

		READ_BOOL(color_profile.enabled);
		READ_BOOL(color_profile.use_image);
		READ_INT(color_profile.input_type);

		if (is_numbered_option(option, "color_profile.input_file_", &i))
			{
			if (i > 0 && i <= COLOR_PROFILE_INPUTS)
				{
				i--;
				read_char_option(f, option, option, value, &options->color_profile.input_file[i]);
				}
			continue;
			}

		if (is_numbered_option(option, "color_profile.input_name_", &i))
			{
			if (i > 0 && i <= COLOR_PROFILE_INPUTS)
				{
				i--;
				read_char_option(f, option, option, value, &options->color_profile.input_name[i]);
				}
			continue;
			}

		READ_INT(color_profile.screen_type);
		READ_CHAR(color_profile.screen_file);

		/* Shell command */
		READ_CHAR(shell.path);
		READ_CHAR(shell.options);

		/* Helpers */
		READ_CHAR(helpers.html_browser.command_name);
		READ_CHAR(helpers.html_browser.command_line);

		/* External Programs */

		if (is_numbered_option(option, "external_", &i))
			{
			if (i > 0 && i <= GQ_EDITOR_SLOTS)
				{
				const gchar *ptr;

				i--;
				editor_set_name(i, quoted_value(value_all, &ptr));
				editor_set_command(i, quoted_value(ptr, NULL));
				}
			continue;
			}

		/* Exif */
		if (0 == g_ascii_strncasecmp(option, "exif.display.", 13))
			{
			for (i = 0; ExifUIList[i].key; i++)
				if (0 == g_ascii_strcasecmp(option + 13, ExifUIList[i].key))
					ExifUIList[i].current = strtol(value, NULL, 10);
			continue;
			}

		/* metadata */		
		COMPAT_READ_BOOL(enable_metadata_dirs, metadata.enable_metadata_dirs); /* 2008/12/20 */
		READ_BOOL(metadata.enable_metadata_dirs);
		COMPAT_READ_BOOL(save_metadata_in_image_file, metadata.save_in_image_file); /* 2008/12/20 */
		READ_BOOL(metadata.save_in_image_file);
		READ_BOOL(metadata.save_legacy_IPTC);
		READ_BOOL(metadata.warn_on_write_problems);
		READ_BOOL(metadata.save_legacy_format);
		READ_BOOL(metadata.sync_grouped_files);
		READ_BOOL(metadata.confirm_write);
		READ_BOOL(metadata.confirm_after_timeout);
		READ_INT(metadata.confirm_timeout);
		READ_BOOL(metadata.confirm_on_image_change);
		READ_BOOL(metadata.confirm_on_dir_change);

		/* Documentation */
		READ_CHAR(documentation.helpdir);
		READ_CHAR(documentation.htmldir);
		
		}

	fclose(f);
	return TRUE;
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
