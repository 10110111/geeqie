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
#include "ui_menu.h"
#include "ui_misc.h"
#include "histogram.h"

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


GtkWidget *bar_pane_histogram_new(const gchar *title, gint height)
{
	PaneHistogramData *phd;

	phd = g_new0(PaneHistogramData, 1);
	
	phd->pane.pane_set_fd = bar_pane_histogram_set_fd;
	phd->pane.title = g_strdup(title);
	
	phd->histogram = histogram_new();
	
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


	gtk_widget_show(phd->widget);

	file_data_register_notify_func(bar_pane_histogram_notify_cb, phd, NOTIFY_PRIORITY_LOW);

	return phd->widget;
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
