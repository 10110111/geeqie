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


#ifndef EDITORS_H
#define EDITORS_H


#define	EDITOR_KEEP_FS            0x00000001
#define	EDITOR_VERBOSE            0x00000002
#define	EDITOR_VERBOSE_MULTI      0x00000004
#define EDITOR_TERMINAL		  0x00000008

#define	EDITOR_DEST               0x00000100
#define	EDITOR_FOR_EACH           0x00000200
#define	EDITOR_SINGLE_COMMAND     0x00000400

#define	EDITOR_ERROR_EMPTY        0x00020000
#define	EDITOR_ERROR_SYNTAX       0x00040000
#define	EDITOR_ERROR_INCOMPATIBLE 0x00080000
#define	EDITOR_ERROR_NO_FILE      0x00100000
#define	EDITOR_ERROR_CANT_EXEC    0x00200000
#define	EDITOR_ERROR_STATUS       0x00400000
#define	EDITOR_ERROR_SKIPPED      0x00800000

#define	EDITOR_ERROR_MASK         0xffff0000


/* return values from callback function */
enum {
	EDITOR_CB_CONTINUE = 0, /* continue multiple editor execution on remaining files*/
	EDITOR_CB_SKIP,         /* skip the remaining files */
	EDITOR_CB_SUSPEND       /* suspend execution, one of editor_resume or editor_skip
				   must be called later */
};

extern GHashTable *editors;

void editor_load_descriptions(void);
GList *editor_list_get(void);


/*
Callback is called even on skipped files, with the EDITOR_ERROR_SKIPPED flag set.
It is a good place to call file_data_change_info_free().

ed - pointer that can be used for editor_resume/editor_skip or NULL if all files were already processed
flags - flags above
list - list of procesed FileData structures, typically single file or whole list passed to start_editor_*
data - generic pointer
*/
typedef gint (*EditorCallback) (gpointer ed, gint flags, GList *list, gpointer data);


void editor_resume(gpointer ed);
void editor_skip(gpointer ed);



gint start_editor_from_file(const gchar *key, FileData *fd);
gint start_editor_from_filelist(const gchar *key, GList *list);
gint start_editor_from_file_full(const gchar *key, FileData *fd, EditorCallback cb, gpointer data);
gint start_editor_from_filelist_full(const gchar *key, GList *list, EditorCallback cb, gpointer data);
gint editor_window_flag_set(const gchar *key);
gint editor_is_filter(const gchar *key);
const gchar *editor_get_error_str(gint flags);

const gchar *editor_get_name(const gchar *key);

gboolean is_valid_editor_command(const gchar *key);

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
