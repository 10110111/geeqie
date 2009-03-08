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
#include "bar_exif.h"

#include "exif.h"
#include "metadata.h"
#include "filedata.h"
#include "history_list.h"
#include "misc.h"
#include "ui_misc.h"
#include "ui_menu.h"
#include "bar.h"
#include "rcfile.h"
#include "dnd.h"
#include "ui_utildlg.h"


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
	GtkWidget *ebox;
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
	
	gboolean all_hidden;
	gboolean show_all;
	
	FileData *fd;
};

typedef struct _ConfDialogData ConfDialogData;
struct _ConfDialogData
{
	GtkWidget *widget; /* pane or entry, devidet by presenceof "pane_data" or "entry_data" */

	/* dialog parts */
	GenericDialog *gd;
	GtkWidget *key_entry;
	GtkWidget *title_entry;
	gboolean if_set;
};

static void bar_pane_exif_entry_dnd_init(GtkWidget *entry);
static void bar_pane_exif_entry_update_title(ExifEntry *ee);
static void bar_pane_exif_update(PaneExifData *ped);
static gboolean bar_pane_exif_menu_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data);

static void bar_pane_exif_entry_destroy(GtkWidget *widget, gpointer data)
{
	ExifEntry *ee = data;

	g_free(ee->key);
	g_free(ee->title);
	g_free(ee);
}


static GtkWidget *bar_pane_exif_add_entry(PaneExifData *ped, const gchar *key, const gchar *title, gint if_set)
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
	
	ee->ebox = gtk_event_box_new();
	g_object_set_data(G_OBJECT(ee->ebox), "entry_data", ee);
	g_signal_connect_after(G_OBJECT(ee->ebox), "destroy",
			       G_CALLBACK(bar_pane_exif_entry_destroy), ee);
	
	ee->hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(ee->ebox), ee->hbox);
	gtk_widget_show(ee->hbox);

	ee->title_label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(ee->title_label), 1.0, 0.5);
	gtk_size_group_add_widget(ped->size_group, ee->title_label);
	gtk_box_pack_start(GTK_BOX(ee->hbox), ee->title_label, FALSE, TRUE, 0);
	gtk_widget_show(ee->title_label);
	
	ee->value_label = gtk_label_new(NULL);
//	gtk_label_set_width_chars(GTK_LABEL(ee->value_label), 20);
	gtk_label_set_ellipsize(GTK_LABEL(ee->value_label), PANGO_ELLIPSIZE_END);
//	gtk_widget_set_size_request(ee->value_label, 100, -1);
	gtk_misc_set_alignment(GTK_MISC(ee->value_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(ee->hbox), ee->value_label, TRUE, TRUE, 1);
	gtk_widget_show(ee->value_label);
	gtk_box_pack_start(GTK_BOX(ped->vbox), ee->ebox, FALSE, FALSE, 0);

	bar_pane_exif_entry_dnd_init(ee->ebox);
	g_signal_connect(ee->ebox, "button_press_event", G_CALLBACK(bar_pane_exif_menu_cb), ped);
	
	bar_pane_exif_entry_update_title(ee);
	bar_pane_exif_update(ped);
	
	return ee->ebox;
}

static void bar_pane_exif_reparent_entry(GtkWidget *entry, GtkWidget *pane)
{
	GtkWidget *old_pane = entry->parent;
	PaneExifData *ped = g_object_get_data(G_OBJECT(pane), "pane_data");
	PaneExifData *old_ped = g_object_get_data(G_OBJECT(old_pane), "pane_data");
	ExifEntry *ee = g_object_get_data(G_OBJECT(entry), "entry_data");
	if (!ped || !old_ped || !ee) return;
	
	g_object_ref(entry);
	
	gtk_size_group_remove_widget(old_ped->size_group, ee->title_label);
	gtk_container_remove(GTK_CONTAINER(old_ped->vbox), entry);
	
	gtk_size_group_add_widget(ped->size_group, ee->title_label);
	gtk_box_pack_start(GTK_BOX(ped->vbox), entry, FALSE, FALSE, 0);
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

	if (!ped->show_all && ee->if_set && (!text || !*text))
		{
		gtk_label_set_text(GTK_LABEL(ee->value_label), NULL);
		gtk_widget_hide(entry);
		}
	else
		{
		gtk_label_set_text(GTK_LABEL(ee->value_label), text);
#if GTK_CHECK_VERSION(2,12,0)
    		gtk_widget_set_tooltip_text(ee->hbox, text);
#endif
		gtk_widget_show(entry);
		ped->all_hidden = FALSE;
		}
		
	g_free(text);
	
	if (update_title) bar_pane_exif_entry_update_title(ee);
}

static void bar_pane_exif_update(PaneExifData *ped)
{
	GList *list, *work;

	ped->all_hidden = TRUE;

	list = gtk_container_get_children(GTK_CONTAINER(ped->vbox));	
	work = list;
	while (work)
		{
		GtkWidget *entry = work->data;
		work = work->next;
	
		
		bar_pane_exif_update_entry(ped, entry, FALSE);
		}
	g_list_free(list);

	gtk_widget_set_sensitive(ped->pane.title, !ped->all_hidden);
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

/*
 *-------------------------------------------------------------------
 * dnd
 *-------------------------------------------------------------------
 */

static GtkTargetEntry bar_pane_exif_drag_types[] = {
	{ TARGET_APP_EXIF_ENTRY_STRING, GTK_TARGET_SAME_APP, TARGET_APP_EXIF_ENTRY },
	{ "text/plain", 0, TARGET_TEXT_PLAIN }
};
static gint n_exif_entry_drag_types = 2;

static GtkTargetEntry bar_pane_exif_drop_types[] = {
	{ TARGET_APP_EXIF_ENTRY_STRING, GTK_TARGET_SAME_APP, TARGET_APP_EXIF_ENTRY },
	{ "text/plain", 0, TARGET_TEXT_PLAIN }
};
static gint n_exif_entry_drop_types = 2;


static void bar_pane_exif_entry_dnd_get(GtkWidget *entry, GdkDragContext *context,
				     GtkSelectionData *selection_data, guint info,
				     guint time, gpointer data)
{
	ExifEntry *ee = g_object_get_data(G_OBJECT(entry), "entry_data");

	switch (info)
		{

		case TARGET_APP_EXIF_ENTRY:
			gtk_selection_data_set(selection_data, selection_data->target,
					       8, (gpointer) &entry, sizeof(entry));
			break;

		case TARGET_TEXT_PLAIN:
		default:
			gtk_selection_data_set_text(selection_data, ee->key, -1);
			break;
		}
	
}

static void bar_pane_exif_dnd_receive(GtkWidget *pane, GdkDragContext *context,
					  gint x, gint y,
					  GtkSelectionData *selection_data, guint info,
					  guint time, gpointer data)
{
	PaneExifData *ped;
	GList *work, *list;
	gint pos;
	GtkWidget *new_entry = NULL;
	ped = g_object_get_data(G_OBJECT(pane), "pane_data");
	if (!ped) return;

	switch (info)
		{
		case TARGET_APP_EXIF_ENTRY:
			new_entry = *(gpointer *)selection_data->data;
			
			if (new_entry->parent && new_entry->parent != ped->vbox) bar_pane_exif_reparent_entry(new_entry, pane);
			
			break;
		default:
			/* FIXME: this needs a check for valid exif keys */
			new_entry = bar_pane_exif_add_entry(ped, (gchar *)selection_data->data, NULL, TRUE);
			break;
		}


	list = gtk_container_get_children(GTK_CONTAINER(ped->vbox));	
	work = list;
	pos = 0;
	while (work)
		{
		gint nx, ny;
		GtkWidget *entry = work->data;
		work = work->next;
		
		if (entry == new_entry) continue;
		
		if (GTK_WIDGET_DRAWABLE(entry) && 
		    gtk_widget_translate_coordinates(pane, entry, x, y, &nx, &ny) &&
		    ny < entry->allocation.height / 2) break;
		pos++;
		}
	g_list_free(list);

	gtk_box_reorder_child(GTK_BOX(ped->vbox), new_entry, pos);
}

static void bar_pane_exif_entry_dnd_begin(GtkWidget *entry, GdkDragContext *context, gpointer data)
{
	ExifEntry *ee = g_object_get_data(G_OBJECT(entry), "entry_data");
	if (!ee) return;
	dnd_set_drag_label(entry, context, ee->key);
}

static void bar_pane_exif_entry_dnd_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
}

static void bar_pane_exif_entry_dnd_init(GtkWidget *entry)
{
	ExifEntry *ee = g_object_get_data(G_OBJECT(entry), "entry_data");

	gtk_drag_source_set(entry, GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			    bar_pane_exif_drag_types, n_exif_entry_drag_types,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(entry), "drag_data_get",
			 G_CALLBACK(bar_pane_exif_entry_dnd_get), ee);

	g_signal_connect(G_OBJECT(entry), "drag_begin",
			 G_CALLBACK(bar_pane_exif_entry_dnd_begin), ee);
	g_signal_connect(G_OBJECT(entry), "drag_end",
			 G_CALLBACK(bar_pane_exif_entry_dnd_end), ee);
}

static void bar_pane_exif_dnd_init(GtkWidget *pane)
{
	gtk_drag_dest_set(pane,
			  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
			  bar_pane_exif_drop_types, n_exif_entry_drop_types,
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(pane), "drag_data_received",
			 G_CALLBACK(bar_pane_exif_dnd_receive), NULL);
}

static void bar_pane_exif_edit_close_cb(GtkWidget *widget, gpointer data)
{
	GenericDialog *gd = data;
	generic_dialog_close(gd);
}

static void bar_pane_exif_edit_destroy_cb(GtkWidget *widget, gpointer data)
{
	ConfDialogData *cdd = data;
	g_signal_handlers_disconnect_by_func(cdd->widget, G_CALLBACK(bar_pane_exif_edit_close_cb), cdd->gd);
	g_free(cdd);
}

static void bar_pane_exif_edit_cancel_cb(GenericDialog *gd, gpointer data)
{
}


static void bar_pane_exif_edit_ok_cb(GenericDialog *gd, gpointer data)
{
	ConfDialogData *cdd = data;
	
	/* either one or the other */
	PaneExifData *ped = g_object_get_data(G_OBJECT(cdd->widget), "pane_data");
	ExifEntry *ee = g_object_get_data(G_OBJECT(cdd->widget), "entry_data");

	if (ped)
		{
		bar_pane_exif_add_entry(ped, 
					gtk_entry_get_text(GTK_ENTRY(cdd->key_entry)),
					gtk_entry_get_text(GTK_ENTRY(cdd->title_entry)),
					cdd->if_set);
		}
	if (ee)
		{
		const gchar *title;
		GtkWidget *pane = cdd->widget->parent;
		while (pane)
			{
			ped = g_object_get_data(G_OBJECT(pane), "pane_data");
			if (ped) break;
			pane = pane->parent;
			}
		
		if (!pane) return;
		
		g_free(ee->key);
		ee->key = g_strdup(gtk_entry_get_text(GTK_ENTRY(cdd->key_entry)));
		title = gtk_entry_get_text(GTK_ENTRY(cdd->title_entry));
		if (strcmp(ee->title, title) != 0)
			{
			g_free(ee->title);
			ee->title = g_strdup(title);
			ee->auto_title = FALSE;
			}
		
		ee->if_set = cdd->if_set;

		bar_pane_exif_entry_update_title(ee);
		bar_pane_exif_update(ped);
		}
}

static void bar_pane_exif_conf_dialog(GtkWidget *widget)
{
	ConfDialogData *cdd;
	GenericDialog *gd;
	GtkWidget *table;

	/* the widget can be either ExifEntry (for editing) or Pane (for new entry)
	   we can decide it by the attached data */
	ExifEntry *ee = g_object_get_data(G_OBJECT(widget), "entry_data");

	cdd = g_new0(ConfDialogData, 1);
	
	cdd->widget = widget;


	cdd->if_set = ee ? ee->if_set : TRUE;

	cdd->gd = gd = generic_dialog_new(ee ? _("Configure entry") : _("Add entry"), "exif_entry_edit",
				widget, TRUE,
				bar_pane_exif_edit_cancel_cb, cdd);
	g_signal_connect(G_OBJECT(gd->dialog), "destroy",
			 G_CALLBACK(bar_pane_exif_edit_destroy_cb), cdd);

	/* in case the entry is deleted during editing */
	g_signal_connect(G_OBJECT(widget), "destroy",
			 G_CALLBACK(bar_pane_exif_edit_close_cb), gd);

	generic_dialog_add_message(gd, NULL, ee ? _("Configure entry") : _("Add entry"), NULL);

	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL,
				  bar_pane_exif_edit_ok_cb, TRUE);

	table = pref_table_new(gd->vbox, 3, 2, FALSE, TRUE);
	pref_table_label(table, 0, 0, _("Key:"), 1.0);

	cdd->key_entry = gtk_entry_new();
	gtk_widget_set_size_request(cdd->key_entry, 300, -1);
	if (ee) gtk_entry_set_text(GTK_ENTRY(cdd->key_entry), ee->key);
	gtk_table_attach_defaults(GTK_TABLE(table), cdd->key_entry, 1, 2, 0, 1);
	generic_dialog_attach_default(gd, cdd->key_entry);
	gtk_widget_show(cdd->key_entry);

	pref_table_label(table, 0, 1, _("Title:"), 1.0);

	cdd->title_entry = gtk_entry_new();
	gtk_widget_set_size_request(cdd->title_entry, 300, -1);
	if (ee) gtk_entry_set_text(GTK_ENTRY(cdd->title_entry), ee->title);
	gtk_table_attach_defaults(GTK_TABLE(table), cdd->title_entry, 1, 2, 1, 2);
	generic_dialog_attach_default(gd, cdd->title_entry);
	gtk_widget_show(cdd->title_entry);

	pref_checkbox_new_int(gd->vbox, _("Show only if set"), cdd->if_set, &cdd->if_set);

	gtk_widget_show(gd->dialog);
}

static void bar_pane_exif_conf_dialog_cb(GtkWidget *menu_widget, gpointer data)
{
	GtkWidget *widget = data;
	bar_pane_exif_conf_dialog(widget);
}

static void bar_pane_exif_delete_entry_cb(GtkWidget *menu_widget, gpointer data)
{
	GtkWidget *entry = data;
	gtk_widget_destroy(entry);
}

static void bar_pane_exif_toggle_show_all_cb(GtkWidget *menu_widget, gpointer data)
{
	PaneExifData *ped = data;
	ped->show_all = !ped->show_all;
	bar_pane_exif_update(ped);
}

static void bar_pane_exif_menu_popup(GtkWidget *widget, PaneExifData *ped)
{
	GtkWidget *menu;
	/* the widget can be either ExifEntry (for editing) or Pane (for new entry)
	   we can decide it by the attached data */
	ExifEntry *ee = g_object_get_data(G_OBJECT(widget), "entry_data");

	menu = popup_menu_short_lived();

	if (ee)
		{
		/* for the entry */
		gchar *conf = g_strdup_printf(_("Configure \"%s\""), ee->title);
		gchar *del = g_strdup_printf(_("Delete \"%s\""), ee->title);
		menu_item_add_stock(menu, conf, GTK_STOCK_EDIT, G_CALLBACK(bar_pane_exif_conf_dialog_cb), widget);
		menu_item_add_stock(menu, del, GTK_STOCK_DELETE, G_CALLBACK(bar_pane_exif_delete_entry_cb), widget);
		menu_item_add_divider(menu);
		g_free(conf);
		g_free(del);
		}
	/* for the pane */
	menu_item_add_stock(menu, _("Add entry"), GTK_STOCK_ADD, G_CALLBACK(bar_pane_exif_conf_dialog_cb), ped->widget);
	menu_item_add_check(menu, _("Show hidden entries"), ped->show_all, G_CALLBACK(bar_pane_exif_toggle_show_all_cb), ped);
	
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
}


static gboolean bar_pane_exif_menu_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data) 
{ 
	PaneExifData *ped = data;
	if (bevent->button == MOUSE_BUTTON_RIGHT)
		{
		bar_pane_exif_menu_popup(widget, ped);
		return TRUE;
		}
	return FALSE;
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
	ped->pane.title = bar_pane_expander_title(title);
	ped->pane.expanded = expanded;

	ped->size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	ped->widget = gtk_event_box_new();;
	ped->vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	gtk_container_add(GTK_CONTAINER(ped->widget), ped->vbox);
	gtk_widget_show(ped->vbox);

	ped->min_height = MIN_HEIGHT;
	g_object_set_data(G_OBJECT(ped->widget), "pane_data", ped);
	g_signal_connect_after(G_OBJECT(ped->widget), "destroy",
			       G_CALLBACK(bar_pane_exif_destroy), ped);
	g_signal_connect(G_OBJECT(ped->widget), "size-request",
			 G_CALLBACK(bar_pane_exif_size_request), ped);
	g_signal_connect(G_OBJECT(ped->widget), "size-allocate",
			 G_CALLBACK(bar_pane_exif_size_allocate), ped);
	
	bar_pane_exif_dnd_init(ped->widget);
	g_signal_connect(ped->widget, "button_press_event", G_CALLBACK(bar_pane_exif_menu_cb), ped);

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
