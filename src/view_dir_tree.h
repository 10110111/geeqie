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

#ifndef VIEW_DIR_TREE_H
#define VIEW_DIR_TREE_H

ViewDir *vdtree_new(ViewDir *vd, const gchar *path);

gint vdtree_set_path(ViewDir *vd, const gchar *path);
void vdtree_refresh(ViewDir *vd);

const gchar *vdtree_row_get_path(ViewDir *vd, gint row);
gint vdtree_find_row(ViewDir *vd, FileData *fd, GtkTreeIter *iter, GtkTreeIter *parent);


#endif

