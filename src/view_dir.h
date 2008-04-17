/*
 * Geeqie
 * (C) 2008 Vladimir Nadvornik
 *
 * Author: Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef VIEW_DIR_H
#define VIEW_DIR_H

enum {
	DIR_COLUMN_POINTER = 0,
	DIR_COLUMN_ICON,
	DIR_COLUMN_NAME,
	DIR_COLUMN_COLOR,
	DIR_COLUMN_COUNT
};

extern GtkRadioActionEntry menu_view_dir_radio_entries[2];

ViewDir *vd_new(DirViewType type, const gchar *path);

void vd_set_select_func(ViewDir *vdl, void (*func)(ViewDir *vdl, const gchar *path, gpointer data), gpointer data);

void vd_set_layout(ViewDir *vdl, LayoutWindow *layout);

gint vd_set_path(ViewDir *vdl, const gchar *path);
void vd_refresh(ViewDir *vdl);
gint vd_find_row(ViewDir *vd, FileData *fd, GtkTreeIter *iter);

const gchar *vd_row_get_path(ViewDir *vdl, gint row);

void vd_color_set(ViewDir *vd, FileData *fd, gint color_set);
void vd_popup_destroy_cb(GtkWidget *widget, gpointer data);

GtkWidget *vd_drop_menu(ViewDir *vd, gint active);
GtkWidget *vd_pop_menu(ViewDir *vd, FileData *fd);

void vd_dnd_drop_scroll_cancel(ViewDir *vd);
void vd_dnd_init(ViewDir *vd);

void vd_menu_position_cb(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data);

void vd_activate_cb(GtkTreeView *tview, GtkTreePath *tpath, GtkTreeViewColumn *column, gpointer data);
void vd_color_cb(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);

gint vd_release_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);
gint vd_press_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
gint vd_press_cb(GtkWidget *widget,  GdkEventButton *bevent, gpointer data);

#endif


