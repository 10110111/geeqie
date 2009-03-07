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

#include <glib/gprintf.h>

#include "main.h"
#include "bar_keywords.h"

#include "filedata.h"
#include "history_list.h"
#include "metadata.h"
#include "misc.h"
#include "ui_fileops.h"
#include "ui_misc.h"
#include "ui_utildlg.h"
#include "utilops.h"
#include "bar.h"
#include "ui_menu.h"
#include "rcfile.h"
#include "layout.h"

static const gchar *keyword_favorite_defaults[] = {
	N_("Favorite"),
	N_("Todo"),
	N_("People"),
	N_("Places"),
	N_("Art"),
	N_("Nature"),
	N_("Possessions"),
	NULL
};


static void bar_pane_keywords_keyword_update_all(void);
static void bar_pane_keywords_changed(GtkTextBuffer *buffer, gpointer data);

/*
 *-------------------------------------------------------------------
 * keyword / comment utils
 *-------------------------------------------------------------------
 */


GList *keyword_list_pull(GtkWidget *text_widget)
{
	GList *list;
	gchar *text;

	text = text_widget_text_pull(text_widget);
	list = string_to_keywords_list(text);

	g_free(text);

	return list;
}

static void keyword_list_push(GtkWidget *textview, GList *list)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_delete(buffer, &start, &end);

	while (list)
		{
		const gchar *word = list->data;
		GtkTextIter iter;

		gtk_text_buffer_get_end_iter(buffer, &iter);
		if (word) gtk_text_buffer_insert(buffer, &iter, word, -1);
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_insert(buffer, &iter, "\n", -1);

		list = list->next;
		}
}


/*
 *-------------------------------------------------------------------
 * info bar
 *-------------------------------------------------------------------
 */


enum {
	FILTER_KEYWORD_COLUMN_TOGGLE = 0,
	FILTER_KEYWORD_COLUMN_MARK,
	FILTER_KEYWORD_COLUMN_NAME,
	FILTER_KEYWORD_COLUMN_IS_KEYWORD,
	FILTER_KEYWORD_COLUMN_COUNT
};

static GType filter_keyword_column_types[] = {G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN};

typedef struct _PaneKeywordsData PaneKeywordsData;
struct _PaneKeywordsData
{
	PaneData pane;
	GtkWidget *widget;

	GtkWidget *keyword_view;
	GtkWidget *keyword_treeview;

	FileData *fd;
	gchar *key;
};


static GList *bar_list = NULL;


static void bar_pane_keywords_write(PaneKeywordsData *pkd)
{
	GList *list;

	if (!pkd->fd) return;

	list = keyword_list_pull(pkd->keyword_view);

	metadata_write_list(pkd->fd, KEYWORD_KEY, list);

	string_list_free(list);
}

static gchar *bar_pane_keywords_get_mark_text(const gchar *key)
{
	gint i;
	static gchar buf[10];
	
	for (i = 0; i < FILEDATA_MARKS_SIZE; i++)
		{
		FileDataGetMarkFunc get_mark_func;
		FileDataSetMarkFunc set_mark_func;
		gpointer data;
		file_data_get_registered_mark_func(i, &get_mark_func, &set_mark_func, &data);
		if (get_mark_func == meta_data_get_keyword_mark && strcmp(data, key) == 0) 
			{
			g_sprintf(buf, " %d ", i + 1);
			return buf;
			}
		}
	return " ... ";
}

gboolean bar_keyword_tree_expand_if_set(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	PaneKeywordsData *pkd = data;
	gboolean set;

	gtk_tree_model_get(model, iter, FILTER_KEYWORD_COLUMN_TOGGLE, &set, -1);
	
	if (set && !gtk_tree_view_row_expanded(GTK_TREE_VIEW(pkd->keyword_treeview), path))
		{
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(pkd->keyword_treeview), path);
		}
	return FALSE;
}

static void bar_keyword_tree_sync(PaneKeywordsData *pkd)
{
	GtkTreeModelFilter *store;

	store = GTK_TREE_MODEL_FILTER(gtk_tree_view_get_model(GTK_TREE_VIEW(pkd->keyword_treeview)));

	gtk_tree_model_filter_refilter(store);
	gtk_tree_model_foreach(GTK_TREE_MODEL(store), bar_keyword_tree_expand_if_set, pkd);
	
}

static void bar_pane_keywords_keyword_update_all(void)
{
	GList *work;

	work = bar_list;
	while (work)
		{
		PaneKeywordsData *pkd;
//		GList *keywords;

		pkd = work->data;
		work = work->next;

		bar_keyword_tree_sync(pkd);
		}
}

static void bar_pane_keywords_update(PaneKeywordsData *pkd)
{
	GList *keywords = NULL;
	GtkTextBuffer *keyword_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pkd->keyword_view));

	g_signal_handlers_block_by_func(keyword_buffer, bar_pane_keywords_changed, pkd);

	keywords = metadata_read_list(pkd->fd, KEYWORD_KEY, METADATA_PLAIN);
	keyword_list_push(pkd->keyword_view, keywords);
	bar_keyword_tree_sync(pkd);
	string_list_free(keywords);
	
	g_signal_handlers_unblock_by_func(keyword_buffer, bar_pane_keywords_changed, pkd);

}

void bar_pane_keywords_set_fd(GtkWidget *pane, FileData *fd)
{
	PaneKeywordsData *pkd;

	pkd = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!pkd) return;

	file_data_unref(pkd->fd);
	pkd->fd = file_data_ref(fd);

	bar_pane_keywords_update(pkd);
}

static void bar_pane_keywords_write_config(GtkWidget *pane, GString *outstr, gint indent)
{
	PaneKeywordsData *pkd;

	pkd = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!pkd) return;

	WRITE_STRING("<pane_keywords\n");
	indent++;
	write_char_option(outstr, indent, "pane.title", gtk_label_get_text(GTK_LABEL(pkd->pane.title)));
	WRITE_BOOL(*pkd, pane.expanded);
	WRITE_CHAR(*pkd, key);
	indent--;
	WRITE_STRING("/>\n");
}

gint bar_pane_keywords_event(GtkWidget *bar, GdkEvent *event)
{
	PaneKeywordsData *pkd;

	pkd = g_object_get_data(G_OBJECT(bar), "pane_data");
	if (!pkd) return FALSE;

	if (GTK_WIDGET_HAS_FOCUS(pkd->keyword_view)) return gtk_widget_event(pkd->keyword_view, event);

	return FALSE;
}

static void bar_pane_keywords_keyword_toggle(GtkCellRendererToggle *toggle, const gchar *path, gpointer data)
{
	PaneKeywordsData *pkd = data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *tpath;
	gboolean active;
	GList *list;
	GtkTreeIter child_iter;
	GtkTreeModel *keyword_tree;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(pkd->keyword_treeview));

	tpath = gtk_tree_path_new_from_string(path);
	gtk_tree_model_get_iter(model, &iter, tpath);
	gtk_tree_path_free(tpath);

	gtk_tree_model_get(model, &iter, FILTER_KEYWORD_COLUMN_TOGGLE, &active, -1);
	active = (!active);


	keyword_tree = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child_iter, &iter);

	list = keyword_list_pull(pkd->keyword_view);
	if (active) 
		keyword_tree_set(keyword_tree, &child_iter, &list);
	else
		keyword_tree_reset(keyword_tree, &child_iter, &list);
		
	keyword_list_push(pkd->keyword_view, list);
	string_list_free(list);
	bar_keyword_tree_sync(pkd);
}

void bar_pane_keywords_filter_modify(GtkTreeModel *model, GtkTreeIter *iter, GValue *value, gint column, gpointer data)
{
	PaneKeywordsData *pkd = data;
	GtkTreeModel *keyword_tree = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));
	GtkTreeIter child_iter;
	
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child_iter, iter);
	
	memset(value, 0, sizeof (GValue));

	switch (column)
		{
		case FILTER_KEYWORD_COLUMN_TOGGLE:
			{
			GList *keywords = keyword_list_pull(pkd->keyword_view);
			gboolean set = keyword_tree_is_set(keyword_tree, &child_iter, keywords);
			string_list_free(keywords);
			
			g_value_init(value, G_TYPE_BOOLEAN);
			g_value_set_boolean(value, set);
			break;
			}
		case FILTER_KEYWORD_COLUMN_MARK:
			gtk_tree_model_get_value(keyword_tree, &child_iter, KEYWORD_COLUMN_MARK, value);
			break;
		case FILTER_KEYWORD_COLUMN_NAME:
			gtk_tree_model_get_value(keyword_tree, &child_iter, KEYWORD_COLUMN_NAME, value);
			break;
		case FILTER_KEYWORD_COLUMN_IS_KEYWORD:
			gtk_tree_model_get_value(keyword_tree, &child_iter, KEYWORD_COLUMN_IS_KEYWORD, value);
			break;
		}
	return;

}

static void bar_pane_keywords_set_selection(PaneKeywordsData *pkd, gboolean append)
{
	GList *keywords = NULL;
	GList *list = NULL;
	GList *work;

	keywords = keyword_list_pull(pkd->keyword_view);

	list = layout_selection_list(pkd->pane.lw);
	work = list;
	while (work)
		{
		FileData *fd = work->data;
		work = work->next;

		if (append)
			{
			metadata_append_list(fd, KEYWORD_KEY, keywords);
			}
		else
			{
			metadata_write_list(fd, KEYWORD_KEY, keywords);
			}
		}

	filelist_free(list);
	string_list_free(keywords);
}

static void bar_pane_keywords_sel_add_cb(GtkWidget *button, gpointer data)
{
	PaneKeywordsData *pkd = data;

	bar_pane_keywords_set_selection(pkd, TRUE);
}

static void bar_pane_keywords_sel_replace_cb(GtkWidget *button, gpointer data)
{
	PaneKeywordsData *pkd = data;

	bar_pane_keywords_set_selection(pkd, FALSE);
}

static void bar_pane_keywords_populate_popup_cb(GtkTextView *textview, GtkMenu *menu, gpointer data)
{
	PaneKeywordsData *pkd = data;

	menu_item_add_divider(GTK_WIDGET(menu));
	menu_item_add_stock(GTK_WIDGET(menu), _("Add keywords to selected files"), GTK_STOCK_ADD, G_CALLBACK(bar_pane_keywords_sel_add_cb), pkd);
	menu_item_add_stock(GTK_WIDGET(menu), _("Replace existing keywords in selected files"), GTK_STOCK_CONVERT, G_CALLBACK(bar_pane_keywords_sel_replace_cb), pkd);
}


static void bar_pane_keywords_notify_cb(FileData *fd, NotifyType type, gpointer data)
{
	PaneKeywordsData *pkd = data;
	if (fd == pkd->fd) bar_pane_keywords_update(pkd);
}

static void bar_pane_keywords_changed(GtkTextBuffer *buffer, gpointer data)
{
	PaneKeywordsData *pkd = data;

	file_data_unregister_notify_func(bar_pane_keywords_notify_cb, pkd);
	bar_pane_keywords_write(pkd);
	bar_keyword_tree_sync(pkd);
	file_data_register_notify_func(bar_pane_keywords_notify_cb, pkd, NOTIFY_PRIORITY_LOW);
}

static void bar_pane_keywords_mark_edited(GtkCellRendererText *cell, const gchar *path, const gchar *text, gpointer data)
{
/*	PaneKeywordsData *pkd = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreePath *tpath;
	gchar *key = NULL;
	gint i;
	FileDataGetMarkFunc get_mark_func;
	FileDataSetMarkFunc set_mark_func;
	gpointer mark_func_data;

	file_data_unregister_notify_func(bar_pane_keywords_notify_cb, pkd);

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(pkd->keyword_treeview));

	tpath = gtk_tree_path_new_from_string(path);
	gtk_tree_model_get_iter(store, &iter, tpath);
	gtk_tree_path_free(tpath);

	gtk_tree_model_get(store, &iter, FILTER_KEYWORD_COLUMN_TEXT, &key, -1);

	for (i = 0; i < FILEDATA_MARKS_SIZE; i++)
		{
		file_data_get_registered_mark_func(i, &get_mark_func, &set_mark_func, &mark_func_data);
		if (get_mark_func == meta_data_get_keyword_mark && strcmp(mark_func_data, key) == 0) 
			{
			g_free(mark_func_data);
			file_data_register_mark_func(i, NULL, NULL, NULL);
			}
		}

	if (sscanf(text, " %d ", &i) &&i >=1 && i <= FILEDATA_MARKS_SIZE)
		{
		i--;
		file_data_get_registered_mark_func(i, &get_mark_func, &set_mark_func, &mark_func_data);
		if (get_mark_func == meta_data_get_keyword_mark && mark_func_data) g_free(mark_func_data); 
		file_data_register_mark_func(i, meta_data_get_keyword_mark, meta_data_set_keyword_mark, g_strdup(key));
		}

	g_free(key);

	file_data_register_notify_func(bar_pane_keywords_notify_cb, pkd, NOTIFY_PRIORITY_LOW);
	bar_pane_keywords_update(pkd);
*/
}

void bar_pane_keywords_close(GtkWidget *bar)
{
	PaneKeywordsData *pkd;

	pkd = g_object_get_data(G_OBJECT(bar), "pane_data");
	if (!pkd) return;

	gtk_widget_destroy(pkd->widget);
}

static void bar_pane_keywords_destroy(GtkWidget *widget, gpointer data)
{
	PaneKeywordsData *pkd = data;

	file_data_unregister_notify_func(bar_pane_keywords_notify_cb, pkd);

	file_data_unref(pkd->fd);
	g_free(pkd->key);

	g_free(pkd);
}

static GtkTreeModel *create_marks_list(void)
{
	GtkListStore *model;
	GtkTreeIter iter;
	gint i;

	/* create list store */
	model = gtk_list_store_new(1, G_TYPE_STRING);
	for (i = 0; i < FILEDATA_MARKS_SIZE; i++)
		{
		gchar str[10];
		g_sprintf(str, " %d ", i + 1);
		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, str, -1);
		}
	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter, 0, " ... ", -1);
	return GTK_TREE_MODEL(model);
}


GtkWidget *bar_pane_keywords_new(const gchar *title, const gchar *key, gboolean expanded)
{
	PaneKeywordsData *pkd;
	GtkWidget *hbox;
	GtkWidget *scrolled;
	GtkTextBuffer *buffer;
	GtkTreeModel *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	pkd = g_new0(PaneKeywordsData, 1);

	pkd->pane.pane_set_fd = bar_pane_keywords_set_fd;
	pkd->pane.pane_event = bar_pane_keywords_event;
	pkd->pane.pane_write_config = bar_pane_keywords_write_config;
	pkd->pane.title = bar_pane_expander_title(title);

	pkd->pane.expanded = expanded;

	pkd->key = g_strdup(key);
	

	hbox = gtk_hbox_new(FALSE, PREF_PAD_GAP);

	pkd->widget = hbox;
	g_object_set_data(G_OBJECT(pkd->widget), "pane_data", pkd);
	g_signal_connect(G_OBJECT(pkd->widget), "destroy",
			 G_CALLBACK(bar_pane_keywords_destroy), pkd);
	gtk_widget_show(hbox);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	pkd->keyword_view = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scrolled), pkd->keyword_view);
	g_signal_connect(G_OBJECT(pkd->keyword_view), "populate-popup",
			 G_CALLBACK(bar_pane_keywords_populate_popup_cb), pkd);
	gtk_widget_show(pkd->keyword_view);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pkd->keyword_view));
	g_signal_connect(G_OBJECT(buffer), "changed",
			 G_CALLBACK(bar_pane_keywords_changed), pkd);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);


	if (!keyword_tree) keyword_tree_new_default();

	store = gtk_tree_model_filter_new(GTK_TREE_MODEL(keyword_tree), NULL);

	gtk_tree_model_filter_set_modify_func(GTK_TREE_MODEL_FILTER(store),
					      FILTER_KEYWORD_COLUMN_COUNT,
					      filter_keyword_column_types,
					      bar_pane_keywords_filter_modify,
					      pkd,
					      NULL);

	pkd->keyword_treeview = gtk_tree_view_new_with_model(store);
	g_object_unref(store);
	
	gtk_widget_set_size_request(pkd->keyword_treeview, -1, 400);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pkd->keyword_treeview), FALSE);

//	gtk_tree_view_set_search_column(GTK_TREE_VIEW(pkd->keyword_treeview), FILTER_KEYWORD_COLUMN_);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

	renderer = gtk_cell_renderer_combo_new();
	g_object_set(G_OBJECT(renderer), "editable", (gboolean)TRUE,
					 "model", create_marks_list(),
					 "text-column", 0,
					 "has-entry", FALSE,
					 NULL);

	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", FILTER_KEYWORD_COLUMN_MARK);
	g_signal_connect(renderer, "edited",
			  G_CALLBACK (bar_pane_keywords_mark_edited), pkd);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pkd->keyword_treeview), column);


	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "active", FILTER_KEYWORD_COLUMN_TOGGLE);
	gtk_tree_view_column_add_attribute(column, renderer, "visible", FILTER_KEYWORD_COLUMN_IS_KEYWORD);
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(bar_pane_keywords_keyword_toggle), pkd);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", FILTER_KEYWORD_COLUMN_NAME);

	gtk_tree_view_append_column(GTK_TREE_VIEW(pkd->keyword_treeview), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(pkd->keyword_treeview), column);

	gtk_container_add(GTK_CONTAINER(scrolled), pkd->keyword_treeview);
	gtk_widget_show(pkd->keyword_treeview);

	file_data_register_notify_func(bar_pane_keywords_notify_cb, pkd, NOTIFY_PRIORITY_LOW);

	return pkd->widget;
}

GtkWidget *bar_pane_keywords_new_from_config(const gchar **attribute_names, const gchar **attribute_values)
{
	gchar *title = g_strdup(_("NoName"));
	gchar *key = g_strdup(COMMENT_KEY);
	gboolean expanded = TRUE;

	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		if (READ_CHAR_FULL("pane.title", title)) continue;
		if (READ_CHAR_FULL("key", key)) continue;
		if (READ_BOOL_FULL("pane.expanded", expanded)) continue;
		

		DEBUG_1("unknown attribute %s = %s", option, value);
		}
	
	return bar_pane_keywords_new(title, key, expanded);
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
