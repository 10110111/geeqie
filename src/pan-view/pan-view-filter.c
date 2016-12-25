/*
 * Copyright (C) 2006 John Ellis
 * Copyright (C) 2008 - 2016 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pan-view-filter.h"

#include "image.h"
#include "pan-item.h"
#include "pan-util.h"
#include "pan-view.h"
#include "ui_tabcomp.h"
#include "ui_misc.h"

PanViewFilterUi *pan_filter_ui_new(PanWindow *pw)
{
	PanViewFilterUi *ui = g_new0(PanViewFilterUi, 1);
	GtkWidget *combo;
	GtkWidget *hbox;

	// Build the actual filter UI.
	ui->filter_box = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
	pref_spacer(ui->filter_box, 0);
	pref_label_new(ui->filter_box, _("Keyword Filter:"));

	hbox = gtk_hbox_new(TRUE, PREF_PAD_SPACE);
	gtk_box_pack_start(GTK_BOX(ui->filter_box), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	combo = tab_completion_new_with_history(&ui->filter_entry, "", "pan_view_filter", -1,
						pan_filter_activate_cb, pw);
	gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	// TODO(xsdg): Figure out whether it's useful to keep this label around.
	ui->filter_label = gtk_label_new("");
	//gtk_box_pack_start(GTK_BOX(hbox), ui->filter_label, FALSE, FALSE, 0);
	//gtk_widget_show(ui->filter_label);

	ui->filter_kw_hbox = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox), ui->filter_kw_hbox, TRUE, TRUE, 0);
	gtk_widget_show(ui->filter_kw_hbox);

	// Build the spin-button to show/hide the filter UI.
	ui->filter_button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(ui->filter_button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click(GTK_BUTTON(ui->filter_button), FALSE);
	hbox = gtk_hbox_new(FALSE, PREF_PAD_GAP);
	gtk_container_add(GTK_CONTAINER(ui->filter_button), hbox);
	gtk_widget_show(hbox);
	ui->filter_button_arrow = gtk_arrow_new(GTK_ARROW_UP, GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(hbox), ui->filter_button_arrow, FALSE, FALSE, 0);
	gtk_widget_show(ui->filter_button_arrow);
	pref_label_new(hbox, _("Filter"));

	g_signal_connect(G_OBJECT(ui->filter_button), "clicked",
			 G_CALLBACK(pan_filter_toggle_cb), pw);

	/* Since we're using the GHashTable as a HashSet (in which key and value pointers
	 * are always identical), specifying key _and_ value destructor callbacks will
	 * cause a double-free.
	 */
	ui->filter_kw_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	return ui;
}

void pan_filter_ui_destroy(PanViewFilterUi **ui_ptr)
{
	if (ui_ptr == NULL || *ui_ptr == NULL) return;

	// Note that g_clear_pointer handles already-NULL pointers.
	g_clear_pointer(&(*ui_ptr)->filter_kw_table, g_hash_table_destroy);

	g_free(*ui_ptr);
	*ui_ptr = NULL;
}

static void pan_filter_status(PanWindow *pw, const gchar *text)
{
	gtk_label_set_text(GTK_LABEL(pw->filter_ui->filter_label), (text) ? text : "");
}

static void pan_filter_kw_button_cb(GtkButton *widget, gpointer data)
{
	PanWindow *pw = data;
	PanViewFilterUi *ui = pw->filter_ui;

	g_hash_table_remove(ui->filter_kw_table, gtk_button_get_label(GTK_BUTTON(widget)));
	gtk_widget_destroy(GTK_WIDGET(widget));

	pan_filter_status(pw, _("Removed keyword…"));
	pan_layout_update(pw);
}

void pan_filter_activate_cb(const gchar *text, gpointer data)
{
	GtkWidget *kw_button;
	PanWindow *pw = data;
	PanViewFilterUi *ui = pw->filter_ui;

	if (!text) return;

	gtk_entry_set_text(GTK_ENTRY(ui->filter_entry), "");

	if (g_hash_table_contains(ui->filter_kw_table, text))
		{
		pan_filter_status(pw, _("Already added…"));
		return;
		}

	tab_completion_append_to_history(ui->filter_entry, text);

	g_hash_table_add(ui->filter_kw_table, g_strdup(text));

	kw_button = gtk_button_new_with_label(text);
	gtk_box_pack_start(GTK_BOX(ui->filter_kw_hbox), kw_button, FALSE, FALSE, 0);
	gtk_widget_show(kw_button);

	g_signal_connect(G_OBJECT(kw_button), "clicked",
			 G_CALLBACK(pan_filter_kw_button_cb), pw);

	pan_filter_status(pw, _("Added keyword…"));
	pan_layout_update(pw);
}

void pan_filter_activate(PanWindow *pw)
{
	gchar *text;

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(pw->filter_ui->filter_entry)));
	pan_filter_activate_cb(text, pw);
	g_free(text);
}

void pan_filter_toggle_cb(GtkWidget *button, gpointer data)
{
	PanWindow *pw = data;
	PanViewFilterUi *ui = pw->filter_ui;
	gboolean visible;

	visible = gtk_widget_get_visible(ui->filter_box);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == visible) return;

	if (visible)
		{
		gtk_widget_hide(ui->filter_box);
		gtk_arrow_set(GTK_ARROW(ui->filter_button_arrow), GTK_ARROW_UP, GTK_SHADOW_NONE);
		}
	else
		{
		gtk_widget_show(ui->filter_box);
		gtk_arrow_set(GTK_ARROW(ui->filter_button_arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
		gtk_widget_grab_focus(ui->filter_entry);
		}
}

void pan_filter_toggle_visible(PanWindow *pw, gboolean enable)
{
	PanViewFilterUi *ui = pw->filter_ui;
	if (pw->fs) return;

	if (enable)
		{
		if (gtk_widget_get_visible(ui->filter_box))
			{
			gtk_widget_grab_focus(ui->filter_entry);
			}
		else
			{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->filter_button), TRUE);
			}
		}
	else
		{
		if (gtk_widget_get_visible(ui->filter_entry))
			{
			if (gtk_widget_has_focus(ui->filter_entry))
				{
				gtk_widget_grab_focus(GTK_WIDGET(pw->imd->widget));
				}
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->filter_button), FALSE);
			}
		}
}
