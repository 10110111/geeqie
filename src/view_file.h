/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef VIEW_FILE_H
#define VIEW_FILE_H

#define VIEW_FILE_TYPES_COUNT 2

ViewFile *vf_new(FileViewType type, const gchar *path);

void vf_set_status_func(ViewFile *vf, void (*func)(ViewFile *vf, gpointer data), gpointer data);
void vf_set_thumb_status_func(ViewFile *vf, void (*func)(ViewFile *vf, gdouble val, const gchar *text, gpointer data), gpointer data);

void vf_set_layout(ViewFile *vf, LayoutWindow *layout);

gint vf_set_path(ViewFile *vf, const gchar *path);
gint vf_refresh(ViewFile *vf);

void vf_thumb_set(ViewFile *vf, gint enable);
void vf_marks_set(ViewFile *vf, gint enable);
void vf_sort_set(ViewFile *vf, SortType type, gint ascend);

FileData *vf_index_get_data(ViewFile *vf, gint row);
gchar *vf_index_get_path(ViewFile *vf, gint row);
gint vf_index_by_path(ViewFile *vf, const gchar *path);
gint vf_index_by_fd(ViewFile *vf, FileData *in_fd);
gint vf_count(ViewFile *vf, gint64 *bytes);
GList *vf_get_list(ViewFile *vf);

gint vf_index_is_selected(ViewFile *vf, gint row);
gint vf_selection_count(ViewFile *vf, gint64 *bytes);
GList *vf_selection_get_list(ViewFile *vf);
GList *vf_selection_get_list_by_index(ViewFile *vf);

void vf_select_all(ViewFile *vf);
void vf_select_none(ViewFile *vf);
void vf_select_by_fd(ViewFile *vf, FileData *fd);

void vf_mark_to_selection(ViewFile *vf, gint mark, MarkToSelectionMode mode);
void vf_selection_to_mark(ViewFile *vf, gint mark, SelectionToMarkMode mode);

void vf_select_marked(ViewFile *vf, gint mark);
void vf_mark_selected(ViewFile *vf, gint mark, gint value);

gint vf_maint_renamed(ViewFile *vf, FileData *fd);
gint vf_maint_removed(ViewFile *vf, FileData *fd, GList *ignore_list);
gint vf_maint_moved(ViewFile *vf, FileData *fd, GList *ignore_list);


#endif /* VIEW_FILE_H */
