/*
 * Geeqie
 * (C) 2004 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef VIEW_DIR_LIST_H
#define VIEW_DIR_LIST_H


ViewDir *vdlist_new(const gchar *path);

gint vdlist_set_path(ViewDir *vdl, const gchar *path);
void vdlist_refresh(ViewDir *vdl);

const gchar *vdlist_row_get_path(ViewDir *vdl, gint row);
gint vdlist_find_row(ViewDir *vd, FileData *fd, GtkTreeIter *iter);


#endif


