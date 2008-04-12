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

	write_bool_option(ssi, "show_icon_names", options->show_icon_names);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "tree_descend_folders", options->tree_descend_subdirs);
	write_bool_option(ssi, "lazy_image_sync", options->lazy_image_sync);
	write_bool_option(ssi, "update_on_time_change", options->update_on_time_change);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "enable_startup_path", options->startup_path_enable);
	write_char_option(ssi, "startup_path", options->startup_path);

	write_bool_option(ssi, "progressive_keyboard_scrolling", options->progressive_key_scrolling);
	write_bool_option(ssi, "local_metadata", options->enable_metadata_dirs);

	write_bool_option(ssi, "confirm_delete", options->confirm_delete);
	write_bool_option(ssi, "enable_delete_key", options->enable_delete_key);
	write_bool_option(ssi, "safe_delete", options->safe_delete_enable);
	write_char_option(ssi, "safe_delete_path", options->safe_delete_path);
	write_int_option(ssi, "safe_delete_size", options->safe_delete_size);
	secure_fputc(ssi, '\n');
	
	write_int_option(ssi, "custom_similarity_threshold", options->dupe_custom_threshold);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "mouse_wheel_scrolls", options->mousewheel_scrolls);
	write_bool_option(ssi, "in_place_rename", options->enable_in_place_rename);
	write_int_option(ssi, "open_recent_max", options->recent_list_max);
	write_bool_option(ssi, "display_dialogs_under_mouse", options->place_dialogs_under_mouse);
	

	secure_fprintf(ssi, "\n##### Layout Options #####\n\n");

	write_int_option(ssi, "layout.style", options->layout.style);
	write_char_option(ssi, "layout.order", options->layout.order);
	write_bool_option(ssi, "layout.view_as_icons", options->layout.view_as_icons);
	write_bool_option(ssi, "layout.view_as_tree", options->layout.view_as_tree);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "layout.save_window_positions", options->layout.save_window_positions);
	secure_fputc(ssi, '\n');

	write_int_option(ssi, "layout.main_window.x", options->layout.main_window.x);
	write_int_option(ssi, "layout.main_window.y", options->layout.main_window.y);
	write_int_option(ssi, "layout.main_window.w", options->layout.main_window.w);
	write_int_option(ssi, "layout.main_window.h", options->layout.main_window.h);
	write_bool_option(ssi, "layout.main_window.maximized", options->layout.main_window.maximized);
	write_int_option(ssi, "layout.main_window.hdivider_pos", options->layout.main_window.hdivider_pos);
	write_int_option(ssi, "layout.main_window.vdivider_pos", options->layout.main_window.vdivider_pos);
	secure_fputc(ssi, '\n');

	write_int_option(ssi, "layout.float_window.x", options->layout.float_window.x);
	write_int_option(ssi, "layout.float_window.y", options->layout.float_window.y);
	write_int_option(ssi, "layout.float_window.w", options->layout.float_window.w);
	write_int_option(ssi, "layout.float_window.h", options->layout.float_window.h);
	write_int_option(ssi, "layout.float_window.vdivider_pos", options->layout.float_window.vdivider_pos);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "layout.tools_float", options->layout.tools_float);
	write_bool_option(ssi, "layout.tools_hidden", options->layout.tools_hidden);
	write_bool_option(ssi, "layout.tools_restore_state", options->layout.tools_restore_state);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "layout.toolbar_hidden", options->layout.toolbar_hidden);


	secure_fprintf(ssi, "\n##### Image Options #####\n\n");

	secure_fprintf(ssi, "image.zoom_mode: ");
	if (options->image.zoom_mode == ZOOM_RESET_ORIGINAL) secure_fprintf(ssi, "original\n");
	if (options->image.zoom_mode == ZOOM_RESET_FIT_WINDOW) secure_fprintf(ssi, "fit\n");
	if (options->image.zoom_mode == ZOOM_RESET_NONE) secure_fprintf(ssi, "dont_change\n");
	write_bool_option(ssi, "image.zoom_2pass", options->image.zoom_2pass);
	write_bool_option(ssi, "image.zoom_to_fit_allow_expand", options->image.zoom_to_fit_allow_expand);
	write_int_option(ssi, "image.zoom_quality", options->image.zoom_quality);
	write_int_option(ssi, "image.zoom_increment", options->image.zoom_increment);
	write_bool_option(ssi, "image.fit_window_to_image", options->image.fit_window_to_image);
	write_bool_option(ssi, "image.limit_window_size", options->image.limit_window_size);
	write_int_option(ssi, "image.max_window_size", options->image.max_window_size);
	write_bool_option(ssi, "image.limit_autofit_size", options->image.limit_autofit_size);
	write_int_option(ssi, "image.max_autofit_size", options->image.max_autofit_size);
	write_int_option(ssi, "image.scroll_reset_method", options->image.scroll_reset_method);
	write_int_option(ssi, "image.tile_cache_max", options->image.tile_cache_max);
	write_int_option(ssi, "image.dither_quality", options->image.dither_quality);
	write_bool_option(ssi, "image.enable_read_ahead", options->image.enable_read_ahead);
	write_bool_option(ssi, "image.exif_rotate_enable", options->image.exif_rotate_enable);
	write_bool_option(ssi, "image.use_custom_border_color", options->image.use_custom_border_color);
	write_color_option(ssi, "image.border_color", &options->image.border_color);


	secure_fprintf(ssi, "\n##### Thumbnails Options #####\n\n");

	write_bool_option(ssi, "thumbnails.enabled", options->thumbnails.enabled);
	write_int_option(ssi, "thumbnails.max_width", options->thumbnails.max_width);
	write_int_option(ssi, "thumbnails.max_height", options->thumbnails.max_height);
	write_bool_option(ssi, "thumbnails.enable_caching", options->thumbnails.enable_caching);
	write_bool_option(ssi, "thumbnails.cache_into_dirs", options->thumbnails.cache_into_dirs);
	write_bool_option(ssi, "thumbnails.fast", options->thumbnails.fast);
	write_bool_option(ssi, "thumbnails.use_xvpics", options->thumbnails.use_xvpics);
	write_bool_option(ssi, "thumbnails.spec_standard", options->thumbnails.spec_standard);
	write_int_option(ssi, "thumbnails.quality", options->thumbnails.quality);


	secure_fprintf(ssi, "\n##### File sorting Options #####\n\n");

	write_int_option(ssi, "file_sort.method", (gint)options->file_sort.method);
	write_bool_option(ssi, "file_sort.ascending", options->file_sort.ascending);
	write_bool_option(ssi, "file_sort.case_sensitive", options->file_sort.case_sensitive);

	
	secure_fprintf(ssi, "\n##### Fullscreen Options #####\n\n");

	write_int_option(ssi, "fullscreen.screen", options->fullscreen.screen);
	write_bool_option(ssi, "fullscreen.clean_flip", options->fullscreen.clean_flip);
	write_bool_option(ssi, "fullscreen.disable_saver", options->fullscreen.disable_saver);
	write_bool_option(ssi, "fullscreen.above", options->fullscreen.above);
	write_bool_option(ssi, "fullscreen.show_info", options->fullscreen.show_info);
	write_char_option(ssi, "fullscreen.info", options->fullscreen.info);

	secure_fprintf(ssi, "\n##### Slideshow Options #####\n\n");

	write_int_unit_option(ssi, "slideshow.delay", options->slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
	write_bool_option(ssi, "slideshow.random", options->slideshow.random);
	write_bool_option(ssi, "slideshow.repeat", options->slideshow.repeat);


	secure_fprintf(ssi, "\n##### Collection Options #####\n\n");

	write_bool_option(ssi, "collections.rectangular_selection", options->collections.rectangular_selection);


	secure_fprintf(ssi, "\n##### Filtering Options #####\n\n");

	write_bool_option(ssi, "file_filter.show_dot_files", options->file_filter.show_dot_files);
	write_bool_option(ssi, "file_filter.disable", options->file_filter.disable);
	secure_fputc(ssi, '\n');

	filter_write_list(ssi);
	

	secure_fprintf(ssi, "\n##### Sidecars Options #####\n\n");

	sidecar_ext_write(ssi);


	secure_fprintf(ssi, "\n##### Color Profiles #####\n\n");

#ifndef HAVE_LCMS
	secure_fprintf(ssi, "# NOTICE: %s was not built with support for color profiles,\n"
		  	   "#         color profile options will have no effect.\n\n", GQ_APPNAME);
#endif

	write_bool_option(ssi, "color_profile.enabled", options->color_profile.enabled);
	write_bool_option(ssi, "color_profile.use_image", options->color_profile.use_image);
	write_int_option(ssi, "color_profile.input_type", options->color_profile.input_type);
	secure_fputc(ssi, '\n');

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
	secure_fputc(ssi, '\n');
	write_int_option(ssi, "color_profile.screen_type", options->color_profile.screen_type);
	write_char_option(ssi, "color_profile.screen_file", options->color_profile.screen_file);

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

		/* layout options */

		options->layout.style = read_int_option(f, option,
			"layout.style", value, options->layout.style);
		options->layout.order = read_char_option(f, option,
			"layout.order", value, options->layout.order);
		options->layout.view_as_icons = read_bool_option(f, option,
			"layout.view_as_icons", value, options->layout.view_as_icons);
		options->layout.view_as_tree = read_bool_option(f, option,
			"layout.view_as_tree", value, options->layout.view_as_tree);
		/* window positions */

		options->layout.save_window_positions = read_bool_option(f, option,
			"layout.save_window_positions", value, options->layout.save_window_positions);

		options->layout.main_window.x = read_int_option(f, option,
			"layout.main_window.x", value, options->layout.main_window.x);
		options->layout.main_window.y = read_int_option(f, option,
			"layout.main_window.y", value, options->layout.main_window.y);
		options->layout.main_window.w = read_int_option(f, option,
			"layout.main_window.w", value, options->layout.main_window.w);
		options->layout.main_window.h = read_int_option(f, option,
			"layout.main_window.h", value, options->layout.main_window.h);
		options->layout.main_window.maximized = read_bool_option(f, option,
			"layout.main_window.maximized", value, options->layout.main_window.maximized);
		options->layout.float_window.x = read_int_option(f, option,
			"layout.float_window.x", value, options->layout.float_window.x);
		options->layout.float_window.y = read_int_option(f, option,
			"layout.float_window.y", value, options->layout.float_window.y);
		options->layout.float_window.w = read_int_option(f, option,
			"layout.float_window.w", value, options->layout.float_window.w);
		options->layout.float_window.h = read_int_option(f, option,
			"layout.float_window.h", value, options->layout.float_window.h);
		options->layout.float_window.vdivider_pos = read_int_option(f, option,
			"layout.float_window.vdivider_pos", value, options->layout.float_window.vdivider_pos);
		options->layout.main_window.hdivider_pos = read_int_option(f, option,
			"layout.main_window.hdivider_pos", value,options->layout.main_window.hdivider_pos);
		options->layout.main_window.vdivider_pos = read_int_option(f, option,
			"layout.main_window.vdivider_pos", value, options->layout.main_window.vdivider_pos);
		options->layout.tools_float = read_bool_option(f, option,
			"layout.tools_float", value, options->layout.tools_float);
		options->layout.tools_hidden = read_bool_option(f, option,
			"layout.tools_hidden", value, options->layout.tools_hidden);
		options->layout.tools_restore_state = read_bool_option(f, option,
			"layout.tools_restore_state", value, options->layout.tools_restore_state);
		options->layout.toolbar_hidden = read_bool_option(f, option,
			"layout.toolbar_hidden", value, options->layout.toolbar_hidden);


		/* general options */
		options->show_icon_names = read_bool_option(f, option,
			"show_icon_names", value, options->show_icon_names);

		options->tree_descend_subdirs = read_bool_option(f, option,
			"tree_descend_folders", value, options->tree_descend_subdirs);
		options->lazy_image_sync = read_bool_option(f, option,
			"lazy_image_sync", value, options->lazy_image_sync);
		options->update_on_time_change = read_bool_option(f, option,
			"update_on_time_change", value, options->update_on_time_change);
	
		options->startup_path_enable = read_bool_option(f, option,
			"enable_startup_path", value, options->startup_path_enable);
		options->startup_path = read_char_option(f, option,
			"startup_path", value_all, options->startup_path);

		/* image options */
		if (strcasecmp(option, "image.zoom_mode") == 0)
                        {
                        if (strcasecmp(value, "original") == 0) options->image.zoom_mode = ZOOM_RESET_ORIGINAL;
                        if (strcasecmp(value, "fit") == 0) options->image.zoom_mode = ZOOM_RESET_FIT_WINDOW;
                        if (strcasecmp(value, "dont_change") == 0) options->image.zoom_mode = ZOOM_RESET_NONE;
                        }
		options->image.zoom_2pass = read_bool_option(f, option,
			"image.zoom_2pass", value, options->image.zoom_2pass);
		options->image.zoom_to_fit_allow_expand = read_bool_option(f, option,
			"image.zoom_to_fit_allow_expand", value, options->image.zoom_to_fit_allow_expand);
		options->image.fit_window_to_image = read_bool_option(f, option,
			"image.fit_window_to_image", value, options->image.fit_window_to_image);
		options->image.limit_window_size = read_bool_option(f, option,
			"image.limit_window_size", value, options->image.limit_window_size);
		options->image.max_window_size = read_int_option(f, option,
			"image.max_window_size", value, options->image.max_window_size);
		options->image.limit_autofit_size = read_bool_option(f, option,
			"image.limit_autofit_size", value, options->image.limit_autofit_size);
		options->image.max_autofit_size = read_int_option(f, option,
			"image.max_autofit_size", value, options->image.max_autofit_size);
		options->image.scroll_reset_method = read_int_option(f, option,
			"image.scroll_reset_method", value, options->image.scroll_reset_method);
		options->image.tile_cache_max = read_int_option(f, option,
			"image.tile_cache_max", value, options->image.tile_cache_max);
		options->image.zoom_quality = CLAMP(read_int_option(f, option,
			"image.zoom_quality", value, options->image.zoom_quality), GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		options->image.dither_quality = CLAMP(read_int_option(f, option,
			"image.dither_quality", value, options->image.dither_quality), GDK_RGB_DITHER_NONE, GDK_RGB_DITHER_MAX);
		options->image.zoom_increment = read_int_option(f, option,
			"image.zoom_increment", value, options->image.zoom_increment);
		options->image.enable_read_ahead = read_bool_option(f, option,
			"image.enable_read_ahead", value, options->image.enable_read_ahead);
		options->image.exif_rotate_enable = read_bool_option(f, option,
			"image.exif_rotate_enable", value, options->image.exif_rotate_enable);
		options->image.use_custom_border_color = read_bool_option(f, option,
			"image.use_custom_border_color", value, options->image.use_custom_border_color);
		read_color_option(f, option,
			"image.border_color", value, &options->image.border_color);

		options->progressive_key_scrolling = read_bool_option(f, option,
			"progressive_keyboard_scrolling", value, options->progressive_key_scrolling);


		/* thumbnails options */
		options->thumbnails.enabled = read_bool_option(f, option,
			"thumbnails.enabled", value, options->thumbnails.enabled);
		options->thumbnails.max_width = read_int_option(f, option,
			"thumbnails.max_width", value, options->thumbnails.max_width);
		if (options->thumbnails.max_width < 16) options->thumbnails.max_width = 16;
		options->thumbnails.max_height = read_int_option(f, option,
			"thumbnails.max_height", value, options->thumbnails.max_height);
		if (options->thumbnails.max_height < 16) options->thumbnails.max_height = 16;
		options->thumbnails.enable_caching = read_bool_option(f, option,
			"thumbnails.enable_caching", value, options->thumbnails.enable_caching);
		options->thumbnails.cache_into_dirs = read_bool_option(f, option,
			"thumbnails.cache_into_dirs", value, options->thumbnails.cache_into_dirs);
		options->thumbnails.fast = read_bool_option(f, option,
			"thumbnails.fast", value, options->thumbnails.fast);
		options->thumbnails.use_xvpics = read_bool_option(f, option,
			"thumbnails.use_xvpics", value, options->thumbnails.use_xvpics);
		options->thumbnails.spec_standard = read_bool_option(f, option,
			"thumbnails.spec_standard", value, options->thumbnails.spec_standard);
		options->thumbnails.quality = CLAMP(read_int_option(f, option,
			"thumbnails.quality", value, options->thumbnails.quality), GDK_INTERP_NEAREST, GDK_INTERP_HYPER);

		options->enable_metadata_dirs = read_bool_option(f, option,
			"local_metadata", value, options->enable_metadata_dirs);

		/* file sorting options */
		options->file_sort.method = (SortType)read_int_option(f, option,
			"file_sort.method", value, (gint)options->file_sort.method);
		options->file_sort.ascending = read_bool_option(f, option,
			"file_sort.ascending", value, options->file_sort.ascending);
		options->file_sort.case_sensitive = read_bool_option(f, option,
			"file_sort.case_sensitive", value, options->file_sort.case_sensitive);

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

		options->mousewheel_scrolls = read_bool_option(f, option,
			"mouse_wheel_scrolls", value, options->mousewheel_scrolls);
		options->enable_in_place_rename = read_bool_option(f, option,
			"in_place_rename", value, options->enable_in_place_rename);

		options->recent_list_max = read_int_option(f, option,
			"open_recent_max", value, options->recent_list_max);

		options->place_dialogs_under_mouse = read_bool_option(f, option,
			"display_dialogs_under_mouse", value, options->place_dialogs_under_mouse);

		options->fullscreen.screen = read_int_option(f, option,
			"fullscreen.screen", value, options->fullscreen.screen);
		options->fullscreen.clean_flip = read_bool_option(f, option,
			"fullscreen.clean_flip", value, options->fullscreen.clean_flip);
		options->fullscreen.disable_saver = read_bool_option(f, option,
			"fullscreen.disable_saver", value, options->fullscreen.disable_saver);
		options->fullscreen.above = read_bool_option(f, option,
			"fullscreen.above", value, options->fullscreen.above);
		options->fullscreen.show_info = read_bool_option(f, option,
			"fullscreen.show_info", value, options->fullscreen.show_info);
		options->fullscreen.info = read_char_option(f, option,
			"fullscreen.info", value_all, options->fullscreen.info);

		options->dupe_custom_threshold = read_int_option(f, option,
			"custom_similarity_threshold", value, options->dupe_custom_threshold);

		/* slideshow options */

		options->slideshow.delay = read_int_unit_option(f, option,
			"slideshow.delay", value, options->slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
		options->slideshow.random = read_bool_option(f, option,
			"slideshow.random", value, options->slideshow.random);
		options->slideshow.repeat = read_bool_option(f, option,
			"slideshow.repeat", value, options->slideshow.repeat);

		/* filtering options */

		options->file_filter.show_dot_files = read_bool_option(f, option,
			"file_filter.show_dot_files", value, options->file_filter.show_dot_files);
		options->file_filter.disable = read_bool_option(f, option,
			"file_filter.disable", value, options->file_filter.disable);

		if (strcasecmp(option, "file_filter.ext") == 0)
			{
			filter_parse(value_all);
			}

		if (strcasecmp(option, "sidecar_ext") == 0)
			{
			sidecar_ext_parse(value_all, TRUE);
			}
		
		/* Color Profiles */

		options->color_profile.enabled = read_bool_option(f, option,
			"color_profile.enabled", value, options->color_profile.enabled);
		options->color_profile.use_image = read_bool_option(f, option,
			"color_profile.use_image", value, options->color_profile.use_image);
		options->color_profile.input_type = read_int_option(f, option,
			"color_profile.input_type", value, options->color_profile.input_type);

		if (strncasecmp(option, "color_profile.input_file_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				options->color_profile.input_file[i] = read_char_option(f, option,
					option, value, options->color_profile.input_file[i]);
				}
			}
		if (strncasecmp(option, "color_profile.input_name_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				options->color_profile.input_name[i] = read_char_option(f, option,
					option, value, options->color_profile.input_name[i]);
				}
			}

		options->color_profile.screen_type = read_int_option(f, option,
			"color_profile.screen_type", value, options->color_profile.screen_type);
		options->color_profile.screen_file = read_char_option(f, option,
			"color_profile.screen_file", value, options->color_profile.screen_file);

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

		/* collection options */

		options->collections.rectangular_selection = read_bool_option(f, option,
			"collections.rectangular_selection", value, options->collections.rectangular_selection);

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

