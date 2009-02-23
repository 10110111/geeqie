/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
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

#include "bar.h"
#include "bar_comment.h"
#include "bar_exif.h"
#include "bar_histogram.h"
#include "bar_keywords.h"
#include "editors.h"
#include "filefilter.h"
#include "misc.h"
#include "pixbuf-renderer.h"
#include "secure_save.h"
#include "slideshow.h"
#include "ui_fileops.h"
#include "layout.h"
#include "layout_util.h"
#include "bar.h"


/*
 *-----------------------------------------------------------------------------
 * line write/parse routines (public)
 *-----------------------------------------------------------------------------
 */

void write_indent(GString *str, gint indent)
{
	g_string_append_printf(str, "%*s", indent * 4, "");
}

void write_char_option(GString *str, gint indent, const gchar *label, const gchar *text)
{
	gchar *escval = g_markup_escape_text(text ? text : "", -1);
	write_indent(str, indent);
	g_string_append_printf(str, "%s = \"%s\"\n", label, escval);
	g_free(escval);
}

gboolean read_char_option(const gchar *option, const gchar *label, const gchar *value, gchar **text)
{
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!text) return FALSE;

	g_free(*text);
	*text = g_strdup(value);
	return TRUE;
}

/* Since gdk_color_to_string() is only available since gtk 2.12
 * here is an equivalent stub function. */
static gchar *color_to_string(GdkColor *color)
{
	return g_strdup_printf("#%04X%04X%04X", color->red, color->green, color->blue);
}

void write_color_option(GString *str, gint indent, gchar *label, GdkColor *color)
{
	if (color)
		{
		gchar *colorstring = color_to_string(color);

		write_char_option(str, indent, label, colorstring);
		g_free(colorstring);
		}
	else
		write_char_option(str, indent, label, "");
}

gboolean read_color_option(const gchar *option, const gchar *label, const gchar *value, GdkColor *color)
{
	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!color) return FALSE;

	if (!*value) return FALSE;
	gdk_color_parse(value, color);
	return TRUE;
}

void write_int_option(GString *str, gint indent, const gchar *label, gint n)
{
	write_indent(str, indent);
	g_string_append_printf(str, "%s = \"%d\"\n", label, n);
}

gboolean read_int_option(const gchar *option, const gchar *label, const gchar *value, gint *n)
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

void write_uint_option(GString *str, gint indent, const gchar *label, guint n)
{
	write_indent(str, indent);
	g_string_append_printf(str, "%s = \"%u\"\n", label, n);
}

gboolean read_uint_option(const gchar *option, const gchar *label, const gchar *value, guint *n)
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

gboolean read_uint_option_clamp(const gchar *option, const gchar *label, const gchar *value, guint *n, guint min, guint max)
{
	gboolean ret;

	ret = read_uint_option(option, label, value, n);
	if (ret) *n = CLAMP(*n, min, max);

	return ret;
}


gboolean read_int_option_clamp(const gchar *option, const gchar *label, const gchar *value, gint *n, gint min, gint max)
{
	gboolean ret;

	ret = read_int_option(option, label, value, n);
	if (ret) *n = CLAMP(*n, min, max);

	return ret;
}

void write_int_unit_option(GString *str, gint indent, gchar *label, gint n, gint subunits)
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

	write_indent(str, indent);
	g_string_append_printf(str, "%s = \"%d.%d\"\n", label, l, r);
}

gboolean read_int_unit_option(const gchar *option, const gchar *label, const gchar *value, gint *n, gint subunits)
{
	gint l, r;
	gchar *ptr, *buf;

	if (g_ascii_strcasecmp(option, label) != 0) return FALSE;
	if (!n) return FALSE;

	buf = g_strdup(value);
	ptr = buf;
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
	g_free(buf);
	
	return TRUE;
}

void write_bool_option(GString *str, gint indent, gchar *label, gint n)
{
	write_indent(str, indent);
	g_string_append_printf(str, "%s = \"%s\"\n", label, n ? "true" : "false");
}

gboolean read_bool_option(const gchar *option, const gchar *label, const gchar *value, gint *n)
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
 * write fuctions for elements (private)
 *-----------------------------------------------------------------------------
 */

static void write_global_attributes(GString *outstr, gint indent)
{
//	WRITE_SUBTITLE("General Options");

	WRITE_BOOL(*options, show_icon_names);
	WRITE_BOOL(*options, show_copy_path);
	WRITE_SEPARATOR();

	WRITE_BOOL(*options, tree_descend_subdirs);
	WRITE_BOOL(*options, lazy_image_sync);
	WRITE_BOOL(*options, update_on_time_change);
	WRITE_SEPARATOR();

	WRITE_BOOL(*options, progressive_key_scrolling);

	WRITE_UINT(*options, duplicates_similarity_threshold);
	WRITE_SEPARATOR();

	WRITE_BOOL(*options, mousewheel_scrolls);
	WRITE_INT(*options, open_recent_list_maxsize);
	WRITE_INT(*options, dnd_icon_size);
	WRITE_BOOL(*options, place_dialogs_under_mouse);


//	WRITE_SUBTITLE("Startup Options");

	WRITE_BOOL(*options, startup.restore_path);
	WRITE_BOOL(*options, startup.use_last_path);
	WRITE_CHAR(*options, startup.path);


//	WRITE_SUBTITLE("File operations Options");

	WRITE_BOOL(*options, file_ops.enable_in_place_rename);
	WRITE_BOOL(*options, file_ops.confirm_delete);
	WRITE_BOOL(*options, file_ops.enable_delete_key);
	WRITE_BOOL(*options, file_ops.safe_delete_enable);
	WRITE_CHAR(*options, file_ops.safe_delete_path);
	WRITE_INT(*options, file_ops.safe_delete_folder_maxsize);




//	WRITE_SUBTITLE("Properties dialog Options");
	WRITE_CHAR(*options, properties.tabs_order);

//	WRITE_SUBTITLE("Image Options");

	WRITE_UINT(*options, image.zoom_mode);

//	g_string_append_printf(outstr, "# image.zoom_mode possible values are:\n"
//			    "#   original\n"
//			    "#   fit\n"
//			    "#   dont_change\n");
//	g_string_append_printf(outstr, "image.zoom_mode: ");
//	switch (options->image.zoom_mode)
//	{
//	case ZOOM_RESET_ORIGINAL: g_string_append_printf(outstr, "original\n"); break;
//	case ZOOM_RESET_FIT_WINDOW: g_string_append_printf(outstr, "fit\n"); break;
//	case ZOOM_RESET_NONE: g_string_append_printf(outstr, "dont_change\n"); break;
//	}
	WRITE_SEPARATOR();
	WRITE_BOOL(*options, image.zoom_2pass);
	WRITE_BOOL(*options, image.zoom_to_fit_allow_expand);
	WRITE_UINT(*options, image.zoom_quality);
	WRITE_INT(*options, image.zoom_increment);
	WRITE_BOOL(*options, image.fit_window_to_image);
	WRITE_BOOL(*options, image.limit_window_size);
	WRITE_INT(*options, image.max_window_size);
	WRITE_BOOL(*options, image.limit_autofit_size);
	WRITE_INT(*options, image.max_autofit_size);
	WRITE_UINT(*options, image.scroll_reset_method);
	WRITE_INT(*options, image.tile_cache_max);
	WRITE_INT(*options, image.image_cache_max);
	WRITE_UINT(*options, image.dither_quality);
	WRITE_BOOL(*options, image.enable_read_ahead);
	WRITE_BOOL(*options, image.exif_rotate_enable);
	WRITE_BOOL(*options, image.use_custom_border_color);
	WRITE_COLOR(*options, image.border_color);
	WRITE_INT(*options, image.read_buffer_size);
	WRITE_INT(*options, image.idle_read_loop_count);

//	WRITE_SUBTITLE("Thumbnails Options");

	WRITE_INT(*options, thumbnails.max_width);
	WRITE_INT(*options, thumbnails.max_height);
	WRITE_BOOL(*options, thumbnails.enable_caching);
	WRITE_BOOL(*options, thumbnails.cache_into_dirs);
	WRITE_BOOL(*options, thumbnails.fast);
	WRITE_BOOL(*options, thumbnails.use_xvpics);
	WRITE_BOOL(*options, thumbnails.spec_standard);
	WRITE_UINT(*options, thumbnails.quality);
	WRITE_BOOL(*options, thumbnails.use_exif);


//	WRITE_SUBTITLE("File sorting Options");

	WRITE_INT(*options, file_sort.method);
	WRITE_BOOL(*options, file_sort.ascending);
	WRITE_BOOL(*options, file_sort.case_sensitive);


//	WRITE_SUBTITLE("Fullscreen Options");

	WRITE_INT(*options, fullscreen.screen);
	WRITE_BOOL(*options, fullscreen.clean_flip);
	WRITE_BOOL(*options, fullscreen.disable_saver);
	WRITE_BOOL(*options, fullscreen.above);


//	WRITE_SUBTITLE("Histogram Options");
	WRITE_UINT(*options, histogram.last_channel_mode);
	WRITE_UINT(*options, histogram.last_log_mode);


//	WRITE_SUBTITLE("Image Overlay Options");
	WRITE_UINT(*options, image_overlay.common.state);
	WRITE_BOOL(*options, image_overlay.common.show_at_startup);
	WRITE_CHAR(*options, image_overlay.common.template_string);
	WRITE_SEPARATOR();

//	g_string_append_printf(outstr, "# these are relative positions:\n");
//	g_string_append_printf(outstr, "# x >= 0: |x| pixels from left border\n");
//	g_string_append_printf(outstr, "# x < 0 : |x| pixels from right border\n");
//	g_string_append_printf(outstr, "# y >= 0: |y| pixels from top border\n");
//	g_string_append_printf(outstr, "# y < 0 : |y| pixels from bottom border\n");
	WRITE_INT(*options, image_overlay.common.x);
	WRITE_INT(*options, image_overlay.common.y);


//	WRITE_SUBTITLE("Slideshow Options");

	WRITE_INT_UNIT(*options, slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
	WRITE_BOOL(*options, slideshow.random);
	WRITE_BOOL(*options, slideshow.repeat);


//	WRITE_SUBTITLE("Collection Options");

	WRITE_BOOL(*options, collections.rectangular_selection);


//	WRITE_SUBTITLE("Filtering Options");

	WRITE_BOOL(*options, file_filter.show_hidden_files);
	WRITE_BOOL(*options, file_filter.show_dot_directory);
	WRITE_BOOL(*options, file_filter.disable);
	WRITE_SEPARATOR();


//	WRITE_SUBTITLE("Sidecars Options");

	WRITE_CHAR(*options, sidecar.ext);



//	WRITE_SUBTITLE("Shell command");
	WRITE_CHAR(*options, shell.path);
	WRITE_CHAR(*options, shell.options);


//	WRITE_SUBTITLE("Helpers");
//	g_string_append_printf(outstr, "# Html browser\n");
//	g_string_append_printf(outstr, "# command_name is: the binary's name to look for in the path\n");
//	g_string_append_printf(outstr, "# If command_name is empty, the program will try various common html browsers\n");
//	g_string_append_printf(outstr, "# command_line is:\n");
//	g_string_append_printf(outstr, "# \"\" (empty string)  = execute binary with html file path as command line\n");
//	g_string_append_printf(outstr, "# \"string\"           = execute string and use results for command line\n");
//	g_string_append_printf(outstr, "# \"!string\"          = use text following ! as command line, replacing optional %%s with html file path\n");
	WRITE_CHAR(*options, helpers.html_browser.command_name);
	WRITE_CHAR(*options, helpers.html_browser.command_line);

/* FIXME:
	WRITE_SUBTITLE("Exif Options");
	g_string_append_printf(outstr, "# Display: 0: never\n"
			    "#          1: if set\n"
			    "#          2: always\n\n");
	for (i = 0; ExifUIList[i].key; i++)
		{
		g_string_append_printf(outstr, "exif.display.");
		write_int_option(outstr, 2, (gchar *)ExifUIList[i].key, ExifUIList[i].current);
		}
*/

//	WRITE_SUBTITLE("Metadata Options");
	WRITE_BOOL(*options, metadata.enable_metadata_dirs);
	WRITE_BOOL(*options, metadata.save_in_image_file); 
	WRITE_BOOL(*options, metadata.save_legacy_IPTC);
	WRITE_BOOL(*options, metadata.warn_on_write_problems);
	WRITE_BOOL(*options, metadata.save_legacy_format);
	WRITE_BOOL(*options, metadata.sync_grouped_files);
	WRITE_BOOL(*options, metadata.confirm_write);
	WRITE_INT(*options, metadata.confirm_timeout);
	WRITE_BOOL(*options, metadata.confirm_after_timeout);
	WRITE_BOOL(*options, metadata.confirm_on_image_change);
	WRITE_BOOL(*options, metadata.confirm_on_dir_change);

}

static void write_color_profile(GString *outstr, gint indent)
{
	gint i;
#ifndef HAVE_LCMS
	g_string_append_printf(outstr, "<!-- NOTICE: %s was not built with support for color profiles,\n"
			    "         color profile options will have no effect.\n-->\n", GQ_APPNAME);
#endif

	write_indent(outstr, indent);
	g_string_append_printf(outstr, "<color_profiles\n");
	indent++;
	WRITE_INT(options->color_profile, screen_type);
	WRITE_CHAR(options->color_profile, screen_file);
	WRITE_BOOL(options->color_profile, enabled);
	WRITE_BOOL(options->color_profile, use_image);
	WRITE_INT(options->color_profile, input_type);
	indent--;
	write_indent(outstr, indent);
	g_string_append_printf(outstr, ">\n");

	indent++;
	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		write_indent(outstr, indent);
		g_string_append_printf(outstr, "<profile\n");
		indent++;
		write_char_option(outstr, indent, "input_file", options->color_profile.input_file[i]);
		write_char_option(outstr, indent, "input_name", options->color_profile.input_name[i]);
		indent--;
		write_indent(outstr, indent);
		g_string_append_printf(outstr, "/>\n");
		}
	indent--;
	write_indent(outstr, indent);
	g_string_append_printf(outstr, "</color_profiles>\n");
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
	GString *outstr;
	gint indent = 0;
	GList *work;
	
	rc_pathl = path_from_utf8(utf8_path);
	ssi = secure_open(rc_pathl);
	g_free(rc_pathl);
	if (!ssi)
		{
		log_printf(_("error saving config file: %s\n"), utf8_path);
		return FALSE;
		}

	outstr = g_string_new("");
	g_string_append_printf(outstr, "<!--\n");
	g_string_append_printf(outstr, "######################################################################\n");
	g_string_append_printf(outstr, "# %30s config file      version %-10s #\n", GQ_APPNAME, VERSION);
	g_string_append_printf(outstr, "######################################################################\n");
	WRITE_SEPARATOR();

	g_string_append_printf(outstr, "# Note: This file is autogenerated. Options can be changed here,\n");
	g_string_append_printf(outstr, "#       but user comments and formatting will be lost.\n");
	WRITE_SEPARATOR();
	g_string_append_printf(outstr, "-->\n");
	WRITE_SEPARATOR();
	g_string_append_printf(outstr, "<global\n");
	indent++;
	write_global_attributes(outstr, indent);
	write_indent(outstr, indent);
	g_string_append_printf(outstr, ">\n");

	write_color_profile(outstr, indent);

	WRITE_SEPARATOR();
	filter_write_list(outstr, indent);

	WRITE_SEPARATOR();
	WRITE_SUBTITLE("Layout Options - defaults");
	write_indent(outstr, indent);
	g_string_append_printf(outstr, "<layout\n");
	layout_write_attributes(&options->layout, outstr, indent + 1);
	write_indent(outstr, indent);
	g_string_append_printf(outstr, "/>\n");

	indent--;
	g_string_append_printf(outstr, "</global>\n");

	WRITE_SEPARATOR();
	WRITE_SUBTITLE("Layout Options");

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		layout_write_config(lw, outstr, indent);
		work = work->next;

		}



	secure_fputs(ssi, outstr->str);
	g_string_free(outstr, TRUE);

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
 * loading attributes for elements (private)
 *-----------------------------------------------------------------------------
 */


static gboolean load_global_params(const gchar **attribute_names, const gchar **attribute_values)
{
	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;


		/* general options */
		READ_BOOL(*options, show_icon_names);
		READ_BOOL(*options, show_copy_path);

		READ_BOOL(*options, tree_descend_subdirs);
		READ_BOOL(*options, lazy_image_sync);
		READ_BOOL(*options, update_on_time_change);

		READ_UINT_CLAMP(*options, duplicates_similarity_threshold, 0, 100);

		READ_BOOL(*options, progressive_key_scrolling);

		READ_BOOL(*options, mousewheel_scrolls);

		READ_INT(*options, open_recent_list_maxsize);
		READ_INT(*options, dnd_icon_size);
		READ_BOOL(*options, place_dialogs_under_mouse);

		/* startup options */
		
		READ_BOOL(*options, startup.restore_path);

		READ_BOOL(*options, startup.use_last_path);

		READ_CHAR(*options, startup.path);
	

		/* properties dialog options */
		READ_CHAR(*options, properties.tabs_order);

		/* image options */
		READ_UINT_CLAMP(*options, image.zoom_mode, 0, ZOOM_RESET_NONE);
		READ_BOOL(*options, image.zoom_2pass);
		READ_BOOL(*options, image.zoom_to_fit_allow_expand);
		READ_BOOL(*options, image.fit_window_to_image);
		READ_BOOL(*options, image.limit_window_size);
		READ_INT(*options, image.max_window_size);
		READ_BOOL(*options, image.limit_autofit_size);
		READ_INT(*options, image.max_autofit_size);
		READ_UINT_CLAMP(*options, image.scroll_reset_method, 0, PR_SCROLL_RESET_COUNT - 1);
		READ_INT(*options, image.tile_cache_max);
		READ_INT(*options, image.image_cache_max);
		READ_UINT_CLAMP(*options, image.zoom_quality, GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		READ_UINT_CLAMP(*options, image.dither_quality, GDK_RGB_DITHER_NONE, GDK_RGB_DITHER_MAX);
		READ_INT(*options, image.zoom_increment);
		READ_BOOL(*options, image.enable_read_ahead);
		READ_BOOL(*options, image.exif_rotate_enable);
		READ_BOOL(*options, image.use_custom_border_color);
		READ_COLOR(*options, image.border_color);
		READ_INT_CLAMP(*options, image.read_buffer_size, IMAGE_LOADER_READ_BUFFER_SIZE_MIN, IMAGE_LOADER_READ_BUFFER_SIZE_MAX);
		READ_INT_CLAMP(*options, image.idle_read_loop_count, IMAGE_LOADER_IDLE_READ_LOOP_COUNT_MIN, IMAGE_LOADER_IDLE_READ_LOOP_COUNT_MAX);


		/* thumbnails options */
		READ_INT_CLAMP(*options, thumbnails.max_width, 16, 512);
		READ_INT_CLAMP(*options, thumbnails.max_height, 16, 512);

		READ_BOOL(*options, thumbnails.enable_caching);
		READ_BOOL(*options, thumbnails.cache_into_dirs);
		READ_BOOL(*options, thumbnails.fast);
		READ_BOOL(*options, thumbnails.use_xvpics);
		READ_BOOL(*options, thumbnails.spec_standard);
		READ_UINT_CLAMP(*options, thumbnails.quality, GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		READ_BOOL(*options, thumbnails.use_exif);

		/* file sorting options */
		READ_UINT(*options, file_sort.method);
		READ_BOOL(*options, file_sort.ascending);
		READ_BOOL(*options, file_sort.case_sensitive);

		/* file operations *options */
		READ_BOOL(*options, file_ops.enable_in_place_rename);
		READ_BOOL(*options, file_ops.confirm_delete);
		READ_BOOL(*options, file_ops.enable_delete_key);
		READ_BOOL(*options, file_ops.safe_delete_enable);
		READ_CHAR(*options, file_ops.safe_delete_path);
		READ_INT(*options, file_ops.safe_delete_folder_maxsize);

		/* fullscreen options */
		READ_INT(*options, fullscreen.screen);
		READ_BOOL(*options, fullscreen.clean_flip);
		READ_BOOL(*options, fullscreen.disable_saver);
		READ_BOOL(*options, fullscreen.above);

		/* histogram */
		READ_UINT(*options, histogram.last_channel_mode);
		READ_UINT(*options, histogram.last_log_mode);

		/* image overlay */
		READ_UINT(*options, image_overlay.common.state);
		READ_BOOL(*options, image_overlay.common.show_at_startup);
		READ_CHAR(*options, image_overlay.common.template_string);

		READ_INT(*options, image_overlay.common.x);
		READ_INT(*options, image_overlay.common.y);


		/* slideshow options */
		READ_INT_UNIT(*options, slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
		READ_BOOL(*options, slideshow.random);
		READ_BOOL(*options, slideshow.repeat);

		/* collection options */

		READ_BOOL(*options, collections.rectangular_selection);

		/* filtering options */

		READ_BOOL(*options, file_filter.show_hidden_files);
		READ_BOOL(*options, file_filter.show_dot_directory);
		READ_BOOL(*options, file_filter.disable);
		READ_CHAR(*options, sidecar.ext);

		/* Color Profiles */

		/* Shell command */
		READ_CHAR(*options, shell.path);
		READ_CHAR(*options, shell.options);

		/* Helpers */
		READ_CHAR(*options, helpers.html_browser.command_name);
		READ_CHAR(*options, helpers.html_browser.command_line);
		/* Exif */
/*
		if (0 == g_ascii_strncasecmp(option, "exif.display.", 13))
			{
			for (i = 0; ExifUIList[i].key; i++)
				if (0 == g_ascii_strcasecmp(option + 13, ExifUIList[i].key))
					ExifUIList[i].current = strtol(value, NULL, 10);
			continue;
			}
*/
		/* metadata */		
		READ_BOOL(*options, metadata.enable_metadata_dirs);
		READ_BOOL(*options, metadata.save_in_image_file);
		READ_BOOL(*options, metadata.save_legacy_IPTC);
		READ_BOOL(*options, metadata.warn_on_write_problems);
		READ_BOOL(*options, metadata.save_legacy_format);
		READ_BOOL(*options, metadata.sync_grouped_files);
		READ_BOOL(*options, metadata.confirm_write);
		READ_BOOL(*options, metadata.confirm_after_timeout);
		READ_INT(*options, metadata.confirm_timeout);
		READ_BOOL(*options, metadata.confirm_on_image_change);
		READ_BOOL(*options, metadata.confirm_on_dir_change);

		DEBUG_1("unknown attribute %s = %s", option, value);
		}

	return TRUE;
}

static void options_load_color_profiles(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;


		READ_BOOL(options->color_profile, enabled);
		READ_BOOL(options->color_profile, use_image);
		READ_INT(options->color_profile, input_type);
		READ_INT(options->color_profile, screen_type);
		READ_CHAR(options->color_profile, screen_file);

		DEBUG_1("unknown attribute %s = %s", option, value);
		}

}

static void options_load_profile(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	gint i = GPOINTER_TO_INT(data);
	if (i < 0 || i >= COLOR_PROFILE_INPUTS) return;
	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		READ_CHAR_FULL("input_file", options->color_profile.input_file[i]);
		READ_CHAR_FULL("input_name", options->color_profile.input_name[i]);
		

		DEBUG_1("unknown attribute %s = %s", option, value);
		}
	i++;
	options_parse_func_set_data(parser_data, GINT_TO_POINTER(i));

}



/*
 *-----------------------------------------------------------------------------
 * xml file structure (private)
 *-----------------------------------------------------------------------------
 */
struct _GQParserData
{
	GList *parse_func_stack;
	gboolean startup; /* reading config for the first time - add commandline and call init_after_global_options() */
	gboolean global_found;
};


void options_parse_leaf(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	DEBUG_1("unexpected: %s", element_name);
	options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
}

static void options_parse_color_profiles(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	if (g_ascii_strcasecmp(element_name, "profile") == 0)
		{
		options_load_profile(parser_data, context, element_name, attribute_names, attribute_values, data, error);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else
		{
		DEBUG_1("unexpected profile: %s", element_name);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
}

static void options_parse_filter(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	if (g_ascii_strcasecmp(element_name, "file_type") == 0)
		{
		filter_load_file_type(attribute_names, attribute_values);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else
		{
		DEBUG_1("unexpected filter: %s", element_name);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
}

static void options_parse_filter_end(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, gpointer data, GError **error)
{
	DEBUG_1(" filter end");
	filter_add_defaults();
	filter_rebuild();
}

static void options_parse_global(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	if (g_ascii_strcasecmp(element_name, "color_profiles") == 0)
		{
		options_load_color_profiles(parser_data, context, element_name, attribute_names, attribute_values, data, error);
		options_parse_func_push(parser_data, options_parse_color_profiles, NULL, GINT_TO_POINTER(0));
		}
	else if (g_ascii_strcasecmp(element_name, "filter") == 0)
		{
		options_parse_func_push(parser_data, options_parse_filter, options_parse_filter_end, NULL);
		}
	else if (g_ascii_strcasecmp(element_name, "layout") == 0)
		{
		layout_load_attributes(&options->layout, attribute_names, attribute_values);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else
		{
		DEBUG_1("unexpected global: %s", element_name);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
}

static void options_parse_bar(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	GtkWidget *bar = data;
	if (g_ascii_strcasecmp(element_name, "pane_comment") == 0)
		{
		GtkWidget *pane = bar_pane_comment_new_from_config(attribute_names, attribute_values);
		bar_add(bar, pane);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else if (g_ascii_strcasecmp(element_name, "pane_exif") == 0)
		{
		GtkWidget *pane = bar_pane_exif_new_from_config(attribute_names, attribute_values);
		bar_add(bar, pane);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else if (g_ascii_strcasecmp(element_name, "pane_histogram") == 0)
		{
		GtkWidget *pane = bar_pane_histogram_new_from_config(attribute_names, attribute_values);
		bar_add(bar, pane);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else if (g_ascii_strcasecmp(element_name, "pane_keywords") == 0)
		{
		GtkWidget *pane = bar_pane_keywords_new_from_config(attribute_names, attribute_values);
		bar_add(bar, pane);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
	else
		{
		DEBUG_1("unexpected in <bar>: <%s>", element_name);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
}

static void options_parse_layout(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	LayoutWindow *lw = data;
	if (g_ascii_strcasecmp(element_name, "bar") == 0)
		{
		if (lw->bar) 
			layout_bar_close(lw);
		layout_bar_new(lw, FALSE);
		options_parse_func_push(parser_data, options_parse_bar, NULL, lw->bar);
		}
	else
		{
		DEBUG_1("unexpected in <layout>: <%s>", element_name);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
}

static void options_parse_toplevel(GQParserData *parser_data, GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer data, GError **error)
{
	if (g_ascii_strcasecmp(element_name, "global") == 0)
		{
		load_global_params(attribute_names, attribute_values);
		options_parse_func_push(parser_data, options_parse_global, NULL, NULL);
		return;
		}
	
	if (parser_data->startup && !parser_data->global_found)
		{
		DEBUG_1(" global end");
		parser_data->global_found = TRUE;
		init_after_global_options();
		}
	
	if (g_ascii_strcasecmp(element_name, "layout") == 0)
		{
		LayoutWindow *lw;
		lw = layout_new_from_config(attribute_names, attribute_values, parser_data->startup);
		options_parse_func_push(parser_data, options_parse_layout, NULL, lw);
		}
	else
		{
		DEBUG_1("unexpected in <toplevel>: <%s>", element_name);
		options_parse_func_push(parser_data, options_parse_leaf, NULL, NULL);
		}
}





/*
 *-----------------------------------------------------------------------------
 * parser
 *-----------------------------------------------------------------------------
 */


struct _GQParserFuncData
{
	GQParserStartFunc start_func;
	GQParserEndFunc end_func;
//	GQParserTextFunc text_func;
	gpointer data;
};

void options_parse_func_push(GQParserData *parser_data, GQParserStartFunc start_func, GQParserEndFunc end_func, gpointer data)
{
	GQParserFuncData *func_data = g_new0(GQParserFuncData, 1);
	func_data->start_func = start_func;
	func_data->end_func = end_func;
	func_data->data = data;
	
	parser_data->parse_func_stack = g_list_prepend(parser_data->parse_func_stack, func_data);
}

void options_parse_func_pop(GQParserData *parser_data)
{
	g_free(parser_data->parse_func_stack->data);
	parser_data->parse_func_stack = g_list_delete_link(parser_data->parse_func_stack, parser_data->parse_func_stack);
}

void options_parse_func_set_data(GQParserData *parser_data, gpointer data)
{
	GQParserFuncData *func = parser_data->parse_func_stack->data;
	func->data = data;
}


static void start_element(GMarkupParseContext *context,
			  const gchar *element_name,
			  const gchar **attribute_names,
			  const gchar **attribute_values,
			  gpointer user_data,
			  GError **error) 
{
	GQParserData *parser_data = user_data;
	GQParserFuncData *func = parser_data->parse_func_stack->data; 
	DEBUG_1("start %s", element_name);
	
	if (func->start_func)
		func->start_func(parser_data, context, element_name, attribute_names, attribute_values, func->data, error);
}

static void end_element(GMarkupParseContext *context,
			  const gchar *element_name,
			  gpointer user_data,
			  GError **error) 
{
	GQParserData *parser_data = user_data;
	GQParserFuncData *func = parser_data->parse_func_stack->data; 
	DEBUG_1("end %s", element_name);

	if (func->end_func)
		func->end_func(parser_data, context, element_name, func->data, error);

	options_parse_func_pop(parser_data);
}

static GMarkupParser parser = {
	start_element,
	end_element,
	NULL,
	NULL,
	NULL
};

/*
 *-----------------------------------------------------------------------------
 * load configuration (public)
 *-----------------------------------------------------------------------------
 */

gboolean load_options_from(const gchar *utf8_path, ConfOptions *options, gboolean startup)
{
	gsize size;
	gchar *buf;
	GMarkupParseContext *context;
	gboolean ret = TRUE;
	GQParserData *parser_data;

	if (g_file_get_contents (utf8_path, &buf, &size, NULL) == FALSE) 
		{
		return FALSE;
		}
	
	parser_data = g_new0(GQParserData, 1);
	
	parser_data->startup = startup;
	options_parse_func_push(parser_data, options_parse_toplevel, NULL, NULL);
	
	context = g_markup_parse_context_new(&parser, 0, parser_data, NULL);

	if (g_markup_parse_context_parse (context, buf, size, NULL) == FALSE)
		{
		ret = FALSE;
		DEBUG_1("Parse failed");
		}
		
	g_free(parser_data);

	g_free(buf);
	g_markup_parse_context_free (context);
	return ret;
}
	


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
