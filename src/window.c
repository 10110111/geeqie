/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik / Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "window.h"

#include "pixbuf_util.h"
#include "ui_fileops.h"
#include "ui_help.h"

GtkWidget *window_new(GtkWindowType type, const gchar *name, const gchar *icon,
		      const gchar *icon_file, const gchar *subtitle)
{
	gchar *title;
	GtkWidget *window;

	window = gtk_window_new(type);
	if (!window) return NULL;

	if (subtitle)
		{
		title = g_strdup_printf("%s - %s", subtitle, GQ_APPNAME);
		}
	else
		{
		title = g_strdup_printf("%s", GQ_APPNAME);
		}

	gtk_window_set_title(GTK_WINDOW(window), title);
	g_free(title);

	window_set_icon(window, icon, icon_file);
	gtk_window_set_role(GTK_WINDOW(window), name);
	gtk_window_set_wmclass(GTK_WINDOW(window), name, GQ_WMCLASS);

	return window;
}

void window_set_icon(GtkWidget *window, const gchar *icon, const gchar *file)
{
	if (!icon && !file) icon = PIXBUF_INLINE_ICON;

	if (icon)
		{
		GdkPixbuf *pixbuf;

		pixbuf = pixbuf_inline(icon);
		if (pixbuf)
			{
			gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
			g_object_unref(pixbuf);
			}
		}
	else
		{
		gtk_window_set_icon_from_file(GTK_WINDOW(window), file, NULL);
		}
}

gint window_maximized(GtkWidget *window)
{
	GdkWindowState state;

	if (!window || !window->window) return FALSE;

	state = gdk_window_get_state(window->window);
	return (state & GDK_WINDOW_STATE_MAXIMIZED);
}

/*
 *-----------------------------------------------------------------------------
 * Open browser with the help Documentation
 *-----------------------------------------------------------------------------
 */

static gchar *command_result(const gchar *binary, const gchar *command)
{
	gchar *result = NULL;
	FILE *f;
	gchar buf[2048];
	gint l;

	if (!binary || binary[0] == '\0') return NULL;
	if (!file_in_path(binary)) return NULL;

	if (!command || command[0] == '\0') return g_strdup(binary);
	if (command[0] == '!') return g_strdup(command + 1);

	f = popen(command, "r");
	if (!f) return NULL;

	while ((l = fread(buf, sizeof(gchar), sizeof(buf), f)) > 0)
		{
		if (!result)
			{
			gint n = 0;

			while (n < l && buf[n] != '\n' && buf[n] != '\r') n++;
			if (n > 0) result = g_strndup(buf, n);
			}
		}

	pclose(f);

	return result;
}

static void help_browser_command(const gchar *command, const gchar *path)
{
	gchar *result;
	gchar *buf;
	gchar *begin;
	gchar *end;

	if (!command || !path) return;

	DEBUG_1("Help command pre \"%s\", \"%s\"", command, path);

	buf = g_strdup(command);
	begin = strstr(buf, "%s");
	if (begin)
		{
		*begin = '\0';
		end = begin + 2;
		begin = buf;

		result = g_strdup_printf("%s%s%s &", begin, path, end);
		}
	else
		{
		result = g_strdup_printf("%s \"%s\" &", command, path);
		}
	g_free(buf);

	DEBUG_1("Help command post [%s]", result);

	system(result);

	g_free(result);
}

/*
 * each set of 2 strings is one browser:
 *   the 1st is the binary to look for in the path
 *   the 2nd has 3 capabilities:
 *        NULL     exec binary with html file path as command line
 *        string   exec string and use results for command line
 *        !string  use text following ! as command line, replacing optional %s with html file path
*/
static gchar *html_browsers[] =
{
	/* Our specific script */
	GQ_APPNAME_LC "_html_browser", NULL,
	/* Redhat has a nifty htmlview script to start the user's preferred browser */
	"htmlview",	NULL,
	/* Debian has even better approach with alternatives */
	"sensible-browser", NULL,
	/* GNOME 2 */
	"gconftool-2",	"gconftool-2 -g /desktop/gnome/url-handlers/http/command",
	/* KDE */
	"kfmclient",	"!kfmclient exec \"%s\"",
	/* use fallbacks */
	"firefox",	NULL,
	"mozilla",	NULL,
	"konqueror",	NULL,
	"netscape",	NULL,
	NULL,		NULL
};

static void help_browser_run(void)
{
	gchar *path;
	gchar *result;
	gint i;

	result = command_result(options->helpers.html_browser.command_name, options->helpers.html_browser.command_line);

	i = 0;
	while (!result && html_browsers[i])
		{
		result = command_result(html_browsers[i], html_browsers[i+1]);
		i += 2;
		}

	if (!result)
		{
		log_printf("Unable to detect an installed browser.\n");
		return;
		}

	path = g_build_filename(options->documentation.htmldir, "index.html", NULL);
	help_browser_command(result, path);
	g_free(path);

	g_free(result);
}

/*
 *-----------------------------------------------------------------------------
 * help window
 *-----------------------------------------------------------------------------
 */

static GtkWidget *help_window = NULL;

static void help_window_destroy_cb(GtkWidget *window, gpointer data)
{
	help_window = NULL;
}

void help_window_show(const gchar *key)
{
	gchar *path;

	if (key && strcmp(key, "html_contents") == 0)
		{
		help_browser_run();
		return;
		}

	if (help_window)
		{
		gtk_window_present(GTK_WINDOW(help_window));
		if (key) help_window_set_key(help_window, key);
		return;
		}

	path = g_build_filename(options->documentation.helpdir, "README", NULL);
	help_window = help_window_new(_("Help"), GQ_WMCLASS, "help", path, key);
	g_free(path);

	g_signal_connect(G_OBJECT(help_window), "destroy",
			 G_CALLBACK(help_window_destroy_cb), NULL);
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
