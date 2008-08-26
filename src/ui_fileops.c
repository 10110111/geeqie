/*
 * (SLIK) SimpLIstic sKin functions
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <dirent.h>
#include <utime.h>

#include <glib.h>
#include <gtk/gtk.h>	/* for locale warning dialog */

#include "main.h"
#include "ui_fileops.h"

#include "ui_utildlg.h"	/* for locale warning dialog */

/*
 *-----------------------------------------------------------------------------
 * generic file information and manipulation routines (public)
 *-----------------------------------------------------------------------------
 */



void print_term(const gchar *text_utf8)
{
	gchar *text_l;

	text_l = g_locale_from_utf8(text_utf8, -1, NULL, NULL, NULL);
	fputs((text_l) ? text_l : text_utf8, stdout);
	g_free(text_l);
}

static void encoding_dialog(const gchar *path);

static gint encoding_dialog_idle(gpointer data)
{
	gchar *path = data;

	encoding_dialog(path);
	g_free(path);

	return FALSE;
}

static gint encoding_dialog_delay(gpointer data)
{
	g_idle_add(encoding_dialog_idle, data);

	return 0;
}

static void encoding_dialog(const gchar *path)
{
	static gint warned_user = FALSE;
	GenericDialog *gd;
	GString *string;
	const gchar *lc;
	const gchar *bf;

	/* check that gtk is initialized (loop is level > 0) */
	if (gtk_main_level() == 0)
		{
		/* gtk not initialized */
		gtk_init_add(encoding_dialog_delay, g_strdup(path));
		return;
		}

	if (warned_user) return;
	warned_user = TRUE;

	lc = getenv("LANG");
	bf = getenv("G_BROKEN_FILENAMES");

	string = g_string_new("");
	g_string_append(string, _("One or more filenames are not encoded with the preferred locale character set.\n"));
	g_string_append_printf(string, _("Operations on, and display of these files with %s may not succeed.\n"), PACKAGE);
	g_string_append(string, "\n");
	g_string_append(string, _("If your filenames are not encoded in utf-8, try setting the environment variable G_BROKEN_FILENAMES=1\n"));
	if (bf)
		g_string_append_printf(string, _("It appears G_BROKEN_FILENAMES is set to %s\n"), bf);
	else
		g_string_append(string, _("It appears G_BROKEN_FILENAMES is not set\n"));
	g_string_append(string, "\n");
	g_string_append_printf(string, _("The locale appears to be set to \"%s\"\n(set by the LANG environment variable)\n"), (lc) ? lc : "undefined");
	if (lc && (strstr(lc, "UTF-8") || strstr(lc, "utf-8")))
		{
		gchar *name;
		name = g_convert(path, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
		string = g_string_append(string, _("\nPreferred encoding appears to be UTF-8, however the file:\n"));
		g_string_append_printf(string, "\"%s\"\n", (name) ? name : _("[name not displayable]"));
		
		if (g_utf8_validate(path, -1, NULL))
			g_string_append_printf(string, _("\"%s\" is encoded in valid UTF-8."), (name) ? name : _("[name not displayable]"));
		else
			g_string_append_printf(string, _("\"%s\" is not encoded in valid UTF-8."), (name) ? name : _("[name not displayable]"));
		g_string_append(string, "\n");
		g_free(name);
		}

	gd = generic_dialog_new(_("Filename encoding locale mismatch"),
				GQ_WMCLASS, "locale warning", NULL, TRUE, NULL, NULL);
	generic_dialog_add_button(gd, GTK_STOCK_CLOSE, NULL, NULL, TRUE);

	generic_dialog_add_message(gd, GTK_STOCK_DIALOG_WARNING,
				   _("Filename encoding locale mismatch"), string->str);

	gtk_widget_show(gd->dialog);

	g_string_free(string, TRUE);
}

gchar *path_to_utf8(const gchar *path)
{
	gchar *utf8;
	GError *error = NULL;

	if (!path) return NULL;

	utf8 = g_filename_to_utf8(path, -1, NULL, NULL, &error);
	if (error)
		{
		log_printf("Unable to convert filename to UTF-8:\n%s\n%s\n", path, error->message);
		g_error_free(error);
		encoding_dialog(path);
		}
	if (!utf8)
		{
		/* just let it through, but bad things may happen */
		utf8 = g_strdup(path);
		}

	return utf8;
}

gchar *path_from_utf8(const gchar *utf8)
{
	gchar *path;
	GError *error = NULL;

	if (!utf8) return NULL;

	path = g_filename_from_utf8(utf8, -1, NULL, NULL, &error);
	if (error)
		{
		log_printf("Unable to convert filename to locale from UTF-8:\n%s\n%s\n", utf8, error->message);
		g_error_free(error);
		}
	if (!path)
		{
		/* if invalid UTF-8, text probaby still in original form, so just copy it */
		path = g_strdup(utf8);
		}

	return path;
}

/* first we try the HOME environment var, if that doesn't work, we try g_get_homedir(). */
const gchar *homedir(void)
{
	static gchar *home = NULL;

	if (!home)
		home = path_to_utf8(getenv("HOME"));
	
	if (!home)
		home = path_to_utf8(g_get_home_dir());

	return home;
}

gint stat_utf8(const gchar *s, struct stat *st)
{
	gchar *sl;
	gint ret;

	if (!s) return FALSE;
	sl = path_from_utf8(s);
	ret = (stat(sl, st) == 0);
	g_free(sl);

	return ret;
}

gint lstat_utf8(const gchar *s, struct stat *st)
{
	gchar *sl;
	gint ret;

	if (!s) return FALSE;
	sl = path_from_utf8(s);
	ret = (lstat(sl, st) == 0);
	g_free(sl);

	return ret;
}

gint isname(const gchar *s)
{
	struct stat st;

	return stat_utf8(s, &st);
}

gint isfile(const gchar *s)
{
	struct stat st;

	return (stat_utf8(s, &st) && S_ISREG(st.st_mode));
}

gint isdir(const gchar *s)
{
	struct stat st;

	return (stat_utf8(s ,&st) && S_ISDIR(st.st_mode));
}

gint islink(const gchar *s)
{
	struct stat st;

	return (lstat_utf8(s ,&st) && S_ISLNK(st.st_mode));
}

gint64 filesize(const gchar *s)
{
	struct stat st;

	if (!stat_utf8(s, &st)) return 0;
	return st.st_size;
}

time_t filetime(const gchar *s)
{
	struct stat st;

	if (!stat_utf8(s, &st)) return 0;
	return st.st_mtime;
}

gint filetime_set(const gchar *s, time_t tval)
{
	gint ret = FALSE;

	if (tval > 0)
		{
		struct utimbuf ut;
		gchar *sl;

		ut.actime = ut.modtime = tval;

		sl = path_from_utf8(s);
		ret = (utime(sl, &ut) == 0);
		g_free(sl);
		}

	return ret;
}

gint access_file(const gchar *s, gint mode)
{
	gchar *sl;
	gint ret;

	if (!s) return FALSE;

	sl = path_from_utf8(s);
	ret = (access(sl, mode) == 0);
	g_free(sl);

	return ret;
}

gint unlink_file(const gchar *s)
{
	gchar *sl;
	gint ret;

	if (!s) return FALSE;

	sl = path_from_utf8(s);
	ret = (unlink(sl) == 0);
	g_free(sl);

	return ret;
}

gint symlink_utf8(const gchar *source, const gchar *target)
{
	gchar *sl;
	gchar *tl;
	gint ret;

	if (!source || !target) return FALSE;

	sl = path_from_utf8(source);
	tl = path_from_utf8(target);

	ret = (symlink(sl, tl) == 0);

	g_free(sl);
	g_free(tl);

	return ret;
}

gint mkdir_utf8(const gchar *s, gint mode)
{
	gchar *sl;
	gint ret;

	if (!s) return FALSE;

	sl = path_from_utf8(s);
	ret = (mkdir(sl, mode) == 0);
	g_free(sl);
	return ret;
}

gint rmdir_utf8(const gchar *s)
{
	gchar *sl;
	gint ret;

	if (!s) return FALSE;

	sl = path_from_utf8(s);
	ret = (rmdir(sl) == 0);
	g_free(sl);

	return ret;
}

gint copy_file_attributes(const gchar *s, const gchar *t, gint perms, gint mtime)
{
	struct stat st;
	gchar *sl, *tl;
	gint ret = FALSE;

	if (!s || !t) return FALSE;

	sl = path_from_utf8(s);
	tl = path_from_utf8(t);

	if (stat(sl, &st) == 0)
		{
		struct utimbuf tb;

		ret = TRUE;

		/* set the dest file attributes to that of source (ignoring errors) */

		if (perms && chown(tl, st.st_uid, st.st_gid) < 0) ret = FALSE;
		if (perms && chmod(tl, st.st_mode) < 0) ret = FALSE;

		tb.actime = st.st_atime;
		tb.modtime = st.st_mtime;
		if (mtime && utime(tl, &tb) < 0) ret = FALSE;
		}

	g_free(sl);
	g_free(tl);

	return ret;
}

/* paths are in filesystem encoding */
static gint hard_linked(const gchar *a, const gchar *b)
{
	struct stat sta;
	struct stat stb;

	if (stat(a, &sta) !=  0 || stat(b, &stb) != 0) return FALSE;

	return (sta.st_dev == stb.st_dev &&
		sta.st_ino == stb.st_ino);
}

gint copy_file(const gchar *s, const gchar *t)
{
	FILE *fi = NULL;
	FILE *fo = NULL;
	gchar *sl, *tl;
	gchar buf[4096];
	size_t b;

	sl = path_from_utf8(s);
	tl = path_from_utf8(t);

	if (hard_linked(sl, tl))
		{
		g_free(sl);
		g_free(tl);
		return TRUE;
		}

	fi = fopen(sl, "rb");
	if (fi)
		{
		fo = fopen(tl, "wb");
		if (!fo)
			{
			fclose(fi);
			fi = NULL;
			}
		}

	g_free(sl);
	g_free(tl);

	if (!fi || !fo) return FALSE;

	while ((b = fread(buf, sizeof(gchar), sizeof(buf), fi)) && b != 0)
		{
		if (fwrite(buf, sizeof(gchar), b, fo) != b)
			{
			fclose(fi);
			fclose(fo);
			return FALSE;
			}
		}

	fclose(fi);
	fclose(fo);

	copy_file_attributes(s, t, TRUE, TRUE);

	return TRUE;
}

gint move_file(const gchar *s, const gchar *t)
{
	gchar *sl, *tl;
	gint ret = TRUE;

	if (!s || !t) return FALSE;

	sl = path_from_utf8(s);
	tl = path_from_utf8(t);
	if (rename(sl, tl) < 0)
		{
		/* this may have failed because moving a file across filesystems
		was attempted, so try copy and delete instead */
		if (copy_file(s, t))
			{
			if (unlink(sl) < 0)
				{
				/* err, now we can't delete the source file so return FALSE */
				ret = FALSE;
				}
			}
		else
			{
			ret = FALSE;
			}
		}
	g_free(sl);
	g_free(tl);

	return ret;
}

gint rename_file(const gchar *s, const gchar *t)
{
	gchar *sl, *tl;
	gint ret;

	if (!s || !t) return FALSE;

	sl = path_from_utf8(s);
	tl = path_from_utf8(t);
	ret = (rename(sl, tl) == 0);
	g_free(sl);
	g_free(tl);

	return ret;
}

gchar *get_current_dir(void)
{
	gchar *pathl;
	gchar *path8;

	pathl = g_get_current_dir();
	path8 = path_to_utf8(pathl);
	g_free(pathl);

	return path8;
}

void string_list_free(GList *list)
{
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

GList *string_list_copy(GList *list)
{
	GList *new_list = NULL;
	GList *work;

	work = list;
	while (work)
		{
		gchar *path;

		path = work->data;
		work = work->next;

		new_list = g_list_prepend(new_list, g_strdup(path));
		}

	return g_list_reverse(new_list);
}

gchar *unique_filename(const gchar *path, const gchar *ext, const gchar *divider, gint pad)
{
	gchar *unique;
	gint n = 1;

	if (!ext) ext = "";
	if (!divider) divider = "";

	unique = g_strconcat(path, ext, NULL);
	while (isname(unique))
		{
		g_free(unique);
		if (pad)
			{
			unique = g_strdup_printf("%s%s%03d%s", path, divider, n, ext);
			}
		else
			{
			unique = g_strdup_printf("%s%s%d%s", path, divider, n, ext);
			}
		n++;
		if (n > 999)
			{
			/* well, we tried */
			g_free(unique);
			return NULL;
			}
		}

	return unique;
}

gchar *unique_filename_simple(const gchar *path)
{
	gchar *unique;
	const gchar *name;
	const gchar *ext;

	if (!path) return NULL;

	name = filename_from_path(path);
	if (!name) return NULL;

	ext = extension_from_path(name);

	if (!ext)
		{
		unique = unique_filename(path, NULL, "_", TRUE);
		}
	else
		{
		gchar *base;

		base = remove_extension_from_path(path);
		unique = unique_filename(base, ext, "_", TRUE);
		g_free(base);
		}

	return unique;
}

const gchar *filename_from_path(const gchar *path)
{
	const gchar *base;

	if (!path) return NULL;

	base = strrchr(path, G_DIR_SEPARATOR);
	if (base) return base + 1;

	return path;
}

gchar *remove_level_from_path(const gchar *path)
{
	gint p = 0, n = -1;

	if (!path) return NULL;

	while (path[p])
		{
		if (path[p] == G_DIR_SEPARATOR) n = p;
		p++;
		}
	if (n <= 0) n++;

	return g_strndup(path, (gsize) n);
}

const gchar *extension_from_path(const gchar *path)
{
	if (!path) return NULL;
	return strrchr(path, '.');
}

gint file_extension_match(const gchar *path, const gchar *ext)
{
	gint p;
	gint e;

	if (!path) return FALSE;
	if (!ext) return TRUE;

	p = strlen(path);
	e = strlen(ext);

	/* FIXME: utf8 */
	return (p > e && strncasecmp(path + p - e, ext, e) == 0);
}

gchar *remove_extension_from_path(const gchar *path)
{
	gint p = 0, n = -1;

	if (!path) return NULL;

	while (path[p])
		{
		if (path[p] == '.') n = p;
		p++;
		}
	if (n < 0) n = p;

	return g_strndup(path, (gsize) n);
}

void parse_out_relatives(gchar *path)
{
	gint s, t;

	if (!path) return;

	s = t = 0;

	while (path[s] != '\0')
		{
		if (path[s] == G_DIR_SEPARATOR && path[s+1] == '.')
			{
			/* /. occurence, let's see more */
			gint p = s + 2;

			if (path[p] == G_DIR_SEPARATOR || path[p] == '\0')
				{
				/* /./ or /., just skip this part */
				s = p;
				continue;
				}
			else if (path[p] == '.' && (path[p+1] == G_DIR_SEPARATOR || path[p+1] == '\0'))
				{
				/* /../ or /.., remove previous part, ie. /a/b/../ becomes /a/ */
				s = p + 1;
				if (t > 0) t--;
				while (path[t] != G_DIR_SEPARATOR && t > 0) t--;
				continue;
				}
			}
	
		if (s != t) path[t] = path[s];
		t++;
		s++;
		}

	if (t == 0 && path[t] == G_DIR_SEPARATOR) t++;
	if (t > 1 && path[t-1] == G_DIR_SEPARATOR) t--;
	path[t] = '\0';
}

gint file_in_path(const gchar *name)
{
	gchar *path;
	gchar *namel;
	gint p, l;
	gint ret = FALSE;

	if (!name) return FALSE;
	path = g_strdup(getenv("PATH"));
	if (!path) return FALSE;
	namel = path_from_utf8(name);

	p = 0;
	l = strlen(path);
	while (p < l && !ret)
		{
		gchar *f;
		gint e = p;
		while (path[e] != ':' && path[e] != '\0') e++;
		path[e] = '\0';
		e++;
		f = g_build_filename(path + p, namel, NULL);
		if (isfile(f)) ret = TRUE;
		g_free(f);
		p = e;
		}
	g_free(namel);
	g_free(path);

	return ret;
}
