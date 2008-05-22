/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef VIEW_FILE_LIST_H
#define VIEW_FILE_LIST_H


#include "filedata.h"

gint vflist_press_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
gint vflist_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
gint vflist_release_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);

void vflist_dnd_init(ViewFile *vf);

void vflist_destroy_cb(GtkWidget *widget, gpointer data);
ViewFile *vflist_new(ViewFile *vf, const gchar *path);

gint vflist_set_path(ViewFile *vf, const gchar *path);
gint vflist_refresh(ViewFile *vf);

void vflist_thumb_set(ViewFile *vf, gint enable);
void vflist_marks_set(ViewFile *vf, gint enable);
void vflist_sort_set(ViewFile *vf, SortType type, gint ascend);

GList *vflist_pop_menu_file_list(ViewFile *vf);
void vflist_pop_menu_view_cb(GtkWidget *widget, gpointer data);
void vflist_pop_menu_rename_cb(GtkWidget *widget, gpointer data);
void vflist_pop_menu_refresh_cb(GtkWidget *widget, gpointer data);
void vflist_popup_destroy_cb(GtkWidget *widget, gpointer data);
void vflist_pop_menu_thumbs_cb(GtkWidget *widget, gpointer data);

FileData *vflist_index_get_data(ViewFile *vf, gint row);
gint vflist_index_by_path(ViewFile *vf, const gchar *path);
guint vflist_count(ViewFile *vf, gint64 *bytes);
GList *vflist_get_list(ViewFile *vf);

gint vflist_index_is_selected(ViewFile *vf, gint row);
guint vflist_selection_count(ViewFile *vf, gint64 *bytes);
GList *vflist_selection_get_list(ViewFile *vf);
GList *vflist_selection_get_list_by_index(ViewFile *vf);

void vflist_select_all(ViewFile *vf);
void vflist_select_none(ViewFile *vf);
void vflist_select_invert(ViewFile *vf);
void vflist_select_by_fd(ViewFile *vf, FileData *fd);

void vflist_mark_to_selection(ViewFile *vf, gint mark, MarkToSelectionMode mode);
void vflist_selection_to_mark(ViewFile *vf, gint mark, SelectionToMarkMode mode);

void vflist_select_marked(ViewFile *vf, gint mark);
void vflist_mark_selected(ViewFile *vf, gint mark, gint value);

gint vflist_maint_renamed(ViewFile *vf, FileData *fd);
gint vflist_maint_removed(ViewFile *vf, FileData *fd, GList *ignore_list);
gint vflist_maint_moved(ViewFile *vf, FileData *fd, GList *ignore_list);

void vflist_color_set(ViewFile *vf, FileData *fd, gint color_set);


#endif
