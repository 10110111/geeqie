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

