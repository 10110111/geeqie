/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

enum {
	DIALOG_NEW_DIR,
	DIALOG_COPY,
	DIALOG_MOVE,
	DIALOG_DELETE,
	DIALOG_RENAME
};

typedef struct _FileDataMult FileDataMult;
struct _FileDataMult
{
	gint confirm_all;
	gint confirmed;
	gint skip;
	GList *source_list;
	GList *source_next;
	gchar *dest_base;
	gchar *source;
	gchar *dest;
	gint copy;
};

typedef struct _FileDataSingle FileDataSingle;
struct _FileDataSingle
{
	gint confirmed;
	gchar *source;
	gchar *dest;
	gint copy;
};

static FileDataMult *file_data_multiple_new(GList *source_list, gchar *dest, gint copy);
static void file_data_multiple_free(FileDataMult *fdm);
static void file_util_move_multiple(FileDataMult *fdm);
static void file_util_move_multiple_ok_cb(GtkWidget *widget, gpointer data);
static void file_util_move_multiple_all_cb(GtkWidget *widget, gpointer data);
static void file_util_move_multiple_skip_cb(GtkWidget *widget, gpointer data);
static void file_util_move_multiple_cancel_cb(GtkWidget *widget, gpointer data);
static void file_util_move_multiple(FileDataMult *fdm);

static FileDataSingle *file_data_single_new(gchar *source, gchar *dest, gint copy);
static void file_data_single_free(FileDataSingle *fds);
static void file_util_move_single_ok_cb(GtkWidget *widget, gpointer data);
static void file_util_move_single(FileDataSingle *fds);
static void file_util_move_single_cancel_cb(GtkWidget *widget, gpointer data);
static void file_util_move_do(FileDialog *fd);
static void file_util_move_check(FileDialog *fd);
static void file_util_move_cb(GtkWidget *widget, gpointer data);
static void file_util_move_enter_cb(gchar *path, gpointer data);
static void file_util_move_completion_sync_cb(gchar *path, gpointer data);
static void real_file_util_move(gchar *source_path, GList *source_list, gchar *dest_path, gint copy);

static void file_util_delete_multiple_ok_cb(GtkWidget *w, gpointer data);
static void file_util_delete_multiple_cancel_cb(GtkWidget *w, gpointer data);
static void file_util_delete_multiple(GList *source_list);
static void file_util_delete_ok_cb(GtkWidget *w, gpointer data);
static void file_util_delete_cancel_cb(GtkWidget *w, gpointer data);
static void file_util_delete_single(gchar *path);

static void file_util_rename_multiple_ok_cb(GtkWidget *w, gpointer data);
static void file_util_rename_multiple_cancel_cb(GtkWidget *w, gpointer data);
static void file_util_rename_multiple(FileDialog *fd);
static void file_util_rename_multiple_cb(GtkWidget *w, gpointer data);
static void file_util_rename_multiple_select_cb(GtkWidget *clist,
		gint row, gint column, GdkEventButton *bevent, gpointer data);
static void file_util_rename_multiple_do(GList *source_list);

static void file_util_rename_single_ok_cb(GtkWidget *w, gpointer data);
static void file_util_rename_single_cancel_cb(GtkWidget *w, gpointer data);
static void file_util_rename_single(FileDataSingle *fds);
static void file_util_rename_single_cb(GtkWidget *w, gpointer data);
static void file_util_rename_single_do(gchar *source_path);

static void file_util_create_dir_do(gchar *source, gchar *path);
static void file_util_create_dir_cb(GtkWidget *w, gpointer data);

/*
 *--------------------------------------------------------------------------
 * Move and Copy routines
 *--------------------------------------------------------------------------
 */

/*
 * Multi file move
 */

static FileDataMult *file_data_multiple_new(GList *source_list, gchar *dest, gint copy)
{
	FileDataMult *fdm = g_new0(FileDataMult, 1);
	fdm->confirm_all = FALSE;
	fdm->confirmed = FALSE;
	fdm->skip = FALSE;
	fdm->source_list = source_list;
	fdm->source_next = fdm->source_list;
	fdm->dest_base = g_strdup(dest);
	fdm->source = NULL;
	fdm->dest = NULL;
	fdm->copy = copy;
	return fdm;
}

static void file_data_multiple_free(FileDataMult *fdm)
{
	free_selected_list(fdm->source_list);
	g_free(fdm->dest_base);
	g_free(fdm->dest);
	g_free(fdm);
}

static void file_util_move_multiple_ok_cb(GtkWidget *widget, gpointer data)
{
	FileDataMult *fdm = data;
	fdm->confirmed = TRUE;
	file_util_move_multiple(fdm);
}

static void file_util_move_multiple_all_cb(GtkWidget *widget, gpointer data)
{
	FileDataMult *fdm = data;
	fdm->confirm_all = TRUE;
	file_util_move_multiple(fdm);
}

static void file_util_move_multiple_skip_cb(GtkWidget *widget, gpointer data)
{
	FileDataMult *fdm = data;
	fdm->skip = TRUE;
	file_util_move_multiple(fdm);
}

static void file_util_move_multiple_cancel_cb(GtkWidget *widget, gpointer data)
{
	FileDataMult *fdm = data;
	file_data_multiple_free(fdm);
}

static void file_util_move_multiple(FileDataMult *fdm)
{
	while (fdm->dest || fdm->source_next)
		{
		if (!fdm->dest)
			{
			GList *work = fdm->source_next;
			fdm->source = work->data;
			fdm->dest = g_strconcat(fdm->dest_base, "/", filename_from_path(fdm->source), NULL);
			fdm->source_next = work->next;
			}

		if (fdm->dest && fdm->source && strcmp(fdm->dest, fdm->source) == 0)
			{
			ConfirmDialog *cd;
			gchar *title;
			gchar *text;
			if (fdm->copy)
				{
				title = _("Source to copy matches destination");
				text = g_strdup_printf(_("Unable to copy file:\n%s\nto itself."), fdm->dest);
				}
			else
				{
				title = _("Source to move matches destination");
				text = g_strdup_printf(_("Unable to move file:\n%s\nto itself."), fdm->dest);
				}
			cd = confirm_dialog_new(title, text, file_util_move_multiple_cancel_cb, fdm);
			confirm_dialog_add(cd, _("Continue"), file_util_move_multiple_skip_cb);
			g_free(text);
			return;
			}
		else if (isfile(fdm->dest) && !fdm->confirmed && !fdm->confirm_all && !fdm->skip)
			{
			ConfirmDialog *cd;
			gchar *text = g_strdup_printf(_("Overwrite file:\n %s\n with:\b %s"), fdm->dest, fdm->source);
			cd = confirm_dialog_new_with_image(_("Overwrite file"), text,
						fdm->dest, fdm->source,
						file_util_move_multiple_cancel_cb, fdm);
			confirm_dialog_add(cd, _("Skip"), file_util_move_multiple_skip_cb);
			confirm_dialog_add(cd, _("Yes to all"), file_util_move_multiple_all_cb);
			confirm_dialog_add(cd, _("Yes"), file_util_move_multiple_ok_cb);
			g_free(text);
			return;
			}
		else
			{
			gint success = FALSE;
			if (fdm->skip)
				{
				success = TRUE;
				fdm->skip = FALSE;
				}
			else
				{
				if (fdm->copy)
					{
					success = copy_file(fdm->source, fdm->dest);
					}
				else
					{
					if (move_file(fdm->source, fdm->dest))
						{
						success = TRUE;
						file_is_gone(fdm->source, fdm->source_list);
						}
					}
				}
			if (!success)
				{
				ConfirmDialog *cd;
				gchar *title;
				gchar *text;
				if (fdm->copy)
					{
					title = _("Error copying file");
					text = g_strdup_printf(_("Unable to copy file:\n%sto:\n%s\n during multiple file copy."), fdm->source, fdm->dest);
					}
				else
					{
					title = _("Error moving file");
					text = g_strdup_printf(_("Unable to move file:\n%sto:\n%s\n during multiple file move."), fdm->source, fdm->dest);
					}
				cd = confirm_dialog_new(title, text, file_util_move_multiple_cancel_cb, fdm);
				confirm_dialog_add(cd, _("Continue"), file_util_move_multiple_skip_cb);
				g_free(text);
				return;
				}
			fdm->confirmed = FALSE;
			g_free(fdm->dest);
			fdm->dest = NULL;
			}
		}

	file_data_multiple_free(fdm);
}

/*
 * Single file move
 */

static FileDataSingle *file_data_single_new(gchar *source, gchar *dest, gint copy)
{
	FileDataSingle *fds = g_new0(FileDataSingle, 1);
	fds->confirmed = FALSE;
	fds->source = g_strdup(source);
	fds->dest = g_strdup(dest);
	fds->copy = copy;
	return fds;
}

static void file_data_single_free(FileDataSingle *fds)
{
	g_free(fds->source);
	g_free(fds->dest);
	g_free(fds);
}

static void file_util_move_single_ok_cb(GtkWidget *widget, gpointer data)
{
	FileDataSingle *fds = data;
	fds->confirmed = TRUE;
	file_util_move_single(fds);
}

static void file_util_move_single_cancel_cb(GtkWidget *widget, gpointer data)
{
	FileDataSingle *fds = data;
	file_data_single_free(fds);
}

static void file_util_move_single(FileDataSingle *fds)
{
	if (fds->dest && fds->source && strcmp(fds->dest, fds->source) == 0)
		{
		warning_dialog(_("Source matches destination"),
			       _("Source and destination are the same, operation cancelled."));
		}
	else if (isfile(fds->dest) && !fds->confirmed)
		{
		ConfirmDialog *cd;
		gchar *text = g_strdup_printf(_("Overwrite file:\n%s\n with:\n%s"), fds->dest, fds->source);
		cd = confirm_dialog_new_with_image(_("Overwrite file"), text,
						   fds->dest, fds->source,
						   file_util_move_single_cancel_cb, fds);
		confirm_dialog_add(cd, _("Overwrite"), file_util_move_single_ok_cb);
		g_free(text);
		return;
		}
	else
		{
		gint success = FALSE;
		if (fds->copy)
			{
			success = copy_file(fds->source, fds->dest);
			}
		else
			{
			if (move_file(fds->source, fds->dest))
				{
				success = TRUE;
				file_is_gone(fds->source, NULL);
				}
			}
		if (!success)
			{
			gchar *title;
			gchar *text;
			if (fds->copy)
				{
				title = _("Error copying file");
				text = g_strdup_printf(_("Unable to copy file:\n%s\nto:\n%s"), fds->source, fds->dest);
				}
			else
				{
				title = _("Error moving file");
				text = g_strdup_printf(_("Unable to move file:\n%s\nto:\n%s"), fds->source, fds->dest);
				}
			warning_dialog(title, text);
			g_free(text);
			}
		}

	file_data_single_free(fds);
}

/*
 * file move dialog
 */

static void file_util_move_do(FileDialog *fd)
{
	tab_completion_append_to_history(fd->entry, fd->dest_path);
	if (fd->multiple_files)
		{
		file_util_move_multiple(file_data_multiple_new(fd->source_list, fd->dest_path, fd->type));
		fd->source_list = NULL;
		}
	else
		{
		if (isdir(fd->dest_path))
			{
			gchar *buf = g_strconcat(fd->dest_path, "/", filename_from_path(fd->source_path), NULL);
			g_free(fd->dest_path);
			fd->dest_path = buf;
			}
		file_util_move_single(file_data_single_new(fd->source_path, fd->dest_path, fd->type));
		}

	generic_dialog_close(NULL, fd);
}

static void file_util_move_check(FileDialog *fd)
{
	g_free(fd->dest_path);
	fd->dest_path = remove_trailing_slash(gtk_entry_get_text(GTK_ENTRY(fd->entry)));

	if (fd->multiple_files && !isdir(fd->dest_path))
		{
		if (isfile(fd->dest_path))
			warning_dialog(_("Invalid destination"), _("When operating with multiple files, please select\n a directory, not file."));
		else
			warning_dialog(_("Invalid directory"), _("Please select an existing directory"));
		return;
		}

	file_util_move_do(fd);
}

static void file_util_move_cb(GtkWidget *widget, gpointer data)
{
	FileDialog *fd = data;
	file_util_move_check(fd);
}

static void file_util_move_enter_cb(gchar *path, gpointer data)
{
	FileDialog *fd = data;
	file_util_move_check(fd);
}

static void file_util_move_completion_sync_cb(gchar *path, gpointer data)
{
	FileDialog *fd = data;
	destination_widget_sync_to_entry(fd->entry);
}

static void real_file_util_move(gchar *source_path, GList *source_list, gchar *dest_path, gint copy)
{
	FileDialog *fd;
	gchar *path = NULL;
	gint multiple;
	gchar *text;
	gchar *title;
	gchar *op_text;
	GtkWidget *tabcomp;
	GtkWidget *dest;
	gchar *last_path;

	if (!source_path && !source_list) return;

	if (source_path)
		{
		path = g_strdup(source_path);
		multiple = FALSE;
		}
	else if (source_list->next)
		{
		multiple = TRUE;
		}
	else
		{
		path = g_strdup(source_list->data);
		free_selected_list(source_list);
		source_list = NULL;
		multiple = FALSE;
		}

	if (copy)
		{
		title = _("GQview - copy");
		op_text = _("Copy");
		if (path)
			text = g_strdup_printf(_("Copy file:\n%s\nto:"), path);
		else
			text = g_strdup_printf(_("Copy multiple files from:\n%s\nto:"), dest_path);
		}
	else
		{
		title = _("GQview - move");
		op_text = _("Move");
		if (path)
			text = g_strdup_printf(_("Move file:\n%s\nto:"), path);
		else
			text = g_strdup_printf(_("Move multiple files from:\n%s\nto:"), dest_path);
		}

	fd = generic_dialog_new(title, text, op_text, _("Cancel"),
		file_util_move_cb, generic_dialog_close);

	g_free(text);

	fd->type = copy;
	fd->source_path = path;
	fd->source_list = source_list;
	fd->multiple_files = multiple;

	tabcomp = tab_completion_new_with_history(&fd->entry, fd->dialog, dest_path,
					   "move_copy", 32, file_util_move_enter_cb, fd);
	last_path = tab_completion_set_to_last_history(fd->entry);
	if (last_path)
		{
		fd->dest_path = g_strdup(last_path);
		}
	else
		{
		fd->dest_path = g_strdup(dest_path);
		}
					   
/*	tabcomp = tab_completion_new(&fd->entry, fd->dialog, fd->dest_path, file_util_move_enter_cb, fd);
*/
	gtk_box_pack_start(GTK_BOX(fd->vbox), tabcomp, FALSE, FALSE, 0);
	gtk_widget_show(tabcomp);

	gtk_widget_grab_focus(fd->entry);

	dest = destination_widget_new(fd->dest_path, fd->entry);

	tab_completion_add_tab_func(fd->entry, file_util_move_completion_sync_cb, fd);

	gtk_box_pack_start(GTK_BOX(fd->vbox), dest, TRUE, TRUE, 0);
}

void file_util_move(gchar *source_path, GList *source_list, gchar *dest_path)
{
	real_file_util_move(source_path, source_list, dest_path, FALSE);
}

void file_util_copy(gchar *source_path, GList *source_list, gchar *dest_path)
{
	real_file_util_move(source_path, source_list, dest_path, TRUE);
}

/*
 *--------------------------------------------------------------------------
 * Delete routines
 *--------------------------------------------------------------------------
 */

/*
 * delete multiple files
 */

static void file_util_delete_multiple_ok_cb(GtkWidget *w, gpointer data)
{
	GList *source_list = data;

	while(source_list)
		{
		gchar *path = source_list->data;
		source_list = g_list_remove(source_list, path);
		if (unlink (path) < 0)
			{
			ConfirmDialog *cd;
			gchar *text;
			if (source_list)
				{
				text = g_strdup_printf(_("Unable to delete file:\n %s\n Continue multiple delete operation?"), path);
				cd = confirm_dialog_new(_("Delete failed"), text, file_util_delete_multiple_cancel_cb, source_list);
				confirm_dialog_add(cd, _("Continue"), file_util_delete_multiple_ok_cb);
				}
			else
				{
				text = g_strdup_printf(_("Unable to delete file:\n%s"), path);
				warning_dialog(_("Delete failed"), text);
				}
			g_free(text);
			g_free(path);
			return;
			}
		else
			{
			file_is_gone(path, source_list);
			}
		g_free(path);
		}
}

static void file_util_delete_multiple_cancel_cb(GtkWidget *w, gpointer data)
{
	GList *source_list = data;
	free_selected_list(source_list);
}

static void file_util_delete_multiple(GList *source_list)
{
	if (!confirm_delete)
		{
		file_util_delete_multiple_ok_cb(NULL, source_list);
		}
	else
		{
		ConfirmDialog *cd;
		cd = confirm_dialog_new(_("Delete files"), _("About to delete multiple files..."), file_util_delete_multiple_cancel_cb, source_list);
		confirm_dialog_add(cd, _("Delete"), file_util_delete_multiple_ok_cb);
		}
}

/*
 * delete single file
 */

static void file_util_delete_ok_cb(GtkWidget *w, gpointer data)
{
	gchar *path = data;

	if (unlink (path) < 0)
		{
		gchar *text = g_strdup_printf(_("Unable to delete file:\n%s"), path);
		warning_dialog(_("File deletion failed"), text);
		g_free(text);
		}
	else
		{
		file_is_gone(path, NULL);
		}

	g_free(path);
}

static void file_util_delete_cancel_cb(GtkWidget *w, gpointer data)
{
	gchar *path = data;
	g_free(path);
}

static void file_util_delete_single(gchar *path)
{
	gchar *buf = g_strdup(path);

	if (!confirm_delete)
		{
		file_util_delete_ok_cb(NULL, buf);
		}
	else
		{
		ConfirmDialog *cd;
		gchar *text = g_strdup_printf(_("About to delete the file:\n %s"), buf);
		cd = confirm_dialog_new(_("Delete file"), text, file_util_delete_cancel_cb, buf);
		confirm_dialog_add(cd, _("Delete"), file_util_delete_ok_cb);
		g_free(text);
		}
}

void file_util_delete(gchar *source_path, GList *source_list)
{
	if (!source_path && !source_list) return;

	if (source_path)
		{
		file_util_delete_single(source_path);
		}
	else if (!source_list->next)
		{
		file_util_delete_single(source_list->data);
		free_selected_list(source_list);
		}
	else
		{
		file_util_delete_multiple(source_list);
		}
}

/*
 *--------------------------------------------------------------------------
 * Rename routines
 *--------------------------------------------------------------------------
 */

/*
 * rename multiple files
 */

static void file_util_rename_multiple_ok_cb(GtkWidget *w, gpointer data)
{
	FileDialog *fd = data;
	if (!GTK_WIDGET_VISIBLE(fd->dialog)) gtk_widget_show(fd->dialog);
	fd->type = TRUE;
	file_util_rename_multiple(fd);
}

static void file_util_rename_multiple_cancel_cb(GtkWidget *w, gpointer data)
{
	FileDialog *fd = data;
	if (!GTK_WIDGET_VISIBLE(fd->dialog)) gtk_widget_show(fd->dialog);
	return;
}

static void file_util_rename_multiple(FileDialog *fd)
{
	if (isfile(fd->dest_path) && !fd->type)
		{
		ConfirmDialog *cd;
		gchar *text = g_strdup_printf(_("Overwrite file:\n%s\nby renaming:\n%s"), fd->dest_path, fd->source_path);
		cd = confirm_dialog_new_with_image(_("Overwrite file"), text,
						   fd->dest_path, fd->source_path,
						   file_util_rename_multiple_cancel_cb, fd);
		confirm_dialog_add(cd, _("Overwrite"), file_util_rename_multiple_ok_cb);
		g_free(text);
		gtk_widget_hide(fd->dialog);
		return;
		}
	else
		{
		if (rename (fd->source_path, fd->dest_path) < 0)
			{
			gchar *text = g_strdup_printf(_("Unable to rename file:\n%s\n to:\n%s"), filename_from_path(fd->source_path), filename_from_path(fd->dest_path));
			warning_dialog(_("Error renaming file"), text);
			g_free(text);
			}
		else
			{
			gint row;
			gint n;
			GtkWidget *clist;
			gchar *path;

			file_is_renamed(fd->source_path, fd->dest_path);

			clist = gtk_object_get_user_data(GTK_OBJECT(fd->entry));
			path = gtk_object_get_user_data(GTK_OBJECT(clist));
			row = gtk_clist_find_row_from_data(GTK_CLIST(clist), path);

			n = g_list_length(GTK_CLIST(clist)->row_list);
			if (debug) printf("r=%d n=%d\n", row, n);
			if (n - 1 > row)
				n = row;
			else if (n > 1)
				n = row - 1;
			else
				n = -1;

			if (n >= 0)
				{
				gtk_object_set_user_data(GTK_OBJECT(clist), NULL);
				gtk_clist_remove(GTK_CLIST(clist), row);
				gtk_clist_select_row(GTK_CLIST(clist), n, -1);
				}
			else
				{
				if (debug) printf("closed by #%d\n", n);
				generic_dialog_close(NULL, fd);
				}
			}
		}
}

static void file_util_rename_multiple_cb(GtkWidget *w, gpointer data)
{
	FileDialog *fd = data;
	gchar *base;
	gchar *name;

	name = gtk_entry_get_text(GTK_ENTRY(fd->entry));
	base = remove_level_from_path(fd->source_path);
	g_free(fd->dest_path);
	fd->dest_path = g_strconcat(base, "/", name, NULL);
	g_free(base);

	if (strlen(name) == 0 || strcmp(fd->source_path, fd->dest_path) == 0)
		{
		return;
		}

	fd->type = FALSE;
	file_util_rename_multiple(fd);
}

static void file_util_rename_multiple_select_cb(GtkWidget *clist,
		gint row, gint column, GdkEventButton *bevent, gpointer data)
{
	FileDialog *fd = data;
	GtkWidget *label;
	gchar *name;
	gchar *path;

	label = gtk_object_get_user_data(GTK_OBJECT(fd->dialog));
	path = gtk_clist_get_row_data(GTK_CLIST(clist), row);
	g_free(fd->source_path);
	fd->source_path = g_strdup(path);
	gtk_object_set_user_data(GTK_OBJECT(clist), path);
	name = filename_from_path(fd->source_path);

	gtk_label_set(GTK_LABEL(label), name);
	gtk_entry_set_text(GTK_ENTRY(fd->entry), name);

	gtk_widget_grab_focus(fd->entry);
}

static void file_util_rename_multiple_do(GList *source_list)
{
	FileDialog *fd;
	GtkWidget *scrolled;
	GtkWidget *clist;
	GtkWidget *label;
	GList *work;

	fd = generic_dialog_new(_("GQview - rename"), _("Rename multiple files:"), _("Rename"), _("Cancel"),
		file_util_rename_multiple_cb, generic_dialog_close);

	fd->source_path = g_strdup(source_list->data);
	fd->dest_path = NULL;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(fd->vbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	clist=gtk_clist_new (1);
	gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 0, TRUE);
	gtk_signal_connect (GTK_OBJECT (clist), "select_row",(GtkSignalFunc) file_util_rename_multiple_select_cb, fd);
	gtk_widget_set_usize(clist, 250, 150);
	gtk_container_add (GTK_CONTAINER (scrolled), clist);
	gtk_widget_show (clist);

	gtk_object_set_user_data(GTK_OBJECT(clist), source_list->data);

	work = source_list;
	while(work)
		{
		gint row;
		gchar *buf[2];
		buf[0] = filename_from_path(work->data);
		buf[1] = NULL;
		row = gtk_clist_append(GTK_CLIST(clist), buf);
		gtk_clist_set_row_data_full(GTK_CLIST(clist), row,
				work->data, (GtkDestroyNotify) g_free);
		work = work->next;
		}

	g_list_free(source_list);

	label = gtk_label_new(_("Rename:"));
	gtk_box_pack_start(GTK_BOX(fd->vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	label = gtk_label_new(filename_from_path(fd->source_path));
	gtk_box_pack_start(GTK_BOX(fd->vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_object_set_user_data(GTK_OBJECT(fd->dialog), label);

	label = gtk_label_new(_("to:"));
	gtk_box_pack_start(GTK_BOX(fd->vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	fd->entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(fd->entry), filename_from_path(fd->source_path));
	gtk_box_pack_start(GTK_BOX(fd->vbox), fd->entry, FALSE, FALSE, 0);
	gtk_widget_grab_focus(fd->entry);
	gtk_widget_show(fd->entry);

	gtk_object_set_user_data(GTK_OBJECT(fd->entry), clist);
}

/*
 * rename single file
 */

static void file_util_rename_single_ok_cb(GtkWidget *w, gpointer data)
{
	FileDataSingle *fds = data;
	fds->confirmed = TRUE;
	file_util_rename_single(fds);
}

static void file_util_rename_single_cancel_cb(GtkWidget *w, gpointer data)
{
	FileDataSingle *fds = data;
	file_data_single_free(fds);
}

static void file_util_rename_single(FileDataSingle *fds)
{
	if (isfile(fds->dest) && !fds->confirmed)
		{
		ConfirmDialog *cd;
		gchar *text = g_strdup_printf(_("Overwrite file:\n%s\nby renaming:\n%s"), fds->dest,fds->source);
		cd = confirm_dialog_new_with_image(_("Overwrite file"), text,
						   fds->dest, fds->source,
						   file_util_rename_single_cancel_cb, fds);
		confirm_dialog_add(cd, _("Overwrite"), file_util_rename_single_ok_cb);
		g_free(text);
		return;
		}
	else
		{
		if (rename (fds->source, fds->dest) < 0)
			{
			gchar *text = g_strdup_printf(_("Unable to rename file:\n%s\nto:\n%s"), filename_from_path(fds->source), filename_from_path(fds->dest));
			warning_dialog(_("Error renaming file"), text);
			g_free(text);
			}
		else
			{
			file_is_renamed(fds->source, fds->dest);
			}
		}
	file_data_single_free(fds);
}

static void file_util_rename_single_cb(GtkWidget *w, gpointer data)
{
	FileDialog *fd = data;
	gchar *name = gtk_entry_get_text(GTK_ENTRY(fd->entry));
	gchar *buf = g_strconcat(fd->dest_path, "/", name, NULL);

	if (strlen(name) == 0 || strcmp(fd->source_path, buf) == 0)
		{
		g_free(buf);
		return;
		}

	g_free(fd->dest_path);
	fd->dest_path = buf;

	file_util_rename_single(file_data_single_new(fd->source_path, fd->dest_path, fd->type));

	generic_dialog_close(NULL, fd);
}

static void file_util_rename_single_do(gchar *source_path)
{
	FileDialog *fd;
	gchar *text;
	gchar *name = filename_from_path(source_path);

	text = g_strdup_printf(_("Rename file:\n%s\nto:"), name);
	fd = generic_dialog_new(_("GQview - rename"), text, _("Rename"), _("Cancel"),
		file_util_rename_single_cb, generic_dialog_close);
	g_free(text);

	fd->source_path = g_strdup(source_path);
	fd->dest_path = remove_level_from_path(source_path);

	fd->entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(fd->entry), name);
	gtk_box_pack_start(GTK_BOX(fd->vbox), fd->entry, FALSE, FALSE, 0);
	gtk_widget_grab_focus(fd->entry);
	gtk_widget_show(fd->entry);
}

void file_util_rename(gchar *source_path, GList *source_list)
{
	if (!source_path && !source_list) return;

	if (source_path)
		{
		file_util_rename_single_do(source_path);
		}
	else if (!source_list->next)
		{
		file_util_rename_single_do(source_list->data);
		free_selected_list(source_list);
		}
	else
		{
		file_util_rename_multiple_do(source_list);
		}
}

/*
 *--------------------------------------------------------------------------
 * Create directory routines
 *--------------------------------------------------------------------------
 */

static void file_util_create_dir_do(gchar *source, gchar *path)
{
	if (isfile(path))
		{
		gchar *text = g_strdup_printf(_("The path:\n%s\nalready exists as a file."), filename_from_path(path));
		warning_dialog(_("Could not create directory"), text);
		g_free(text);
		}
	else if (isdir(path))
		{
		gchar *text = g_strdup_printf(_("The directory:\n%s\nalready exists."), filename_from_path(path));
		warning_dialog(_("Directory exists"), text);
		g_free(text);
		}
	else
		{
		if (mkdir (path, 0755) < 0)
			{
			gchar *text = g_strdup_printf(_("Unable to create directory:\n%s"), filename_from_path(path));
			warning_dialog(_("Error creating directory"), text);
			g_free(text);
			}
		else
			{
			if (strcmp(source, current_path) == 0)
				{
				gchar *buf = g_strdup(current_path);
				filelist_change_to(buf);
				g_free(buf);
				}
			}
		}
}

static void file_util_create_dir_cb(GtkWidget *w, gpointer data)
{
	FileDialog *fd = data;
	gchar *name = gtk_entry_get_text(GTK_ENTRY(fd->entry));

	if (strlen(name) == 0) return;

	g_free(fd->dest_path);
	fd->dest_path = g_strconcat(fd->source_path, "/", name, NULL);

	file_util_create_dir_do(fd->source_path, fd->dest_path);

	generic_dialog_close(NULL, fd);
}

void file_util_create_dir(gchar *path)
{
	FileDialog *fd;
	gchar *text;
	gchar *name;

	if (!isdir(path)) return;
	name = filename_from_path(path);

	text = g_strdup_printf(_("Create directory in:\n%s\nnamed:"), path);
	fd = generic_dialog_new(_("GQview - new directory"), text, _("Create"), _("Cancel"),
		file_util_create_dir_cb, generic_dialog_close);
	g_free(text);

	fd->source_path = g_strdup(path);
	fd->dest_path = NULL;

	fd->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(fd->vbox), fd->entry, FALSE, FALSE, 0);
	gtk_widget_grab_focus(fd->entry);
	gtk_widget_show(fd->entry);
}

