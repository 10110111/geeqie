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
 * keyword list dialog
 *-------------------------------------------------------------------
 */

#define KEYWORD_DIALOG_WIDTH  200
#define KEYWORD_DIALOG_HEIGHT 250

typedef struct _KeywordDlg KeywordDlg;
struct _KeywordDlg
{
	GenericDialog *gd;
	GtkWidget *treeview;
};

static KeywordDlg *keyword_dialog = NULL;


static void keyword_dialog_cancel_cb(GenericDialog *gd, gpointer data)
{
	g_free(keyword_dialog);
	keyword_dialog = NULL;
}

static void keyword_dialog_ok_cb(GenericDialog *gd, gpointer data)
{
	KeywordDlg *kd = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	gint valid;

	history_list_free_key("keywords");

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(kd->treeview));
	valid = gtk_tree_model_get_iter_first(store, &iter);
	while (valid)
		{
		gchar *key;

		gtk_tree_model_get(store, &iter, 0, &key, -1);
		valid = gtk_tree_model_iter_next(store, &iter);

		history_list_add_to_key("keywords", key, 0);
		}

	keyword_dialog_cancel_cb(gd, data);

	bar_pane_keywords_keyword_update_all();
}

static void keyword_dialog_add_cb(GtkWidget *button, gpointer data)
{
	KeywordDlg *kd = data;
	GtkTreeSelection *selection;
	GtkTreeModel *store;
	GtkTreeIter sibling;
	GtkTreeIter iter;
	GtkTreePath *tpath;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(kd->treeview));
	if (gtk_tree_selection_get_selected(selection, &store, &sibling))
		{
		gtk_list_store_insert_before(GTK_LIST_STORE(store), &iter, &sibling);
		}
	else
		{
		store = gtk_tree_view_get_model(GTK_TREE_VIEW(kd->treeview));
		gtk_list_store_append(GTK_LIST_STORE(store), &iter);
		}

	gtk_list_store_set(GTK_LIST_STORE(store), &iter, 1, TRUE, -1);

	tpath = gtk_tree_model_get_path(store, &iter);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(kd->treeview), tpath,
				 gtk_tree_view_get_column(GTK_TREE_VIEW(kd->treeview), 0), TRUE);
	gtk_tree_path_free(tpath);
}

static void keyword_dialog_remove_cb(GtkWidget *button, gpointer data)
{
	KeywordDlg *kd = data;
	GtkTreeSelection *selection;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreeIter next;
	GtkTreePath *tpath;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(kd->treeview));
	if (!gtk_tree_selection_get_selected(selection, &store, &iter)) return;

	tpath = NULL;
	next = iter;
	if (gtk_tree_model_iter_next(store, &next))
		{
		tpath = gtk_tree_model_get_path(store, &next);
		}
	else
		{
		tpath = gtk_tree_model_get_path(store, &iter);
		if (!gtk_tree_path_prev(tpath))
			{
			gtk_tree_path_free(tpath);
			tpath = NULL;
			}
		}
	if (tpath)
		{
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(kd->treeview), tpath,
					 gtk_tree_view_get_column(GTK_TREE_VIEW(kd->treeview), 0), FALSE);
		gtk_tree_path_free(tpath);
		}

	gtk_list_store_remove(GTK_LIST_STORE(store), &iter);
}

static void keyword_dialog_edit_cb(GtkCellRendererText *renderer, const gchar *path,
				   const gchar *new_text, gpointer data)
{
	KeywordDlg *kd = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreePath *tpath;

	if (!new_text || strlen(new_text) == 0) return;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(kd->treeview));

	tpath = gtk_tree_path_new_from_string(path);
	gtk_tree_model_get_iter(store, &iter, tpath);
	gtk_tree_path_free(tpath);

	gtk_list_store_set(GTK_LIST_STORE(store), &iter, 0, new_text, -1);
}

static void keyword_dialog_populate(KeywordDlg *kd)
{
	GtkListStore *store;
	GList *list;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(kd->treeview)));
	gtk_list_store_clear(store);

	list = history_list_get_by_key("keywords");
	list = g_list_last(list);
	while (list)
		{
		GtkTreeIter iter;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, list->data,
						 1, TRUE, -1);

		list = list->prev;
		}
}

static void keyword_dialog_show(void)
{
	GtkWidget *scrolled;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *hbox;
	GtkWidget *button;

	if (keyword_dialog)
		{
		gtk_window_present(GTK_WINDOW(keyword_dialog->gd->dialog));
		return;
		}

	keyword_dialog = g_new0(KeywordDlg, 1);

	keyword_dialog->gd = generic_dialog_new(_("Keyword Presets"),
						"keyword_presets", NULL, TRUE,
						keyword_dialog_cancel_cb, keyword_dialog);
	generic_dialog_add_message(keyword_dialog->gd, NULL, _("Favorite keywords list"), NULL);

	generic_dialog_add_button(keyword_dialog->gd, GTK_STOCK_OK, NULL,
				 keyword_dialog_ok_cb, TRUE);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled, KEYWORD_DIALOG_WIDTH, KEYWORD_DIALOG_HEIGHT);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(keyword_dialog->gd->vbox), scrolled, TRUE, TRUE, 5);
	gtk_widget_show(scrolled);

	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	keyword_dialog->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(keyword_dialog->treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(keyword_dialog->treeview), 0);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(keyword_dialog->treeview), TRUE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	renderer = gtk_cell_renderer_text_new();
	g_signal_connect(G_OBJECT(renderer), "edited",
			 G_CALLBACK(keyword_dialog_edit_cb), keyword_dialog);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", 0);
	gtk_tree_view_column_add_attribute(column, renderer, "editable", 1);
	gtk_tree_view_append_column(GTK_TREE_VIEW(keyword_dialog->treeview), column);

	gtk_container_add(GTK_CONTAINER(scrolled), keyword_dialog->treeview);
	gtk_widget_show(keyword_dialog->treeview);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(keyword_dialog->gd->vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(keyword_dialog_add_cb), keyword_dialog);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(keyword_dialog_remove_cb), keyword_dialog);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	keyword_dialog_populate(keyword_dialog);

	gtk_widget_show(keyword_dialog->gd->dialog);
}


static void bar_keyword_edit_cb(GtkWidget *button, gpointer data)
{
	keyword_dialog_show();
}


/*
 *-------------------------------------------------------------------
 * info bar
 *-------------------------------------------------------------------
 */

enum {
	KEYWORD_COLUMN_TOGGLE = 0,
	KEYWORD_COLUMN_TEXT,
	KEYWORD_COLUMN_MARK
};

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

static void bar_keyword_list_sync(PaneKeywordsData *pkd, GList *keywords)
{
	GList *list;
	GtkListStore *store;
	GtkTreeIter iter;

	list = history_list_get_by_key("keywords");
	if (!list)
		{
		/* blank? set up a few example defaults */

		gint i = 0;

		while (keyword_favorite_defaults[i] != NULL)
			{
			history_list_add_to_key("keywords", _(keyword_favorite_defaults[i]), 0);
			i++;
			}

		list = history_list_get_by_key("keywords");
		}

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(pkd->keyword_treeview)));

	gtk_list_store_clear(store);

	list = g_list_last(list);
	while (list)
		{
		gchar *key = list->data;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, KEYWORD_COLUMN_TOGGLE, !!find_string_in_list_utf8nocase(keywords, key),
						 KEYWORD_COLUMN_TEXT, key,
						 KEYWORD_COLUMN_MARK, bar_pane_keywords_get_mark_text(key), -1);

		list = list->prev;
		}
}

static void bar_pane_keywords_keyword_update_all(void)
{
	GList *work;

	work = bar_list;
	while (work)
		{
		PaneKeywordsData *pkd;
		GList *keywords;

		pkd = work->data;
		work = work->next;

		keywords = keyword_list_pull(pkd->keyword_view);
		bar_keyword_list_sync(pkd, keywords);
		string_list_free(keywords);
		}
}

static void bar_pane_keywords_update(PaneKeywordsData *pkd)
{
	GList *keywords = NULL;
	GtkTextBuffer *keyword_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pkd->keyword_view));

	g_signal_handlers_block_by_func(keyword_buffer, bar_pane_keywords_changed, pkd);

	keywords = metadata_read_list(pkd->fd, KEYWORD_KEY, METADATA_PLAIN);
	keyword_list_push(pkd->keyword_view, keywords);
	bar_keyword_list_sync(pkd, keywords);
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

static void bar_pane_keywords_keyword_set(PaneKeywordsData *pkd, const gchar *keyword, gint active)
{
	GList *list;
	gchar *found;

	if (!keyword) return;

	list = keyword_list_pull(pkd->keyword_view);
	found = find_string_in_list_utf8nocase(list, keyword);

	if ((!active && found) || (active && !found))
		{
		if (found)
			{
			list = g_list_remove(list, found);
			g_free(found);
			}
		else
			{
			list = g_list_append(list, g_strdup(keyword));
			}

		keyword_list_push(pkd->keyword_view, list);
		}

	string_list_free(list);
}

static void bar_pane_keywords_keyword_toggle(GtkCellRendererToggle *toggle, const gchar *path, gpointer data)
{
	PaneKeywordsData *pkd = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreePath *tpath;
	gchar *key = NULL;
	gboolean active;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(pkd->keyword_treeview));

	tpath = gtk_tree_path_new_from_string(path);
	gtk_tree_model_get_iter(store, &iter, tpath);
	gtk_tree_path_free(tpath);

	gtk_tree_model_get(store, &iter, KEYWORD_COLUMN_TOGGLE, &active,
					 KEYWORD_COLUMN_TEXT, &key, -1);
	active = (!active);
	gtk_list_store_set(GTK_LIST_STORE(store), &iter, KEYWORD_COLUMN_TOGGLE, active, -1);

	bar_pane_keywords_keyword_set(pkd, key, active);
	g_free(key);
}

static void bar_pane_keywords_set_selection(PaneKeywordsData *pkd, gboolean append)
{
	GList *keywords = NULL;
	GList *list = NULL;
	GList *work;

	if (!pkd->pane.list_func) return;

	keywords = keyword_list_pull(pkd->keyword_view);

	list = pkd->pane.list_func(pkd->pane.list_data);
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
	file_data_register_notify_func(bar_pane_keywords_notify_cb, pkd, NOTIFY_PRIORITY_LOW);
}

static void bar_pane_keywords_mark_edited(GtkCellRendererText *cell, const gchar *path, const gchar *text, gpointer data)
{
	PaneKeywordsData *pkd = data;
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

	gtk_tree_model_get(store, &iter, KEYWORD_COLUMN_TEXT, &key, -1);

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
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	pkd = g_new0(PaneKeywordsData, 1);

	pkd->pane.pane_set_fd = bar_pane_keywords_set_fd;
	pkd->pane.pane_event = bar_pane_keywords_event;
	pkd->pane.pane_write_config = bar_pane_keywords_write_config;
	pkd->pane.title = gtk_label_new(title);
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

	store = gtk_list_store_new(3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING);
	pkd->keyword_treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	gtk_widget_set_size_request(pkd->keyword_treeview, -1, 400);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pkd->keyword_treeview), FALSE);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(pkd->keyword_treeview), KEYWORD_COLUMN_TEXT);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "active", KEYWORD_COLUMN_TOGGLE);
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(bar_pane_keywords_keyword_toggle), pkd);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", KEYWORD_COLUMN_TEXT);

	gtk_tree_view_append_column(GTK_TREE_VIEW(pkd->keyword_treeview), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

	renderer = gtk_cell_renderer_combo_new();
	g_object_set(G_OBJECT(renderer), "editable", (gboolean)TRUE,
					 "model", create_marks_list(),
					 "text-column", 0,
					 "has-entry", FALSE,
					 NULL);

	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", KEYWORD_COLUMN_MARK);
	g_signal_connect(renderer, "edited",
			  G_CALLBACK (bar_pane_keywords_mark_edited), pkd);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pkd->keyword_treeview), column);


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
