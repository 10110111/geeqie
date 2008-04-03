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


#include "gqview.h"
#include "editors.h"

#include "utilops.h"
#include "ui_fileops.h"
#include "ui_spinner.h"
#include "ui_utildlg.h"

#include "filelist.h"

#include <errno.h>


#define EDITOR_WINDOW_WIDTH 500
#define EDITOR_WINDOW_HEIGHT 300

#define COMMAND_SHELL "/bin/sh"
#define COMMAND_OPT  "-c"


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
	gint flags;
	GPid pid;
	gchar *command_template;
	GList *list;
	gint count;
	gint total;
	gboolean stopping;
	EditorVerboseData *vd;
	EditorCallback callback;
	gpointer data;
};


static gchar *editor_slot_defaults[GQVIEW_EDITOR_SLOTS * 2] = {
	N_("The Gimp"), "gimp-remote -n %{.cr2;.crw;.nef;.raw;*}f",
	N_("XV"), "xv %f",
	N_("Xpaint"), "xpaint %f",
	N_("UFraw"), "ufraw %{.cr2;.crw;.nef;.raw}p",
	N_("Add XMP sidecar"), "%vFILE=%{.cr2;.crw;.nef;.raw}p;XMP=`echo \"$FILE\"|sed -e 's|\\.[^.]*$|.xmp|'`; exiftool -tagsfromfile \"$FILE\" \"$XMP\"",
	NULL, NULL,
	NULL, NULL,
	NULL, NULL,
	N_("Rotate jpeg clockwise"), "%vif jpegtran -rotate 90 -copy all -outfile %{.jpg;.jpeg}p_tmp %{.jpg;.jpeg}p; then mv %{.jpg;.jpeg}p_tmp %{.jpg;.jpeg}p;else rm %{.jpg;.jpeg}p_tmp;fi",
	N_("Rotate jpeg counterclockwise"), "%vif jpegtran -rotate 270 -copy all -outfile %{.jpg;.jpeg}p_tmp %{.jpg;.jpeg}p; then mv %{.jpg;.jpeg}p_tmp %{.jpg;.jpeg}p;else rm %{.jpg;.jpeg}p_tmp;fi",
	/* special slots */
#if 1
	/* for testing */
	"External Copy command", "%vset -x;cp %p %d",
	"External Move command", "%vset -x;mv %p %d",
	"External Rename command", "%vset -x;mv %p %d",
	"External Delete command", "%vset -x;rm %f",
	"External New Folder command", NULL
#else
	"External Copy command", NULL,
	"External Move command", NULL,
	"External Rename command", NULL,
	"External Delete command", NULL,
	"External New Folder command", NULL
#endif
};

static void editor_verbose_window_progress(EditorData *ed, const gchar *text);
static gint editor_command_next_start(EditorData *ed);
static gint editor_command_next_finish(EditorData *ed, gint status);
static gint editor_command_done(EditorData *ed);

/*
 *-----------------------------------------------------------------------------
 * external editor routines
 *-----------------------------------------------------------------------------
 */

void editor_reset_defaults(void)
{
	gint i;

	for (i = 0; i < GQVIEW_EDITOR_SLOTS; i++)
		{
		g_free(editor_name[i]);
		editor_name[i] = g_strdup(_(editor_slot_defaults[i * 2]));
		g_free(editor_command[i]);
		editor_command[i] = g_strdup(editor_slot_defaults[i * 2 + 1]);
		}
}

static void editor_verbose_data_free(EditorData *ed)
{
	if (!ed->vd) return;
	g_free(ed->vd);
	ed->vd = NULL;
}

static void editor_data_free(EditorData *ed)
{
	editor_verbose_data_free(ed);
	g_free(ed->command_template);
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

	vd->gd = file_util_gen_dlg(_("Edit command results"), GQ_WMCLASS, "editor_results",
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
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ed->vd->progress), (double)ed->count / ed->total);
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
					editor_verbose_window_fill(ed->vd, "Geeqie: Error converting text to valid utf8\n", -1);
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
	PATH_DEST
} PathType;


static gchar *editor_command_path_parse(const FileData *fd, PathType type, const gchar *extensions)
{
	GString *string;
	gchar *pathl;
	const gchar *p = NULL;

	string = g_string_new("");
	
	if (type == PATH_FILE)
		{
		GList *ext_list = filter_to_list(extensions);
		GList *work = ext_list;
		
		if (!work)
			p = fd->path;
		else
			{
			while(work)
				{
				gchar *ext = work->data;
				work = work->next;
				
				if (strcmp(ext, "*") == 0 || 
				    strcasecmp(ext, fd->extension) == 0)
					{
					p = fd->path;
					break;
					}
				
				GList *work2 = fd->sidecar_files;
				
				while(work2)
					{
					FileData *sfd = work2->data;
					work2 = work2->next;
					
					if (strcasecmp(ext, sfd->extension) == 0)
						{
						p = sfd->path;
						break;
						}
					}
				if (p) break;
				}
			string_list_free(ext_list);
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

	pathl = path_from_utf8(string->str);
	g_string_free(string, TRUE);

	return pathl;
}


/*
 * The supported macros for editor commands:
 *
 *   %f   first occurence replaced by quoted sequence of filenames, command is run once.
 *        only one occurence of this macro is supported.
 *        ([ls %f] results in [ls "file1" "file2" ... "lastfile"])
 *   %p   command is run for each filename in turn, each instance replaced with single filename.
 *        multiple occurences of this macro is supported for complex shell commands.
 *        This macro will BLOCK THE APPLICATION until it completes, since command is run once
 *        for every file in syncronous order. To avoid blocking add the %v macro, below.
 *        ([ls %p] results in [ls "file1"], [ls "file2"] ... [ls "lastfile"])
 *   none if no macro is supplied, the result is equivalent to "command %f"
 *        ([ls] results in [ls "file1" "file2" ... "lastfile"])
 *
 *  Only one of the macros %f or %p may be used in a given commmand.
 *
 *   %v   must be the first two characters[1] in a command, causes a window to display
 *        showing the output of the command(s).
 *   %V   same as %v except in the case of %p only displays a window for multiple files,
 *        operating on a single file is suppresses the output dialog.
 *
 *   %w   must be first two characters in a command, presence will disable full screen
 *        from exiting upon invocation.
 *
 *
 * [1] Note: %v,%V may also be preceded by "%w".
 */


gint editor_command_parse(const gchar *template, GList *list, gchar **output)
{
	gint flags = 0;
	const gchar *p = template;
	GString *result = NULL;
	gchar *extensions = NULL;
	
	if (output)
		result = g_string_new("");

	if (!template || template[0] == '\0') 
		{
		flags |= EDITOR_ERROR_EMPTY;
		goto err;
		}

	
	/* global flags */
	while (*p == '%')
		{
		switch (*++p)
			{
			case 'w':
				flags |= EDITOR_KEEP_FS;
				p++;
				break;
			case 'v':
				flags |= EDITOR_VERBOSE;
				p++;
				break;
			case 'V':
				flags |= EDITOR_VERBOSE_MULTI;
				p++;
				break;
			}
		}
	
	/* command */
	
	while (*p)
		{
		if (*p != '%')
			{
			if (output) result = g_string_append_c(result, *p);
			}
		else /* *p == '%' */
			{
			extensions = NULL;
			gchar *pathl = NULL;

			p++;
			
			/* for example "%f" or "%{.crw;.raw;.cr2}f" */
			if (*p == '{')
				{
				p++;
				gchar *end = strchr(p, '}');
				if (!end)
					{
					flags |= EDITOR_ERROR_SYNTAX;
					goto err;
					}
				
				extensions = g_strndup(p, end - p);
				p = end + 1;
				}
			
			switch (*p) 
				{
				case 'd':
					flags |= EDITOR_DEST;
				case 'p':
					flags |= EDITOR_FOR_EACH;
					if (flags & EDITOR_SINGLE_COMMAND)
						{
						flags |= EDITOR_ERROR_INCOMPATIBLE;
						goto err;
						}
					if (output)
						{
						/* use the first file from the list */
						if (!list || !list->data) 
							{
							flags |= EDITOR_ERROR_NO_FILE;
							goto err;
							}
						pathl = editor_command_path_parse((FileData *)list->data, (*p == 'd') ? PATH_DEST : PATH_FILE, extensions);
						if (!pathl) 
							{
							flags |= EDITOR_ERROR_NO_FILE;
							goto err;
							}
						result = g_string_append_c(result, '"');
						result = g_string_append(result, pathl);
						g_free(pathl);
						result = g_string_append_c(result, '"');
						}
					break;	

				case 'f':
					flags |= EDITOR_SINGLE_COMMAND;
					if (flags & (EDITOR_FOR_EACH | EDITOR_DEST))
						{
						flags |= EDITOR_ERROR_INCOMPATIBLE;
						goto err;
						}

					if (output)
						{
						/* use whole list */
						GList *work = list;
						gboolean ok = FALSE;
						while (work)
							{
							FileData *fd = work->data;
							pathl = editor_command_path_parse(fd, PATH_FILE, extensions);

							if (pathl)
								{
								ok = TRUE;
								if (work != list) g_string_append_c(result, ' ');
								result = g_string_append_c(result, '"');
								result = g_string_append(result, pathl);
								g_free(pathl);
								result = g_string_append_c(result, '"');
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
				default:
					flags |= EDITOR_ERROR_SYNTAX;
					goto err;
				}
			if (extensions) g_free(extensions);
			extensions = NULL;
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
	if (extensions) g_free(extensions);
	return flags;
}

static void editor_child_exit_cb (GPid pid, gint status, gpointer data)
{
	EditorData *ed = data;
	g_spawn_close_pid(pid);
	ed->pid = -1;
	
	editor_command_next_finish(ed, status);
}


static gint editor_command_one(const gchar *template, GList *list, EditorData *ed)
{
	gchar *command;
	gchar *working_directory;
	FileData *fd = list->data;
	gchar *args[4];
	GPid pid;
        gint standard_output;
        gint standard_error;
	gboolean ok;


	ed->pid = -1;

	working_directory = remove_level_from_path(fd->path);
	
	ed->flags = editor_command_parse(template, list, &command);

	ok = !(ed->flags & EDITOR_ERROR_MASK);


	args[0] = COMMAND_SHELL;
	args[1] = COMMAND_OPT;
	args[2] = command;
	args[3] = NULL;
	
	if (ok)
		{
		ok = g_spawn_async_with_pipes(working_directory, args, NULL, 
				      G_SPAWN_DO_NOT_REAP_CHILD, /* GSpawnFlags */
                                      NULL, NULL,
                                      &pid, 
				      NULL, 
				      ed->vd ? &standard_output : NULL, 
				      ed->vd ? &standard_error : NULL, 
				      NULL);
		
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

			buf = g_strdup_printf(_("Failed to run command:\n%s\n"), template);
			editor_verbose_window_fill(ed->vd, buf, strlen(buf));
			g_free(buf);

			}
		else 
			{
		
			GIOChannel *channel_output;
			GIOChannel *channel_error;
			channel_output = g_io_channel_unix_new(standard_output);
			g_io_channel_set_flags(channel_output, G_IO_FLAG_NONBLOCK, NULL);

			g_io_add_watch_full(channel_output, G_PRIORITY_HIGH, G_IO_IN | G_IO_ERR | G_IO_HUP,
					    editor_verbose_io_cb, ed, NULL);
			g_io_channel_unref(channel_output);

			channel_error = g_io_channel_unix_new(standard_error);
			g_io_channel_set_flags(channel_error, G_IO_FLAG_NONBLOCK, NULL);

			g_io_add_watch_full(channel_error, G_PRIORITY_HIGH, G_IO_IN | G_IO_ERR | G_IO_HUP,
					    editor_verbose_io_cb, ed, NULL);
			g_io_channel_unref(channel_error);
			}
		}
	

	
	g_free(command);
	g_free(working_directory);

	return ed->flags & EDITOR_ERROR_MASK;
}

static gint editor_command_next_start(EditorData *ed)
{

	if (ed->vd) editor_verbose_window_fill(ed->vd, "\n", 1);

	if (ed->list && ed->count < ed->total)
		{
		FileData *fd;
		gint error;

		fd = ed->list->data;

		if (ed->vd)
			{
			editor_verbose_window_progress(ed, (ed->flags & EDITOR_FOR_EACH) ? fd->path : _("running..."));
			}
		ed->count++;

		error = editor_command_one(ed->command_template, ed->list, ed);
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
		else
			/* command was not started, call the finish immediately */
			return editor_command_next_finish(ed, 0);
		}
	
	/* everything is done */
	return editor_command_done(ed);
}

static gint editor_command_next_finish(EditorData *ed, gint status)
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
			cont = ed->callback(ed->list ? ed : NULL, ed->flags, fd_element, ed->data);
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

	if (cont == EDITOR_CB_SUSPEND)
		return ed->flags & EDITOR_ERROR_MASK;
	else if (cont == EDITOR_CB_SKIP)
		return editor_command_done(ed);
	else
		return editor_command_next_start(ed);

}

static gint editor_command_done(EditorData *ed)
{
	gint flags;
	const gchar *text;

	if (ed->vd)
		{
		if (ed->count == ed->total)
			{
			text = _("done");
			}
		else
			{
			text = _("stopped by user");
			}
		editor_verbose_window_progress(ed, text);
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

	flags = ed->flags & EDITOR_ERROR_MASK;

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

static gint editor_command_start(const gchar *template, const gchar *text, GList *list, EditorCallback cb, gpointer data)
{
	EditorData *ed;
	gint flags = editor_command_parse(template, NULL, NULL);
	
	if (flags & EDITOR_ERROR_MASK) return flags & EDITOR_ERROR_MASK;

	ed = g_new0(EditorData, 1);
	ed->list = filelist_copy(list);
	ed->flags = flags;
	ed->command_template = g_strdup(template);
	ed->total = (flags & EDITOR_SINGLE_COMMAND) ? 1 : g_list_length(list);
	ed->count = 0;
	ed->stopping = FALSE;
	ed->callback = cb;
	ed->data =  data;
	
	if ((flags & EDITOR_VERBOSE_MULTI) && list && list->next)
		flags |= EDITOR_VERBOSE;
	
	
	if (flags & EDITOR_VERBOSE)
		editor_verbose_window(ed, text);
		
	editor_command_next_start(ed); 
	/* errors from editor_command_next_start will be handled via callback */
	return flags & EDITOR_ERROR_MASK;
}

gint start_editor_from_filelist_full(gint n, GList *list, EditorCallback cb, gpointer data)
{
	gchar *command;
	gint error;

	if (n < 0 || n >= GQVIEW_EDITOR_SLOTS || !list ||
	    !editor_command[n] ||
	    strlen(editor_command[n]) == 0) return FALSE;

	command = g_locale_from_utf8(editor_command[n], -1, NULL, NULL, NULL);
	error = editor_command_start(command, editor_name[n], list, cb, data);
	g_free(command);
	return error;
}

gint start_editor_from_filelist(gint n, GList *list)
{
	return start_editor_from_filelist_full(n, list,  NULL, NULL);
}


gint start_editor_from_file_full(gint n, FileData *fd, EditorCallback cb, gpointer data)
{
	GList *list;
	gint error;

	if (!fd) return FALSE;

	list = g_list_append(NULL, fd);
	error = start_editor_from_filelist_full(n, list, cb, data);
	g_list_free(list);
	return error;
}

gint start_editor_from_file(gint n, FileData *fd)
{
	return start_editor_from_file_full(n, fd, NULL, NULL);
}

gint editor_window_flag_set(gint n)
{
	if (n < 0 || n >= GQVIEW_EDITOR_SLOTS ||
	    !editor_command[n] ||
	    strlen(editor_command[n]) == 0) return TRUE;

	return (editor_command_parse(editor_command[n], NULL, NULL) & EDITOR_KEEP_FS);
}


const gchar *editor_get_error_str(gint flags)
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
