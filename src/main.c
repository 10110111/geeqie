/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include <gdk/gdkkeysyms.h> /* for keyboard values */

static void parse_command_line(int argc, char *argv[], gchar **path, gchar **file);
static void setup_default_options();

/*
 *-----------------------------------------------------------------------------
 * path manipulation routines (public)
 *-----------------------------------------------------------------------------
 */ 

gchar *filename_from_path(char *t)
{
	char *p;

	p = t + strlen(t);
	while(p > t && p[0] != '/') p--;
	if (p[0] == '/') p++;
	return p;
}

gchar *remove_level_from_path(gchar *path)
{
	gchar *new_path;
	gchar *ptr = path;
	gint p;

	if (!path) return NULL;

	p = strlen(path) - 1;
	if (p < 0) return NULL;
	while(ptr[p] != '/' && p > 0) p--;
	if (p == 0 && ptr[p] == '/') p++;
	new_path = g_strndup(path, (guint)p);
	return new_path;
}

void parse_out_relatives(gchar *path)
{
	gint s, t;

	if (!path) return;

	s = t = 0;

	while (path[s] != '\0')
		{
		if (path[s] == '/' && path[s+1] == '.' && (path[s+2] == '/' || path[s+2] == '\0') )
			{
			s += 2;
			}
		else if (path[s] == '/' && path[s+1] == '.' && path[s+2] == '.' && (path[s+3] == '/' || path[s+3] == '\0') )
			{
			s += 3;
			if (t > 0) t--;
			while (path[t] != '/' && t > 0) t--;
			}
		else
			{
			if (s != t) path[t] = path[s];
			t++;
			s++;
			}
		}
	if (t == 0 && path[t] == '/') t++;
	if (t > 1 && path[t-1] == '/') t--;
	path[t] = '\0';
}

/*
 *-----------------------------------------------------------------------------
 * external editor start routines (public)
 *-----------------------------------------------------------------------------
 */ 

void start_editor_from_file(gint n, gchar *path)
{
	gchar *cmd;
	if (!path) return;
	cmd = g_strdup_printf("%s \"%s\" &", editor_command[n], path);
	printf(_("GQview running: %s\n"),cmd);
	system(cmd);
	g_free(cmd);
}

void start_editor_from_image(gint n)
{
	start_editor_from_file(n, image_get_path());
}

void start_editor_from_list(gint n)
{
	gchar *cmd;
	gchar *buf;
	GList *list = file_get_selected_list();
	GList *work;
	if (!list) return;
	work = list;
	cmd = g_strconcat(editor_command[n], " ", NULL);
	while(work)
		{
		buf = cmd;
		cmd = g_strconcat(buf, "\"", work->data, "\" ", NULL);
		g_free(buf);
		work = work->next;
		}
	buf = cmd;
	cmd = g_strconcat(buf, "&", NULL);
	g_free(buf);
	printf(_("GQview running: %s\n"),cmd);
	system(cmd);
	g_free(cmd);
	free_selected_list(list);
}

/*
 *-----------------------------------------------------------------------------
 * keyboard functions
 *-----------------------------------------------------------------------------
 */

void keyboard_scroll_calc(gint *x, gint *y, GdkEventKey *event)
{
	static gint delta = 0;
	static guint32 time_old = 0;
	static guint keyval_old = 0;

	if (progressive_key_scrolling)
		{
		guint32 time_diff;

		time_diff = event->time - time_old;

		/* key pressed within 125ms ? (1/8 second) */
		if (time_diff > 125 || event->keyval != keyval_old) delta = 0;

		time_old = event->time;
		keyval_old = event->keyval;

		delta += 2;
		}
	else
		{
		delta = 8;
		}

	*x = *x * delta;
	*y = *y * delta;
}

gint key_press_cb(GtkWidget *widget, GdkEventKey *event)
{
	gint stop_signal = FALSE;
	gint x = 0;
	gint y = 0;

	if (GTK_WIDGET_HAS_FOCUS(path_entry))
		{
		if (event->keyval == GDK_Escape)
			gtk_entry_set_text(GTK_ENTRY(path_entry), current_path);
		return stop_signal;
		}

	if (full_screen_window || GTK_WIDGET_HAS_FOCUS(main_image->viewport))
		{
		switch (event->keyval)
			{
			case GDK_Left:
				x -= 1;
				stop_signal = TRUE;
				break;
			case GDK_Right:
				x += 1;
				stop_signal = TRUE;
				break;
			case GDK_Up:
				y -= 1;
				stop_signal = TRUE;
				break;
			case GDK_Down:
				y += 1;
				stop_signal = TRUE;
				break;
			case GDK_BackSpace:
				file_prev_image();
				stop_signal = TRUE;
				break;
			case GDK_space:
				file_next_image();
				stop_signal = TRUE;
				break;
			}
		}

	switch (event->keyval)
		{
		case '+':
			image_adjust_zoom(1);
			break;
		case GDK_Page_Up:
			file_prev_image();
			stop_signal = TRUE;
			break;
		case GDK_Page_Down:
			file_next_image();
			stop_signal = TRUE;
			break;
		case GDK_Home:
			file_first_image();
			stop_signal = TRUE;
			break;
		case GDK_End:
			file_last_image();
			stop_signal = TRUE;
			break;
		case GDK_Delete:
			file_util_delete(image_get_path(), NULL);
			stop_signal = TRUE;
			break;
		case GDK_Escape:
			interrupt_thumbs();
			stop_signal = TRUE;
			break;
		case 'Q': case 'q':
			if (event->state == 0 || (event->state & GDK_MODIFIER_MASK) == GDK_LOCK_MASK)
				{
				exit_gqview();
				return FALSE;
				}
			break;
		}

	if (event->state & GDK_SHIFT_MASK)
		{
		x *= 3;
		y *= 3;
		}

	if (x != 0 || y!= 0)
		{
		keyboard_scroll_calc(&x, &y, event);
		image_scroll(x, y);
		}

	if (stop_signal) gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

	return stop_signal;
}

/*
 *-----------------------------------------------------------------------------
 * command line parser (private) hehe, who needs popt anyway?
 *-----------------------------------------------------------------------------
 */ 

static gint startup_full_screen = FALSE;
static gint startup_in_slideshow = FALSE;

static void parse_command_line(int argc, char *argv[], gchar **path, gchar **file)
{
	if (argc > 1)
		{
		gint i;
		gchar *base_dir = get_current_dir();
		i = 1;
		while (i < argc)
			{
			gchar *cmd_line = argv[i];
			gchar *cmd_all = g_strconcat(base_dir, "/", cmd_line, NULL);

			if (!*path && cmd_line[0] == '/' && isdir(cmd_line))
				{
				*path = g_strdup(cmd_line);
				}
			else if (!*path && isdir(cmd_all))
				{
				*path = g_strdup(cmd_all);
				}
			else if (!*file && cmd_line[0] == '/' && isfile(cmd_line))
				{
				g_free(*path);
				*path = remove_level_from_path(cmd_line);
				*file = g_strdup(cmd_line);
				}
			else if (!*file && isfile(cmd_all))
				{
				g_free(*path);
				*path = remove_level_from_path(cmd_all);
				*file = g_strdup(cmd_all);
				}
			else if (strcmp(cmd_line, "--debug") == 0)
				{
				debug = TRUE;
				printf("debugging output enabled\n");
				}
			else if (strcmp(cmd_line, "+t") == 0 ||
				 strcmp(cmd_line, "--with-tools") == 0)
				{
				tools_float = FALSE;
				tools_hidden = FALSE;
				}
			else if (strcmp(cmd_line, "-t") == 0 ||
				 strcmp(cmd_line, "--without-tools") == 0)
				{
				tools_hidden = TRUE;
				}
			else if (strcmp(cmd_line, "-f") == 0 ||
				 strcmp(cmd_line, "--fullscreen") == 0)
				{
				startup_full_screen = TRUE;
				}
			else if (strcmp(cmd_line, "-s") == 0 ||
				 strcmp(cmd_line, "--slideshow") == 0)
				{
				startup_in_slideshow = TRUE;
				}
			else if (strcmp(cmd_line, "-h") == 0 ||
				 strcmp(cmd_line, "--help") == 0)
				{
				printf("GQview version %s\n", VERSION);
				printf(_("Usage: gqview [options] [path]\n\n"));
				printf(_("valid options are:\n"));
				printf(_("  +t, --with-tools           force show of tools\n"));
				printf(_("  -t, --without-tools        force hide of tools\n"));
				printf(_("  -f, --fullscreen           start in full screen mode\n"));
				printf(_("  -s, --slideshow            start in slideshow mode\n"));
				printf(_("  --debug                    turn on debug output\n"));
				printf(_("  -h, --help                 show this message\n\n"));
				exit (0);
				}
			else 
				{
				printf(_("invalid or ignored: %s\nUse -help for options\n"), cmd_line);
				}
			g_free(cmd_all);
			i++;
			}
		g_free(base_dir);
		parse_out_relatives(*path);
		parse_out_relatives(*file);
		}
}

/*
 *-----------------------------------------------------------------------------
 * startup, init, and exit
 *-----------------------------------------------------------------------------
 */ 

static void setup_default_options()
{
	gint i;

	for(i=0; i<8; i++)
		{
		editor_name[i] = NULL;
		editor_command[i] = NULL;
		}

	editor_name[0] = g_strdup(_("The Gimp"));
	editor_command[0] = g_strdup("gimp");

	editor_name[1] = g_strdup(_("Electric Eyes"));
	editor_command[1] = g_strdup("ee");

	editor_name[2] = g_strdup(_("XV"));
	editor_command[2] = g_strdup("xv");

	editor_name[3] = g_strdup(_("Xpaint"));
	editor_command[3] = g_strdup("xpaint");

	custom_filter = g_strdup(".eim;");
}

void exit_gqview()
{
	full_screen_stop();

	gdk_window_get_position (mainwindow->window, &main_window_x, &main_window_y);
	gdk_window_get_size(mainwindow->window, &main_window_w, &main_window_h);

	if (toolwindow)
		{
		gdk_window_get_position (toolwindow->window, &float_window_x, &float_window_y);
		gdk_window_get_size(toolwindow->window, &float_window_w, &float_window_h);
		}
	save_options();

	gtk_main_quit();
}

int main (int argc, char *argv[])
{
	gchar *cmd_path = NULL;
	gchar *cmd_file = NULL;

	/* setup locale, i18n */
	gtk_set_locale();
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	/* setup random seed for random slideshow */
        srand (time (0));

	gtk_init (&argc, &argv);
	gdk_imlib_init();

	/* push the correct color depths to gtk, (for 8-bit psuedo color displays)
	 * they should be popped, too, I guess...
	 */
	gtk_widget_push_visual(gdk_imlib_get_visual());
	gtk_widget_push_colormap(gdk_imlib_get_colormap());

	setup_default_options();
	load_options();

	parse_command_line(argc, argv, &cmd_path, &cmd_file);

	if (cmd_path)
		current_path = g_strdup(cmd_path);
	else if (startup_path_enable && startup_path && isdir(startup_path))
		current_path = g_strdup(startup_path);
	else
		current_path = get_current_dir();

	create_main_window();
	update_edit_menus(mainwindow_accel_grp);
	rebuild_file_filter();
	filelist_refresh();

	init_dnd();

	while(gtk_events_pending()) gtk_main_iteration();
	image_change_to(cmd_file);

	g_free(cmd_path);
	g_free(cmd_file);

	if (startup_full_screen) full_screen_toggle();
	if (startup_in_slideshow) slideshow_start();

	gtk_main ();
	return 0;
}


