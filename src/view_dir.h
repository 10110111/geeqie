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

void vd_destroy(ViewDir *vd);

ViewDir *vd_new(DirViewType type, const gchar *path);

void vd_set_select_func(ViewDir *vdl, void (*func)(ViewDir *vdl, const gchar *path, gpointer data), gpointer data);

void vd_set_layout(ViewDir *vdl, LayoutWindow *layout);

gint vd_set_path(ViewDir *vdl, const gchar *path);
void vd_refresh(ViewDir *vdl);

const gchar *vd_row_get_path(ViewDir *vdl, gint row);

void vd_color_set(ViewDir *vd, FileData *fd, gint color_set);
void vd_popup_destroy_cb(GtkWidget *widget, gpointer data);

GtkWidget *vd_drop_menu(ViewDir *vd, gint active);


#endif


