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


#include "main.h"
#include "editors.h"

#include "filedata.h"
#include "filefilter.h"
#include "misc.h"
#include "ui_fileops.h"
#include "ui_spinner.h"
#include "ui_utildlg.h"
#include "utilops.h"

#include <errno.h>


#define EDITOR_WINDOW_WIDTH 500
#define EDITOR_WINDOW_HEIGHT 300



typedef struct _EditorVerboseData EditorVerboseData;
struct _EditorVerboseData {
	GenericDialog *gd;
	GtkWidget *button_close;
	GtkWidget *button_stop;
	GtkWidget *text;
	GtkWidget *progress;
	GtkWidget *spinner;
};

typedef struct _EditorData EditorData;
struct _EditorData {
	EditorFlags flags;
	GPid pid;
	GList *list;
	gint count;
	gint total;
	gboolean stopping;
	EditorVerboseData *vd;
	EditorCallback callback;
	gpointer data;
	const EditorDescription *editor;
};


static void editor_verbose_window_progress(EditorData *ed, const gchar *text);
static EditorFlags editor_command_next_start(EditorData *ed);
static EditorFlags editor_command_next_finish(EditorData *ed, gint status);
static EditorFlags editor_command_done(EditorData *ed);

/*
 *-----------------------------------------------------------------------------
 * external editor routines
 *-----------------------------------------------------------------------------
 */

GHashTable *editors = NULL;

#ifdef G_KEY_FILE_DESKTOP_GROUP
#define DESKTOP_GROUP G_KEY_FILE_DESKTOP_GROUP
#else
#define DESKTOP_GROUP "Desktop Entry"
#endif

void editor_description_free(EditorDescription *editor)
{
	if (!editor) return;
	
	g_free(editor->key);
	g_free(editor->name);
	g_free(editor->icon);
	g_free(editor->exec);
	g_free(editor->menu_path);
	g_free(editor->hotkey);
	string_list_free(editor->ext_list);
	g_free(editor->file);
	g_free(editor);
}

static GList *editor_mime_types_to_extensions(gchar **mime_types)
{
	/* FIXME: this should be rewritten to use the shared mime database, as soon as we switch to gio */
	
	static const gchar *conv_table[][2] = {
		{"application/x-ufraw",	"%raw"},
		{"image/*",		"*"},
		{"image/bmp",		".bmp"},
		{"image/gif",		".gif"},
		{"image/jpeg",		".jpeg;.jpg"},
		{"image/jpg",		".jpg;.jpeg"},
		{"image/pcx",		".pcx"},
		{"image/png",		".png"},
		{"image/svg",		".svg"},
		{"image/svg+xml",	".svg"},
		{"image/svg+xml-compressed", 	".svg"},	
		{"image/tiff",		".tiff;.tif"},
		{"image/x-bmp",		".bmp"},
		{"image/x-canon-crw",	".crw"},
		{"image/x-cr2",		".cr2"},
		{"image/x-dcraw",	"%raw"},
		{"image/x-ico",		".ico"},
		{"image/x-mrw",		".mrw"},
		{"image/x-MS-bmp",	".bmp"},
		{"image/x-nef",		".nef"},
		{"image/x-orf",		".orf"},
		{"image/x-pcx",		".pcx"},
		{"image/xpm",		".xpm"},
		{"image/x-png",		".png"},
		{"image/x-portable-anymap",	".pam"},	
		{"image/x-portable-bitmap",	".pbm"},
		{"image/x-portable-graymap",	".pgm"},
		{"image/x-portable-pixmap",	".ppm"},
		{"image/x-psd",		".psd"},
		{"image/x-raf",		".raf"},
		{"image/x-sgi",		".sgi"},
		{"image/x-tga",		".tga"},
		{"image/x-xbitmap",	".xbm"},
		{"image/x-xcf",		".xcf"},
		{"image/x-xpixmap",	".xpm"},
		{"image/x-x3f",		".x3f"},
		{NULL, NULL}};
	
	gint i, j;
	GList *list = NULL;
	
	for (i = 0; mime_types[i]; i++) 
		for (j = 0; conv_table[j][0]; j++)
			if (strcmp(mime_types[i], conv_table[j][0]) == 0)
				list = g_list_concat(list, filter_to_list(conv_table[j][1]));
	
	return list;
}

static gboolean editor_read_desktop_file(const gchar *path)
{
	GKeyFile *key_file;
	EditorDescription *editor;
	gchar *extensions;
	gchar *type;
	const gchar *key = filename_from_path(path);
	gchar **categories, **only_show_in, **not_show_in;
	gchar *try_exec;

	if (g_hash_table_lookup(editors, key)) return FALSE; /* the file found earlier wins */
	
	key_file = g_key_file_new();
	if (!g_key_file_load_from_file(key_file, path, 0, NULL))
		{
		g_key_file_free(key_file);
		return FALSE;
		}

	type = g_key_file_get_string(key_file, DESKTOP_GROUP, "Type", NULL);
	if (!type || strcmp(type, "Application") != 0)
		{
		/* We only consider desktop entries of Application type */
		return FALSE;
		}
	
	editor = g_new0(EditorDescription, 1);
	
	editor->key = g_strdup(key);
	editor->file = g_strdup(path);

	g_hash_table_insert(editors, editor->key, editor);

	if (g_key_file_get_boolean(key_file, DESKTOP_GROUP, "Hidden", NULL)
	    || g_key_file_get_boolean(key_file, DESKTOP_GROUP, "NoDisplay", NULL))
	    	{
	    	editor->hidden = TRUE;
		}

	categories = g_key_file_get_string_list(key_file, DESKTOP_GROUP, "Categories", NULL, NULL);
	if (categories)
		{
		gboolean found = FALSE;
		gint i;
		for (i = 0; categories[i]; i++) 
			/* IMHO "Graphics" is exactly the category that we are interested in, so this does not have to be configurable */
			if (strcmp(categories[i], "Graphics") == 0 ||
			    strcmp(categories[i], "X-Geeqie") == 0) 
				{
				found = TRUE;
				break;
				}
		if (!found) editor->hidden = TRUE;
		g_strfreev(categories);
		}
	else
		{
		editor->hidden = TRUE;
		}

	only_show_in = g_key_file_get_string_list(key_file, DESKTOP_GROUP, "OnlyShowIn", NULL, NULL);
	if (only_show_in)
		{
		gboolean found = FALSE;
		gint i;
		for (i = 0; only_show_in[i]; i++) 
			if (strcmp(only_show_in[i], "X-Geeqie") == 0)
				{
				found = TRUE;
				break;
				}
		if (!found) editor->hidden = TRUE;
		g_strfreev(only_show_in);
		}

	not_show_in = g_key_file_get_string_list(key_file, DESKTOP_GROUP, "NotShowIn", NULL, NULL);
	if (not_show_in)
		{
		gboolean found = FALSE;
		gint i;
		for (i = 0; not_show_in[i]; i++) 
			if (strcmp(not_show_in[i], "X-Geeqie") == 0)
				{
				found = TRUE;
				break;
				}
		if (found) editor->hidden = TRUE;
		g_strfreev(not_show_in);
		}
		
		
	try_exec = g_key_file_get_string(key_file, DESKTOP_GROUP, "TryExec", NULL);
	if (try_exec && !editor->hidden)
		{
		gchar *try_exec_res = g_find_program_in_path(try_exec);
		if (!try_exec_res) editor->hidden = TRUE;
		g_free(try_exec_res);
		g_free(try_exec);
		}
		
	if (editor->hidden) 
		{
		/* hidden editors will be deleted, no need to parse the rest */
		g_key_file_free(key_file);
		return TRUE;
		}
	
	editor->name = g_key_file_get_locale_string(key_file, DESKTOP_GROUP, "Name", NULL, NULL);
	editor->icon = g_key_file_get_string(key_file, DESKTOP_GROUP, "Icon", NULL);

	editor->exec = g_key_file_get_string(key_file, DESKTOP_GROUP, "Exec", NULL);
	
	/* we take only editors that accept parameters, FIXME: the test can be improved */
	if (!strchr(editor->exec, '%')) editor->hidden = TRUE; 
	
	editor->menu_path = g_key_file_get_string(key_file, DESKTOP_GROUP, "X-Geeqie-Menu-Path", NULL);
	if (!editor->menu_path) editor->menu_path = g_strdup("EditMenu/ExternalMenu");
	
	editor->hotkey = g_key_file_get_string(key_file, DESKTOP_GROUP, "X-Geeqie-Hotkey", NULL);

	extensions = g_key_file_get_string(key_file, DESKTOP_GROUP, "X-Geeqie-File-Extensions", NULL);
	if (extensions)
		editor->ext_list = filter_to_list(extensions);
	else
		{
		gchar **mime_types = g_key_file_get_string_list(key_file, DESKTOP_GROUP, "MimeType", NULL, NULL);
		if (mime_types)
			{
			editor->ext_list = editor_mime_types_to_extensions(mime_types);
			g_strfreev(mime_types);
			if (!editor->ext_list) editor->hidden = TRUE; 
			}
		}
		
	if (g_key_file_get_boolean(key_file, DESKTOP_GROUP, "X-Geeqie-Keep-Fullscreen", NULL)) editor->flags |= EDITOR_KEEP_FS;
	if (g_key_file_get_boolean(key_file, DESKTOP_GROUP, "X-Geeqie-Verbose", NULL)) editor->flags |= EDITOR_VERBOSE;
	if (g_key_file_get_boolean(key_file, DESKTOP_GROUP, "X-Geeqie-Verbose-Multi", NULL)) editor->flags |= EDITOR_VERBOSE_MULTI;
	if (g_key_file_get_boolean(key_file, DESKTOP_GROUP, "X-Geeqie-Filter", NULL)) editor->flags |= EDITOR_DEST;
	if (g_key_file_get_boolean(key_file, DESKTOP_GROUP, "Terminal", NULL)) editor->flags |= EDITOR_TERMINAL;
	
	editor->flags |= editor_command_parse(editor, NULL, NULL);
	g_key_file_free(key_file);
	
	return TRUE;	
}

static gboolean editor_remove_desktop_file_cb(gpointer key, gpointer value, gpointer user_data)
{
	EditorDescription *editor = value;
	return editor->hidden;
}

static void editor_read_desktop_dir(const gchar *path)
{
	DIR *dp;
	struct dirent *dir;
	gchar *pathl;

	pathl = path_from_utf8(path);
	dp = opendir(pathl);
	g_free(pathl);
	if (!dp)
		{
		/* dir not found */
		return;
		}
	while ((dir = readdir(dp)) != NULL)
		{
		gchar *namel = dir->d_name;
		
		if (g_str_has_suffix(namel, ".desktop"))
			{
			gchar *name = path_to_utf8(namel);
			gchar *dpath = g_build_filename(path, name, NULL);
			editor_read_desktop_file(dpath);
			g_free(dpath);
			g_free(name);
			}	
		}
	closedir(dp);
}

void editor_load_descriptions(void)
{
	gchar *path;
	gchar *xdg_data_dirs;
	gchar *all_dirs;
	gchar **split_dirs;
	gint i;
	
	if (!editors)
		{
		editors = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)editor_description_free);
		}

	xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs && xdg_data_dirs[0])
		xdg_data_dirs = path_to_utf8(xdg_data_dirs);
	else
		xdg_data_dirs = g_strdup("/usr/share");
	
	all_dirs = g_strconcat(get_rc_dir(), ":", GQ_APP_DIR, ":", xdg_data_home_get(), ":", xdg_data_dirs, NULL);
	
	g_free(xdg_data_dirs);

	split_dirs = g_strsplit(all_dirs, ":", 0);
	
	g_free(all_dirs);

	for (i = 0; split_dirs[i]; i++)
		{
		path = g_build_filename(split_dirs[i], "applications", NULL);
		editor_read_desktop_dir(path);
		g_free(path);
		}
		
	g_strfreev(split_dirs);
	
	g_hash_table_foreach_remove(editors, editor_remove_desktop_file_cb, NULL);
}

static void editor_list_add_cb(gpointer key, gpointer value, gpointer data)
{
	GList **listp = data;
	EditorDescription *editor = value;
	
	/* do not show the special commands in any list, they are called explicitly */ 
	if (strcmp(editor->key, CMD_COPY) == 0 ||
	    strcmp(editor->key, CMD_MOVE) == 0 ||  
	    strcmp(editor->key, CMD_RENAME) == 0 ||
	    strcmp(editor->key, CMD_DELETE) == 0 ||
	    strcmp(editor->key, CMD_FOLDER) == 0) return;

	*listp = g_list_prepend(*listp, editor);
}

static gint editor_sort(gconstpointer a, gconstpointer b)
{
	const EditorDescription *ea = a;
	const EditorDescription *eb = b;
	gint ret;
	
	ret = strcmp(ea->menu_path, eb->menu_path);
	if (ret != 0) return ret;
	
	return g_utf8_collate(ea->name, eb->name);
}

GList *editor_list_get(void)
{
	GList *editors_list = NULL;
	g_hash_table_foreach(editors, editor_list_add_cb, &editors_list);
	editors_list = g_list_sort(editors_list, editor_sort);

	return editors_list;
}

/* ------------------------------ */


static void editor_verbose_data_free(EditorData *ed)
{
	if (!ed->vd) return;
	g_free(ed->vd);
	ed->vd = NULL;
}

static void editor_data_free(EditorData *ed)
{
	editor_verbose_data_free(ed);
	g_free(ed);
}

static void editor_verbose_window_close(GenericDialog *gd, gpointer data)
{
	EditorData *ed = data;

	generic_dialog_close(gd);
	editor_verbose_data_free(ed);
	if (ed->pid == -1) editor_data_free(ed); /* the process has already terminated */
}

static void editor_verbose_window_stop(GenericDialog *gd, gpointer data)
{
	EditorData *ed = data;
	ed->stopping = TRUE;
	ed->count = 0;
	editor_verbose_window_progress(ed, _("stopping..."));
}

static void editor_verbose_window_enable_close(EditorVerboseData *vd)
{
	vd->gd->cancel_cb = editor_verbose_window_close;

	spinner_set_interval(vd->spinner, -1);
	gtk_widget_set_sensitive(vd->button_stop, FALSE);
	gtk_widget_set_sensitive(vd->button_close, TRUE);
}

static EditorVerboseData *editor_verbose_window(EditorData *ed, const gchar *text)
{
	EditorVerboseData *vd;
	GtkWidget *scrolled;
	GtkWidget *hbox;
	gchar *buf;

	vd = g_new0(EditorVerboseData, 1);

	vd->gd = file_util_gen_dlg(_("Edit command results"), "editor_results",
				   NULL, FALSE,
				   NULL, ed);
	buf = g_strdup_printf(_("Output of %s"), text);
	generic_dialog_add_message(vd->gd, NULL, buf, NULL);
	g_free(buf);
	vd->button_stop = generic_dialog_add_button(vd->gd, GTK_STOCK_STOP, NULL,
						   editor_verbose_window_stop, FALSE);
	gtk_widget_set_sensitive(vd->button_stop, FALSE);
	vd->button_close = generic_dialog_add_button(vd->gd, GTK_STOCK_CLOSE, NULL,
						    editor_verbose_window_close, TRUE);
	gtk_widget_set_sensitive(vd->button_close, FALSE);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vd->gd->vbox), scrolled, TRUE, TRUE, 5);
	gtk_widget_show(scrolled);

	vd->text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(vd->text), FALSE);
	gtk_widget_set_size_request(vd->text, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_HEIGHT);
	gtk_container_add(GTK_CONTAINER(scrolled), vd->text);
	gtk_widget_show(vd->text);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vd->gd->vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	vd->progress = gtk_progress_bar_new();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vd->progress), 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), vd->progress, TRUE, TRUE, 0);
	gtk_widget_show(vd->progress);

	vd->spinner = spinner_new(NULL, SPINNER_SPEED);
	gtk_box_pack_start(GTK_BOX(hbox), vd->spinner, FALSE, FALSE, 0);
	gtk_widget_show(vd->spinner);

	gtk_widget_show(vd->gd->dialog);

	ed->vd = vd;
	return vd;
}

static void editor_verbose_window_fill(EditorVerboseData *vd, gchar *text, gint len)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(vd->text));
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, -1);
	gtk_text_buffer_insert(buffer, &iter, text, len);
}

static void editor_verbose_window_progress(EditorData *ed, const gchar *text)
{
	if (!ed->vd) return;

	if (ed->total)
		{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ed->vd->progress), (gdouble)ed->count / ed->total);
		}

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ed->vd->progress), (text) ? text : "");
}

static gboolean editor_verbose_io_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
	EditorData *ed = data;
	gchar buf[512];
	gsize count;

	if (condition & G_IO_IN)
		{
		while (g_io_channel_read_chars(source, buf, sizeof(buf), &count, NULL) == G_IO_STATUS_NORMAL)
			{
			if (!g_utf8_validate(buf, count, NULL))
				{
				gchar *utf8;

				utf8 = g_locale_to_utf8(buf, count, NULL, NULL, NULL);
				if (utf8)
					{
					editor_verbose_window_fill(ed->vd, utf8, -1);
					g_free(utf8);
					}
				else
					{
					editor_verbose_window_fill(ed->vd, "Error converting text to valid utf8\n", -1);
					}
				}
			else
				{
				editor_verbose_window_fill(ed->vd, buf, count);
				}
			}
		}

	if (condition & (G_IO_ERR | G_IO_HUP))
		{
		g_io_channel_shutdown(source, TRUE, NULL);
		return FALSE;
		}

	return TRUE;
}

typedef enum {
	PATH_FILE,
	PATH_FILE_URL,
	PATH_DEST
} PathType;


static gchar *editor_command_path_parse(const FileData *fd, PathType type, const EditorDescription *editor)
{
	GString *string;
	gchar *pathl;
	const gchar *p = NULL;

	string = g_string_new("");

	if (type == PATH_FILE || type == PATH_FILE_URL)
		{
		GList *work = editor->ext_list;

		if (!work)
			p = fd->path;
		else
			{
			while (work)
				{
				GList *work2;
				gchar *ext = work->data;
				work = work->next;

				if (strcmp(ext, "*") == 0 ||
				    g_ascii_strcasecmp(ext, fd->extension) == 0)
					{
					p = fd->path;
					break;
					}

				work2 = fd->sidecar_files;
				while (work2)
					{
					FileData *sfd = work2->data;
					work2 = work2->next;

					if (g_ascii_strcasecmp(ext, sfd->extension) == 0)
						{
						p = sfd->path;
						break;
						}
					}
				if (p) break;
				}
			if (!p) return NULL;
			}
		}
	else if (type == PATH_DEST)
		{
		if (fd->change && fd->change->dest)
			p = fd->change->dest;
		else
			p = "";
		}

	g_assert(p);
	while (*p != '\0')
		{
		/* must escape \, ", `, and $ to avoid problems,
		 * we assume system shell supports bash-like escaping
		 */
		if (strchr("\\\"`$", *p) != NULL)
			{
			string = g_string_append_c(string, '\\');
			}
		string = g_string_append_c(string, *p);
		p++;
		}

	if (type == PATH_FILE_URL) g_string_prepend(string, "file://");
	pathl = path_from_utf8(string->str);
	g_string_free(string, TRUE);

	if (pathl && !pathl[0]) /* empty string case */
		{
		g_free(pathl);
		pathl = NULL;
		}

	return pathl;
}


EditorFlags editor_command_parse(const EditorDescription *editor, GList *list, gchar **output)
{
	EditorFlags flags = 0;
	const gchar *p;
	GString *result = NULL;

	if (output)
		result = g_string_new("");

	if (editor->exec[0] == '\0')
		{
		flags |= EDITOR_ERROR_EMPTY;
		goto err;
		}
	
	p = editor->exec;
	/* skip leading whitespaces if any */
	while (g_ascii_isspace(*p)) p++;

	/* command */

	while (*p)
		{
		if (*p != '%')
			{
			if (output) result = g_string_append_c(result, *p);
			}
		else /* *p == '%' */
			{
			gchar *pathl = NULL;

			p++;

			switch (*p)
				{
				case 'f': /* single file */
				case 'u': /* single url */
					flags |= EDITOR_FOR_EACH;
					if (flags & EDITOR_SINGLE_COMMAND)
						{
						flags |= EDITOR_ERROR_INCOMPATIBLE;
						goto err;
						}
					if (list)
						{
						/* use the first file from the list */
						if (!list->data)
							{
							flags |= EDITOR_ERROR_NO_FILE;
							goto err;
							}
						pathl = editor_command_path_parse((FileData *)list->data,
										  (*p == 'f') ? PATH_FILE : PATH_FILE_URL,
										  editor);
						if (!pathl)
							{
							flags |= EDITOR_ERROR_NO_FILE;
							goto err;
							}
						if (output)
							{
							result = g_string_append_c(result, '"');
							result = g_string_append(result, pathl);
							result = g_string_append_c(result, '"');
							}
						g_free(pathl);
						}
					break;

				case 'F':
				case 'U':
					flags |= EDITOR_SINGLE_COMMAND;
					if (flags & (EDITOR_FOR_EACH | EDITOR_DEST))
						{
						flags |= EDITOR_ERROR_INCOMPATIBLE;
						goto err;
						}

					if (list)
						{
						/* use whole list */
						GList *work = list;
						gboolean ok = FALSE;

						while (work)
							{
							FileData *fd = work->data;
							pathl = editor_command_path_parse(fd, (*p == 'F') ? PATH_FILE : PATH_FILE_URL, editor);
							if (pathl)
								{
								ok = TRUE;

								if (output)
									{
									ok = TRUE;
									if (work != list) g_string_append_c(result, ' ');
									result = g_string_append_c(result, '"');
									result = g_string_append(result, pathl);
									result = g_string_append_c(result, '"');
									}
								g_free(pathl);
								}
							work = work->next;
							}
						if (!ok)
							{
							flags |= EDITOR_ERROR_NO_FILE;
							goto err;
							}
						}
					break;
				case 'i':
					if (output)
						{
						result = g_string_append(result, editor->icon);
						}
					break;
				case 'c':
					if (output)
						{
						result = g_string_append(result, editor->name);
						}
					break;
				case 'k':
					if (output)
						{
						result = g_string_append(result, editor->file);
						}
					break;
				case '%':
					/* %% = % escaping */
					if (output) result = g_string_append_c(result, *p);
					break;
				case 'd':
				case 'D':
				case 'n':
				case 'N':
				case 'v':
				case 'm':
					/* deprecated according to spec, ignore */
					break;
				default:
					flags |= EDITOR_ERROR_SYNTAX;
					goto err;
				}
			}
		p++;
		}

	if (output) *output = g_string_free(result, FALSE);
	return flags;


err:
	if (output)
		{
		g_string_free(result, TRUE);
		*output = NULL;
		}
	return flags;
}


static void editor_child_exit_cb(GPid pid, gint status, gpointer data)
{
	EditorData *ed = data;
	g_spawn_close_pid(pid);
	ed->pid = -1;

	editor_command_next_finish(ed, status);
}


static EditorFlags editor_command_one(const EditorDescription *editor, GList *list, EditorData *ed)
{
	gchar *command;
	FileData *fd = list->data;
	GPid pid;
	gint standard_output;
	gint standard_error;
	gboolean ok;

	ed->pid = -1;
	ed->flags = editor->flags;
	ed->flags |= editor_command_parse(editor, list, &command);

	ok = !EDITOR_ERRORS(ed->flags);

	if (ok)
		{
		ok = (options->shell.path && *options->shell.path);
		if (!ok) log_printf("ERROR: empty shell command\n");
			
		if (ok)
			{
			ok = (access(options->shell.path, X_OK) == 0);
			if (!ok) log_printf("ERROR: cannot execute shell command '%s'\n", options->shell.path);
			}

		if (!ok) ed->flags |= EDITOR_ERROR_CANT_EXEC;
		}

	if (ok)
		{
		gchar *working_directory;
		gchar *args[4];
		guint n = 0;

		working_directory = remove_level_from_path(fd->path);
		args[n++] = options->shell.path;
		if (options->shell.options && *options->shell.options)
			args[n++] = options->shell.options;
		args[n++] = command;
		args[n] = NULL;

		if ((ed->flags & EDITOR_DEST) && fd->change && fd->change->dest) /* FIXME: error handling */
			{
			g_setenv("GEEQIE_DESTINATION", fd->change->dest, TRUE);
			}
		else
			{
			g_unsetenv("GEEQIE_DESTINATION");
			}

		ok = g_spawn_async_with_pipes(working_directory, args, NULL,
				      G_SPAWN_DO_NOT_REAP_CHILD, /* GSpawnFlags */
				      NULL, NULL,
				      &pid,
				      NULL,
				      ed->vd ? &standard_output : NULL,
				      ed->vd ? &standard_error : NULL,
				      NULL);
		
		g_free(working_directory);

		if (!ok) ed->flags |= EDITOR_ERROR_CANT_EXEC;
		}

	if (ok)
		{
		g_child_watch_add(pid, editor_child_exit_cb, ed);
		ed->pid = pid;
		}

	if (ed->vd)
		{
		if (!ok)
			{
			gchar *buf;

			buf = g_strdup_printf(_("Failed to run command:\n%s\n"), editor->file);
			editor_verbose_window_fill(ed->vd, buf, strlen(buf));
			g_free(buf);

			}
		else
			{
			GIOChannel *channel_output;
			GIOChannel *channel_error;

			channel_output = g_io_channel_unix_new(standard_output);
			g_io_channel_set_flags(channel_output, G_IO_FLAG_NONBLOCK, NULL);
			g_io_channel_set_encoding(channel_output, NULL, NULL);

			g_io_add_watch_full(channel_output, G_PRIORITY_HIGH, G_IO_IN | G_IO_ERR | G_IO_HUP,
					    editor_verbose_io_cb, ed, NULL);
			g_io_channel_unref(channel_output);

			channel_error = g_io_channel_unix_new(standard_error);
			g_io_channel_set_flags(channel_error, G_IO_FLAG_NONBLOCK, NULL);
			g_io_channel_set_encoding(channel_error, NULL, NULL);

			g_io_add_watch_full(channel_error, G_PRIORITY_HIGH, G_IO_IN | G_IO_ERR | G_IO_HUP,
					    editor_verbose_io_cb, ed, NULL);
			g_io_channel_unref(channel_error);
			}
		}

	g_free(command);

	return EDITOR_ERRORS(ed->flags);
}

static EditorFlags editor_command_next_start(EditorData *ed)
{
	if (ed->vd) editor_verbose_window_fill(ed->vd, "\n", 1);

	if (ed->list && ed->count < ed->total)
		{
		FileData *fd;
		EditorFlags error;

		fd = ed->list->data;

		if (ed->vd)
			{
			if (ed->flags & EDITOR_FOR_EACH)
				editor_verbose_window_progress(ed, fd->path);
			else
				editor_verbose_window_progress(ed, _("running..."));
			}
		ed->count++;

		error = editor_command_one(ed->editor, ed->list, ed);
		if (!error && ed->vd)
			{
			gtk_widget_set_sensitive(ed->vd->button_stop, (ed->list != NULL) );
			if (ed->flags & EDITOR_FOR_EACH)
				{
				editor_verbose_window_fill(ed->vd, fd->path, strlen(fd->path));
				editor_verbose_window_fill(ed->vd, "\n", 1);
				}
			}

		if (!error)
			return 0;
		
		/* command was not started, call the finish immediately */
		return editor_command_next_finish(ed, 0);
		}

	/* everything is done */
	return editor_command_done(ed);
}

static EditorFlags editor_command_next_finish(EditorData *ed, gint status)
{
	gint cont = ed->stopping ? EDITOR_CB_SKIP : EDITOR_CB_CONTINUE;

	if (status)
		ed->flags |= EDITOR_ERROR_STATUS;

	if (ed->flags & EDITOR_FOR_EACH)
		{
		/* handle the first element from the list */
		GList *fd_element = ed->list;

		ed->list = g_list_remove_link(ed->list, fd_element);
		if (ed->callback)
			{
			cont = ed->callback(ed->list ? ed : NULL, ed->flags, fd_element, ed->data);
			if (ed->stopping && cont == EDITOR_CB_CONTINUE) cont = EDITOR_CB_SKIP;
			}
		filelist_free(fd_element);
		}
	else
		{
		/* handle whole list */
		if (ed->callback)
			cont = ed->callback(NULL, ed->flags, ed->list, ed->data);
		filelist_free(ed->list);
		ed->list = NULL;
		}

	switch (cont)
		{
		case EDITOR_CB_SUSPEND:
			return EDITOR_ERRORS(ed->flags);
		case EDITOR_CB_SKIP:
			return editor_command_done(ed);
		}
	
	return editor_command_next_start(ed);
}

static EditorFlags editor_command_done(EditorData *ed)
{
	EditorFlags flags;

	if (ed->vd)
		{
		if (ed->count == ed->total)
			{
			editor_verbose_window_progress(ed, _("done"));
			}
		else
			{
			editor_verbose_window_progress(ed, _("stopped by user"));
			}
		editor_verbose_window_enable_close(ed->vd);
		}

	/* free the not-handled items */
	if (ed->list)
		{
		ed->flags |= EDITOR_ERROR_SKIPPED;
		if (ed->callback) ed->callback(NULL, ed->flags, ed->list, ed->data);
		filelist_free(ed->list);
		ed->list = NULL;
		}

	ed->count = 0;

	flags = EDITOR_ERRORS(ed->flags);

	if (!ed->vd) editor_data_free(ed);

	return flags;
}

void editor_resume(gpointer ed)
{
	editor_command_next_start(ed);
}

void editor_skip(gpointer ed)
{
	editor_command_done(ed);
}

static EditorFlags editor_command_start(const EditorDescription *editor, const gchar *text, GList *list, EditorCallback cb, gpointer data)
{
	EditorData *ed;
	EditorFlags flags = editor->flags;

	if (EDITOR_ERRORS(flags)) return EDITOR_ERRORS(flags);

	ed = g_new0(EditorData, 1);
	ed->list = filelist_copy(list);
	ed->flags = flags;
	ed->editor = editor;
	ed->total = (flags & EDITOR_SINGLE_COMMAND) ? 1 : g_list_length(list);
	ed->callback = cb;
	ed->data =  data;

	if ((flags & EDITOR_VERBOSE_MULTI) && list && list->next)
		flags |= EDITOR_VERBOSE;

	if (flags & EDITOR_VERBOSE)
		editor_verbose_window(ed, text);

	editor_command_next_start(ed);
	/* errors from editor_command_next_start will be handled via callback */
	return EDITOR_ERRORS(flags);
}

gboolean is_valid_editor_command(const gchar *key)
{
	if (!key) return FALSE;
	return g_hash_table_lookup(editors, key) != NULL;
}

EditorFlags start_editor_from_filelist_full(const gchar *key, GList *list, EditorCallback cb, gpointer data)
{
	EditorFlags error;
	EditorDescription *editor;
	if (!key) return FALSE;
	
	editor = g_hash_table_lookup(editors, key);

	if (!list) return FALSE;
	if (!editor) return FALSE;

	error = editor_command_start(editor, editor->name, list, cb, data);

	if (EDITOR_ERRORS(error))
		{
		gchar *text = g_strdup_printf(_("%s\n\"%s\""), editor_get_error_str(error), editor->file);
		
		file_util_warning_dialog(_("Invalid editor command"), text, GTK_STOCK_DIALOG_ERROR, NULL);
		g_free(text);
		}

	return error;
}

EditorFlags start_editor_from_filelist(const gchar *key, GList *list)
{
	return start_editor_from_filelist_full(key, list,  NULL, NULL);
}

EditorFlags start_editor_from_file_full(const gchar *key, FileData *fd, EditorCallback cb, gpointer data)
{
	GList *list;
	EditorFlags error;

	if (!fd) return FALSE;

	list = g_list_append(NULL, fd);
	error = start_editor_from_filelist_full(key, list, cb, data);
	g_list_free(list);
	return error;
}

EditorFlags start_editor_from_file(const gchar *key, FileData *fd)
{
	return start_editor_from_file_full(key, fd, NULL, NULL);
}

gboolean editor_window_flag_set(const gchar *key)
{
	EditorDescription *editor;
	if (!key) return TRUE;
	
	editor = g_hash_table_lookup(editors, key);
	if (!editor) return TRUE;

	return !!(editor->flags & EDITOR_KEEP_FS);
}

gboolean editor_is_filter(const gchar *key)
{
	EditorDescription *editor;
	if (!key) return TRUE;
	
	editor = g_hash_table_lookup(editors, key);
	if (!editor) return TRUE;

	return !!(editor->flags & EDITOR_DEST);
}

const gchar *editor_get_error_str(EditorFlags flags)
{
	if (flags & EDITOR_ERROR_EMPTY) return _("Editor template is empty.");
	if (flags & EDITOR_ERROR_SYNTAX) return _("Editor template has incorrect syntax.");
	if (flags & EDITOR_ERROR_INCOMPATIBLE) return _("Editor template uses incompatible macros.");
	if (flags & EDITOR_ERROR_NO_FILE) return _("Can't find matching file type.");
	if (flags & EDITOR_ERROR_CANT_EXEC) return _("Can't execute external editor.");
	if (flags & EDITOR_ERROR_STATUS) return _("External editor returned error status.");
	if (flags & EDITOR_ERROR_SKIPPED) return _("File was skipped.");
	return _("Unknown error.");
}

const gchar *editor_get_name(const gchar *key)
{
	EditorDescription *editor = g_hash_table_lookup(editors, key);

	if (!editor) return NULL;

	return editor->name;
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
