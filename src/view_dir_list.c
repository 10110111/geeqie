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

#include "main.h"
#include "view_dir_list.h"

#include "dnd.h"
#include "dupe.h"
#include "filelist.h"
#include "layout.h"
#include "layout_image.h"
#include "layout_util.h"
#include "utilops.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_tree_edit.h"
#include "view_dir.h"

#include <gdk/gdkkeysyms.h> /* for keyboard values */


#define VDLIST_PAD 4

#define VDLIST_INFO(_vd_, _part_) (((ViewDirInfoList *)(_vd_->info))->_part_)


static gint vdlist_auto_scroll_notify_cb(GtkWidget *widget, gint x, gint y, gpointer data);

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

static GtkTargetEntry vdlist_dnd_drop_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
static gint vdlist_dnd_drop_types_count = 1;

static void vdlist_dest_set(ViewDir *vd, gint enable)
{
	if (enable)
		{
		gtk_drag_dest_set(vd->view,
				  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
				  vdlist_dnd_drop_types, vdlist_dnd_drop_types_count,
				  GDK_ACTION_MOVE | GDK_ACTION_COPY);
		}
	else
		{
		gtk_drag_dest_unset(vd->view);
		}
}

static void vdlist_dnd_get(GtkWidget *widget, GdkDragContext *context,
			   GtkSelectionData *selection_data, guint info,
			   guint time, gpointer data)
{
	ViewDir *vd = data;
	GList *list;
	gchar *text = NULL;
	gint length = 0;

	if (!vd->click_fd) return;

	switch (info)
		{
		case TARGET_URI_LIST:
		case TARGET_TEXT_PLAIN:
			list = g_list_prepend(NULL, vd->click_fd);
			text = uri_text_from_filelist(list, &length, (info == TARGET_TEXT_PLAIN));
			g_list_free(list);
			break;
		}
	if (text)
		{
		gtk_selection_data_set (selection_data, selection_data->target,
				8, (guchar *)text, length);
		g_free(text);
		}
}

static void vdlist_dnd_begin(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	ViewDir *vd = data;

	vd_color_set(vd, vd->click_fd, TRUE);
	vdlist_dest_set(vd, FALSE);
}

static void vdlist_dnd_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	ViewDir *vd = data;

	vd_color_set(vd, vd->click_fd, FALSE);

	if (context->action == GDK_ACTION_MOVE)
		{
		vdlist_refresh(vd);
		}
	vdlist_dest_set(vd, TRUE);
}

static void vdlist_dnd_drop_receive(GtkWidget *widget,
				    GdkDragContext *context, gint x, gint y,
				    GtkSelectionData *selection_data, guint info,
				    guint time, gpointer data)
{
	ViewDir *vd = data;
	GtkTreePath *tpath;
	GtkTreeIter iter;
	FileData *fd = NULL;

	vd->click_fd = NULL;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y,
					  &tpath, NULL, NULL, NULL))
		{
		GtkTreeModel *store;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DIR_COLUMN_POINTER, &fd, -1);
		gtk_tree_path_free(tpath);
		}

	if (!fd) return;

	if (info == TARGET_URI_LIST)
		{
		GList *list;
		gint active;

		list = uri_filelist_from_text((gchar *)selection_data->data, TRUE);
		if (!list) return;

		active = access_file(fd->path, W_OK | X_OK);

		vd_color_set(vd, fd, TRUE);
		vd->popup = vd_drop_menu(vd, active);
		gtk_menu_popup(GTK_MENU(vd->popup), NULL, NULL, NULL, NULL, 0, time);

		vd->drop_fd = fd;
		vd->drop_list = list;
		}
}

#if 0
static gint vdlist_get_row_visibility(ViewDir *vd, FileData *fd)
{
	GtkTreeModel *store;
	GtkTreeViewColumn *column;
	GtkTreePath *tpath;
	GtkTreeIter iter;

	GdkRectangle vrect;
	GdkRectangle crect;

	if (!fd || vd_find_row(vd, fd, &iter) < 0) return 0;

	column = gtk_tree_view_get_column(GTK_TREE_VIEW(vd->view), 0);
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
	tpath = gtk_tree_model_get_path(store, &iter);

	gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(vd->view), &vrect);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(vd->view), tpath, column, &crect);
printf("window: %d + %d; cell: %d + %d\n", vrect.y, vrect.height, crect.y, crect.height);
	gtk_tree_path_free(tpath);

	if (crect.y + crect.height < vrect.y) return -1;
	if (crect.y > vrect.y + vrect.height) return 1;
	return 0;
}
#endif

static void vdlist_scroll_to_row(ViewDir *vd, FileData *fd, gfloat y_align)
{
	GtkTreeIter iter;

	if (GTK_WIDGET_REALIZED(vd->view) &&
	    vd_find_row(vd, fd, &iter) >= 0)
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

static void vdlist_drop_update(ViewDir *vd, gint x, gint y)
{
	GtkTreePath *tpath;
	GtkTreeIter iter;
	FileData *fd = NULL;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(vd->view), x, y,
					  &tpath, NULL, NULL, NULL))
		{
		GtkTreeModel *store;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DIR_COLUMN_POINTER, &fd, -1);
		gtk_tree_path_free(tpath);
		}

	if (fd != vd->drop_fd)
		{
		vd_color_set(vd, vd->drop_fd, FALSE);
		vd_color_set(vd, fd, TRUE);
		}

	vd->drop_fd = fd;
}

static void vdlist_dnd_drop_scroll_cancel(ViewDir *vd)
{
	if (vd->drop_scroll_id != -1) g_source_remove(vd->drop_scroll_id);
	vd->drop_scroll_id = -1;
}

static gint vdlist_auto_scroll_idle_cb(gpointer data)
{
	ViewDir *vd = data;

	if (vd->drop_fd)
		{
		GdkWindow *window;
		gint x, y;
		gint w, h;

		window = vd->view->window;
		gdk_window_get_pointer(window, &x, &y, NULL);
		gdk_drawable_get_size(window, &w, &h);
		if (x >= 0 && x < w && y >= 0 && y < h)
			{
			vdlist_drop_update(vd, x, y);
			}
		}

	vd->drop_scroll_id = -1;
	return FALSE;
}

static gint vdlist_auto_scroll_notify_cb(GtkWidget *widget, gint x, gint y, gpointer data)
{
	ViewDir *vd = data;

	if (!vd->drop_fd || vd->drop_list) return FALSE;

	if (vd->drop_scroll_id == -1) vd->drop_scroll_id = g_idle_add(vdlist_auto_scroll_idle_cb, vd);

	return TRUE;
}

static gint vdlist_dnd_drop_motion(GtkWidget *widget, GdkDragContext *context,
				   gint x, gint y, guint time, gpointer data)
{
	ViewDir *vd = data;

	vd->click_fd = NULL;

	if (gtk_drag_get_source_widget(context) == vd->view)
		{
		/* from same window */
		gdk_drag_status(context, 0, time);
		return TRUE;
		}
	else
		{
		gdk_drag_status(context, context->suggested_action, time);
		}

	vdlist_drop_update(vd, x, y);

        if (vd->drop_fd)
		{
		GtkAdjustment *adj = gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(vd->view));
		widget_auto_scroll_start(vd->view, adj, -1, -1, vdlist_auto_scroll_notify_cb, vd);
		}

	return FALSE;
}

static void vdlist_dnd_drop_leave(GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	ViewDir *vd = data;

	if (vd->drop_fd != vd->click_fd) vd_color_set(vd, vd->drop_fd, FALSE);

	vd->drop_fd = NULL;
}

static void vdlist_dnd_init(ViewDir *vd)
{
	gtk_drag_source_set(vd->view, GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			    dnd_file_drag_types, dnd_file_drag_types_count,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(vd->view), "drag_data_get",
			 G_CALLBACK(vdlist_dnd_get), vd);
	g_signal_connect(G_OBJECT(vd->view), "drag_begin",
			 G_CALLBACK(vdlist_dnd_begin), vd);
	g_signal_connect(G_OBJECT(vd->view), "drag_end",
			 G_CALLBACK(vdlist_dnd_end), vd);

	vdlist_dest_set(vd, TRUE);
	g_signal_connect(G_OBJECT(vd->view), "drag_data_received",
			 G_CALLBACK(vdlist_dnd_drop_receive), vd);
	g_signal_connect(G_OBJECT(vd->view), "drag_motion",
			 G_CALLBACK(vdlist_dnd_drop_motion), vd);
	g_signal_connect(G_OBJECT(vd->view), "drag_leave",
			 G_CALLBACK(vdlist_dnd_drop_leave), vd);
}

/*
 *-----------------------------------------------------------------------------
 * main
 *-----------------------------------------------------------------------------
 */ 

static void vdlist_select_row(ViewDir *vd, FileData *fd)
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
				   DIR_COLUMN_NAME, fd->name, -1);

		work = work->next;
		}

	vd->click_fd = NULL;
	vd->drop_fd = NULL;
}

gint vdlist_set_path(ViewDir *vd, const gchar *path)
{
	gint ret;
	FileData *fd;
	gchar *old_path = NULL;
	gchar *filepath;

	if (!path) return FALSE;
	if (vd->path && strcmp(path, vd->path) == 0) return TRUE;

	if (vd->path)
		{
		gchar *base;

		base = remove_level_from_path(vd->path);
		if (strcmp(base, path) == 0)
			{
			old_path = g_strdup(filename_from_path(vd->path));
			}
		g_free(base);
		}

	g_free(vd->path);
	vd->path = g_strdup(path);

	filelist_free(VDLIST_INFO(vd, list));
	VDLIST_INFO(vd, list) = NULL;

	ret = filelist_read(vd->path, NULL, &VDLIST_INFO(vd, list));

	VDLIST_INFO(vd, list) = filelist_sort(VDLIST_INFO(vd, list), SORT_NAME, TRUE);

	/* add . and .. */

	if (strcmp(vd->path, "/") != 0)
		{
		filepath = g_strconcat(vd->path, "/", "..", NULL); 
		fd = file_data_new_simple(filepath);
		VDLIST_INFO(vd, list) = g_list_prepend(VDLIST_INFO(vd, list), fd);
		g_free(filepath);
		}
	
	if (options->file_filter.show_dot_directory)
		{
		filepath = g_strconcat(vd->path, "/", ".", NULL); 
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
	gchar *path;

	path = g_strdup(vd->path);
	vd->path = NULL;
	vdlist_set_path(vd, path);
	g_free(path);
}

static void vdlist_menu_position_cb(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	ViewDir *vd = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreePath *tpath;
	gint cw, ch;

	if (vd_find_row(vd, vd->click_fd, &iter) < 0) return;
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(vd->view));
	tpath = gtk_tree_model_get_path(store, &iter);
	tree_view_get_cell_clamped(GTK_TREE_VIEW(vd->view), tpath, 0, TRUE, x, y, &cw, &ch);
	gtk_tree_path_free(tpath);
	*y += ch;
	popup_menu_position_clamp(menu, x, y, 0);
}

static gint vdlist_press_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
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

	gtk_menu_popup(GTK_MENU(vd->popup), NULL, NULL, vdlist_menu_position_cb, vd, 0, GDK_CURRENT_TIME);

	return TRUE;
}

static gint vdlist_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
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

	if (bevent->button == 3)
		{
		vd->popup = vd_pop_menu(vd, vd->click_fd);
		gtk_menu_popup(GTK_MENU(vd->popup), NULL, NULL, NULL, NULL,
			       bevent->button, bevent->time);
		}

	return TRUE;
}

static gint vdlist_release_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	ViewDir *vd = data;
	GtkTreePath *tpath;
	GtkTreeIter iter;
	FileData *fd = NULL;

	vd_color_set(vd, vd->click_fd, FALSE);

	if (bevent->button != 1) return TRUE;

	if ((bevent->x != 0 || bevent->y != 0) &&
	    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bevent->x, bevent->y,
					  &tpath, NULL, NULL, NULL))
		{
		GtkTreeModel *store;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DIR_COLUMN_POINTER, &fd, -1);
		gtk_tree_path_free(tpath);
		}

	if (fd && vd->click_fd == fd)
		{
		vdlist_select_row(vd, vd->click_fd);
		}

	return TRUE;
}

static void vdlist_select_cb(GtkTreeView *tview, GtkTreePath *tpath, GtkTreeViewColumn *column, gpointer data)
{
	ViewDir *vd = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	FileData *fd;

	store = gtk_tree_view_get_model(tview);
	gtk_tree_model_get_iter(store, &iter, tpath);
	gtk_tree_model_get(store, &iter, DIR_COLUMN_POINTER, &fd, -1);

	vdlist_select_row(vd, fd);
}

static GdkColor *vdlist_color_shifted(GtkWidget *widget)
{
	static GdkColor color;
	static GtkWidget *done = NULL;

	if (done != widget)
		{
		GtkStyle *style;
		
		style = gtk_widget_get_style(widget);
		memcpy(&color, &style->base[GTK_STATE_NORMAL], sizeof(color));
		shift_color(&color, -1, 0);
		done = widget;
		}

	return &color;
}

static void vdlist_color_cb(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
			    GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	ViewDir *vd = data;
	gboolean set;

	gtk_tree_model_get(tree_model, iter, DIR_COLUMN_COLOR, &set, -1);
	g_object_set(G_OBJECT(cell),
		     "cell-background-gdk", vdlist_color_shifted(vd->view),
		     "cell-background-set", set, NULL);
}

static void vdlist_destroy_cb(GtkWidget *widget, gpointer data)
{
	ViewDir *vd = data;

	vdlist_dnd_drop_scroll_cancel(vd);
	widget_auto_scroll_stop(vd->view);

	filelist_free(VDLIST_INFO(vd, list));
}

ViewDir *vdlist_new(ViewDir *vd, const gchar *path)
{
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	vd->info = g_new0(ViewDirInfoList, 1);
	vd->type = DIRVIEW_LIST;
	vd->widget_destroy_cb = vdlist_destroy_cb;

	VDLIST_INFO(vd, list) = NULL;

	store = gtk_list_store_new(4, G_TYPE_POINTER, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_BOOLEAN);
	vd->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(vd->view), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(vd->view), FALSE);
	g_signal_connect(G_OBJECT(vd->view), "row_activated",

			 G_CALLBACK(vdlist_select_cb), vd);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(vd->view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", DIR_COLUMN_ICON);
	gtk_tree_view_column_set_cell_data_func(column, renderer, vdlist_color_cb, vd, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", DIR_COLUMN_NAME);
	gtk_tree_view_column_set_cell_data_func(column, renderer, vdlist_color_cb, vd, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(vd->view), column);

	g_signal_connect(G_OBJECT(vd->view), "key_press_event",
			   G_CALLBACK(vdlist_press_key_cb), vd);
	gtk_container_add(GTK_CONTAINER(vd->widget), vd->view);
	gtk_widget_show(vd->view);

	vdlist_dnd_init(vd);

	g_signal_connect(G_OBJECT(vd->view), "button_press_event",
			 G_CALLBACK(vdlist_press_cb), vd);
	g_signal_connect(G_OBJECT(vd->view), "button_release_event",
			 G_CALLBACK(vdlist_release_cb), vd);

	if (path) vdlist_set_path(vd, path);

	return vd;
}
