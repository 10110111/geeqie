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

void file_data_increment_version(FileData *fd);

gboolean file_data_add_change_info(FileData *fd, FileDataChangeType type, const gchar *src, const gchar *dest);
void file_data_change_info_free(FileDataChangeInfo *fdci, FileData *fd);

gint filelist_sort_compare_filedata(FileData *fa, FileData *fb);
gint filelist_sort_compare_filedata_full(FileData *fa, FileData *fb, SortType method, gint ascend);
GList *filelist_sort(GList *list, SortType method, gint ascend);
GList *filelist_insert_sort(GList *list, FileData *fd, SortType method, gint ascend);
GList *filelist_sort_full(GList *list, SortType method, gint ascend, GCompareFunc cb);
GList *filelist_insert_sort_full(GList *list, void *data, SortType method, gint ascend, GCompareFunc cb);

gint filelist_read(FileData *dir_fd, GList **files, GList **dirs);
gint filelist_read_lstat(FileData *dir_fd, GList **files, GList **dirs);
void filelist_free(GList *list);
GList *filelist_copy(GList *list);
GList *filelist_from_path_list(GList *list);
GList *filelist_to_path_list(GList *list);

GList *filelist_filter(GList *list, gint is_dir_list);

GList *filelist_sort_path(GList *list);
GList *filelist_recursive(FileData *dir_fd);

gchar *file_data_sc_list_to_string(FileData *fd);
gboolean file_data_add_ci(FileData *fd, FileDataChangeType type, const gchar *src, const gchar *dest);
gboolean file_data_sc_add_ci_copy(FileData *fd, const gchar *dest_path);
gboolean file_data_sc_add_ci_move(FileData *fd, const gchar *dest_path);
gboolean file_data_sc_add_ci_rename(FileData *fd, const gchar *dest_path);
gboolean file_data_sc_add_ci_delete(FileData *fd);
gboolean file_data_sc_add_ci_unspecified(FileData *fd, const gchar *dest_path);

gboolean file_data_sc_add_ci_delete_list(GList *fd_list);
gboolean file_data_sc_add_ci_copy_list(GList *fd_list, const gchar *dest);
gboolean file_data_sc_add_ci_move_list(GList *fd_list, const gchar *dest);
gboolean file_data_sc_add_ci_rename_list(GList *fd_list, const gchar *dest);
gboolean file_data_sc_add_ci_unspecified_list(GList *fd_list, const gchar *dest);

gboolean file_data_sc_update_ci_copy_list(GList *fd_list, const gchar *dest);
gboolean file_data_sc_update_ci_move_list(GList *fd_list, const gchar *dest);
gboolean file_data_sc_update_ci_unspecified_list(GList *fd_list, const gchar *dest);


gint file_data_sc_update_ci_copy(FileData *fd, const gchar *dest_path);
gint file_data_sc_update_ci_move(FileData *fd, const gchar *dest_path);
gint file_data_sc_update_ci_rename(FileData *fd, const gchar *dest_path);
gint file_data_sc_update_ci_unspecified(FileData *fd, const gchar *dest_path);
gint file_data_sc_check_ci_dest(FileData *fd);
gboolean file_data_sc_perform_ci(FileData *fd);
gint file_data_sc_apply_ci(FileData *fd);
void file_data_sc_free_ci(FileData *fd);
void file_data_sc_free_ci_list(GList *fd_list);

typedef void (*FileDataNotifyFunc)(FileData *fd, NotifyType type, gpointer data);
gint file_data_register_notify_func(FileDataNotifyFunc func, gpointer data, NotifyPriority priority);
gint file_data_unregister_notify_func(FileDataNotifyFunc func, gpointer data);
void file_data_send_notification(FileData *fd, NotifyType type);

gint file_data_register_real_time_monitor(FileData *fd);
gint file_data_unregister_real_time_monitor(FileData *fd);

#endif
