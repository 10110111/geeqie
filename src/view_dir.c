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

#include "main.h"
#include "view_dir.h"

#include "filelist.h"
#include "ui_menu.h"
#include "utilops.h"
#include "view_dir_list.h"
#include "view_dir_tree.h"

GtkRadioActionEntry menu_view_dir_radio_entries[] = {
  { "FolderList",	NULL,		N_("List"),		"<meta>L",	NULL, DIRVIEW_LIST },
  { "FolderTree",	NULL,		N_("Tr_ee"),		"<control>T",	NULL, DIRVIEW_TREE },
};

ViewDir *vd_new(DirViewType type, const gchar *path)
{
	ViewDir *vd = NULL;

	switch(type)
	{
	case DIRVIEW_LIST: vd = vdlist_new(path); break;
	case DIRVIEW_TREE: vd = vdtree_new(path); break;
	}

	return vd;
}
	
void vd_set_select_func(ViewDir *vd,
                        void (*func)(ViewDir *vd, const gchar *path, gpointer data), gpointer data)
{
        vd->select_func = func;
        vd->select_data = data;
}

void vd_set_layout(ViewDir *vd, LayoutWindow *layout)
{
	vd->layout = layout;
}

gint vd_set_path(ViewDir *vd, const gchar *path)
{
	gint ret = FALSE;

	switch(vd->type)
	{
	case DIRVIEW_LIST: ret = vdlist_set_path(vd, path); break;
	case DIRVIEW_TREE: ret = vdtree_set_path(vd, path); break;
	}

	return ret;
}

void vd_refresh(ViewDir *vd)
{
	switch(vd->type)
	{
	case DIRVIEW_LIST: return vdlist_refresh(vd);
	case DIRVIEW_TREE: return vdtree_refresh(vd);
	}
}

const gchar *vd_row_get_path(ViewDir *vd, gint row)
{
	const gchar *ret = NULL;

	switch(vd->type)
	{
	case DIRVIEW_LIST: ret = vdlist_row_get_path(vd, row); break;
	case DIRVIEW_TREE: ret = vdtree_row_get_path(vd, row); break;
	}

	return ret;
}

void vd_color_set(ViewDir *vd, FileData *fd, gint color_set)
{
	GtkTreeModel *store;
	GtkTreeIter iter;

	switch(vd->type)
	{
	case DIRVIEW_LIST:
		{
		if (vdlist_find_row(vd, fd, &iter) < 0) return;
		store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
		gtk_list_store_set(GTK_LIST_STORE(store), &iter, DIR_COLUMN_COLOR, color_set, -1);
		}
		break;
	case DIRVIEW_TREE:
		{
		if (vdtree_find_row(vd, fd, &iter, NULL) < 0) return;
		store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
		gtk_tree_store_set(GTK_TREE_STORE(store), &iter, DIR_COLUMN_COLOR, color_set, -1);
		}
		break;
	}
}

void vd_popup_destroy_cb(GtkWidget *widget, gpointer data)
{
	ViewDir *vd = data;

	vd_color_set(vd, vd->click_fd, FALSE);
	vd->click_fd = NULL;
	vd->popup = NULL;

	vd_color_set(vd, vd->drop_fd, FALSE);
	filelist_free(vd->drop_list);
	vd->drop_list = NULL;
	vd->drop_fd = NULL;
}

/*
 *-----------------------------------------------------------------------------
 * drop menu (from dnd)
 *-----------------------------------------------------------------------------
 */

static void vd_drop_menu_copy_cb(GtkWidget *widget, gpointer data)
{
	ViewDir *vd = data;
	const gchar *path;
	GList *list;

	if (!vd->drop_fd) return;

	path = vd->drop_fd->path;
	list = vd->drop_list;
	vd->drop_list = NULL;

	file_util_copy_simple(list, path);
}

static void vd_drop_menu_move_cb(GtkWidget *widget, gpointer data)
{
	ViewDir *vd = data;
	const gchar *path;
	GList *list;

	if (!vd->drop_fd) return;

	path = vd->drop_fd->path;
	list = vd->drop_list;

	vd->drop_list = NULL;

	file_util_move_simple(list, path);
}

GtkWidget *vd_drop_menu(ViewDir *vd, gint active)
{
	GtkWidget *menu;

	menu = popup_menu_short_lived();
	g_signal_connect(G_OBJECT(menu), "destroy",
			 G_CALLBACK(vd_popup_destroy_cb), vd);

	menu_item_add_stock_sensitive(menu, _("_Copy"), GTK_STOCK_COPY, active,
				      G_CALLBACK(vd_drop_menu_copy_cb), vd);
	menu_item_add_sensitive(menu, _("_Move"), active, G_CALLBACK(vd_drop_menu_move_cb), vd);

	menu_item_add_divider(menu);
	menu_item_add_stock(menu, _("Cancel"), GTK_STOCK_CANCEL, NULL, vd);

	return menu;
}

