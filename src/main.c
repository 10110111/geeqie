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


#include "main.h"

#include "cache.h"
#include "collect.h"
#include "collect-io.h"
#include "filedata.h"
#include "filefilter.h"
#include "history_list.h"
#include "image-overlay.h"
#include "layout.h"
#include "layout_image.h"
#include "options.h"
#include "remote.h"
#include "similar.h"
#include "ui_fileops.h"
#include "ui_utildlg.h"
#include "cache_maint.h"
#include "thumb.h"

#include <gdk/gdkkeysyms.h> /* for keyboard values */


#include <math.h>
#ifdef G_OS_UNIX
#include <pwd.h>
#endif


static RemoteConnection *remote_connection = NULL;


/*
 *-----------------------------------------------------------------------------
 * misc (public)
 *-----------------------------------------------------------------------------
 */


gdouble get_zoom_increment(void)
{
	return ((options->image.zoom_increment != 0) ? (gdouble)options->image.zoom_increment / 10.0 : 1.0);
}

gchar *utf8_validate_or_convert(const gchar *text)
{
	gint len;

	if (!text) return NULL;
	
	len = strlen(text);
	if (!g_utf8_validate(text, len, NULL))
		return g_convert(text, len, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);

	return g_strdup(text);
}

gint utf8_compare(const gchar *s1, const gchar *s2, gboolean case_sensitive)
{
	gchar *s1_key, *s2_key;
	gchar *s1_t, *s2_t;
	gint ret;

	g_assert(g_utf8_validate(s1, -1, NULL));
	g_assert(g_utf8_validate(s2, -1, NULL));

	if (!case_sensitive)
		{
		s1_t = g_utf8_casefold(s1, -1);
		s2_t = g_utf8_casefold(s2, -1);
		}

	s1_key = g_utf8_collate_key(s1_t, -1);
	s2_key = g_utf8_collate_key(s2_t, -1);

	ret = strcmp(s1_key, s2_key);

	g_free(s1_key);
	g_free(s2_key);

	if (!case_sensitive)
		{
		g_free(s1_t);
		g_free(s2_t);
		}

	return ret;
}

/* Borrowed from gtkfilesystemunix.c */
gchar *expand_tilde(const gchar *filename)
{
#ifndef G_OS_UNIX
	return g_strdup(filename);
#else
	const gchar *notilde;
	const gchar *slash;
	const gchar *home;

	if (filename[0] != '~')
		return g_strdup(filename);

	notilde = filename + 1;
	slash = strchr(notilde, G_DIR_SEPARATOR);
	if (slash == notilde || !*notilde)
		{
		home = g_get_home_dir();
		if (!home)
			return g_strdup(filename);
		}
	else
		{
		gchar *username;
		struct passwd *passwd;

		if (slash)
			username = g_strndup(notilde, slash - notilde);
		else
			username = g_strdup(notilde);

		passwd = getpwnam(username);
		g_free(username);

		if (!passwd)
			return g_strdup(filename);

		home = passwd->pw_dir;
		}

	if (slash)
		return g_build_filename(home, G_DIR_SEPARATOR_S, slash + 1, NULL);
	else
		return g_build_filename(home, G_DIR_SEPARATOR_S, NULL);
#endif
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

	if (event->state & GDK_CONTROL_MASK)
		{
		if (*x < 0) *x = G_MININT / 2;
		if (*x > 0) *x = G_MAXINT / 2;
		if (*y < 0) *y = G_MININT / 2;
		if (*y > 0) *y = G_MAXINT / 2;

		return;
		}

	if (options->progressive_key_scrolling)
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



/*
 *-----------------------------------------------------------------------------
 * command line parser (private) hehe, who needs popt anyway?
 *-----------------------------------------------------------------------------
 */

static gint startup_blank = FALSE;
static gint startup_full_screen = FALSE;
static gint startup_in_slideshow = FALSE;
static gint startup_command_line_collection = FALSE;


static void parse_command_line_add_file(const gchar *file_path, gchar **path, gchar **file,
					GList **list, GList **collection_list)
{
	gchar *path_parsed;

	path_parsed = g_strdup(file_path);
	parse_out_relatives(path_parsed);

	if (file_extension_match(path_parsed, GQ_COLLECTION_EXT))
		{
		*collection_list = g_list_append(*collection_list, path_parsed);
		}
	else
		{
		if (!*path) *path = remove_level_from_path(path_parsed);
		if (!*file) *file = g_strdup(path_parsed);
		*list = g_list_prepend(*list, file_data_new_simple(path_parsed));
		}
}

static void parse_command_line_add_dir(const gchar *dir, gchar **path, gchar **file,
				       GList **list)
{
	GList *files;
	gchar *path_parsed;
	FileData *dir_fd;

	path_parsed = g_strdup(dir);
	parse_out_relatives(path_parsed);
	dir_fd = file_data_new_simple(path_parsed);
	

	if (filelist_read(dir_fd, &files, NULL))
		{
		GList *work;

		files = filelist_filter(files, FALSE);
		files = filelist_sort_path(files);

		work = files;
		while (work)
			{
			FileData *fd = work->data;
			if (!*path) *path = remove_level_from_path(fd->path);
			if (!*file) *file = g_strdup(fd->path);
			*list = g_list_prepend(*list, fd);

			work = work->next;
			}

		g_list_free(files);
		}

	g_free(path_parsed);
	file_data_unref(dir_fd);
}

static void parse_command_line_process_dir(const gchar *dir, gchar **path, gchar **file,
					   GList **list, gchar **first_dir)
{

	if (!*list && !*first_dir)
		{
		*first_dir = g_strdup(dir);
		}
	else
		{
		if (*first_dir)
			{
			parse_command_line_add_dir(*first_dir, path, file, list);
			g_free(*first_dir);
			*first_dir = NULL;
			}
		parse_command_line_add_dir(dir, path, file, list);
		}
}

static void parse_command_line_process_file(const gchar *file_path, gchar **path, gchar **file,
					    GList **list, GList **collection_list, gchar **first_dir)
{

	if (*first_dir)
		{
		parse_command_line_add_dir(*first_dir, path, file, list);
		g_free(*first_dir);
		*first_dir = NULL;
		}
	parse_command_line_add_file(file_path, path, file, list, collection_list);
}

static void parse_command_line(gint argc, gchar *argv[], gchar **path, gchar **file,
			       GList **cmd_list, GList **collection_list,
			       gchar **geometry)
{
	GList *list = NULL;
	GList *remote_list = NULL;
	GList *remote_errors = NULL;
	gint remote_do = FALSE;
	gchar *first_dir = NULL;

	if (argc > 1)
		{
		gint i;
		gchar *base_dir = get_current_dir();
		i = 1;
		while (i < argc)
			{
			const gchar *cmd_line = argv[i];
			gchar *cmd_all = g_build_filename(base_dir, cmd_line, NULL);

			if (cmd_line[0] == G_DIR_SEPARATOR && isdir(cmd_line))
				{
				parse_command_line_process_dir(cmd_line, path, file, &list, &first_dir);
				}
			else if (isdir(cmd_all))
				{
				parse_command_line_process_dir(cmd_all, path, file, &list, &first_dir);
				}
			else if (cmd_line[0] == G_DIR_SEPARATOR && isfile(cmd_line))
				{
				parse_command_line_process_file(cmd_line, path, file,
								&list, collection_list, &first_dir);
				}
			else if (isfile(cmd_all))
				{
				parse_command_line_process_file(cmd_all, path, file,
								&list, collection_list, &first_dir);
				}
			else if (strncmp(cmd_line, "--debug", 7) == 0 && (cmd_line[7] == '\0' || cmd_line[7] == '='))
				{
				/* do nothing but do not produce warnings */
				}
			else if (strcmp(cmd_line, "+t") == 0 ||
				 strcmp(cmd_line, "--with-tools") == 0)
				{
				options->layout.tools_float = FALSE;
				options->layout.tools_hidden = FALSE;

				remote_list = g_list_append(remote_list, "+t");
				}
			else if (strcmp(cmd_line, "-t") == 0 ||
				 strcmp(cmd_line, "--without-tools") == 0)
				{
				options->layout.tools_hidden = TRUE;

				remote_list = g_list_append(remote_list, "-t");
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
			else if (strcmp(cmd_line, "-l") == 0 ||
				 strcmp(cmd_line, "--list") == 0)
				{
				startup_command_line_collection = TRUE;
				}
			else if (strncmp(cmd_line, "--geometry=", 11) == 0)
				{
				if (!*geometry) *geometry = g_strdup(cmd_line + 11);
				}
			else if (strcmp(cmd_line, "-r") == 0 ||
				 strcmp(cmd_line, "--remote") == 0)
				{
				if (!remote_do)
					{
					remote_do = TRUE;
					remote_list = remote_build_list(remote_list, argc - i, &argv[i], &remote_errors);
					}
				}
			else if (strcmp(cmd_line, "-rh") == 0 ||
				 strcmp(cmd_line, "--remote-help") == 0)
				{
				remote_help();
				exit(0);
				}
			else if (strcmp(cmd_line, "--blank") == 0)
				{
				startup_blank = TRUE;
				}
			else if (strcmp(cmd_line, "-v") == 0 ||
				 strcmp(cmd_line, "--version") == 0)
				{
				printf_term("%s %s\n", GQ_APPNAME, VERSION);
				exit(0);
				}
			else if (strcmp(cmd_line, "--alternate") == 0)
				{
				/* enable faster experimental algorithm */
				log_printf("Alternate similarity algorithm enabled\n");
				image_sim_alternate_set(TRUE);
				}
			else if (strcmp(cmd_line, "-h") == 0 ||
				 strcmp(cmd_line, "--help") == 0)
				{
				printf_term("%s %s\n", GQ_APPNAME, VERSION);
				printf_term(_("Usage: %s [options] [path]\n\n"), GQ_APPNAME_LC);
				print_term(_("valid options are:\n"));
				print_term(_("  +t, --with-tools           force show of tools\n"));
				print_term(_("  -t, --without-tools        force hide of tools\n"));
				print_term(_("  -f, --fullscreen           start in full screen mode\n"));
				print_term(_("  -s, --slideshow            start in slideshow mode\n"));
				print_term(_("  -l, --list                 open collection window for command line\n"));
				print_term(_("      --geometry=GEOMETRY    set main window location\n"));
				print_term(_("  -r, --remote               send following commands to open window\n"));
				print_term(_("  -rh,--remote-help          print remote command list\n"));
#ifdef DEBUG
				print_term(_("  --debug[=level]            turn on debug output\n"));
#endif
				print_term(_("  -v, --version              print version info\n"));
				print_term(_("  -h, --help                 show this message\n\n"));

#if 0
				/* these options are not officially supported!
				 * only for testing new features, no need to translate them */
				print_term(  "  --alternate                use alternate similarity algorithm\n");
#endif

				exit(0);
				}
			else if (!remote_do)
				{
				printf_term(_("invalid or ignored: %s\nUse --help for options\n"), cmd_line);
				}

			g_free(cmd_all);
			i++;
			}
		g_free(base_dir);
		parse_out_relatives(*path);
		parse_out_relatives(*file);
		}

	list = g_list_reverse(list);

	if (!*path && first_dir)
		{
		*path = first_dir;
		first_dir = NULL;

		parse_out_relatives(*path);
		}
	g_free(first_dir);

	if (remote_do)
		{
		if (remote_errors)
			{
			GList *work = remote_errors;
			
			printf_term(_("Invalid or ignored remote options: "));
			while (work)
				{
				gchar *opt = work->data;
						
				printf_term("%s%s", (work == remote_errors) ? "" : ", ", opt);
				work = work->next;
				}

			printf_term(_("\nUse --remote-help for valid remote options.\n"));
			}

		remote_control(argv[0], remote_list, *path, list, *collection_list);
		}
	g_list_free(remote_list);

	if (list && list->next)
		{
		*cmd_list = list;
		}
	else
		{
		filelist_free(list);
		*cmd_list = NULL;
		}
}

static void parse_command_line_for_debug_option(gint argc, gchar *argv[])
{
#ifdef DEBUG
	const gchar *debug_option = "--debug";
	gint len = strlen(debug_option);

	if (argc > 1)
		{
		gint i;

		for (i = 1; i < argc; i++)
			{
			const gchar *cmd_line = argv[i];
			if (strncmp(cmd_line, debug_option, len) == 0)
				{
				gint cmd_line_len = strlen(cmd_line);

				/* we now increment the debug state for verbosity */
				if (cmd_line_len == len)
					debug_level_add(1);
				else if (cmd_line[len] == '=' && g_ascii_isdigit(cmd_line[len+1]))
					{
					gint n = atoi(cmd_line + len + 1);
					if (n < 0) n = 1;
					debug_level_add(n);
					}
				}
			}
		}

	DEBUG_1("debugging output enabled (level %d)", get_debug_level());
#endif
}

/*
 *-----------------------------------------------------------------------------
 * startup, init, and exit
 *-----------------------------------------------------------------------------
 */

#define RC_HISTORY_NAME "history"

static void keys_load(void)
{
	gchar *path;

	path = g_build_filename(homedir(), GQ_RC_DIR, RC_HISTORY_NAME, NULL);
	history_list_load(path);
	g_free(path);
}

static void keys_save(void)
{
	gchar *path;

	path = g_build_filename(homedir(), GQ_RC_DIR, RC_HISTORY_NAME, NULL);
	history_list_save(path);
	g_free(path);
}

static void check_for_home_path(gchar *path)
{
	gchar *buf;

	buf = g_build_filename(homedir(), path, NULL);
	if (!isdir(buf))
		{
		log_printf(_("Creating %s dir:%s\n"), GQ_APPNAME, buf);

		if (!mkdir_utf8(buf, 0755))
			{
			log_printf(_("Could not create dir:%s\n"), buf);
			}
		}
	g_free(buf);
}

static void accel_map_save(void)
{
	gchar *path;
	gchar *pathl;

	path = g_build_filename(homedir(), GQ_RC_DIR, "accels", NULL);
	pathl = path_from_utf8(path);
	gtk_accel_map_save(pathl);
	g_free(pathl);
	g_free(path);
}

static void accel_map_load(void)
{
	gchar *path;
	gchar *pathl;

	path = g_build_filename(homedir(), GQ_RC_DIR, "accels", NULL);
	pathl = path_from_utf8(path);
	gtk_accel_map_load(pathl);
	g_free(pathl);
	g_free(path);
}

static void gtkrc_load(void)
{
	gchar *path;
	gchar *pathl;

	/* If a gtkrc file exists in the rc directory, add it to the
	 * list of files to be parsed at the end of gtk_init() */
	path = g_build_filename(homedir(), GQ_RC_DIR, "gtkrc", NULL);
	pathl = path_from_utf8(path);
	if (access(pathl, R_OK) == 0)
		gtk_rc_add_default_file(pathl);
	g_free(pathl);
	g_free(path);
}

static void exit_program_final(void)
{
	LayoutWindow *lw = NULL;

	remote_close(remote_connection);

	collect_manager_flush();

	save_options(options);
	keys_save();
	accel_map_save();

	if (layout_valid(&lw))
		{
		layout_free(lw);
		}

	gtk_main_quit();
}

static GenericDialog *exit_dialog = NULL;

static void exit_confirm_cancel_cb(GenericDialog *gd, gpointer data)
{
	exit_dialog = NULL;
	generic_dialog_close(gd);
}

static void exit_confirm_exit_cb(GenericDialog *gd, gpointer data)
{
	exit_dialog = NULL;
	generic_dialog_close(gd);
	exit_program_final();
}

static gint exit_confirm_dlg(void)
{
	GtkWidget *parent;
	LayoutWindow *lw;
	gchar *msg;

	if (exit_dialog)
		{
		gtk_window_present(GTK_WINDOW(exit_dialog->dialog));
		return TRUE;
		}

	if (!collection_window_modified_exists()) return FALSE;

	parent = NULL;
	lw = NULL;
	if (layout_valid(&lw))
		{
		parent = lw->window;
		}

	msg = g_strdup_printf("%s - %s", GQ_APPNAME, _("exit"));
	exit_dialog = generic_dialog_new(msg,
				GQ_WMCLASS, "exit", parent, FALSE,
				exit_confirm_cancel_cb, NULL);
	g_free(msg);
	msg = g_strdup_printf(_("Quit %s"), GQ_APPNAME);
	generic_dialog_add_message(exit_dialog, GTK_STOCK_DIALOG_QUESTION,
				   msg, _("Collections have been modified. Quit anyway?"));
	g_free(msg);
	generic_dialog_add_button(exit_dialog, GTK_STOCK_QUIT, NULL, exit_confirm_exit_cb, TRUE);

	gtk_widget_show(exit_dialog->dialog);

	return TRUE;
}

void exit_program(void)
{
	layout_image_full_screen_stop(NULL);

	if (exit_confirm_dlg()) return;

	exit_program_final();
}

gint main(gint argc, gchar *argv[])
{
	LayoutWindow *lw;
	gchar *path = NULL;
	gchar *cmd_path = NULL;
	gchar *cmd_file = NULL;
	GList *cmd_list = NULL;
	GList *collection_list = NULL;
	CollectionData *first_collection = NULL;
	gchar *geometry = NULL;
	gchar *buf;
	CollectionData *cd = NULL;

#ifdef HAVE_GTHREAD
	g_thread_init (NULL);
	gdk_threads_init();
	gdk_threads_enter();
#endif
	
	/* init execution time counter (debug only) */
	init_exec_time();

	/* setup locale, i18n */
	gtk_set_locale();

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, GQ_LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif

	/* setup random seed for random slideshow */
	srand(time(NULL));

#if 1
	log_printf("%s %s, This is an alpha release.\n", GQ_APPNAME, VERSION);
#endif

	/* register global notify functions */
	file_data_register_notify_func(cache_notify_cb, NULL, NOTIFY_PRIORITY_HIGH);
	file_data_register_notify_func(thumb_notify_cb, NULL, NOTIFY_PRIORITY_HIGH);
	file_data_register_notify_func(collect_manager_notify_cb, NULL, NOTIFY_PRIORITY_LOW);

	parse_command_line_for_debug_option(argc, argv);

	options = init_options(NULL);
	setup_default_options(options);
	load_options(options);

	parse_command_line(argc, argv, &cmd_path, &cmd_file, &cmd_list, &collection_list, &geometry);

	gtkrc_load();
	gtk_init(&argc, &argv);

	if (gtk_major_version < GTK_MAJOR_VERSION ||
	    (gtk_major_version == GTK_MAJOR_VERSION && gtk_minor_version < GTK_MINOR_VERSION) )
		{
		log_printf("!!! This is a friendly warning.\n");
		log_printf("!!! The version of GTK+ in use now is older than when %s was compiled.\n", GQ_APPNAME);
		log_printf("!!!  compiled with GTK+-%d.%d\n", GTK_MAJOR_VERSION, GTK_MINOR_VERSION);
		log_printf("!!!   running with GTK+-%d.%d\n", gtk_major_version, gtk_minor_version);
		log_printf("!!! %s may quit unexpectedly with a relocation error.\n", GQ_APPNAME);
		}

	check_for_home_path(GQ_RC_DIR);
	check_for_home_path(GQ_RC_DIR_COLLECTIONS);
	check_for_home_path(GQ_CACHE_RC_THUMB);
	check_for_home_path(GQ_CACHE_RC_METADATA);

	keys_load();
	filter_add_defaults();
	filter_rebuild();

	accel_map_load();

	if (startup_blank)
		{
		g_free(cmd_path);
		cmd_path = NULL;
		g_free(cmd_file);
		cmd_file = NULL;
		filelist_free(cmd_list);
		cmd_list = NULL;
		string_list_free(collection_list);
		collection_list = NULL;

		path = NULL;
		}
	else if (cmd_path)
		{
		path = g_strdup(cmd_path);
		}
	else if (options->startup.restore_path && options->startup.path && isdir(options->startup.path))
		{
		path = g_strdup(options->startup.path);
		}
	else
		{
		path = get_current_dir();
		}

	lw = layout_new_with_geometry(NULL, options->layout.tools_float, options->layout.tools_hidden, geometry);
	layout_sort_set(lw, options->file_sort.method, options->file_sort.ascending);

	if (collection_list && !startup_command_line_collection)
		{
		GList *work;

		work = collection_list;
		while (work)
			{
			CollectWindow *cw;
			const gchar *path;

			path = work->data;
			work = work->next;

			cw = collection_window_new(path);
			if (!first_collection && cw) first_collection = cw->cd;
			}
		}

	if (cmd_list ||
	    (startup_command_line_collection && collection_list))
		{
		GList *work;

		if (startup_command_line_collection)
			{
			CollectWindow *cw;

			cw = collection_window_new("");
			cd = cw->cd;
			}
		else
			{
			cd = collection_new("");	/* if we pass NULL, untitled counter is falsely increm. */
			}

		g_free(cd->path);
		cd->path = NULL;
		g_free(cd->name);
		cd->name = g_strdup(_("Command line"));

		collection_path_changed(cd);

		work = cmd_list;
		while (work)
			{
			collection_add(cd, file_data_new_simple((gchar *)work->data), FALSE);
			work = work->next;
			}

		work = collection_list;
		while (work)
			{
			collection_load(cd, (gchar *)work->data, COLLECTION_LOAD_APPEND);
			work = work->next;
			}

		layout_set_path(lw, path);
		if (cd->list) layout_image_set_collection(lw, cd, cd->list->data);

		/* mem leak, we never unref this collection when !startup_command_line_collection
		 * (the image view of the main window does not hold a ref to the collection)
		 * this is sort of unavoidable, for if it did hold a ref, next/back
		 * may not work as expected when closing collection windows.
		 *
		 * collection_unref(cd);
		 */

		}
	else if (cmd_file)
		{
		layout_set_path(lw, cmd_file);
		}
	else
		{
		layout_set_path(lw, path);
		if (first_collection)
			{
			layout_image_set_collection(lw, first_collection,
						    collection_get_first(first_collection));
			}
		}

	image_osd_set(lw->image, options->image_overlay.common.state | (options->image_overlay.common.show_at_startup ? OSD_SHOW_INFO : OSD_SHOW_NOTHING));

	g_free(geometry);
	g_free(cmd_path);
	g_free(cmd_file);
	filelist_free(cmd_list);
	string_list_free(collection_list);
	g_free(path);

	if (startup_full_screen) layout_image_full_screen_start(lw);
	if (startup_in_slideshow) layout_image_slideshow_start(lw);

	buf = g_build_filename(homedir(), GQ_RC_DIR, ".command", NULL);
	remote_connection = remote_server_init(buf, cd);
	g_free(buf);
	
	gtk_main();
#ifdef HAVE_GTHREAD
	gdk_threads_leave();
#endif
	return 0;
}
