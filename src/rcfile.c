/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

static gchar *quoted_value(gchar *text);
static void write_char_option(FILE *f, gchar *label, gchar *text);
static gchar *read_char_option(FILE *f, gchar *option, gchar *label, gchar *value, gchar *text);
static void write_int_option(FILE *f, gchar *label, gint n);
static gint read_int_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n);
static void write_bool_option(FILE *f, gchar *label, gint n);
static gint read_bool_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n);

/*
 *-----------------------------------------------------------------------------
 * line write/parse routines (private)
 *-----------------------------------------------------------------------------
 */ 

static gchar *quoted_value(gchar *text)
{
	gchar *ptr;
	gint c = 0;
	gint l = strlen(text);

	if (l == 0) return NULL;

	while (c < l && text[c] !='"') c++;
	if (text[c] == '"')
		{
		c++;
		ptr = text + c;
		while (c < l && text[c] !='"') c++;
		if (text[c] == '"')
			{
			text[c] = '\0';
			if (strlen(ptr) > 0)
				{
				return g_strdup(ptr);
				}
			}
		}
	else
		/* for compatibility with older formats (<0.3.7)
		 * read a line without quotes too */
		{
		c = 0;
		while (c < l && text[c] !=' ' && text[c] !=8 && text[c] != '\n') c++;
		if (c != 0)
			{
			text[c] = '\0';
			return g_strdup(text);
			}
		}

	return NULL;
}

static void write_char_option(FILE *f, gchar *label, gchar *text)
{
	if (text)
		fprintf(f,"%s: \"%s\"\n", label, text);
	else
		fprintf(f,"%s: \n", label);
}

static gchar *read_char_option(FILE *f, gchar *option, gchar *label, gchar *value, gchar *text)
{
	if (!strcasecmp(option, label))
		{
		g_free(text);
		text = quoted_value(value);
		}
	return text;
}

static void write_int_option(FILE *f, gchar *label, gint n)
{
	fprintf(f,"%s: %d\n\n", label, n);
}

static gint read_int_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n)
{
	if (!strcasecmp(option, label))
		{
		n = strtol(value,NULL,0);
		}
	return n;
}

static void write_bool_option(FILE *f, gchar *label, gint n)
{
	fprintf(f,"%s: ", label);
	if (n) fprintf(f,"true\n"); else fprintf(f,"false\n");
	fprintf(f,"\n");
}

static gint read_bool_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n)
{
	if (!strcasecmp(option, label))
		{
		if (!strcasecmp(value, "true"))
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

void save_options()
{
	FILE *f;
	gchar *rc_path;
	gint i;

	rc_path = g_strconcat(homedir(), "/", RC_FILE_NAME, NULL);

	f = fopen(rc_path,"w");
	if (!f)
		{
		printf(_("error saving config file: %s\n"), rc_path);
		g_free(rc_path);
		return;
		}

	fprintf(f,"######################################################################\n");
	fprintf(f,"#                         GQview config file         version %7s #\n", VERSION);
	fprintf(f,"#                                                                    #\n");
	fprintf(f,"#  Everything in this file can be changed in the option dialogs.     #\n");
	fprintf(f,"#      (so there should be no need to edit this file by hand)        #\n");
	fprintf(f,"#                                                                    #\n");
	fprintf(f,"######################################################################\n");
	fprintf(f,"\n");
	fprintf(f,"##### General Options #####\n\n");

	write_bool_option(f, "enable_startup_path", startup_path_enable);
	write_char_option(f, "startup_path", startup_path);
	fprintf(f,"\n");

	fprintf(f,"zoom_mode: ");
	if (zoom_mode == ZOOM_RESET_ORIGINAL) fprintf(f,"original\n");
	if (zoom_mode == ZOOM_RESET_FIT_WINDOW) fprintf(f,"fit\n");
	if (zoom_mode == ZOOM_RESET_NONE) fprintf(f,"dont_change\n");
	fprintf(f,"\n");

	write_bool_option(f, "fit_window_to_image", fit_window);
	write_bool_option(f, "limit_window_size", limit_window_size);
	write_int_option(f, "max_window_size", max_window_size);
	fprintf(f,"\n");

	write_bool_option(f, "progressive_keyboard_scrolling", progressive_key_scrolling);
	fprintf(f,"\n");

	write_bool_option(f, "enable_thumbnails", thumbnails_enabled);
	write_int_option(f, "thumbnail_width", thumb_max_width);
	write_int_option(f, "thumbnail_height", thumb_max_height);
	write_bool_option(f, "cache_thumbnails", enable_thumb_caching);
	write_bool_option(f, "use_xvpics_thumbnails", use_xvpics_thumbnails);
	fprintf(f,"\n");

	write_bool_option(f, "confirm_delete", confirm_delete);
	fprintf(f,"\n");
	write_bool_option(f, "tools_float", tools_float);
	write_bool_option(f, "tools_hidden", tools_hidden);
	write_bool_option(f, "restore_tool_state", restore_tool);

	fprintf(f,"\n##### Slideshow Options #####\n\n");

	write_int_option(f, "slideshow_delay", slideshow_delay);

	write_bool_option(f, "slideshow_random", slideshow_random);
	write_bool_option(f, "slideshow_repeat", slideshow_repeat);

	fprintf(f,"\n##### Filtering Options #####\n\n");

	write_bool_option(f, "show_dotfiles", show_dot_files);
	write_bool_option(f, "disable_filtering", file_filter_disable);
	fprintf(f,"\n");
	write_bool_option(f, "filter_ppm", filter_include_ppm);
	write_bool_option(f, "filter_png", filter_include_png);
	write_bool_option(f, "filter_jpg", filter_include_jpg);
	write_bool_option(f, "filter_tif", filter_include_tif);
	write_bool_option(f, "filter_pgm", filter_include_pgm);
	write_bool_option(f, "filter_xpm", filter_include_xpm);
	write_bool_option(f, "filter_gif", filter_include_gif);
	write_bool_option(f, "filter_pcx", filter_include_pcx);
	write_bool_option(f, "filter_bmp", filter_include_bmp);
	fprintf(f,"\n");
	write_char_option(f, "custom_filter", custom_filter);

	fprintf(f,"\n##### External Programs #####\n");
	fprintf(f,"# Maximum of 8 programs (external_1 through external 8)\n");
	fprintf(f,"# format: external_n: \"menu name\" \"command line\"\n\n");

	for (i=0; i<8; i++)
		{
		fprintf(f,"external_%d: \"", i+1);
		if (editor_name[i]) fprintf(f, editor_name[i]);
		fprintf(f, "\" \"");
		if (editor_command[i]) fprintf(f, editor_command[i]);
		fprintf(f, "\"\n");
		}

	fprintf(f,"\n##### Window Positions #####\n\n");

	write_bool_option(f, "restore_window_positions", save_window_positions);
	fprintf(f,"\n");
	write_int_option(f, "main_window_x", main_window_x);
	write_int_option(f, "main_window_y", main_window_y);
	write_int_option(f, "main_window_width", main_window_w);
	write_int_option(f, "main_window_height", main_window_h);
	write_int_option(f, "float_window_x", float_window_x);
	write_int_option(f, "float_window_y", float_window_y);
	write_int_option(f, "float_window_width", float_window_w);
	write_int_option(f, "float_window_height", float_window_h);

	fprintf(f,"######################################################################\n");
	fprintf(f,"#                      end of GQview config file                     #\n");
	fprintf(f,"######################################################################\n");

	fclose(f);

	g_free(rc_path);
}

/*
 *-----------------------------------------------------------------------------
 * load configuration (public)
 *-----------------------------------------------------------------------------
 */ 

void load_options()
{
	FILE *f;
	gchar *rc_path;
	gchar s_buf[1024];
	gchar *s_buf_ptr;
	gchar option[1024];
	gchar value[1024];
	gchar value_all[1024];
	gint c,l,i;
	rc_path = g_strconcat(homedir(), "/", RC_FILE_NAME, NULL);

	f = fopen(rc_path,"r");
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
		while (s_buf[c] == ' ' && c < l) c++;
		while (s_buf[c] == 8 && c < l) c++;
		while (s_buf[c] == ' ' && c < l) c++;
		s_buf_ptr = s_buf + c;
		strcpy(value_all,s_buf_ptr);
		while (s_buf[c] != 8 && s_buf[c] != ' ' && s_buf[c] != '\n' && c < l) c++;
		s_buf[c] = '\0';
		strcpy(option,s_buf);
		strcpy(value,s_buf_ptr);

		/* general options */

		startup_path_enable = read_bool_option(f, option,
			"enable_startup_path", value, startup_path_enable);
		startup_path = read_char_option(f, option,
			"startup_path", value_all, startup_path);

		if (!strcasecmp(option,"zoom_mode"))
                        {
                        if (!strcasecmp(value,"original")) zoom_mode = ZOOM_RESET_ORIGINAL;
                        if (!strcasecmp(value,"fit")) zoom_mode = ZOOM_RESET_FIT_WINDOW;
                        if (!strcasecmp(value,"dont_change")) zoom_mode = ZOOM_RESET_NONE;
                        }

		fit_window = read_bool_option(f, option,
			"fit_window_to_image", value, fit_window);
		limit_window_size = read_bool_option(f, option,
			"limit_window_size", value, limit_window_size);
		max_window_size = read_int_option(f, option,
			"max_window_size", value, max_window_size);
		progressive_key_scrolling = read_bool_option(f, option,
			"progressive_keyboard_scrolling", value, progressive_key_scrolling);

		thumbnails_enabled = read_bool_option(f, option,
			"enable_thumbnails", value, thumbnails_enabled);
		thumb_max_width = read_int_option(f, option,
			"thumbnail_width", value, thumb_max_width);
		thumb_max_height = read_int_option(f, option,
			"thumbnail_height", value, thumb_max_height);
		enable_thumb_caching = read_bool_option(f, option,
			"cache_thumbnails", value, enable_thumb_caching);
		use_xvpics_thumbnails = read_bool_option(f, option,
			"use_xvpics_thumbnails", value, use_xvpics_thumbnails);

		confirm_delete = read_bool_option(f, option,
			"confirm_delete", value, confirm_delete);

		tools_float = read_bool_option(f, option,
			"tools_float", value, tools_float);
		tools_hidden = read_bool_option(f, option,
			"tools_hidden", value, tools_hidden);
		restore_tool = read_bool_option(f, option,
			"restore_tool_state", value, restore_tool);

		/* slideshow opitons */

		slideshow_delay = read_int_option(f, option,
			"slideshow_delay", value, slideshow_delay);
		slideshow_random = read_bool_option(f, option,
			"slideshow_random", value, slideshow_random);
		slideshow_repeat = read_bool_option(f, option,
			"slideshow_repeat", value, slideshow_repeat);

		/* filtering options */

		show_dot_files = read_bool_option(f, option,
			"show_dotfiles", value, show_dot_files);
		file_filter_disable = read_bool_option(f, option,
			"disable_filtering", value, file_filter_disable);

		filter_include_ppm = read_bool_option(f, option,
			"filter_ppm", value, filter_include_ppm);
		filter_include_png = read_bool_option(f, option,
			"filter_png", value, filter_include_png);
		filter_include_jpg = read_bool_option(f, option,
			"filter_jpg", value, filter_include_jpg);
		filter_include_tif = read_bool_option(f, option,
			"filter_tif", value, filter_include_tif);
		filter_include_pgm = read_bool_option(f, option,
			"filter_pgm", value, filter_include_pgm);
		filter_include_xpm = read_bool_option(f, option,
			"filter_xpm", value, filter_include_xpm);
		filter_include_gif = read_bool_option(f, option,
			"filter_gif", value, filter_include_gif);
		filter_include_pcx = read_bool_option(f, option,
			"filter_pcx", value, filter_include_pcx);
		filter_include_bmp = read_bool_option(f, option,
			"filter_bmp", value, filter_include_bmp);

		custom_filter = read_char_option(f, option,
			"custom_filter", value_all, custom_filter);

		/* External Programs */

		if (!strncasecmp(option,"external_",9))
			{
			i = strtol(option + 9, NULL, 0);
			if (i>0 && i<9)
				{
				gchar *ptr1, *ptr2;
				i--;
				c = 0;
				l = strlen(value_all);
				ptr1 = value_all;

				g_free(editor_name[i]);
				editor_name[i] = NULL;
				g_free(editor_command[i]);
				editor_command[i] = NULL;

				while (c<l && value_all[c] !='"') c++;
				if (ptr1[c] == '"')
					{
					c++;
					ptr2 = ptr1 + c;
					while (c<l && value_all[c] !='"') c++;
					if (ptr1[c] == '"')
						{
						ptr1[c] = '\0';
						if (ptr1 + c - 1 != ptr2)
							editor_name[i] = g_strdup(ptr2);
						c++;
						while (c<l && value_all[c] !='"') c++;
						if (ptr1[c] == '"')
							{
							c++;
							ptr2 = ptr1 + c;
							while (c<l && value_all[c] !='"') c++;
							if (ptr1[c] == '"' && ptr1 + c - 1 != ptr2)
								{
								ptr1[c] = '\0';
								editor_command[i] = g_strdup(ptr2);
								}
							}
						}
					}
				}
			}

		/* window positions */

		save_window_positions = read_bool_option(f, option,
			"restore_window_positions", value, save_window_positions);

		main_window_x = read_int_option(f, option,
			"main_window_x", value, main_window_x);
		main_window_y = read_int_option(f, option,
			"main_window_y", value, main_window_y);
		main_window_w = read_int_option(f, option,
			"main_window_width", value, main_window_w);
		main_window_h = read_int_option(f, option,
			"main_window_height", value, main_window_h);
		float_window_x = read_int_option(f, option,
			"float_window_x", value, float_window_x);
		float_window_y = read_int_option(f, option,
			"float_window_y", value, float_window_y);
		float_window_w = read_int_option(f, option,
			"float_window_width", value, float_window_w);
		float_window_h = read_int_option(f, option,
			"float_window_height", value, float_window_h);
		}

	fclose(f);
	g_free(rc_path);
}

