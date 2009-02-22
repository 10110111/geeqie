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
#include "bar.h"

#include "filedata.h"
#include "history_list.h"
#include "metadata.h"
#include "misc.h"
#include "ui_fileops.h"
#include "ui_misc.h"
#include "ui_utildlg.h"

#include "ui_menu.h"
#include "bar_comment.h"
#include "bar_keywords.h"
#include "bar_exif.h"
#include "bar_histogram.h"
#include "rcfile.h"

#define BAR_SIZE_INCREMENT 48
#define BAR_ARROW_SIZE 7


typedef struct _BarData BarData;
struct _BarData
{
	GtkWidget *widget;
	GtkWidget *vbox;
	FileData *fd;
	GtkWidget *label_file_name;

	GList *(*list_func)(gpointer);
	gpointer list_data;
};

static void bar_expander_move(GtkWidget *widget, gpointer data, gboolean up)
{
	GtkWidget *expander = data;
	GtkWidget *box;
	gint pos;
	if (!expander) return;
	box = gtk_widget_get_ancestor(expander, GTK_TYPE_BOX);
	if (!box) return;
	
	gtk_container_child_get(GTK_CONTAINER(box), expander, "position", &pos, NULL);
	
	pos = up ? (pos - 1) : (pos + 1);
	if (pos < 0) pos = 0;
	
	gtk_box_reorder_child(GTK_BOX(box), expander, pos);
}


static void bar_expander_move_up_cb(GtkWidget *widget, gpointer data)
{
	bar_expander_move(widget, data, TRUE);
}

static void bar_expander_move_down_cb(GtkWidget *widget, gpointer data)
{
	bar_expander_move(widget, data, FALSE);
}


static void bar_expander_menu_popup(GtkWidget *data)
{
	GtkWidget *menu;

	menu = popup_menu_short_lived();

	menu_item_add_stock(menu, _("Move _up"), GTK_STOCK_GO_UP, G_CALLBACK(bar_expander_move_up_cb), data);
	menu_item_add_stock(menu, _("Move _down"), GTK_STOCK_GO_DOWN, G_CALLBACK(bar_expander_move_down_cb), data);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, data, 0, GDK_CURRENT_TIME);
}


static gboolean bar_expander_menu_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data) 
{ 
	if (bevent->button == MOUSE_BUTTON_RIGHT)
		{
		bar_expander_menu_popup(widget);
		return TRUE;
		}
	return FALSE;
} 


void bar_pane_set_fd_cb(GtkWidget *expander, gpointer data)
{
	GtkWidget *widget = gtk_bin_get_child(GTK_BIN(expander));
	PaneData *pd = g_object_get_data(G_OBJECT(widget), "pane_data");
	if (!pd) return;
	if (pd->pane_set_fd) pd->pane_set_fd(widget, data);
}

void bar_set_fd(GtkWidget *bar, FileData *fd)
{
	BarData *bd;
	bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	if (!bd) return;

	file_data_unref(bd->fd);
	bd->fd = file_data_ref(fd);

	gtk_container_foreach(GTK_CONTAINER(bd->vbox), bar_pane_set_fd_cb, fd);
	
	gtk_label_set_text(GTK_LABEL(bd->label_file_name), (bd->fd) ? bd->fd->name : "");

}

gint bar_event(GtkWidget *bar, GdkEvent *event)
{
	BarData *bd;
	GList *list, *work;
	gint ret = FALSE;
	bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	if (!bd) return FALSE;

	list = gtk_container_get_children(GTK_CONTAINER(bd->vbox));
	
	work = list;
	while (work)
		{
		GtkWidget *widget = gtk_bin_get_child(GTK_BIN(work->data));
		PaneData *pd = g_object_get_data(G_OBJECT(widget), "pane_data");
		if (!pd) continue;
	
		if (pd->pane_event && pd->pane_event(widget, event))
			{
			ret = TRUE;
			break;
			}
		work = work->next;
		}
	g_list_free(list);
	return ret;
}

void bar_write_config(GtkWidget *bar, GString *outstr, gint indent)
{
	BarData *bd;
	GList *list, *work;
	if (!bar) return;
	bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	if (!bd) return;

	write_indent(outstr, indent);
	g_string_append_printf(outstr, "<bar>\n");

	list = gtk_container_get_children(GTK_CONTAINER(bd->vbox));	
	work = list;
	while (work)
		{
		GtkWidget *expander = work->data;
		GtkWidget *widget = gtk_bin_get_child(GTK_BIN(expander));
		PaneData *pd = g_object_get_data(G_OBJECT(widget), "pane_data");
		if (!pd) continue;

		pd->expanded = gtk_expander_get_expanded(GTK_EXPANDER(expander));

		if (pd->pane_write_config)
			pd->pane_write_config(widget, outstr, indent + 1);

		work = work->next;
		}
	g_list_free(list);

	write_indent(outstr, indent);
	g_string_append_printf(outstr, "</bar>\n");
}


void bar_pane_set_selection_func(GtkWidget *pane, GList *(*list_func)(gpointer data), gpointer data)
{
	PaneData *pd;

	pd = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!pd) return;

	pd->list_func = list_func;
	pd->list_data = data;
}

void bar_set_selection_func(GtkWidget *bar, GList *(*list_func)(gpointer data), gpointer data)
{
	BarData *bd;
	GList *list, *work;
	bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	if (!bd) return;

	bd->list_func = list_func;
	bd->list_data = data;

	list = gtk_container_get_children(GTK_CONTAINER(bd->vbox));
	
	work = list;
	while (work)
		{
		GtkWidget *widget = gtk_bin_get_child(GTK_BIN(work->data));
		
		bar_pane_set_selection_func(widget, list_func, data);
	
		work = work->next;
		}
	g_list_free(list);
	return;
}





void bar_add(GtkWidget *bar, GtkWidget *pane)
{
	GtkWidget *expander;
	GtkWidget *label;
	BarData *bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	PaneData *pd = g_object_get_data(G_OBJECT(pane), "pane_data");

	
	if (!bd) return;

	expander = gtk_expander_new(pd ? pd->title : "");
	gtk_box_pack_start(GTK_BOX(bd->vbox), expander, FALSE, TRUE, 0);
	
	g_signal_connect(expander, "button_press_event", G_CALLBACK(bar_expander_menu_cb), bd); 
	
	label = gtk_expander_get_label_widget(GTK_EXPANDER(expander));
//	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	pref_label_bold(label, TRUE, FALSE);

	gtk_container_add(GTK_CONTAINER(expander), pane);
	
	gtk_expander_set_expanded(GTK_EXPANDER(expander), pd->expanded);

	gtk_widget_show(expander);

	if (bd->list_func) bar_pane_set_selection_func(pane, bd->list_func, bd->list_data);
	if (bd->fd && pd && pd->pane_set_fd) pd->pane_set_fd(pane, bd->fd);

}

void bar_populate_default(GtkWidget *bar)
{
	GtkWidget *widget;
	widget = bar_pane_histogram_new(_("Histogram"), 80, TRUE);
	bar_add(bar, widget);

	widget = bar_pane_comment_new(_("Title"), "Xmp.dc.title", TRUE, 40);
	bar_add(bar, widget);

	widget = bar_pane_keywords_new(_("Keywords"), KEYWORD_KEY, TRUE);
	bar_add(bar, widget);

	widget = bar_pane_comment_new(_("Comment"), "Xmp.dc.description", TRUE, 150);
	bar_add(bar, widget);

	widget = bar_pane_exif_new(_("Exif"), TRUE);
	bar_add(bar, widget);
}

static void bar_width(BarData *bd, gint val)
{
	gint size;

	size = bd->widget->allocation.width;
	size = CLAMP(size + val, BAR_SIZE_INCREMENT * 2, BAR_SIZE_INCREMENT * 16);

	gtk_widget_set_size_request(bd->widget, size, -1);
	options->layout.panels.info.width = bd->widget->allocation.width;
}

static void bar_larger(GtkWidget *widget, gpointer data)
{
	BarData *bd = data;

	bar_width(bd, BAR_SIZE_INCREMENT);
}

static void bar_smaller(GtkWidget *widget, gpointer data)
{
	BarData *bd = data;

	bar_width(bd, -BAR_SIZE_INCREMENT);
}

void bar_close(GtkWidget *bar)
{
	BarData *bd;

	bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	if (!bd) return;

	gtk_widget_destroy(bd->widget);
}

static void bar_destroy(GtkWidget *widget, gpointer data)
{
	BarData *bd = data;

	file_data_unref(bd->fd);
	g_free(bd);
}

GtkWidget *bar_new(GtkWidget *bounding_widget)
{
	BarData *bd;
	GtkWidget *box;
	GtkWidget *sizer;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *arrow;
	GtkWidget *scrolled;

	bd = g_new0(BarData, 1);

	bd->widget = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	g_object_set_data(G_OBJECT(bd->widget), "bar_data", bd);
	g_signal_connect(G_OBJECT(bd->widget), "destroy",
			 G_CALLBACK(bar_destroy), bd);

	box = gtk_hbox_new(FALSE, 0);

	sizer = sizer_new(bd->widget, bounding_widget, SIZER_POS_LEFT);
	sizer_set_limits(sizer, BAR_SIZE_INCREMENT * 2, -1, -1 , -1);
	gtk_box_pack_start(GTK_BOX(box), sizer, FALSE, FALSE, 0);
	gtk_widget_show(sizer);

	label = gtk_label_new(_("Filename:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.0);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	bd->label_file_name = gtk_label_new("");
	pref_label_bold(bd->label_file_name, TRUE, FALSE);
	gtk_misc_set_alignment(GTK_MISC(bd->label_file_name), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(box), bd->label_file_name, TRUE, TRUE, 0);
	gtk_widget_show(bd->label_file_name);

	button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(bar_smaller), bd);
	gtk_box_pack_end(GTK_BOX(box), button, FALSE, FALSE, 0);
	arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	gtk_widget_set_size_request(arrow, BAR_ARROW_SIZE, BAR_ARROW_SIZE);
	gtk_container_add(GTK_CONTAINER(button), arrow);
	gtk_widget_show(arrow);
	gtk_widget_show(button);

	button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(bar_larger), bd);
	gtk_box_pack_end(GTK_BOX(box), button, FALSE, FALSE, 0);
	arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	gtk_widget_set_size_request(arrow, BAR_ARROW_SIZE, BAR_ARROW_SIZE);
	gtk_container_add(GTK_CONTAINER(button), arrow);
	gtk_widget_show(arrow);
	gtk_widget_show(button);

	gtk_box_pack_start(GTK_BOX(bd->widget), box, FALSE, FALSE, 0);
	gtk_widget_show(box);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(bd->widget), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);


	bd->vbox = gtk_vbox_new(FALSE, 0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), bd->vbox);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(scrolled))), GTK_SHADOW_NONE);
	
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_NONE);
	gtk_widget_show(bd->vbox);
	return bd->widget;
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
