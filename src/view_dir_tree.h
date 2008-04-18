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

typedef struct _NodeData NodeData;
struct _NodeData
{
	FileData *fd;
	gint expanded;
	time_t last_update;
};

ViewDir *vdtree_new(ViewDir *vd, const gchar *path);

void vdtree_select_row(ViewDir *vd, FileData *fd);

gint vdtree_set_path(ViewDir *vd, const gchar *path);
void vdtree_refresh(ViewDir *vd);

const gchar *vdtree_row_get_path(ViewDir *vd, gint row);
gint vdtree_find_row(ViewDir *vd, FileData *fd, GtkTreeIter *iter, GtkTreeIter *parent);

FileData *vdtree_populate_path(ViewDir *vd, const gchar *path, gint expand, gint force);
void vdtree_rename_by_data(ViewDir *vd, FileData *fd);

gint vdtree_press_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
gint vdtree_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);

void vdtree_destroy_cb(GtkWidget *widget, gpointer data);

#endif

