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
#include "histogram.h"
#include "rcfile.h"

//#define BAR_SIZE_INCREMENT 48
//#define BAR_ARROW_SIZE 7


typedef struct _BarData BarData;
struct _BarData
{
	GtkWidget *widget;
	GtkWidget *vbox;
	FileData *fd;
	GtkWidget *label_file_name;

	LayoutWindow *lw;
	gint width;
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

gboolean bar_event(GtkWidget *bar, GdkEvent *event)
{
	BarData *bd;
	GList *list, *work;
	gboolean ret = FALSE;
	
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

	WRITE_STRING("<bar\n");
	indent++;
	write_bool_option(outstr, indent, "enabled", GTK_WIDGET_VISIBLE(bar));
	write_uint_option(outstr, indent, "width", bd->width);
	indent--;
	WRITE_STRING(">\n");

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

	WRITE_STRING("</bar>\n");
}


void bar_add(GtkWidget *bar, GtkWidget *pane)
{
	GtkWidget *expander;
	BarData *bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	PaneData *pd = g_object_get_data(G_OBJECT(pane), "pane_data");
	
	if (!bd) return;

	pd->lw = bd->lw;
	pd->bar = bar;
	
	expander = gtk_expander_new(NULL);
	if (pd && pd->title)
		{
		gtk_expander_set_label_widget(GTK_EXPANDER(expander), pd->title);
		gtk_widget_show(pd->title);
		}
		
	gtk_box_pack_start(GTK_BOX(bd->vbox), expander, FALSE, TRUE, 0);
	
	g_signal_connect(expander, "button_press_event", G_CALLBACK(bar_expander_menu_cb), bd); 
	
	gtk_container_add(GTK_CONTAINER(expander), pane);
	
	gtk_expander_set_expanded(GTK_EXPANDER(expander), pd->expanded);

	gtk_widget_show(expander);

	if (bd->fd && pd && pd->pane_set_fd) pd->pane_set_fd(pane, bd->fd);

}

static void bar_populate_default(GtkWidget *bar)
{
	GtkWidget *widget;
	
	widget = bar_pane_histogram_new(_("Histogram"), 80, TRUE, HCHAN_RGB, 0);
	bar_add(bar, widget);

	widget = bar_pane_comment_new(_("Title"), "Xmp.dc.title", TRUE, 40);
	bar_add(bar, widget);

	widget = bar_pane_keywords_new(_("Keywords"), KEYWORD_KEY, TRUE);
	bar_add(bar, widget);

	widget = bar_pane_comment_new(_("Comment"), "Xmp.dc.description", TRUE, 150);
	bar_add(bar, widget);

	widget = bar_pane_exif_new(_("Exif"), TRUE, TRUE);
	bar_add(bar, widget);
}

static void bar_size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	BarData *bd = data;
	
	bd->width = allocation->width;
}

gint bar_get_width(GtkWidget *bar)
{
	BarData *bd;
	
	bd = g_object_get_data(G_OBJECT(bar), "bar_data");
	if (!bd) return 0;

	return bd->width;
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

GtkWidget *bar_new(LayoutWindow *lw)
{
	BarData *bd;
	GtkWidget *box;
	GtkWidget *scrolled;

	bd = g_new0(BarData, 1);

	bd->lw = lw;
	
	bd->widget = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	g_object_set_data(G_OBJECT(bd->widget), "bar_data", bd);
	g_signal_connect(G_OBJECT(bd->widget), "destroy",
			 G_CALLBACK(bar_destroy), bd);

	g_signal_connect(G_OBJECT(bd->widget), "size-allocate",
			 G_CALLBACK(bar_size_allocate), bd);

	bd->width = SIDEBAR_DEFAULT_WIDTH;
	gtk_widget_set_size_request(bd->widget, bd->width, -1);

	box = gtk_hbox_new(FALSE, 0);

	bd->label_file_name = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(bd->label_file_name), PANGO_ELLIPSIZE_END);
	gtk_label_set_selectable(GTK_LABEL(bd->label_file_name), TRUE);
	gtk_misc_set_alignment(GTK_MISC(bd->label_file_name), 0.5, 0.5);
	gtk_box_pack_start(GTK_BOX(box), bd->label_file_name, TRUE, TRUE, 0);
	gtk_widget_show(bd->label_file_name);

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

GtkWidget *bar_new_default(LayoutWindow *lw)
{
	GtkWidget *bar = bar_new(lw);
	
	bar_populate_default(bar);
	
	gtk_widget_show(bar);
	
	return bar;
}

GtkWidget *bar_new_from_config(LayoutWindow *lw, const gchar **attribute_names, const gchar **attribute_values)
{
	GtkWidget *bar = bar_new(lw);
	
	gboolean enabled = TRUE;
	gint width = SIDEBAR_DEFAULT_WIDTH;

	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		if (READ_BOOL_FULL("enabled", enabled)) continue;
		if (READ_INT_FULL("width", width)) continue;
		

		DEBUG_1("unknown attribute %s = %s", option, value);
		}
	
	gtk_widget_set_size_request(bar, width, -1);
	if (enabled) gtk_widget_show(bar);
	return bar;
}

GtkWidget *bar_pane_expander_title(const gchar *title)
{
	GtkWidget *widget = gtk_label_new(title);

	pref_label_bold(widget, TRUE, FALSE);
	//gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END); //FIXME: do not work

	return widget;
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
