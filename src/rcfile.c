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
	
#define WRITE_BOOL(_name_) write_bool_option(ssi, #_name_, options->_name_)
#define WRITE_INT(_name_) write_int_option(ssi, #_name_, options->_name_)
#define WRITE_INT_UNIT(_name_, _unit_) write_int_unit_option(ssi, #_name_, options->_name_, _unit_)
#define WRITE_CHAR(_name_) write_char_option(ssi, #_name_, options->_name_)
#define WRITE_COLOR(_name_) write_color_option(ssi, #_name_, &options->_name_)

#define WRITE_SEPARATOR() secure_fputc(ssi, '\n')
#define WRITE_SUBTITLE(_title_) secure_fprintf(ssi, "\n\n##### "_title_" #####\n\n")

	secure_fprintf(ssi, "######################################################################\n");
	secure_fprintf(ssi, "# %30s config file         version %7s #\n", GQ_APPNAME, VERSION);
	secure_fprintf(ssi, "######################################################################\n");
	WRITE_SEPARATOR();

	secure_fprintf(ssi, "# Note: This file is autogenerated. Options can be changed here,\n");
	secure_fprintf(ssi, "#       but user comments and formatting will be lost.\n");
	WRITE_SEPARATOR();

	WRITE_SUBTITLE("General Options");

	WRITE_BOOL(show_icon_names);
	WRITE_SEPARATOR();

	WRITE_BOOL(tree_descend_subdirs);
	WRITE_BOOL(lazy_image_sync);
	WRITE_BOOL(update_on_time_change);
	WRITE_SEPARATOR();

	WRITE_BOOL(startup_path_enable);
	WRITE_CHAR(startup_path);

	WRITE_BOOL(progressive_key_scrolling);
	WRITE_BOOL(enable_metadata_dirs);

	WRITE_INT(duplicates_similarity_threshold);
	WRITE_SEPARATOR();

	WRITE_BOOL(mousewheel_scrolls);
	WRITE_INT(open_recent_list_maxsize);
	WRITE_BOOL(place_dialogs_under_mouse);


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
	WRITE_BOOL(layout.view_as_icons);
	WRITE_BOOL(layout.view_as_tree);
	WRITE_BOOL(layout.show_thumbnails);
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

	WRITE_BOOL(layout.tools_float);
	WRITE_BOOL(layout.tools_hidden);
	WRITE_BOOL(layout.tools_restore_state);
	WRITE_SEPARATOR();

	WRITE_BOOL(layout.toolbar_hidden);


	WRITE_SUBTITLE("Image Options");

	secure_fprintf(ssi, "image.zoom_mode: ");
	if (options->image.zoom_mode == ZOOM_RESET_ORIGINAL) secure_fprintf(ssi, "original\n");
	if (options->image.zoom_mode == ZOOM_RESET_FIT_WINDOW) secure_fprintf(ssi, "fit\n");
	if (options->image.zoom_mode == ZOOM_RESET_NONE) secure_fprintf(ssi, "dont_change\n");
	WRITE_BOOL(image.zoom_2pass);
	WRITE_BOOL(image.zoom_to_fit_allow_expand);
	WRITE_INT(image.zoom_quality);
	WRITE_INT(image.zoom_increment);
	WRITE_BOOL(image.fit_window_to_image);
	WRITE_BOOL(image.limit_window_size);
	WRITE_INT(image.max_window_size);
	WRITE_BOOL(image.limit_autofit_size);
	WRITE_INT(image.max_autofit_size);
	WRITE_INT(image.scroll_reset_method);
	WRITE_INT(image.tile_cache_max);
	WRITE_INT(image.dither_quality);
	WRITE_BOOL(image.enable_read_ahead);
	WRITE_BOOL(image.exif_rotate_enable);
	WRITE_BOOL(image.use_custom_border_color);
	WRITE_COLOR(image.border_color);


	WRITE_SUBTITLE("Thumbnails Options");

	WRITE_INT(thumbnails.max_width);
	WRITE_INT(thumbnails.max_height);
	WRITE_BOOL(thumbnails.enable_caching);
	WRITE_BOOL(thumbnails.cache_into_dirs);
	WRITE_BOOL(thumbnails.fast);
	WRITE_BOOL(thumbnails.use_xvpics);
	WRITE_BOOL(thumbnails.spec_standard);
	WRITE_INT(thumbnails.quality);


	WRITE_SUBTITLE("File sorting Options");

	WRITE_INT(file_sort.method);
	WRITE_BOOL(file_sort.ascending);
	WRITE_BOOL(file_sort.case_sensitive);

	
	WRITE_SUBTITLE("Fullscreen Options");

	WRITE_INT(fullscreen.screen);
	WRITE_BOOL(fullscreen.clean_flip);
	WRITE_BOOL(fullscreen.disable_saver);
	WRITE_BOOL(fullscreen.above);
	WRITE_BOOL(fullscreen.show_info);
	WRITE_CHAR(fullscreen.info);

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

	sidecar_ext_write(ssi);


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

	WRITE_SUBTITLE("External Programs");
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


	WRITE_SUBTITLE("Exif Options");
	secure_fprintf(ssi, "# Display: 0: never\n"
			    "#          1: if set\n"
			    "#          2: always\n\n");
	for (i = 0; ExifUIList[i].key; i++)
		{
		secure_fprintf(ssi, "exif.display.");
		write_int_option(ssi, (gchar *)ExifUIList[i].key, ExifUIList[i].current);
		}

	WRITE_SEPARATOR();
	WRITE_SEPARATOR();

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
		READ_BOOL(file_filter.show_dot_directory);
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

