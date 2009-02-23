/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "advanced_exif.h"

#include "exif.h"
#include "metadata.h"
#include "filedata.h"
#include "history_list.h"
#include "misc.h"
#include "ui_misc.h"
#include "window.h"

/* FIXME: not needed when bar_exif.c is improved */
#include "bar_exif.h"

#include <math.h>

#define ADVANCED_EXIF_DATA_COLUMN_WIDTH 200

/*
 *-------------------------------------------------------------------
 * EXIF window
 *-------------------------------------------------------------------
 */

typedef struct _ExifWin ExifWin;
struct _ExifWin
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *scrolled;
	GtkWidget *listview;

	FileData *fd;
};

enum {
	EXIF_ADVCOL_ENABLED = 0,
	EXIF_ADVCOL_TAG,
	EXIF_ADVCOL_NAME,
	EXIF_ADVCOL_VALUE,
	EXIF_ADVCOL_FORMAT,
	EXIF_ADVCOL_ELEMENTS,
	EXIF_ADVCOL_DESCRIPTION,
	EXIF_ADVCOL_COUNT
};

static gint advanced_exif_row_enabled(const gchar *name)
{
	GList *list;

	if (!name) return FALSE;

	list = history_list_get_by_key("exif_extras");
	while (list)
		{
		if (strcmp(name, (gchar *)(list->data)) == 0) return TRUE;
		list = list->next;
	}

	return FALSE;
}

static void advanced_exif_update(ExifWin *ew)
{
	ExifData *exif;

	GtkListStore *store;
	GtkTreeIter iter;
	ExifData *exif_original;
	ExifItem *item;

	exif = exif_read_fd(ew->fd);
	
	gtk_widget_set_sensitive(ew->scrolled, !!exif);

	if (!exif) return;
	
	exif_original = exif_get_original(exif);

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ew->listview)));
	gtk_list_store_clear(store);

	item = exif_get_first_item(exif_original);
	while (item)
		{
		gchar *tag;
		gchar *tag_name;
		gchar *text;
		gchar *utf8_text;
		const gchar *format;
		gchar *elements;
		gchar *description;

		tag = g_strdup_printf("0x%04x", exif_item_get_tag_id(item));
		tag_name = exif_item_get_tag_name(item);
		format = exif_item_get_format_name(item, TRUE);
		text = exif_item_get_data_as_text(item);
		utf8_text = utf8_validate_or_convert(text);
		g_free(text);
		elements = g_strdup_printf("%d", exif_item_get_elements(item));
		description = exif_item_get_description(item);
		if (!description || *description == '\0') 
			{
			g_free(description);
			description = g_strdup(tag_name);
			}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				EXIF_ADVCOL_ENABLED, advanced_exif_row_enabled(tag_name),
				EXIF_ADVCOL_TAG, tag,
				EXIF_ADVCOL_NAME, tag_name,
				EXIF_ADVCOL_VALUE, utf8_text,
				EXIF_ADVCOL_FORMAT, format,
				EXIF_ADVCOL_ELEMENTS, elements,
				EXIF_ADVCOL_DESCRIPTION, description, -1);
		g_free(tag);
		g_free(utf8_text);
		g_free(elements);
		g_free(description);
		g_free(tag_name);
		item = exif_get_next_item(exif_original);
		}
	exif_free_fd(ew->fd, exif);

}

static void advanced_exif_clear(ExifWin *ew)
{
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ew->listview)));
	gtk_list_store_clear(store);
}

void advanced_exif_set_fd(GtkWidget *window, FileData *fd)
{
	ExifWin *ew;

	ew = g_object_get_data(G_OBJECT(window), "advanced_exif_data");
	if (!ew) return;

	/* store this, advanced view toggle needs to reload data */
	file_data_unref(ew->fd);
	ew->fd = file_data_ref(fd);

	advanced_exif_clear(ew);
	advanced_exif_update(ew);
}

static void advanced_exif_row_toggled_cb(GtkCellRendererToggle *toggle, const gchar *path, gpointer data)
{
	GtkWidget *listview = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreePath *tpath;
	gchar *name = NULL;
	gboolean active;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(listview));

	tpath = gtk_tree_path_new_from_string(path);
	gtk_tree_model_get_iter(store, &iter, tpath);
	gtk_tree_path_free(tpath);

	gtk_tree_model_get(store, &iter, EXIF_ADVCOL_ENABLED, &active,
					 EXIF_ADVCOL_NAME, &name, -1);
	active = (!active);

	if (active &&
	    g_list_length(history_list_get_by_key("exif_extras")) >= EXIF_BAR_CUSTOM_COUNT)
		{
		active = FALSE;
		}

	gtk_list_store_set(GTK_LIST_STORE(store), &iter, EXIF_ADVCOL_ENABLED, active, -1);

	if (active)
		{
		history_list_add_to_key("exif_extras", name, EXIF_BAR_CUSTOM_COUNT);
		}
	else
		{
		history_list_item_change("exif_extras", name, NULL);
		}

	g_free(name);
}

static void advanced_exif_add_column_check(GtkWidget *listview, const gchar *title, gint n)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, title);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "active", n);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(advanced_exif_row_toggled_cb), listview);
}

static void advanced_exif_add_column(GtkWidget *listview, const gchar *title, gint n, gint sizable)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, title);

	if (sizable)
		{
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
		gtk_tree_view_column_set_fixed_width(column, ADVANCED_EXIF_DATA_COLUMN_WIDTH);
		gtk_tree_view_column_set_resizable(column, TRUE);
		}
	else
		{
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
		}

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", n);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
}

void advanced_exif_close(GtkWidget *window)
{
	ExifWin *ew;

	ew = g_object_get_data(G_OBJECT(window), "advanced_exif_data");
	if (!ew) return;

	gtk_widget_destroy(ew->vbox);
}

static void advanced_exif_destroy(GtkWidget *widget, gpointer data)
{
	ExifWin *ew = data;
	file_data_unref(ew->fd);
	g_free(ew);
}

GtkWidget *advanced_exif_new(void)
{
	ExifWin *ew;
	GtkListStore *store;
	GdkGeometry geometry;

	ew = g_new0(ExifWin, 1);


	ew->window = window_new(GTK_WINDOW_TOPLEVEL, "view", NULL, NULL, _("Metadata"));

	geometry.min_width = 900;
	geometry.min_height = 600;
	gtk_window_set_geometry_hints(GTK_WINDOW(ew->window), NULL, &geometry, GDK_HINT_MIN_SIZE);

	gtk_window_set_resizable(GTK_WINDOW(ew->window), TRUE);

	g_object_set_data(G_OBJECT(ew->window), "advanced_exif_data", ew);
	g_signal_connect_after(G_OBJECT(ew->window), "destroy",
			       G_CALLBACK(advanced_exif_destroy), ew);

	ew->vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	gtk_container_add(GTK_CONTAINER(ew->window), ew->vbox);
	gtk_widget_show(ew->vbox);


	store = gtk_list_store_new(7, G_TYPE_BOOLEAN,
				      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	ew->listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(ew->listview), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ew->listview), TRUE);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(ew->listview), EXIF_ADVCOL_NAME);

	advanced_exif_add_column_check(ew->listview, "", EXIF_ADVCOL_ENABLED);

	advanced_exif_add_column(ew->listview, _("Description"), EXIF_ADVCOL_DESCRIPTION, FALSE);
	advanced_exif_add_column(ew->listview, _("Value"), EXIF_ADVCOL_VALUE, TRUE);
	advanced_exif_add_column(ew->listview, _("Name"), EXIF_ADVCOL_NAME, FALSE);
	advanced_exif_add_column(ew->listview, _("Tag"), EXIF_ADVCOL_TAG, FALSE);
	advanced_exif_add_column(ew->listview, _("Format"), EXIF_ADVCOL_FORMAT, FALSE);
	advanced_exif_add_column(ew->listview, _("Elements"), EXIF_ADVCOL_ELEMENTS, FALSE);

	ew->scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ew->scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ew->scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(ew->vbox), ew->scrolled, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(ew->scrolled), ew->listview);
	gtk_widget_show(ew->listview);
	gtk_widget_show(ew->scrolled);

	gtk_widget_show(ew->window);
	return ew->window;
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */