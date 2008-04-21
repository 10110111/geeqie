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


#ifndef FILELIST_H
#define FILELIST_H


typedef struct _FilterEntry FilterEntry;
struct _FilterEntry {
	gchar *key;
	gchar *description;
	gchar *extensions;
	FileFormatClass file_class;
	gint enabled;
};

/* you can change, but not add or remove entries from the returned list */
GList *filter_get_list(void);
void filter_remove_entry(FilterEntry *fe);

void filter_add(const gchar *key, const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled);
void filter_add_unique(const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled);
void filter_add_defaults(void);
void filter_reset(void);
void filter_rebuild(void);
GList *filter_to_list(const gchar *extensions);

gint filter_name_exists(const gchar *name);
gint filter_file_class(const gchar *name, FileFormatClass file_class);

void filter_write_list(SecureSaveInfo *ssi);
void filter_parse(const gchar *text);

void sidecar_ext_parse(const gchar *text, gint quoted);
void sidecar_ext_write(SecureSaveInfo *ssi);
char *sidecar_ext_to_string();
void sidecar_ext_add_defaults();

gint ishidden(const gchar *name);


GList *path_list_filter(GList *list, gint is_dir_list);

GList *path_list_sort(GList *list);
GList *path_list_recursive(const gchar *path);

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
