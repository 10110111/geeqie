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

static void read_char_option(FILE *f, gchar *option, gchar *label, gchar *value, gchar **text)
{
	if (text && strcasecmp(option, label) == 0)
		{
		g_free(*text);
		*text = quoted_value(value, NULL);
		}
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

static void read_color_option(FILE *f, gchar *option, gchar *label, gchar *value, GdkColor *color)
{
	if (color && strcasecmp(option, label) == 0)
		{
		gchar *colorstr = quoted_value(value, NULL);
		if (colorstr) gdk_color_parse(colorstr, color);
		g_free(colorstr);
		}
}


static void write_int_option(SecureSaveInfo *ssi, gchar *label, gint n)
{
	secure_fprintf(ssi, "%s: %d\n", label, n);
}

static void read_int_option(FILE *f, gchar *option, gchar *label, gchar *value, gint *n)
{
	if (n && strcasecmp(option, label) == 0)
		{
		*n = strtol(value, NULL, 10);
		}
}

static void read_uint_option(FILE *f, gchar *option, gchar *label, gchar *value, guint *n)
{
	if (n && strcasecmp(option, label) == 0)
		{
		*n = strtoul(value, NULL, 10);
		}
}



static void read_int_option_clamp(FILE *f, gchar *option, gchar *label, gchar *value, gint *n, gint min, gint max)
{
	if (n && strcasecmp(option, label) == 0)
		{
		*n = CLAMP(strtol(value, NULL, 10), min, max);
		}
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

static void read_int_unit_option(FILE *f, gchar *option, gchar *label, gchar *value, gint *n, gint subunits)
{
	if (n && strcasecmp(option, label) == 0)
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

		*n = l * subunits + r;
		}
}

static void write_bool_option(SecureSaveInfo *ssi, gchar *label, gint n)
{
	secure_fprintf(ssi, "%s: ", label);
	if (n) secure_fprintf(ssi, "true\n"); else secure_fprintf(ssi, "false\n");
}

static void read_bool_option(FILE *f, gchar *option, gchar *label, gchar *value, gint *n)
{
	if (n && strcasecmp(option, label) == 0)
		{
		if (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)
			*n = TRUE;
		else
			*n = FALSE;
		}
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

	write_bool_option(ssi, "tree_descend_subdirs", options->tree_descend_subdirs);
	write_bool_option(ssi, "lazy_image_sync", options->lazy_image_sync);
	write_bool_option(ssi, "update_on_time_change", options->update_on_time_change);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "startup_path_enable", options->startup_path_enable);
	write_char_option(ssi, "startup_path", options->startup_path);

	write_bool_option(ssi, "progressive_key_scrolling", options->progressive_key_scrolling);
	write_bool_option(ssi, "enable_metadata_dirs", options->enable_metadata_dirs);

	write_int_option(ssi, "duplicates_similarity_threshold", options->duplicates_similarity_threshold);
	secure_fputc(ssi, '\n');

	write_bool_option(ssi, "mousewheel_scrolls", options->mousewheel_scrolls);
	write_int_option(ssi, "open_recent_list_maxsize", options->open_recent_list_maxsize);
	write_bool_option(ssi, "place_dialogs_under_mouse", options->place_dialogs_under_mouse);


	secure_fprintf(ssi, "\n##### File operations Options #####\n\n");

	write_bool_option(ssi, "file_ops.enable_in_place_rename", options->file_ops.enable_in_place_rename);
	write_bool_option(ssi, "file_ops.confirm_delete", options->file_ops.confirm_delete);
	write_bool_option(ssi, "file_ops.enable_delete_key", options->file_ops.enable_delete_key);
	write_bool_option(ssi, "file_ops.safe_delete_enable", options->file_ops.safe_delete_enable);
	write_char_option(ssi, "file_ops.safe_delete_path", options->file_ops.safe_delete_path);
	write_int_option(ssi, "file_ops.safe_delete_folder_maxsize", options->file_ops.safe_delete_folder_maxsize);

	
	secure_fprintf(ssi, "\n##### Layout Options #####\n\n");

	write_int_option(ssi, "layout.style", options->layout.style);
	write_char_option(ssi, "layout.order", options->layout.order);
	write_bool_option(ssi, "layout.view_as_icons", options->layout.view_as_icons);
	write_bool_option(ssi, "layout.view_as_tree", options->layout.view_as_tree);
	write_bool_option(ssi, "layout.show_thumbnails", options->layout.show_thumbnails);
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

	write_bool_option(ssi, "file_filter.show_hidden_files", options->file_filter.show_hidden_files);
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
	secure_fprintf(ssi, "# Maximum of %d programs (external_1 through external_%d)\n", GQ_EDITOR_GENERIC_SLOTS, GQ_EDITOR_GENERIC_SLOTS);
	secure_fprintf(ssi, "# external_%d through external_%d are used for file ops\n", GQ_EDITOR_GENERIC_SLOTS + 1, GQ_EDITOR_SLOTS);
	secure_fprintf(ssi, "# format: external_n: \"menu name\" \"command line\"\n\n");

	for (i = 0; i < GQ_EDITOR_SLOTS; i++)
		{
		if (i == GQ_EDITOR_GENERIC_SLOTS) secure_fputc(ssi, '\n');
		gchar *qname = escquote_value(options->editor_name[i]);
		gchar *qcommand = escquote_value(options->editor_command[i]);
		secure_fprintf(ssi, "external_%d: %s %s\n", i+1, qname, qcommand);
		g_free(qname);
		g_free(qcommand);
		}


	secure_fprintf(ssi, "\n##### Exif Options #####\n# Display:\n#   0: never\n#   1: if set\n#   2: always\n\n");
	for (i = 0; ExifUIList[i].key; i++)
		{
		secure_fprintf(ssi, "exif.display.");
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
	gchar option[1024];
	gchar value[1024];
	gchar value_all[1024];
	gint i;

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

	while (fgets(s_buf, sizeof(s_buf), f))
		{
		gchar *option_start, *value_start;
		gchar *p = s_buf;

		while(g_ascii_isspace(*p)) p++;
		if (!*p || *p == '\n' || *p == '#') continue;
		option_start = p;
		while(*p && *p != ':') p++;
		if (!*p) continue;
		*p = '\0';
		p++;
		strncpy(option, option_start, sizeof(option));
		while(g_ascii_isspace(*p)) p++;
		value_start = p;
		strncpy(value_all, value_start, sizeof(value_all));
		while(*p && !g_ascii_isspace(*p) && *p != '\n') p++;
		*p = '\0';
		strncpy(value, value_start, sizeof(value));

#define READ_BOOL(_name_) read_bool_option(f, option, #_name_, value, &options->_name_)
#define READ_INT(_name_) read_int_option(f, option, #_name_, value, &options->_name_)
#define READ_UINT(_name_) read_uint_option(f, option, #_name_, value, &options->_name_)
#define READ_INT_CLAMP(_name_, _min_, _max_) read_int_option_clamp(f, option, #_name_, value, &options->_name_, _min_, _max_)
#define READ_INT_UNIT(_name_, _unit_) read_int_unit_option(f, option, #_name_, value, &options->_name_, _unit_)
#define READ_CHAR(_name_) read_char_option(f, option, #_name_, value_all, &options->_name_)
#define READ_COLOR(_name_) read_color_option(f, option, #_name_, value, &options->_name_)

		/* general options */
		READ_BOOL(show_icon_names);
		READ_BOOL(show_icon_names);

		READ_BOOL(tree_descend_subdirs);
		READ_BOOL(lazy_image_sync);
		READ_BOOL(update_on_time_change);
	
		READ_BOOL(startup_path_enable);
		READ_CHAR(startup_path);

		READ_INT(duplicates_similarity_threshold);

		READ_BOOL(progressive_key_scrolling);

		READ_BOOL(enable_metadata_dirs);

		READ_BOOL(mousewheel_scrolls);
	
		READ_INT(open_recent_list_maxsize);

		READ_BOOL(place_dialogs_under_mouse);


		/* layout options */

		READ_INT(layout.style);
		READ_CHAR(layout.order);
		READ_BOOL(layout.view_as_icons);
		READ_BOOL(layout.view_as_tree);
		READ_BOOL(layout.show_thumbnails);

		/* window positions */

		READ_BOOL(layout.save_window_positions);

		READ_INT(layout.main_window.x);
		READ_INT(layout.main_window.y);
		READ_INT(layout.main_window.w);
		READ_INT(layout.main_window.h);
		READ_BOOL(layout.main_window.maximized);
		READ_INT(layout.float_window.x);
		READ_INT(layout.float_window.y);
		READ_INT(layout.float_window.w);
		READ_INT(layout.float_window.h);
		READ_INT(layout.float_window.vdivider_pos);
		READ_INT(layout.main_window.hdivider_pos);
		READ_INT(layout.main_window.vdivider_pos);
		READ_BOOL(layout.tools_float);
		READ_BOOL(layout.tools_hidden);
		READ_BOOL(layout.tools_restore_state);
		READ_BOOL(layout.toolbar_hidden);


		/* image options */
		if (strcasecmp(option, "image.zoom_mode") == 0)
                        {
                        if (strcasecmp(value, "original") == 0) options->image.zoom_mode = ZOOM_RESET_ORIGINAL;
                        if (strcasecmp(value, "fit") == 0) options->image.zoom_mode = ZOOM_RESET_FIT_WINDOW;
                        if (strcasecmp(value, "dont_change") == 0) options->image.zoom_mode = ZOOM_RESET_NONE;
                        }
		READ_BOOL(image.zoom_2pass);
		READ_BOOL(image.zoom_to_fit_allow_expand);
		READ_BOOL(image.fit_window_to_image);
		READ_BOOL(image.limit_window_size);
		READ_INT(image.max_window_size);
		READ_BOOL(image.limit_autofit_size);
		READ_INT(image.max_autofit_size);
		READ_INT(image.scroll_reset_method);
		READ_INT(image.tile_cache_max);
		READ_INT_CLAMP(image.zoom_quality, GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		READ_INT_CLAMP(image.dither_quality, GDK_RGB_DITHER_NONE, GDK_RGB_DITHER_MAX);
		READ_INT(image.zoom_increment);
		READ_BOOL(image.enable_read_ahead);
		READ_BOOL(image.exif_rotate_enable);
		READ_BOOL(image.use_custom_border_color);
		READ_COLOR(image.border_color);


		/* thumbnails options */
		READ_INT_CLAMP(thumbnails.max_width, 16, 512);
		READ_INT_CLAMP(thumbnails.max_height, 16, 512);

		READ_BOOL(thumbnails.enable_caching);
		READ_BOOL(thumbnails.cache_into_dirs);
		READ_BOOL(thumbnails.fast);
		READ_BOOL(thumbnails.use_xvpics);
		READ_BOOL(thumbnails.spec_standard);
		READ_INT_CLAMP(thumbnails.quality, GDK_INTERP_NEAREST, GDK_INTERP_HYPER);

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
		READ_BOOL(fullscreen.show_info);
		READ_CHAR(fullscreen.info);

		/* slideshow options */

		READ_INT_UNIT(slideshow.delay, SLIDESHOW_SUBSECOND_PRECISION);
		READ_BOOL(slideshow.random);
		READ_BOOL(slideshow.repeat);

		/* collection options */

		READ_BOOL(collections.rectangular_selection);

		/* filtering options */

		READ_BOOL(file_filter.show_hidden_files);
		READ_BOOL(file_filter.disable);

		if (strcasecmp(option, "file_filter.ext") == 0)
			{
			filter_parse(value_all);
			}

		if (strcasecmp(option, "sidecar.ext") == 0)
			{
			sidecar_ext_parse(value_all, TRUE);
			}
		
		/* Color Profiles */

		READ_BOOL(color_profile.enabled);
		READ_BOOL(color_profile.use_image);
		READ_INT(color_profile.input_type);

		if (strncasecmp(option, "color_profile.input_file_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				read_char_option(f, option, option, value, &options->color_profile.input_file[i]);
				}
			}
		if (strncasecmp(option, "color_profile.input_name_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				read_char_option(f, option, option, value, &options->color_profile.input_name[i]);
				}
			}

		READ_INT(color_profile.screen_type);
		READ_CHAR(color_profile.screen_file);

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

		/* Exif */
		if (0 == strncasecmp(option, "exif.display.", 13))
			{
			for (i = 0; ExifUIList[i].key; i++)
				if (0 == strcasecmp(option + 13, ExifUIList[i].key))
					ExifUIList[i].current = strtol(value, NULL, 10);
		  	}
		}

	fclose(f);
	g_free(rc_path);
}

