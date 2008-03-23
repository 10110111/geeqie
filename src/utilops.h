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


#ifndef UTILOPS_H
#define UTILOPS_H


#include "ui_utildlg.h"


void file_maint_renamed(FileData *fd);
void file_maint_removed(FileData *fd, GList *ignore_list);
void file_maint_moved(FileData *fd, GList *ignore_list);
void file_maint_copied(FileData *fd);

GenericDialog *file_util_gen_dlg(const gchar *title,
				 const gchar *wmclass, const gchar *wmsubclass,
				 GtkWidget *parent, gint auto_close,
				 void (*cancel_cb)(GenericDialog *, gpointer), gpointer data);
FileDialog *file_util_file_dlg(const gchar *title,
			       const gchar *wmclass, const gchar *wmsubclass,
			       GtkWidget *parent,
			       void (*cancel_cb)(FileDialog *, gpointer), gpointer data);
GenericDialog *file_util_warning_dialog(const gchar *heading, const gchar *message,
					const gchar *icon_stock_id, GtkWidget *parent);

void file_util_trash_clear(void);

void file_util_delete(FileData *source_fd, GList *source_list, GtkWidget *parent);
void file_util_move(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent);
void file_util_copy(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent);
void file_util_rename(FileData *source_fd, GList *source_list, GtkWidget *parent);

void file_util_create_dir(const gchar *path, GtkWidget *parent);
gint file_util_rename_dir(FileData *source_fd, const gchar *new_path, GtkWidget *parent);

/* these avoid the location entry dialog, list must be files only and
 * dest_path must be a valid directory path
*/
void file_util_move_simple(GList *list, const gchar *dest_path);
void file_util_copy_simple(GList *list, const gchar *dest_path);


void file_util_delete_dir(FileData *source_fd, GtkWidget *parent);


#endif

