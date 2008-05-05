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


#ifndef FILEDATA_H
#define FILEDATA_H

gchar *text_from_size(gint64 size);
gchar *text_from_size_abrev(gint64 size);
const gchar *text_from_time(time_t t);

/* this expects a utf-8 path */
FileData *file_data_new_simple(const gchar *path_utf8);

FileData *file_data_ref(FileData *fd);
void file_data_unref(FileData *fd);

void file_data_do_change(FileData *fd);
gboolean file_data_add_change_info(FileData *fd, FileDataChangeType type, const gchar *src, const gchar *dest);
void file_data_change_info_free(FileDataChangeInfo *fdci, FileData *fd);

gchar *sidecar_file_data_list_to_string(FileData *fd);

gint filelist_sort_compare_filedata(FileData *fa, FileData *fb);
gint filelist_sort_compare_filedata_full(FileData *fa, FileData *fb, SortType method, gint ascend);
GList *filelist_sort(GList *list, SortType method, gint ascend);
GList *filelist_insert_sort(GList *list, FileData *fd, SortType method, gint ascend);
GList *filelist_sort_full(GList *list, SortType method, gint ascend, GCompareFunc cb);
GList *filelist_insert_sort_full(GList *list, void *data, SortType method, gint ascend, GCompareFunc cb);

gint filelist_read(const gchar *path, GList **files, GList **dirs);
gint filelist_read_lstat(const gchar *path, GList **files, GList **dirs);
void filelist_free(GList *list);
GList *filelist_copy(GList *list);
GList *filelist_from_path_list(GList *list);
GList *filelist_to_path_list(GList *list);

GList *filelist_filter(GList *list, gint is_dir_list);

GList *filelist_sort_path(GList *list);
GList *filelist_recursive(const gchar *path);

#endif