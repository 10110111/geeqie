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


#ifndef FILEFILTER_H
#define FILEFILTER_H


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
gchar *sidecar_ext_to_string(void);
void sidecar_ext_add_defaults(void);
GList *sidecar_ext_get_list(void);

gint ishidden(const gchar *name);

#endif
