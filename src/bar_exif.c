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

ExifUI ExifUIList[]={
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("Camera")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("DateTime")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("ShutterSpeed")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("Aperture")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("ExposureBias")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("ISOSpeedRating")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("FocalLength")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("FocalLength35mmFilm")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("Flash")},
	{ 0, 0, EXIF_UI_IFSET,	"Exif.Photo.ExposureProgram"},
	{ 0, 0, EXIF_UI_IFSET,	"Exif.Photo.MeteringMode"},
	{ 0, 0, EXIF_UI_IFSET,	"Exif.Photo.LightSource"},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("ColorProfile")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("SubjectDistance")},
	{ 0, 0, EXIF_UI_IFSET,	EXIF_FORMATTED("Resolution")},
	{ 0, 0, EXIF_UI_IFSET,	"Exif.Image.Orientation"},
	{ 0, 0, EXIF_UI_IFSET,  EXIF_FORMATTED("GPSPosition")},
	{ 0, 0, EXIF_UI_IFSET,  EXIF_FORMATTED("GPSAltitude")},
	{ 0, 0, EXIF_UI_IFSET,	"Exif.Image.ImageDescription"},
	{ 0, 0, EXIF_UI_IFSET,	"Exif.Image.Copyright"},
	{ 0, 0, EXIF_UI_OFF,	NULL}
};


/*
 *-------------------------------------------------------------------
 * table util
 *-------------------------------------------------------------------
 */

static void table_add_line_custom(GtkWidget *table, gint x, gint y,
				  const gchar *text1, const gchar *text2,
				  GtkWidget **label1, GtkWidget **label2,
				  GtkWidget **remove)
{
	GtkWidget *label;
	gchar *buf;

	buf = g_strconcat((text1) ? text1 : "fixme", ":", NULL);
	if (!text2) text2 = "";

	label = gtk_label_new(buf);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.0);
	pref_label_bold(label, TRUE, FALSE);
	gtk_table_attach(GTK_TABLE(table), label,
			 x + 1, x + 2, y, y + 1,
			 GTK_FILL, GTK_FILL,
			 2, 2);
	*label1 = label;

	label = gtk_label_new(text2);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_table_attach(GTK_TABLE(table), label,
			 x + 2, x + 3, y, y + 1,
			 GTK_FILL, GTK_FILL,
			 2, 2);
	*label2 = label;

	if (remove)
		{
		*remove = gtk_check_button_new();
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*remove), TRUE);

		gtk_table_attach(GTK_TABLE(table), *remove,
				 x, x + 1, y, y + 1,
				 GTK_FILL, GTK_FILL,
				 2, 2);
		}

	g_free(buf);
}

static GtkWidget *table_add_line(GtkWidget *table, gint x, gint y,
				 const gchar *description, const gchar *text,
				 GtkWidget **keyret)
{
	GtkWidget *key;
	GtkWidget *label;

	table_add_line_custom(table, x, y, description, text, &key, &label, NULL);
	gtk_widget_show(key);
	gtk_widget_show(label);
	if (keyret) *keyret = key;

	return label;
}


/*
 *-------------------------------------------------------------------
 * EXIF widget
 *-------------------------------------------------------------------
 */

typedef struct _PaneExifData PaneExifData;
struct _PaneExifData
{
	PaneData pane;
	GtkWidget *vbox;
	GtkWidget *scrolled;
	GtkWidget *table;
	GtkWidget **keys;
	GtkWidget **labels;

	GtkWidget *custom_sep;
	GtkWidget *custom_name[EXIF_BAR_CUSTOM_COUNT];
	GtkWidget *custom_value[EXIF_BAR_CUSTOM_COUNT];
	GtkWidget *custom_remove[EXIF_BAR_CUSTOM_COUNT];

	FileData *fd;

	gint allow_search;
};

static void bar_pane_exif_sensitive(PaneExifData *ped, gint enable)
{
	gtk_widget_set_sensitive(ped->table, enable);
}

static void bar_pane_exif_update(PaneExifData *ped)
{
	ExifData *exif;
	gint i;
	GList *list;

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

	for (i = 0; ExifUIList[i].key; i++)
		{
		gchar *text;

		if (ExifUIList[i].current == EXIF_UI_OFF)
			{
			gtk_widget_hide(ped->labels[i]);
			gtk_widget_hide(ped->keys[i]);
			continue;
			}
		text =  metadata_read_string(ped->fd, ExifUIList[i].key, METADATA_FORMATTED);
		if (ExifUIList[i].current == EXIF_UI_IFSET
		    && (!text || !*text))
			{
			gtk_widget_hide(ped->labels[i]);
			gtk_widget_hide(ped->keys[i]);
			g_free(text);
			continue;
			}
		gtk_widget_show(ped->labels[i]);
		gtk_widget_show(ped->keys[i]);
		gtk_label_set_text(GTK_LABEL(ped->labels[i]), text);
		g_free(text);
		}

	list = g_list_last(history_list_get_by_key("exif_extras"));
	if (list)
		{
		gtk_widget_show(ped->custom_sep);
		}
	else
		{
		gtk_widget_hide(ped->custom_sep);
		}
	i = 0;
	while (list && i < EXIF_BAR_CUSTOM_COUNT)
		{
		gchar *text;
		gchar *name;
		gchar *buf;
		gchar *description;

		name = list->data;
		list = list->prev;
		
		text =  metadata_read_string(ped->fd, name, METADATA_FORMATTED);

		description = exif_get_tag_description_by_key(name);
		if (!description || *description == '\0') 
			{
			g_free(description);
			description = g_strdup(name);
			}
		buf = g_strconcat(description, ":", NULL);
		g_free(description);
		
		gtk_label_set_text(GTK_LABEL(ped->custom_name[i]), buf);
		g_free(buf);
		gtk_label_set_text(GTK_LABEL(ped->custom_value[i]), text);
		g_free(text);

		gtk_widget_show(ped->custom_name[i]);
		gtk_widget_show(ped->custom_value[i]);
		g_object_set_data(G_OBJECT(ped->custom_remove[i]), "key", name);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ped->custom_remove[i]), TRUE);
		gtk_widget_show(ped->custom_remove[i]);

		i++;
		}
	while (i < EXIF_BAR_CUSTOM_COUNT)
		{
		g_object_set_data(G_OBJECT(ped->custom_remove[i]), "key", NULL);
		gtk_widget_hide(ped->custom_name[i]);
		gtk_widget_hide(ped->custom_value[i]);
		gtk_widget_hide(ped->custom_remove[i]);

		i++;
		}
}

static void bar_pane_exif_clear(PaneExifData *ped)
{
	gint i;

	if (!GTK_WIDGET_SENSITIVE(ped->labels[0])) return;

	for (i = 0; ExifUIList[i].key; i++)
		{
		gtk_label_set_text(GTK_LABEL(ped->labels[i]), "");
		}
	for (i = 0; i < EXIF_BAR_CUSTOM_COUNT; i++)
		{
		gtk_label_set_text(GTK_LABEL(ped->custom_value[i]), "");
		}
}

void bar_pane_exif_set_fd(GtkWidget *widget, FileData *fd)
{
	PaneExifData *ped;

	ped = g_object_get_data(G_OBJECT(widget), "pane_data");
	if (!ped) return;

	/* store this, advanced view toggle needs to reload data */
	file_data_unref(ped->fd);
	ped->fd = file_data_ref(fd);

	bar_pane_exif_clear(ped);
	bar_pane_exif_update(ped);
}

static void bar_pane_exif_write_config(GtkWidget *pane, GString *outstr, gint indent)
{
	PaneExifData *ped;

	ped = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!ped) return;

	WRITE_STRING("<pane_exif\n");
	indent++;
	WRITE_CHAR(*ped, pane.title);
	WRITE_BOOL(*ped, pane.expanded);
	indent--;
	WRITE_STRING("/>\n");
}


static void bar_pane_exif_remove_advanced_cb(GtkWidget *widget, gpointer data)
{
	PaneExifData *ped = data;
	const gchar *key;

	/* continue only if the toggle was deactivated */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) return;

	key = g_object_get_data(G_OBJECT(widget), "key");
	if (!key) return;

	history_list_item_change("exif_extras", key, NULL);

	bar_pane_exif_update(ped);
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

	g_free(ped->keys);
	g_free(ped->labels);
	file_data_unref(ped->fd);
	g_free(ped);
}

GtkWidget *bar_pane_exif_new(const gchar *title, gboolean expanded)
{
	PaneExifData *ped;
	GtkWidget *table;
	GtkWidget *viewport;
	GtkWidget *hbox;
	gint i;
	gint exif_len;

	for (exif_len = 0; ExifUIList[exif_len].key; exif_len++)
	      ;

	ped = g_new0(PaneExifData, 1);

	ped->pane.pane_set_fd = bar_pane_exif_set_fd;
	ped->pane.pane_write_config = bar_pane_exif_write_config;
	ped->pane.title = g_strdup(title);
	ped->pane.expanded = expanded;

	ped->keys = g_new0(GtkWidget *, exif_len);
	ped->labels = g_new0(GtkWidget *, exif_len);

	ped->vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	g_object_set_data(G_OBJECT(ped->vbox), "pane_data", ped);
	g_signal_connect_after(G_OBJECT(ped->vbox), "destroy",
			       G_CALLBACK(bar_pane_exif_destroy), ped);


	table = gtk_table_new(3, exif_len + 1 + EXIF_BAR_CUSTOM_COUNT, FALSE);

	ped->table = table;

	for (i = 0; ExifUIList[i].key; i++)
		{
		gchar *text;

		text = exif_get_description_by_key(ExifUIList[i].key);
		ped->labels[i] = table_add_line(table, 0, i, text, NULL,
		      &ped->keys[i]);
		g_free(text);
		}

	ped->custom_sep = gtk_hseparator_new();
	gtk_table_attach(GTK_TABLE(table), ped->custom_sep, 0, 3,
					   exif_len, exif_len + 1,
					   GTK_FILL, GTK_FILL, 2, 2);

	for (i = 0; i < EXIF_BAR_CUSTOM_COUNT; i++)
		{
		table_add_line_custom(table, 0, exif_len + 1 + i,
				      "", "",  &ped->custom_name[i], &ped->custom_value[i],
				      &ped->custom_remove[i]);
		g_signal_connect(G_OBJECT(ped->custom_remove[i]), "clicked", 
				 G_CALLBACK(bar_pane_exif_remove_advanced_cb), ped);
		}

	ped->scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ped->scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(ped->scrolled), viewport);
	gtk_widget_show(viewport);

	gtk_container_add(GTK_CONTAINER(viewport), table);
	gtk_widget_show(table);

	gtk_box_pack_start(GTK_BOX(ped->vbox), ped->scrolled, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
	gtk_box_pack_end(GTK_BOX(ped->vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_widget_show(ped->scrolled);

	gtk_widget_show(ped->vbox);

	return ped->vbox;
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
	
	return bar_pane_exif_new(title, expanded);
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
