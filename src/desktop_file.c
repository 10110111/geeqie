/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: Vladimir Nadvornik
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "desktop_file.h"

#include "editors.h"
#include "filedata.h"
#include "misc.h"
#include "ui_misc.h"
#include "ui_fileops.h"
#include "ui_utildlg.h"
#include "pixbuf_util.h"
#include "window.h"
#include "utilops.h"
#include "layout_util.h"

#define CONFIG_WINDOW_DEF_WIDTH		700
#define CONFIG_WINDOW_DEF_HEIGHT	400



typedef struct _EditorWindow EditorWindow;
struct _EditorWindow
{
	GtkWidget *window;
	GtkWidget *entry;
	GtkWidget *save_button;
	GtkTextBuffer *buffer;
	gchar *desktop_name;
	gboolean modified;
};


static GtkWidget *editor_list_window = NULL;
GtkWidget *editor_list_view = NULL;


static gboolean editor_window_save(EditorWindow *ew)
{
	gchar *path;
	gchar *text;
	GtkTextIter start, end;
	GError *error = NULL;
	gboolean ret = TRUE;
	const gchar *name = gtk_entry_get_text(GTK_ENTRY(ew->entry));
	
	if (!name || !name[0]) 
		{
		file_util_warning_dialog(_("Can't save"), _("Please specify file name."), GTK_STOCK_DIALOG_ERROR, NULL);
		return FALSE;
		}
	
	gtk_text_buffer_get_bounds(ew->buffer, &start, &end);
	text = gtk_text_buffer_get_text(ew->buffer, &start, &end, FALSE);

	path = g_build_filename(get_rc_dir(), "applications", name, NULL);

	if (!g_file_set_contents(path, text, -1, &error)) 
		{
		file_util_warning_dialog(_("Can't save"), error->message, GTK_STOCK_DIALOG_ERROR, NULL);
		g_error_free(error);
		ret = FALSE;
		}
	
	g_free(path);
	g_free(text);
	layout_editors_reload_all();
	return ret;
}

static void editor_window_close_cb(GtkWidget *widget, gpointer data)
{
	EditorWindow *ew = data;

	g_free(ew->desktop_name);
	gtk_widget_destroy(ew->window);
	g_free(ew);
}

static gint editor_window_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	editor_window_close_cb(w, data);
	return TRUE;
}

static void editor_window_save_cb(GtkWidget *widget, gpointer data)
{
	EditorWindow *ew = data;

	if (ew->modified)
		{
		editor_window_save(ew);
		}
}

static void editor_window_text_modified_cb(GtkWidget *widget, gpointer data)
{
	EditorWindow *ew = data;
	
	if (gtk_text_buffer_get_modified(ew->buffer))
		{
		gtk_widget_set_sensitive(ew->save_button, TRUE);
		ew->modified = TRUE;
		}
}

static void editor_window_entry_changed_cb(GtkWidget *widget, gpointer data)
{
	EditorWindow *ew = data;
	const gchar *content = gtk_entry_get_text(GTK_ENTRY(ew->entry));
	gboolean modified = (!ew->desktop_name && *content);

	if (!modified)
		{
		modified = strcmp(ew->desktop_name, content);
		}
	
	gtk_widget_set_sensitive(ew->save_button, modified);
	ew->modified = modified;
}

static void editor_window_new(const gchar *src_path, const gchar *desktop_name)
{
	EditorWindow *ew;
	GtkWidget *win_vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *ct_button;
	GtkWidget *button_hbox;
	GtkWidget *scrolled;
	GtkWidget *text_view;
	gchar *text;
	gsize size;

	ew = g_new0(EditorWindow, 1);


	ew->window = window_new(GTK_WINDOW_TOPLEVEL, "Desktop", PIXBUF_INLINE_ICON_CONFIG, NULL, _("Desktop file"));
	gtk_window_set_type_hint(GTK_WINDOW(ew->window), GDK_WINDOW_TYPE_HINT_DIALOG);

	g_signal_connect(G_OBJECT(ew->window), "delete_event",
			 G_CALLBACK(editor_window_delete_cb), ew);

	gtk_window_set_default_size(GTK_WINDOW(ew->window), CONFIG_WINDOW_DEF_WIDTH, CONFIG_WINDOW_DEF_HEIGHT);
	gtk_window_set_resizable(GTK_WINDOW(ew->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(ew->window), PREF_PAD_BORDER);

	win_vbox = gtk_vbox_new(FALSE, PREF_PAD_SPACE);
	gtk_container_add(GTK_CONTAINER(ew->window), win_vbox);
	gtk_widget_show(win_vbox);

	hbox = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
	gtk_box_pack_end(GTK_BOX(win_vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	ew->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), ew->entry, TRUE, TRUE, 0);
	ew->desktop_name = NULL;
	if (desktop_name)
		{
		gtk_entry_set_text(GTK_ENTRY(ew->entry), desktop_name);
		ew->desktop_name = g_strdup(desktop_name);
		}
	gtk_widget_show(ew->entry);
	g_signal_connect(G_OBJECT(ew->entry), "changed", G_CALLBACK(editor_window_entry_changed_cb), ew);

	button_hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(button_hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(button_hbox), PREF_PAD_BUTTON_GAP);
	gtk_box_pack_end(GTK_BOX(hbox), button_hbox, FALSE, FALSE, 0);
	gtk_widget_show(button_hbox);

	ew->save_button = pref_button_new(NULL, GTK_STOCK_SAVE, NULL, FALSE,
				 G_CALLBACK(editor_window_save_cb), ew);
	gtk_container_add(GTK_CONTAINER(button_hbox), ew->save_button);
	GTK_WIDGET_SET_FLAGS(ew->save_button, GTK_CAN_DEFAULT);
	gtk_widget_set_sensitive(ew->save_button, FALSE);
	gtk_widget_show(ew->save_button);
	ct_button = ew->save_button;

	button = pref_button_new(NULL, GTK_STOCK_CANCEL, NULL, FALSE,
				 G_CALLBACK(editor_window_close_cb), ew);
	gtk_container_add(GTK_CONTAINER(button_hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	if (!generic_dialog_get_alternative_button_order(ew->window))
		{
		gtk_box_reorder_child(GTK_BOX(button_hbox), ct_button, -1);
		}


	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(win_vbox), scrolled, TRUE, TRUE, 5);
	gtk_widget_show(scrolled);

	text_view = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scrolled), text_view);
	gtk_widget_show(text_view);

	ew->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
	if (g_file_get_contents(src_path, &text, &size, NULL))
		{
		gtk_text_buffer_set_text(ew->buffer, text, size);
		}
	gtk_text_buffer_set_modified(ew->buffer, FALSE);
	g_signal_connect(G_OBJECT(ew->buffer), "modified-changed",
			 G_CALLBACK(editor_window_text_modified_cb), ew);

	gtk_widget_show(ew->window);
}


static void editor_list_window_close_cb(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(editor_list_window);
	editor_list_window = NULL;
	editor_list_view = NULL;
}

static gboolean editor_list_window_delete(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	editor_list_window_close_cb(NULL, NULL);
	return TRUE;
}

static void editor_list_window_edit_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor_list_view)); 
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(sel, NULL, &iter)) 
		{
		GtkTreeModel *store = gtk_tree_view_get_model(GTK_TREE_VIEW(editor_list_view));
		gchar *path;
		gchar *key;

		gtk_tree_model_get(store, &iter,
				   DESKTOP_FILE_COLUMN_PATH, &path,
				   DESKTOP_FILE_COLUMN_KEY, &key, -1);
		editor_window_new(path, key);	
		g_free(key);
		g_free(path);
		}
}

static void editor_list_window_new_cb(GtkWidget *widget, gpointer data)
{
	editor_window_new(DESKTOP_FILE_TEMPLATE, _("new.desktop"));
}


static gint editor_list_window_sort_cb(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
	gint n = GPOINTER_TO_INT(data);
	gint ret = 0;

	switch (n)
		{
		case DESKTOP_FILE_COLUMN_KEY:
		case DESKTOP_FILE_COLUMN_NAME:
		case DESKTOP_FILE_COLUMN_PATH:
			{
			gchar *s1, *s2;

			gtk_tree_model_get(model, a, n, &s1, -1);
			gtk_tree_model_get(model, b, n, &s2, -1);

			if (!s1 || !s2)
				{
			  	if (!s1 && !s2) break;
			  	ret = s1 ? 1 : -1;
				}
			else
				{
			  	ret = g_utf8_collate(s1, s2);
				}

			g_free(s1);
			g_free(s2);
			}
			break;
	
		case DESKTOP_FILE_COLUMN_HIDDEN:
			{
			gint *v1, *v2;

			gtk_tree_model_get(model, a, n, &v1, -1);
			gtk_tree_model_get(model, b, n, &v2, -1);

			if (v1 == v2)
				ret = 0;
			else if (v1 < v2)
				ret = 1;
			else
				ret = -1;
			}
			break;

    		default:
       			g_return_val_if_reached(0);
		}

	return ret;
}

static void editor_list_window_create(void)
{
	GtkWidget *win_vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *scrolled;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreeModel *store;
	GtkTreeSortable *sortable;
	
	editor_list_window = window_new(GTK_WINDOW_TOPLEVEL, "editors", PIXBUF_INLINE_ICON_CONFIG, NULL, _("Editors"));
	gtk_window_set_type_hint(GTK_WINDOW(editor_list_window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(editor_list_window), "delete_event",
			 G_CALLBACK(editor_list_window_delete), NULL);
	gtk_window_set_default_size(GTK_WINDOW(editor_list_window), CONFIG_WINDOW_DEF_WIDTH, CONFIG_WINDOW_DEF_HEIGHT);
	gtk_window_set_resizable(GTK_WINDOW(editor_list_window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(editor_list_window), PREF_PAD_BORDER);

	win_vbox = gtk_vbox_new(FALSE, PREF_PAD_SPACE);
	gtk_container_add(GTK_CONTAINER(editor_list_window), win_vbox);
	gtk_widget_show(win_vbox);

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), PREF_PAD_BUTTON_GAP);
	gtk_box_pack_end(GTK_BOX(win_vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);


	button = pref_button_new(NULL, GTK_STOCK_NEW, NULL, FALSE,
				 G_CALLBACK(editor_list_window_new_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_EDIT, NULL, FALSE,
				 G_CALLBACK(editor_list_window_edit_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_CLOSE, NULL, FALSE,
				 G_CALLBACK(editor_list_window_close_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(win_vbox), scrolled, TRUE, TRUE, 5);
	gtk_widget_show(scrolled);

	editor_list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(desktop_file_list));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor_list_view));
	gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection), GTK_SELECTION_SINGLE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(editor_list_view), FALSE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Desktop file"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", DESKTOP_FILE_COLUMN_KEY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(editor_list_view), column);
	gtk_tree_view_column_set_sort_column_id(column, DESKTOP_FILE_COLUMN_KEY);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Hidden"));
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "active", DESKTOP_FILE_COLUMN_HIDDEN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(editor_list_view), column);
	gtk_tree_view_column_set_sort_column_id(column, DESKTOP_FILE_COLUMN_HIDDEN);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Name"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", DESKTOP_FILE_COLUMN_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(editor_list_view), column);
	gtk_tree_view_column_set_sort_column_id(column, DESKTOP_FILE_COLUMN_NAME);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Path"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", DESKTOP_FILE_COLUMN_PATH);
	gtk_tree_view_append_column(GTK_TREE_VIEW(editor_list_view), column);
	gtk_tree_view_column_set_sort_column_id(column, DESKTOP_FILE_COLUMN_PATH);

	/* set up sorting */
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(editor_list_view));
	sortable = GTK_TREE_SORTABLE(store);
	gtk_tree_sortable_set_sort_func(sortable, DESKTOP_FILE_COLUMN_KEY, editor_list_window_sort_cb,
					GINT_TO_POINTER(DESKTOP_FILE_COLUMN_KEY), NULL);
	gtk_tree_sortable_set_sort_func(sortable, DESKTOP_FILE_COLUMN_HIDDEN, editor_list_window_sort_cb,
					GINT_TO_POINTER(DESKTOP_FILE_COLUMN_HIDDEN), NULL);
	gtk_tree_sortable_set_sort_func(sortable, DESKTOP_FILE_COLUMN_NAME, editor_list_window_sort_cb,
					GINT_TO_POINTER(DESKTOP_FILE_COLUMN_NAME), NULL);
	gtk_tree_sortable_set_sort_func(sortable, DESKTOP_FILE_COLUMN_PATH, editor_list_window_sort_cb,
					GINT_TO_POINTER(DESKTOP_FILE_COLUMN_PATH), NULL);

	/* set initial sort order */
    	gtk_tree_sortable_set_sort_column_id(sortable, DESKTOP_FILE_COLUMN_KEY, GTK_SORT_ASCENDING);

	gtk_container_add(GTK_CONTAINER(scrolled), editor_list_view);
	gtk_widget_show(editor_list_view);

	gtk_widget_show(editor_list_window);
}

/*
 *-----------------------------------------------------------------------------
 * config window show (public)
 *-----------------------------------------------------------------------------
 */

void show_editor_list_window(void)
{
	if (editor_list_window)
		{
		gtk_window_present(GTK_WINDOW(editor_list_window));
		return;
		}

	editor_list_window_create();
}
