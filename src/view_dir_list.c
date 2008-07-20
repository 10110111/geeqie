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

#include "main.h"
#include "view_dir_list.h"

#include "dnd.h"
#include "dupe.h"
#include "filedata.h"
#include "layout.h"
#include "layout_image.h"
#include "layout_util.h"
#include "utilops.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_tree_edit.h"
#include "view_dir.h"

#include <gdk/gdkkeysyms.h> /* for keyboard values */


#define VDLIST_INFO(_vd_, _part_) (((ViewDirInfoList *)(_vd_->info))->_part_)


/*
 *-----------------------------------------------------------------------------
 * misc
 *-----------------------------------------------------------------------------
 */

gint vdlist_find_row(ViewDir *vd, FileData *fd, GtkTreeIter *iter)
{
	GtkTreeModel *store;
	gint valid;
	gint row = 0;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
	valid = gtk_tree_model_get_iter_first(store, iter);
	while (valid)
		{
		FileData *fd_n;
		gtk_tree_model_get(GTK_TREE_MODEL(store), iter, DIR_COLUMN_POINTER, &fd_n, -1);
		if (fd_n == fd) return row;

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter);
		row++;
		}

	return -1;
}


FileData *vdlist_row_by_path(ViewDir *vd, const gchar *path, gint *row)
{
	GList *work;
	gint n;

	if (!path)
		{
		if (row) *row = -1;
		return NULL;
		}

	n = 0;
	work = VDLIST_INFO(vd, list);
	while (work)
		{
		FileData *fd = work->data;
		if (strcmp(fd->path, path) == 0)
			{
			if (row) *row = n;
			return fd;
			}
		work = work->next;
		n++;
		}

	if (row) *row = -1;
	return NULL;
}

/*
 *-----------------------------------------------------------------------------
 * dnd
 *-----------------------------------------------------------------------------
 */

static void vdlist_scroll_to_row(ViewDir *vd, FileData *fd, gfloat y_align)
{
	GtkTreeIter iter;

	if (GTK_WIDGET_REALIZED(vd->view) && vd_find_row(vd, fd, &iter) >= 0)
		{
		GtkTreeModel *store;
		GtkTreePath *tpath;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
		tpath = gtk_tree_model_get_path(store, &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(vd->view), tpath, NULL, TRUE, y_align, 0.0);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(vd->view), tpath, NULL, FALSE);
		gtk_tree_path_free(tpath);

		if (!GTK_WIDGET_HAS_FOCUS(vd->view)) gtk_widget_grab_focus(vd->view);
		}
}

/*
 *-----------------------------------------------------------------------------
 * main
 *-----------------------------------------------------------------------------
 */

void vdlist_select_row(ViewDir *vd, FileData *fd)
{
	if (fd && vd->select_func)
		{
		gchar *path;

		path = g_strdup(fd->path);
		vd->select_func(vd, path, vd->select_data);
		g_free(path);
		}
}

const gchar *vdlist_row_get_path(ViewDir *vd, gint row)
{
	FileData *fd;

	fd = g_list_nth_data(VDLIST_INFO(vd, list), row);

	if (fd) return fd->path;

	return NULL;
}

static void vdlist_populate(ViewDir *vd)
{
	GtkListStore *store;
	GList *work;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view)));
	gtk_list_store_clear(store);

	work = VDLIST_INFO(vd, list);
	while (work)
		{
		FileData *fd;
		GtkTreeIter iter;
		GdkPixbuf *pixbuf;
		const gchar *date = "";

		fd = work->data;

		if (access_file(fd->path, R_OK | X_OK) && fd->name)
			{
			if (fd->name[0] == '.' && fd->name[1] == '\0')
				{
				pixbuf = vd->pf->open;
				}
			else if (fd->name[0] == '.' && fd->name[1] == '.' && fd->name[2] == '\0')
				{
				pixbuf = vd->pf->parent;
				}
			else
				{
				pixbuf = vd->pf->close;
				if (options->layout.show_directory_date)
					date = text_from_time(fd->date);
				}
			}
		else
			{
			pixbuf = vd->pf->deny;
			}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   DIR_COLUMN_POINTER, fd,
				   DIR_COLUMN_ICON, pixbuf,
				   DIR_COLUMN_NAME, fd->name,
				   DIR_COLUMN_DATE, date,
				   -1);

		work = work->next;
		}

	vd->click_fd = NULL;
	vd->drop_fd = NULL;
}

gint vdlist_set_fd(ViewDir *vd, FileData *dir_fd)
{
	gint ret;
	FileData *fd;
	gchar *old_path = NULL;
	gchar *filepath;

	if (!dir_fd) return FALSE;
	if (vd->dir_fd == dir_fd) return TRUE;

	if (vd->dir_fd)
		{
		gchar *base;

		base = remove_level_from_path(vd->dir_fd->path);
		if (strcmp(base, dir_fd->path) == 0)
			{
			old_path = g_strdup(vd->dir_fd->name);
			}
		g_free(base);
		}

	file_data_unref(vd->dir_fd);
	vd->dir_fd = file_data_ref(dir_fd);

	filelist_free(VDLIST_INFO(vd, list));

	ret = filelist_read(vd->dir_fd, NULL, &VDLIST_INFO(vd, list));
	VDLIST_INFO(vd, list) = filelist_sort(VDLIST_INFO(vd, list), SORT_NAME, TRUE);

	/* add . and .. */

	if (strcmp(vd->dir_fd->path, G_DIR_SEPARATOR_S) != 0)
		{
		filepath = g_build_filename(vd->dir_fd->path, "..", NULL);
		fd = file_data_new_simple(filepath);
		VDLIST_INFO(vd, list) = g_list_prepend(VDLIST_INFO(vd, list), fd);
		g_free(filepath);
		}

	if (options->file_filter.show_dot_directory)
		{
		filepath = g_build_filename(vd->dir_fd->path, ".", NULL);
		fd = file_data_new_simple(filepath);
		VDLIST_INFO(vd, list) = g_list_prepend(VDLIST_INFO(vd, list), fd);
		g_free(filepath);
	}

	vdlist_populate(vd);

	if (old_path)
		{
		/* scroll to make last path visible */
		FileData *found = NULL;
		GList *work;

		work = VDLIST_INFO(vd, list);
		while (work && !found)
			{
			FileData *fd = work->data;
			if (strcmp(old_path, fd->name) == 0) found = fd;
			work = work->next;
			}

		if (found) vdlist_scroll_to_row(vd, found, 0.5);

		g_free(old_path);
		return ret;
		}

	if (GTK_WIDGET_REALIZED(vd->view))
		{
		gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(vd->view), 0, 0);
		}

	return ret;
}

void vdlist_refresh(ViewDir *vd)
{
	FileData *dir_fd;

	dir_fd = vd->dir_fd;
	vd->dir_fd = NULL;
	vdlist_set_fd(vd, dir_fd);
	file_data_unref(dir_fd);
}

gint vdlist_press_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	ViewDir *vd = data;
	GtkTreePath *tpath;

	if (event->keyval != GDK_Menu) return FALSE;

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(vd->view), &tpath, NULL);
	if (tpath)
		{
		GtkTreeModel *store;
		GtkTreeIter iter;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DIR_COLUMN_POINTER, &vd->click_fd, -1);

		gtk_tree_path_free(tpath);
		}
	else
		{
		vd->click_fd = NULL;
		}

	vd_color_set(vd, vd->click_fd, TRUE);

	vd->popup = vd_pop_menu(vd, vd->click_fd);

	gtk_menu_popup(GTK_MENU(vd->popup), NULL, NULL, vd_menu_position_cb, vd, 0, GDK_CURRENT_TIME);

	return TRUE;
}

gint vdlist_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ViewDir *vd = data;
	GtkTreePath *tpath;
	GtkTreeIter iter;
	FileData *fd = NULL;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bevent->x, bevent->y,
					  &tpath, NULL, NULL, NULL))
		{
		GtkTreeModel *store;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DIR_COLUMN_POINTER, &fd, -1);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), tpath, NULL, FALSE);
		gtk_tree_path_free(tpath);
		}

	vd->click_fd = fd;
	vd_color_set(vd, vd->click_fd, TRUE);

	if (bevent->button == MOUSE_BUTTON_RIGHT)
		{
		vd->popup = vd_pop_menu(vd, vd->click_fd);
		gtk_menu_popup(GTK_MENU(vd->popup), NULL, NULL, NULL, NULL,
			       bevent->button, bevent->time);
		}

	return TRUE;
}

void vdlist_destroy_cb(GtkWidget *widget, gpointer data)
{
	ViewDir *vd = data;

	vd_dnd_drop_scroll_cancel(vd);
	widget_auto_scroll_stop(vd->view);

	filelist_free(VDLIST_INFO(vd, list));
}

ViewDir *vdlist_new(ViewDir *vd, FileData *dir_fd)
{
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	vd->info = g_new0(ViewDirInfoList, 1);
	vd->type = DIRVIEW_LIST;

	VDLIST_INFO(vd, list) = NULL;

	store = gtk_list_store_new(5, G_TYPE_POINTER, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
	vd->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(vd->view), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(vd->view), FALSE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(vd->view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", DIR_COLUMN_ICON);
	gtk_tree_view_column_set_cell_data_func(column, renderer, vd_color_cb, vd, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", DIR_COLUMN_NAME);
	gtk_tree_view_column_set_cell_data_func(column, renderer, vd_color_cb, vd, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", DIR_COLUMN_DATE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, vd_color_cb, vd, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(vd->view), column);

	return vd;
}
