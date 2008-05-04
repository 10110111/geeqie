/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "view_file.h"

#include "debug.h"
#include "view_file_list.h"
#include "view_file_icon.h"


void vf_thumb_set(ViewFile *vf, gint enable);
void vf_marks_set(ViewFile *vf, gint enable);
void vf_sort_set(ViewFile *vf, SortType type, gint ascend);

FileData *vf_index_get_data(ViewFile *vf, gint row);
gchar *vf_index_get_path(ViewFile *vf, gint row);
gint vf_index_by_path(ViewFile *vf, const gchar *path);
gint vf_index_by_fd(ViewFile *vf, FileData *in_fd);
gint vf_count(ViewFile *vf, gint64 *bytes);
GList *vf_get_list(ViewFile *vf);

gint vf_index_is_selected(ViewFile *vf, gint row);
gint vf_selection_count(ViewFile *vf, gint64 *bytes);
GList *vf_selection_get_list(ViewFile *vf);
GList *vf_selection_get_list_by_index(ViewFile *vf);

void vf_select_all(ViewFile *vf);
void vf_select_none(ViewFile *vf);
void vf_select_by_path(ViewFile *vf, const gchar *path);
void vf_select_by_fd(ViewFile *vf, FileData *fd);

void vf_mark_to_selection(ViewFile *vf, gint mark, MarkToSelectionMode mode);
void vf_selection_to_mark(ViewFile *vf, gint mark, SelectionToMarkMode mode);

void vf_select_marked(ViewFile *vf, gint mark);
void vf_mark_selected(ViewFile *vf, gint mark, gint value);

gint vf_maint_renamed(ViewFile *vf, FileData *fd);
gint vf_maint_removed(ViewFile *vf, FileData *fd, GList *ignore_list);
gint vf_maint_moved(ViewFile *vf, FileData *fd, GList *ignore_list);


static gint vf_press_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	gint ret = FALSE;

	return ret;
}

static gint vf_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	gint ret = FALSE;

	return ret;
}

static gint vf_release_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	gint ret = FALSE;

	return ret;
}

static void vf_dnd_init(ViewFile *vf)
{
}

gint vf_refresh(ViewFile *vf)
{
	gint ret = TRUE;

	return ret;
}

gint vf_set_path(ViewFile *vf, const gchar *path)
{
	return FALSE;
}

static void vf_destroy_cb(GtkWidget *widget, gpointer data)
{
	ViewFile *vf = data;

	if (vf->popup)
		{
		g_signal_handlers_disconnect_matched(G_OBJECT(vf->popup), G_SIGNAL_MATCH_DATA,
						     0, 0, 0, NULL, vf);
		gtk_widget_destroy(vf->popup);
		}

	g_free(vf->path);
	g_free(vf);
}

ViewFile *vf_new(FileViewType type, const gchar *path)
{
	ViewFile *vf;

	vf = g_new0(ViewFile, 1);

	vf->path = NULL;
	vf->list = NULL;

	vf->sort_method = SORT_NAME;
	vf->sort_ascend = TRUE;
	
	vf->thumbs_running = FALSE;
	vf->thumbs_count = 0;
	vf->thumbs_loader = NULL;
	vf->thumbs_filedata = NULL;

	vf->popup = NULL;

	vf->widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(vf->widget), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(vf->widget),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	vf->listview = NULL; /* FIXME */

	g_signal_connect(G_OBJECT(vf->widget), "destroy",
			 G_CALLBACK(vf_destroy_cb), vf);

	g_signal_connect(G_OBJECT(vf->listview), "key_press_event",
			 G_CALLBACK(vf_press_key_cb), vf);
	g_signal_connect(G_OBJECT(vf->listview), "button_press_event",
			 G_CALLBACK(vf_press_cb), vf);
	g_signal_connect(G_OBJECT(vf->listview), "button_release_event",
			 G_CALLBACK(vf_release_cb), vf);

	gtk_container_add(GTK_CONTAINER(vf->widget), vf->listview);
	gtk_widget_show(vf->listview);

	vf_dnd_init(vf);

	if (path) vf_set_path(vf, path);

	return vf;
}

void vf_set_status_func(ViewFile *vf, void (*func)(ViewFile *vf, gpointer data), gpointer data)
{
	vf->func_status = func;
	vf->data_status = data;
}

void vf_set_thumb_status_func(ViewFile *vf, void (*func)(ViewFile *vf, gdouble val, const gchar *text, gpointer data), gpointer data)
{
	vf->func_thumb_status = func;
	vf->data_thumb_status = data;
}

void vf_set_layout(ViewFile *vf, LayoutWindow *layout)
{
	vf->layout = layout;
}
