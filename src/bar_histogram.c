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
#include "bar_comment.h"

#include "bar.h"
#include "metadata.h"
#include "filedata.h"
#include "menu.h"
#include "ui_menu.h"
#include "ui_misc.h"
#include "histogram.h"
#include "rcfile.h"

/*
 *-------------------------------------------------------------------
 * keyword / comment utils
 *-------------------------------------------------------------------
 */



typedef struct _PaneHistogramData PaneHistogramData;
struct _PaneHistogramData
{
	PaneData pane;
	GtkWidget *widget;
	GtkWidget *drawing_area;
	Histogram *histogram;
	gint histogram_width;
	gint histogram_height;
	GdkPixbuf *pixbuf;
	FileData *fd;
};


static void bar_pane_histogram_update(PaneHistogramData *phd)
{
	const HistMap *histmap;
	if (phd->pixbuf) g_object_unref(phd->pixbuf);
	phd->pixbuf = NULL;

	if (!phd->histogram_width || !phd->histogram_height || !phd->fd) return;

	gtk_widget_queue_draw_area(GTK_WIDGET(phd->drawing_area), 0, 0, phd->histogram_width, phd->histogram_height);
	
	histmap = histmap_get(phd->fd);
	
	if (!histmap) return;
	
	phd->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, phd->histogram_width, phd->histogram_height);
	gdk_pixbuf_fill(phd->pixbuf, 0xffffffff);
	histogram_draw(phd->histogram, histmap, phd->pixbuf, 0, 0, phd->histogram_width, phd->histogram_height);
}


static void bar_pane_histogram_set_fd(GtkWidget *pane, FileData *fd)
{
	PaneHistogramData *phd;

	phd = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!phd) return;

	file_data_unref(phd->fd);
	phd->fd = file_data_ref(fd);

	bar_pane_histogram_update(phd);
}

static void bar_pane_histogram_write_config(GtkWidget *pane, GString *outstr, gint indent)
{
	PaneHistogramData *phd;

	phd = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!phd) return;

	WRITE_STRING("<pane_histogram\n");
	indent++;
	WRITE_CHAR(*phd, pane.title);
	WRITE_BOOL(*phd, pane.expanded);
	indent--;
	WRITE_STRING("/>\n");
}


static void bar_pane_histogram_notify_cb(FileData *fd, NotifyType type, gpointer data)
{
	PaneHistogramData *phd = data;
	if (fd == phd->fd) bar_pane_histogram_update(phd);
}

static gboolean bar_pane_histogram_expose_event_cb (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	PaneHistogramData *phd = data;
	if (!phd || !phd->pixbuf) return TRUE;
	
	gdk_draw_pixbuf(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			phd->pixbuf,
			0, 0,
			0, 0,
			-1, -1,
			GDK_RGB_DITHER_NORMAL, 0, 0);
	return TRUE;
}

static void bar_pane_histogram_size_cb(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	PaneHistogramData *phd = data;

	phd->histogram_width = allocation->width;
	phd->histogram_height = allocation->height;
	bar_pane_histogram_update(phd);
}

static void bar_pane_histogram_close(GtkWidget *pane)
{
	PaneHistogramData *phd;

	phd = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!phd) return;

	gtk_widget_destroy(phd->widget);
}

static void bar_pane_histogram_destroy(GtkWidget *widget, gpointer data)
{
	PaneHistogramData *phd = data;

	file_data_unregister_notify_func(bar_pane_histogram_notify_cb, phd);

	file_data_unref(phd->fd);
	g_free(phd->pane.title);
	histogram_free(phd->histogram);
	if (phd->pixbuf) g_object_unref(phd->pixbuf);

	g_free(phd);
}

static void bar_pane_histogram_popup_channels_cb(GtkWidget *widget, gpointer data)
{
	PaneHistogramData *phd;
	gint channel;

	phd = submenu_item_get_data(widget);

	if (!phd) return;

	channel = GPOINTER_TO_INT(data);
	histogram_set_channel(phd->histogram, channel);
	bar_pane_histogram_update(phd);
}

static void bar_pane_histogram_popup_logmode_cb(GtkWidget *widget, gpointer data)
{
	PaneHistogramData *phd;
	gint logmode;

	phd = submenu_item_get_data(widget);

	if (!phd) return;

	logmode = GPOINTER_TO_INT(data);
	histogram_set_mode(phd->histogram, logmode);
	bar_pane_histogram_update(phd);
}

static GtkWidget *bar_pane_histogram_add_radio(GtkWidget *menu, GtkWidget *parent,
					const gchar *label,
					GCallback func, gint value,
					gboolean show_current, gint show_value)
{
	GtkWidget *item;

	if (show_current)
		{
		item = menu_item_add_radio(menu, parent,
					   label, (value == show_value),
					   func, GINT_TO_POINTER((gint)value));
		}
	else
		{
		item = menu_item_add(menu, label,
				     func, GINT_TO_POINTER((gint)value));
		}

	return item;
}

GtkWidget *bar_pane_histogram_add_channels(GtkWidget *menu, GCallback func, gpointer data,
			    		   gboolean show_current, gint value)
{
	GtkWidget *submenu;
	GtkWidget *parent;

	submenu = gtk_menu_new();
	g_object_set_data(G_OBJECT(submenu), "submenu_data", data);

	parent = bar_pane_histogram_add_radio(submenu, NULL, _("_Red"), func, HCHAN_R, show_current, value);
	bar_pane_histogram_add_radio(submenu, parent, _("_Green"), func, HCHAN_G, show_current, value);
	bar_pane_histogram_add_radio(submenu, parent, _("_Blue"),func, HCHAN_B, show_current, value);
	bar_pane_histogram_add_radio(submenu, parent, _("_RGB"),func, HCHAN_RGB, show_current, value);
	bar_pane_histogram_add_radio(submenu, parent, _("_Value"),func, HCHAN_MAX, show_current, value);

	if (menu)
		{
		GtkWidget *item;

		item = menu_item_add(menu, _("Channels"), NULL, NULL);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
		return item;
		}

	return submenu;
}
GtkWidget *bar_pane_histogram_add_logmode(GtkWidget *menu, GCallback func, gpointer data,
			    		   gboolean show_current, gint value)
{
	GtkWidget *submenu;
	GtkWidget *parent;

	submenu = gtk_menu_new();
	g_object_set_data(G_OBJECT(submenu), "submenu_data", data);

	parent = bar_pane_histogram_add_radio(submenu, NULL, _("_Linear"), func, 0, show_current, value);
	bar_pane_histogram_add_radio(submenu, parent, _("Lo_garithmical"), func, 1, show_current, value);

	if (menu)
		{
		GtkWidget *item;

		item = menu_item_add(menu, _("Mode"), NULL, NULL);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
		return item;
		}

	return submenu;
}


static GtkWidget *bar_pane_histogram_menu(PaneHistogramData *phd)
{
	GtkWidget *menu;
	static gboolean show_current = FALSE; /* FIXME: TRUE -> buggy behavior */

	menu = popup_menu_short_lived();
	bar_pane_histogram_add_channels(menu, G_CALLBACK(bar_pane_histogram_popup_channels_cb), phd,
					show_current, histogram_get_channel(phd->histogram));
	bar_pane_histogram_add_logmode(menu, G_CALLBACK(bar_pane_histogram_popup_logmode_cb), phd,
				       show_current, histogram_get_mode(phd->histogram));

	return menu;
}

static gboolean bar_pane_histogram_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	PaneHistogramData *phd = data;

	if (bevent->button == MOUSE_BUTTON_RIGHT)
		{
		GtkWidget *menu;

		menu = bar_pane_histogram_menu(phd);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
		return TRUE;
	}

	return FALSE;
}


GtkWidget *bar_pane_histogram_new(const gchar *title, gint height, gint expanded)
{
	PaneHistogramData *phd;

	phd = g_new0(PaneHistogramData, 1);
	
	phd->pane.pane_set_fd = bar_pane_histogram_set_fd;
	phd->pane.pane_write_config = bar_pane_histogram_write_config;
	phd->pane.title = g_strdup(title);
	phd->pane.expanded = expanded;
	
	phd->histogram = histogram_new();

	histogram_set_channel(phd->histogram, HCHAN_RGB);
	histogram_set_mode(phd->histogram, 0);

	phd->widget = gtk_vbox_new(FALSE, PREF_PAD_GAP);

	g_object_set_data(G_OBJECT(phd->widget), "pane_data", phd);
	g_signal_connect(G_OBJECT(phd->widget), "destroy",
			 G_CALLBACK(bar_pane_histogram_destroy), phd);
	

	gtk_widget_set_size_request(GTK_WIDGET(phd->widget), -1, height);

	phd->drawing_area = gtk_drawing_area_new();
	g_signal_connect_after(G_OBJECT(phd->drawing_area), "size_allocate",
                               G_CALLBACK(bar_pane_histogram_size_cb), phd);

	g_signal_connect(G_OBJECT(phd->drawing_area), "expose_event",  
			 G_CALLBACK(bar_pane_histogram_expose_event_cb), phd);
			 
	gtk_box_pack_start(GTK_BOX(phd->widget), phd->drawing_area, TRUE, TRUE, 0);
	gtk_widget_show(phd->drawing_area);
	gtk_widget_add_events(phd->drawing_area, GDK_BUTTON_PRESS_MASK);

	g_signal_connect(G_OBJECT(phd->drawing_area), "button_press_event", G_CALLBACK(bar_pane_histogram_press_cb), phd);

	gtk_widget_show(phd->widget);

	file_data_register_notify_func(bar_pane_histogram_notify_cb, phd, NOTIFY_PRIORITY_LOW);

	return phd->widget;
}

GtkWidget *bar_pane_histogram_new_from_config(const gchar **attribute_names, const gchar **attribute_values)
{
	gchar *title = g_strdup(_("NoName"));
	gboolean expanded = TRUE;
	gint height = 80;

	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		if (READ_CHAR_FULL("pane.title", title)) continue;
		if (READ_BOOL_FULL("pane.expanded", expanded)) continue;
		

		DEBUG_1("unknown attribute %s = %s", option, value);
		}
	
	return bar_pane_histogram_new(title, height, expanded);
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
