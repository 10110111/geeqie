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
#include "bar_exif.h"

#include "exif.h"
#include "metadata.h"
#include "filedata.h"
#include "history_list.h"
#include "misc.h"
#include "ui_misc.h"
#include "bar.h"
#include "rcfile.h"


#include <math.h>

#define MIN_HEIGHT 25
/*
 *-------------------------------------------------------------------
 * EXIF widget
 *-------------------------------------------------------------------
 */

typedef struct _ExifEntry ExifEntry;
struct _ExifEntry
{
	GtkWidget *hbox;
	GtkWidget *title_label;
	GtkWidget *value_label;

	gchar *key;
	gchar *title;
	gboolean if_set;
	gboolean auto_title;
};
	
	
typedef struct _PaneExifData PaneExifData;
struct _PaneExifData
{
	PaneData pane;
	GtkWidget *vbox;
	GtkWidget *widget;
	GtkSizeGroup *size_group;

	gint min_height;
	
	FileData *fd;
};

static void bar_pane_exif_update_entry(PaneExifData *ped, GtkWidget *entry, gboolean update_title);

static void bar_pane_exif_entry_destroy(GtkWidget *widget, gpointer data)
{
	ExifEntry *ee = data;

	g_free(ee->key);
	g_free(ee->title);
	g_free(ee);
}


static void bar_pane_exif_add_entry(PaneExifData *ped, const gchar *key, const gchar *title, gint if_set)
{
	ExifEntry *ee = g_new0(ExifEntry, 1);
	
	ee->key = g_strdup(key);
	if (title && title[0])
		{
		ee->title = g_strdup(title);
		}
	else
		{
		ee->title = exif_get_description_by_key(key);
		ee->auto_title = TRUE;
		}
		
	ee->if_set = if_set;
	
	ee->hbox = gtk_hbox_new(FALSE, 0);
	g_object_set_data(G_OBJECT(ee->hbox), "entry_data", ee);
	g_signal_connect_after(G_OBJECT(ee->hbox), "destroy",
			       G_CALLBACK(bar_pane_exif_entry_destroy), ee);
	
	ee->title_label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(ee->title_label), 1.0, 0.0);
	gtk_size_group_add_widget(ped->size_group, ee->title_label);
	gtk_box_pack_start(GTK_BOX(ee->hbox), ee->title_label, FALSE, TRUE, 0);
	gtk_widget_show(ee->title_label);
	
	ee->value_label = gtk_label_new(NULL);
//	gtk_label_set_width_chars(GTK_LABEL(ee->value_label), 20);
	gtk_label_set_ellipsize(GTK_LABEL(ee->value_label), PANGO_ELLIPSIZE_END);
//	gtk_widget_set_size_request(ee->value_label, 100, -1);
	gtk_misc_set_alignment(GTK_MISC(ee->value_label), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(ee->hbox), ee->value_label, TRUE, TRUE, 1);
	gtk_widget_show(ee->value_label);
	
	gtk_box_pack_start(GTK_BOX(ped->vbox), ee->hbox, TRUE, TRUE, 0);
	bar_pane_exif_update_entry(ped, ee->hbox, TRUE);
}
	
static void bar_pane_exif_entry_update_title(ExifEntry *ee)
{
	gchar *markup;

	markup = g_markup_printf_escaped("<span size='small'>%s:</span>", (ee->title) ? ee->title : "fixme");
	gtk_label_set_markup(GTK_LABEL(ee->title_label), markup);
	g_free(markup);
}

static void bar_pane_exif_update_entry(PaneExifData *ped, GtkWidget *entry, gboolean update_title)
{
	gchar *text;
	ExifEntry *ee = g_object_get_data(G_OBJECT(entry), "entry_data");
	if (!ee) return;
	text = metadata_read_string(ped->fd, ee->key, METADATA_FORMATTED);

	if (ee->if_set && (!text || !*text))
		{
		gtk_label_set_text(GTK_LABEL(ee->value_label), NULL);
		gtk_widget_hide(ee->hbox);
		}
	else
		{
		gtk_label_set_text(GTK_LABEL(ee->value_label), text);
#if GTK_CHECK_VERSION(2,12,0)
    		gtk_widget_set_tooltip_text(ee->hbox, text);
#endif
		gtk_widget_show(ee->hbox);
		}
		
	g_free(text);
	
	if (update_title) bar_pane_exif_entry_update_title(ee);
}

static void bar_pane_exif_update(PaneExifData *ped)
{
	GList *list, *work;

#if 0
	ExifData *exif;
	/* do we have any exif at all ? */
	exif = exif_read_fd(ped->fd);

	if (!exif)
		{
		bar_pane_exif_sensitive(ped, FALSE);
		return;
		}
	else
		{
		/* we will use high level functions so we can release it for now.
		   it will stay in the cache */
		exif_free_fd(ped->fd, exif);
		exif = NULL;
		}

	bar_pane_exif_sensitive(ped, TRUE);
#endif	
	list = gtk_container_get_children(GTK_CONTAINER(ped->vbox));	
	work = list;
	while (work)
		{
		GtkWidget *entry = work->data;
		work = work->next;
		
		bar_pane_exif_update_entry(ped, entry, FALSE);
		}
	g_list_free(list);
}

void bar_pane_exif_set_fd(GtkWidget *widget, FileData *fd)
{
	PaneExifData *ped;

	ped = g_object_get_data(G_OBJECT(widget), "pane_data");
	if (!ped) return;

	file_data_unref(ped->fd);
	ped->fd = file_data_ref(fd);

	bar_pane_exif_update(ped);
}

static void bar_pane_exif_entry_write_config(GtkWidget *entry, GString *outstr, gint indent)
{
	ExifEntry *ee = g_object_get_data(G_OBJECT(entry), "entry_data");
	if (!ee) return;

	WRITE_STRING("<entry\n");
	indent++;
	WRITE_CHAR(*ee, key);
	if (!ee->auto_title) WRITE_CHAR(*ee, title);
	WRITE_BOOL(*ee, if_set);
	indent--;
	WRITE_STRING("/>\n");
}

static void bar_pane_exif_write_config(GtkWidget *pane, GString *outstr, gint indent)
{
	PaneExifData *ped;
	GList *work, *list;
	
	ped = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!ped) return;

	WRITE_STRING("<pane_exif\n");
	indent++;
	write_char_option(outstr, indent, "pane.title", gtk_label_get_text(GTK_LABEL(ped->pane.title)));
	WRITE_BOOL(*ped, pane.expanded);
	indent--;
	WRITE_STRING(">\n");
	indent++;
	
	list = gtk_container_get_children(GTK_CONTAINER(ped->vbox));	
	work = list;
	while (work)
		{
		GtkWidget *entry = work->data;
		work = work->next;
		
		bar_pane_exif_entry_write_config(entry, outstr, indent);
		}
	g_list_free(list);
	indent--;
	WRITE_STRING("</pane_exif>\n");
}


void bar_pane_exif_close(GtkWidget *widget)
{
	PaneExifData *ped;

	ped = g_object_get_data(G_OBJECT(widget), "pane_data");
	if (!ped) return;

	gtk_widget_destroy(ped->vbox);
}

static void bar_pane_exif_destroy(GtkWidget *widget, gpointer data)
{
	PaneExifData *ped = data;

	g_object_unref(ped->size_group);
	file_data_unref(ped->fd);
	g_free(ped);
}

static void bar_pane_exif_size_request(GtkWidget *pane, GtkRequisition *requisition, gpointer data)
{
	PaneExifData *ped = data;
	if (requisition->height < ped->min_height)
		{
		requisition->height = ped->min_height;
		}
}

static void bar_pane_exif_size_allocate(GtkWidget *pane, GtkAllocation *alloc, gpointer data)
{
	PaneExifData *ped = data;
	ped->min_height = alloc->height;
}

GtkWidget *bar_pane_exif_new(const gchar *title, gboolean expanded, gboolean populate)
{
	PaneExifData *ped;

	ped = g_new0(PaneExifData, 1);

	ped->pane.pane_set_fd = bar_pane_exif_set_fd;
	ped->pane.pane_write_config = bar_pane_exif_write_config;
	ped->pane.title = gtk_label_new(title);
	ped->pane.expanded = expanded;

	ped->size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	ped->vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	ped->widget = ped->vbox;
	ped->min_height = MIN_HEIGHT;
	g_object_set_data(G_OBJECT(ped->widget), "pane_data", ped);
	g_signal_connect_after(G_OBJECT(ped->widget), "destroy",
			       G_CALLBACK(bar_pane_exif_destroy), ped);
	g_signal_connect(G_OBJECT(ped->widget), "size-request",
			 G_CALLBACK(bar_pane_exif_size_request), ped);
	g_signal_connect(G_OBJECT(ped->widget), "size-allocate",
			 G_CALLBACK(bar_pane_exif_size_allocate), ped);

	if (populate)
		{
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("Camera"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("DateTime"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("ShutterSpeed"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("Aperture"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("ExposureBias"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("ISOSpeedRating"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("FocalLength"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("FocalLength35mmFilm"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("Flash"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, "Exif.Photo.ExposureProgram", NULL, TRUE);
		bar_pane_exif_add_entry(ped, "Exif.Photo.MeteringMode", NULL, TRUE);
		bar_pane_exif_add_entry(ped, "Exif.Photo.LightSource", NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("ColorProfile"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("SubjectDistance"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("Resolution"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, "Exif.Image.Orientation", NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("GPSPosition"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, EXIF_FORMATTED("GPSAltitude"), NULL, TRUE);
		bar_pane_exif_add_entry(ped, "Exif.Image.ImageDescription", NULL, TRUE);
		bar_pane_exif_add_entry(ped, "Exif.Image.Copyright", NULL, TRUE);
		}
	
	gtk_widget_show(ped->widget);

	return ped->widget;
}

GtkWidget *bar_pane_exif_new_from_config(const gchar **attribute_names, const gchar **attribute_values)
{
	gchar *title = g_strdup(_("NoName"));
	gboolean expanded = TRUE;

	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		if (READ_CHAR_FULL("pane.title", title)) continue;
		if (READ_BOOL_FULL("pane.expanded", expanded)) continue;
		

		DEBUG_1("unknown attribute %s = %s", option, value);
		}
	
	return bar_pane_exif_new(title, expanded, FALSE);
}

void bar_pane_exif_entry_add_from_config(GtkWidget *pane, const gchar **attribute_names, const gchar **attribute_values)
{
	PaneExifData *ped;
	gchar *key = NULL;
	gchar *title = NULL;
	gboolean if_set = TRUE;

	ped = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!ped) return;

	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		if (READ_CHAR_FULL("key", key)) continue;
		if (READ_CHAR_FULL("title", title)) continue;
		if (READ_BOOL_FULL("if_set", if_set)) continue;
		

		DEBUG_1("unknown attribute %s = %s", option, value);
		}
	if (key && key[0]) bar_pane_exif_add_entry(ped, key, title, if_set);
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
