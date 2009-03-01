/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "layout.h"

#include "color-man.h"
#include "filedata.h"
#include "histogram.h"
#include "image.h"
#include "image-overlay.h"
#include "layout_config.h"
#include "layout_image.h"
#include "layout_util.h"
#include "menu.h"
#include "pixbuf-renderer.h"
#include "pixbuf_util.h"
#include "utilops.h"
#include "view_dir.h"
#include "view_file.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_misc.h"
#include "ui_tabcomp.h"
#include "window.h"
#include "metadata.h"
#include "rcfile.h"
#include "bar.h"
#include "bar_sort.h"

#ifdef HAVE_LIRC
#include "lirc.h"
#endif

#define MAINWINDOW_DEF_WIDTH 700
#define MAINWINDOW_DEF_HEIGHT 500

#define MAIN_WINDOW_DIV_HPOS (MAINWINDOW_DEF_WIDTH / 2)
#define MAIN_WINDOW_DIV_VPOS (MAINWINDOW_DEF_HEIGHT / 2)

#define TOOLWINDOW_DEF_WIDTH 260
#define TOOLWINDOW_DEF_HEIGHT 450

#define PROGRESS_WIDTH 150
#define ZOOM_LABEL_WIDTH 64

#define PANE_DIVIDER_SIZE 10


GList *layout_window_list = NULL;


static void layout_list_scroll_to_subpart(LayoutWindow *lw, const gchar *needle);


/*
 *-----------------------------------------------------------------------------
 * misc
 *-----------------------------------------------------------------------------
 */

gint layout_valid(LayoutWindow **lw)
{
	if (*lw == NULL)
		{
		if (layout_window_list) *lw = layout_window_list->data;
		return (*lw != NULL);
		}

	return (g_list_find(layout_window_list, *lw) != NULL);
}

LayoutWindow *layout_find_by_image(ImageWindow *imd)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		if (lw->image == imd) return lw;
		}

	return NULL;
}

LayoutWindow *layout_find_by_image_fd(ImageWindow *imd)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;
		if (lw->image->image_fd == imd->image_fd)
			return lw;

		}

	return NULL;
}

/*
 *-----------------------------------------------------------------------------
 * menu, toolbar, and dir view
 *-----------------------------------------------------------------------------
 */

static void layout_path_entry_changed_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;
	gchar *buf;

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) < 0) return;

	buf = g_strdup(gtk_entry_get_text(GTK_ENTRY(lw->path_entry)));
	if (!buf || (lw->dir_fd && strcmp(buf, lw->dir_fd->path) == 0))
		{
		g_free(buf);
		return;
		}

	layout_set_path(lw, buf);

	g_free(buf);
}

static void layout_path_entry_tab_cb(const gchar *path, gpointer data)
{
	LayoutWindow *lw = data;
	gchar *buf;
	gchar *base;

	buf = g_strdup(path);
	parse_out_relatives(buf);
	base = remove_level_from_path(buf);

	if (isdir(buf))
		{
		if ((!lw->dir_fd || strcmp(lw->dir_fd->path, buf) != 0) && layout_set_path(lw, buf))
			{
			gint pos = -1;
			/* put the G_DIR_SEPARATOR back, if we are in tab completion for a dir and result was path change */
			gtk_editable_insert_text(GTK_EDITABLE(lw->path_entry), G_DIR_SEPARATOR_S, -1, &pos);
			gtk_editable_set_position(GTK_EDITABLE(lw->path_entry),
						  strlen(gtk_entry_get_text(GTK_ENTRY(lw->path_entry))));
			}
		}
	else if (lw->dir_fd && strcmp(lw->dir_fd->path, base) == 0)
		{
		layout_list_scroll_to_subpart(lw, filename_from_path(buf));
		}

	g_free(base);
	g_free(buf);
}

static void layout_path_entry_cb(const gchar *path, gpointer data)
{
	LayoutWindow *lw = data;
	gchar *buf;

	buf = g_strdup(path);
	parse_out_relatives(buf);

	layout_set_path(lw, buf);

	g_free(buf);
}

static void layout_vd_select_cb(ViewDir *vd, const gchar *path, gpointer data)
{
	LayoutWindow *lw = data;

	layout_set_path(lw, path);
}

static void layout_path_entry_tab_append_cb(const gchar *path, gpointer data, gint n)
{
	LayoutWindow *lw = data;

	if (!lw || !lw->back_button) return;
	if (!layout_valid(&lw)) return;

	if (n >= 2)
		{
		/* Enable back button */
		gtk_widget_set_sensitive(lw->back_button, TRUE);
		}
	else
		{
		/* Disable back button */
		gtk_widget_set_sensitive(lw->back_button, FALSE);
		}
}

static GtkWidget *layout_tool_setup(LayoutWindow *lw)
{
	GtkWidget *box;
	GtkWidget *menu_bar;
	GtkWidget *tabcomp;

	box = gtk_vbox_new(FALSE, 0);

	menu_bar = layout_actions_menu_bar(lw);
	gtk_box_pack_start(GTK_BOX(box), menu_bar, FALSE, FALSE, 0);
	gtk_widget_show(menu_bar);

	lw->toolbar = layout_actions_toolbar(lw);
	gtk_box_pack_start(GTK_BOX(box), lw->toolbar, FALSE, FALSE, 0);
	if (!lw->options.toolbar_hidden) gtk_widget_show(lw->toolbar);

	tabcomp = tab_completion_new_with_history(&lw->path_entry, NULL, "path_list", -1,
						  layout_path_entry_cb, lw);
	tab_completion_add_tab_func(lw->path_entry, layout_path_entry_tab_cb, lw);
	tab_completion_add_append_func(lw->path_entry, layout_path_entry_tab_append_cb, lw);
	gtk_box_pack_start(GTK_BOX(box), tabcomp, FALSE, FALSE, 0);
	gtk_widget_show(tabcomp);

	g_signal_connect(G_OBJECT(lw->path_entry->parent), "changed",
			 G_CALLBACK(layout_path_entry_changed_cb), lw);

	lw->vd = vd_new(lw->options.dir_view_type, lw->dir_fd);
	vd_set_layout(lw->vd, lw);
	vd_set_select_func(lw->vd, layout_vd_select_cb, lw);

	lw->dir_view = lw->vd->widget;

	gtk_box_pack_start(GTK_BOX(box), lw->dir_view, TRUE, TRUE, 0);
	gtk_widget_show(lw->dir_view);

	gtk_widget_show(box);

	return box;
}

/*
 *-----------------------------------------------------------------------------
 * sort button (and menu)
 *-----------------------------------------------------------------------------
 */

static void layout_sort_menu_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw;
	SortType type;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) return;

	lw = submenu_item_get_data(widget);
	if (!lw) return;

	type = (SortType)GPOINTER_TO_INT(data);

	layout_sort_set(lw, type, lw->sort_ascend);
}

static void layout_sort_menu_ascend_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_sort_set(lw, lw->sort_method, !lw->sort_ascend);
}

static void layout_sort_menu_hide_cb(GtkWidget *widget, gpointer data)
{
	/* destroy the menu */
#if GTK_CHECK_VERSION(2,12,0)
	g_object_unref(widget);
#else
	gtk_widget_unref(GTK_WIDGET(widget));
#endif
}

static void layout_sort_button_press_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;
	GtkWidget *menu;
	GdkEvent *event;
	guint32 etime;

	menu = submenu_add_sort(NULL, G_CALLBACK(layout_sort_menu_cb), lw, FALSE, FALSE, TRUE, lw->sort_method);

	/* take ownership of menu */
#ifdef GTK_OBJECT_FLOATING
	/* GTK+ < 2.10 */
	g_object_ref(G_OBJECT(menu));
	gtk_object_sink(GTK_OBJECT(menu));
#else
	/* GTK+ >= 2.10 */
	g_object_ref_sink(G_OBJECT(menu));
#endif

	/* ascending option */
	menu_item_add_divider(menu);
	menu_item_add_check(menu, _("Ascending"), lw->sort_ascend, G_CALLBACK(layout_sort_menu_ascend_cb), lw);

	g_signal_connect(G_OBJECT(menu), "selection_done",
			 G_CALLBACK(layout_sort_menu_hide_cb), NULL);

	event = gtk_get_current_event();
	if (event)
		{
		etime = gdk_event_get_time(event);
		gdk_event_free(event);
		}
	else
		{
		etime = 0;
		}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, etime);
}

static GtkWidget *layout_sort_button(LayoutWindow *lw)
{
	GtkWidget *button;

	button = gtk_button_new_with_label(sort_type_get_text(lw->sort_method));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(layout_sort_button_press_cb), lw);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	return button;
}

/*
 *-----------------------------------------------------------------------------
 * color profile button (and menu)
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_LCMS

static void layout_color_menu_enable_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;

	layout_image_color_profile_set_use(lw, (!layout_image_color_profile_get_use(lw)));
	layout_image_refresh(lw);
}

static void layout_color_menu_use_image_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;
	gint input, screen, use_image;

	if (!layout_image_color_profile_get(lw, &input, &screen, &use_image)) return;
	layout_image_color_profile_set(lw, input, screen, !use_image);
	layout_image_refresh(lw);
}

#define COLOR_MENU_KEY "color_menu_key"

static void layout_color_menu_input_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;
	gint type;
	gint input, screen, use_image;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) return;

	type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), COLOR_MENU_KEY));
	if (type < 0 || type >= COLOR_PROFILE_FILE + COLOR_PROFILE_INPUTS) return;

	if (!layout_image_color_profile_get(lw, &input, &screen, &use_image)) return;
	if (type == input) return;

	layout_image_color_profile_set(lw, type, screen, use_image);
	layout_image_refresh(lw);
}

static void layout_color_menu_screen_cb(GtkWidget *widget, gpointer data)
{
	LayoutWindow *lw = data;
	gint type;
	gint input, screen, use_image;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) return;

	type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), COLOR_MENU_KEY));
	if (type < 0 || type > 1) return;

	if (!layout_image_color_profile_get(lw, &input, &screen, &use_image)) return;
	if (type == screen) return;

	layout_image_color_profile_set(lw, input, type, use_image);
	layout_image_refresh(lw);
}

static gchar *layout_color_name_parse(const gchar *name)
{
	if (!name || !*name) return g_strdup(_("Empty"));
	return g_strdelimit(g_strdup(name), "_", '-');
}

#endif /* HAVE_LCMS */

static void layout_color_button_press_cb(GtkWidget *widget, gpointer data)
{
#ifndef HAVE_LCMS
	gchar *msg = g_strdup_printf(_("This installation of %s was not built with support for color profiles."), GQ_APPNAME);
	file_util_warning_dialog(_("Color profiles not supported"),
				 msg,
				 GTK_STOCK_DIALOG_INFO, widget);
	g_free(msg);
	return;
#else
	LayoutWindow *lw = data;
	GtkWidget *menu;
	GtkWidget *item;
	gchar *buf;
	gint active;
	gint input = 0;
	gint screen = 0;
	gint use_image = 0;
	gint from_image = 0;
	gint image_profile;
	gint i;
	
	if (!layout_image_color_profile_get(lw, &input, &screen, &use_image)) return;

	image_profile = layout_image_color_profile_get_from_image(lw);
	from_image = use_image && (image_profile != COLOR_PROFILE_NONE);
	menu = popup_menu_short_lived();

	active = layout_image_color_profile_get_use(lw);
	menu_item_add_check(menu, _("Use _color profiles"), active,
			    G_CALLBACK(layout_color_menu_enable_cb), lw);

	menu_item_add_divider(menu);

	item = menu_item_add_check(menu, _("Use profile from _image"), use_image,
			    G_CALLBACK(layout_color_menu_use_image_cb), lw);
	gtk_widget_set_sensitive(item, active);

	for (i = COLOR_PROFILE_SRGB; i < COLOR_PROFILE_FILE; i++)
		{
		const gchar *label;

		switch (i)
			{
			case COLOR_PROFILE_SRGB: 	label = _("sRGB"); break;
			case COLOR_PROFILE_ADOBERGB:	label = _("AdobeRGB compatible"); break;
			default:			label = "fixme"; break;
			}
		buf = g_strdup_printf(_("Input _%d: %s%s"), i, label, (i == image_profile) ? " *" : "");
	  	item = menu_item_add_radio(menu, (i == COLOR_PROFILE_SRGB) ? NULL : item,
				   buf, (input == i),
				   G_CALLBACK(layout_color_menu_input_cb), lw);
		g_free(buf);
		g_object_set_data(G_OBJECT(item), COLOR_MENU_KEY, GINT_TO_POINTER(i));
		gtk_widget_set_sensitive(item, active && !from_image);
		}

	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		const gchar *name = options->color_profile.input_name[i];
		const gchar *file = options->color_profile.input_file[i];
		gchar *end;

		if (!name || !name[0]) name = filename_from_path(file);

		end = layout_color_name_parse(name);
		buf = g_strdup_printf(_("Input _%d: %s"), i + COLOR_PROFILE_FILE, end);
		g_free(end);

		item = menu_item_add_radio(menu, item,
					   buf, (i + COLOR_PROFILE_FILE == input),
					   G_CALLBACK(layout_color_menu_input_cb), lw);
		g_free(buf);
		g_object_set_data(G_OBJECT(item), COLOR_MENU_KEY, GINT_TO_POINTER(i + COLOR_PROFILE_FILE));
		gtk_widget_set_sensitive(item, active && !from_image && is_readable_file(file));
		}

	menu_item_add_divider(menu);

	item = menu_item_add_radio(menu, NULL,
				   _("Screen sRGB"), (screen == 0),
				   G_CALLBACK(layout_color_menu_screen_cb), lw);

	g_object_set_data(G_OBJECT(item), COLOR_MENU_KEY, GINT_TO_POINTER(0));
	gtk_widget_set_sensitive(item, active);

	item = menu_item_add_radio(menu, item,
				   _("_Screen profile"), (screen == 1),
				   G_CALLBACK(layout_color_menu_screen_cb), lw);
	g_object_set_data(G_OBJECT(item), COLOR_MENU_KEY, GINT_TO_POINTER(1));
	gtk_widget_set_sensitive(item, active && is_readable_file(options->color_profile.screen_file));

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
#endif /* HAVE_LCMS */
}

static GtkWidget *layout_color_button(LayoutWindow *lw)
{
	GtkWidget *button;
	GtkWidget *image;
	gint enable;

	button = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_SELECT_COLOR, GTK_ICON_SIZE_MENU);
	gtk_container_add(GTK_CONTAINER(button), image);
	gtk_widget_show(image);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(layout_color_button_press_cb), lw);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

#ifdef HAVE_LCMS
	enable = (lw->image) ? lw->image->color_profile_enable : FALSE;
#else
	enable = FALSE;
#endif
	gtk_widget_set_sensitive(image, enable);

	return button;
}

/*
 *-----------------------------------------------------------------------------
 * write button
 *-----------------------------------------------------------------------------
 */

static void layout_write_button_press_cb(GtkWidget *widget, gpointer data)
{
	metadata_write_queue_confirm(NULL, NULL);
}

static GtkWidget *layout_write_button(LayoutWindow *lw)
{
	GtkWidget *button;
	GtkWidget *image;

	button = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
	gtk_container_add(GTK_CONTAINER(button), image);
	gtk_widget_show(image);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(layout_write_button_press_cb), lw);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	
	gtk_widget_set_sensitive(button, metadata_queue_length() > 0);
	
	return button;
}


/*
 *-----------------------------------------------------------------------------
 * status bar
 *-----------------------------------------------------------------------------
 */

void layout_status_update_write(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;
	if (!lw->info_write) return;

	gtk_widget_set_sensitive(lw->info_write, metadata_queue_length() > 0);
	/* FIXME: maybe show also the number of files */
}

void layout_status_update_write_all(void)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		layout_status_update_write(lw);
		}
}


void layout_status_update_progress(LayoutWindow *lw, gdouble val, const gchar *text)
{
	if (!layout_valid(&lw)) return;
	if (!lw->info_progress_bar) return;

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(lw->info_progress_bar), val);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(lw->info_progress_bar), (text) ? text : " ");
}

void layout_status_update_info(LayoutWindow *lw, const gchar *text)
{
	gchar *buf = NULL;

	if (!layout_valid(&lw)) return;

	if (!text)
		{
		guint n;
		gint64 n_bytes = 0;
	
		n = layout_list_count(lw, &n_bytes);
		
		if (n)
			{
			guint s;
			gint64 s_bytes = 0;
			const gchar *ss;

			if (layout_image_slideshow_active(lw))
				{
				if (!layout_image_slideshow_paused(lw))
					{
					ss = _(" Slideshow");
					}
				else
					{
					ss = _(" Paused");
					}
				}
			else
				{
				ss = "";
				}
	
			s = layout_selection_count(lw, &s_bytes);
	
			layout_bars_new_selection(lw, s);
	
			if (s > 0)
				{
				gchar *b = text_from_size_abrev(n_bytes);
				gchar *sb = text_from_size_abrev(s_bytes);
				buf = g_strdup_printf(_("%s, %d files (%s, %d)%s"), b, n, sb, s, ss);
				g_free(b);
				g_free(sb);
				}
			else if (n > 0)
				{
				gchar *b = text_from_size_abrev(n_bytes);
				buf = g_strdup_printf(_("%s, %d files%s"), b, n, ss);
				g_free(b);
				}
			else
				{
				buf = g_strdup_printf(_("%d files%s"), n, ss);
				}
	
			text = buf;
	
			image_osd_update(lw->image);
			}
		else
			{
			text = "";
			}
	}
	
	if (lw->info_status) gtk_label_set_text(GTK_LABEL(lw->info_status), text);
	g_free(buf);
}

void layout_status_update_image(LayoutWindow *lw)
{
	guint64 n;

	if (!layout_valid(&lw) || !lw->image) return;

	n = layout_list_count(lw, NULL);
	
	if (!n)
		{
		gtk_label_set_text(GTK_LABEL(lw->info_zoom), "");
		gtk_label_set_text(GTK_LABEL(lw->info_details), "");
		}
	else
		{
		gchar *text;
		gchar *b;

		text = image_zoom_get_as_text(lw->image);
		gtk_label_set_text(GTK_LABEL(lw->info_zoom), text);
		g_free(text);

		b = image_get_fd(lw->image) ? text_from_size(image_get_fd(lw->image)->size) : g_strdup("0");

		if (lw->image->unknown)
			{
			if (image_get_path(lw->image) && !access_file(image_get_path(lw->image), R_OK))
				{
				text = g_strdup_printf(_("(no read permission) %s bytes"), b);
				}
			else
				{
				text = g_strdup_printf(_("( ? x ? ) %s bytes"), b);
				}
			}
		else
			{
			gint width, height;
	
			image_get_image_size(lw->image, &width, &height);
			text = g_strdup_printf(_("( %d x %d ) %s bytes"),
					       width, height, b);
			}

		g_free(b);
		
		gtk_label_set_text(GTK_LABEL(lw->info_details), text);
		g_free(text);
		}
}

void layout_status_update_all(LayoutWindow *lw)
{
	layout_status_update_progress(lw, 0.0, NULL);
	layout_status_update_info(lw, NULL);
	layout_status_update_image(lw);
	layout_status_update_write(lw);
}

static GtkWidget *layout_status_label(gchar *text, GtkWidget *box, gint start, gint size, gint expand)
{
	GtkWidget *label;
	GtkWidget *frame;

	frame = gtk_frame_new(NULL);
	if (size) gtk_widget_set_size_request(frame, size, -1);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	if (start)
		{
		gtk_box_pack_start(GTK_BOX(box), frame, expand, expand, 0);
		}
	else
		{
		gtk_box_pack_end(GTK_BOX(box), frame, expand, expand, 0);
		}
	gtk_widget_show(frame);

	label = gtk_label_new(text ? text : "");
	gtk_container_add(GTK_CONTAINER(frame), label);
	gtk_widget_show(label);

	return label;
}

static void layout_status_setup(LayoutWindow *lw, GtkWidget *box, gint small_format)
{
	GtkWidget *hbox;

	if (lw->info_box) return;

	if (small_format)
		{
		lw->info_box = gtk_vbox_new(FALSE, 0);
		}
	else
		{
		lw->info_box = gtk_hbox_new(FALSE, 0);
		}
	gtk_box_pack_end(GTK_BOX(box), lw->info_box, FALSE, FALSE, 0);
	gtk_widget_show(lw->info_box);

	if (small_format)
		{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(lw->info_box), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);
		}
	else
		{
		hbox = lw->info_box;
		}
	lw->info_progress_bar = gtk_progress_bar_new();
	gtk_widget_set_size_request(lw->info_progress_bar, PROGRESS_WIDTH, -1);
	gtk_box_pack_start(GTK_BOX(hbox), lw->info_progress_bar, FALSE, FALSE, 0);
	gtk_widget_show(lw->info_progress_bar);

	lw->info_sort = layout_sort_button(lw);
	gtk_box_pack_start(GTK_BOX(hbox), lw->info_sort, FALSE, FALSE, 0);
	gtk_widget_show(lw->info_sort);

	lw->info_color = layout_color_button(lw);
	gtk_widget_show(lw->info_color);

	lw->info_write = layout_write_button(lw);
	gtk_widget_show(lw->info_write);

	if (small_format) gtk_box_pack_end(GTK_BOX(hbox), lw->info_color, FALSE, FALSE, 0);
	if (small_format) gtk_box_pack_end(GTK_BOX(hbox), lw->info_write, FALSE, FALSE, 0);

	lw->info_status = layout_status_label(NULL, lw->info_box, TRUE, 0, (!small_format));

	if (small_format)
		{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(lw->info_box), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);
		}
	else
		{
		hbox = lw->info_box;
		}
	lw->info_details = layout_status_label(NULL, hbox, TRUE, 0, TRUE);
	if (!small_format) gtk_box_pack_start(GTK_BOX(hbox), lw->info_color, FALSE, FALSE, 0);
	if (!small_format) gtk_box_pack_start(GTK_BOX(hbox), lw->info_write, FALSE, FALSE, 0);
	lw->info_zoom = layout_status_label(NULL, hbox, FALSE, ZOOM_LABEL_WIDTH, FALSE);
}

/*
 *-----------------------------------------------------------------------------
 * views
 *-----------------------------------------------------------------------------
 */

static GtkWidget *layout_tools_new(LayoutWindow *lw)
{
	lw->dir_view = layout_tool_setup(lw);
	return lw->dir_view;
}

static void layout_list_status_cb(ViewFile *vf, gpointer data)
{
	LayoutWindow *lw = data;

	layout_status_update_info(lw, NULL);
}

static void layout_list_thumb_cb(ViewFile *vf, gdouble val, const gchar *text, gpointer data)
{
	LayoutWindow *lw = data;

	layout_status_update_progress(lw, val, text);
}

static GtkWidget *layout_list_new(LayoutWindow *lw)
{
	lw->vf = vf_new(lw->file_view_type, NULL);
	vf_set_layout(lw->vf, lw);

	vf_set_status_func(lw->vf, layout_list_status_cb, lw);
	vf_set_thumb_status_func(lw->vf, layout_list_thumb_cb, lw);

	vf_marks_set(lw->vf, lw->options.show_marks);

	switch (lw->file_view_type)
	{
	case FILEVIEW_ICON:
		break;
	case FILEVIEW_LIST:
		vf_thumb_set(lw->vf, lw->options.show_thumbnails);
		break;
	}

	return lw->vf->widget;
}

static void layout_list_sync_thumb(LayoutWindow *lw)
{
	if (lw->vf) vf_thumb_set(lw->vf, lw->options.show_thumbnails);
}

static void layout_list_sync_marks(LayoutWindow *lw)
{
	if (lw->vf) vf_marks_set(lw->vf, lw->options.show_marks);
}

static void layout_list_scroll_to_subpart(LayoutWindow *lw, const gchar *needle)
{
	if (!lw) return;
#if 0
	if (lw->vf) vf_scroll_to_subpart(lw->vf, needle);
#endif
}

GList *layout_list(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return NULL;

	if (lw->vf) return vf_get_list(lw->vf);

	return NULL;
}

guint layout_list_count(LayoutWindow *lw, gint64 *bytes)
{
	if (!layout_valid(&lw)) return 0;

	if (lw->vf) return vf_count(lw->vf, bytes);

	return 0;
}

FileData *layout_list_get_fd(LayoutWindow *lw, gint index)
{
	if (!layout_valid(&lw)) return NULL;

	if (lw->vf) return vf_index_get_data(lw->vf, index);

	return NULL;
}

gint layout_list_get_index(LayoutWindow *lw, FileData *fd)
{
	if (!layout_valid(&lw) || !fd) return -1;

	if (lw->vf) return vf_index_by_path(lw->vf, fd->path);

	return -1;
}

void layout_list_sync_fd(LayoutWindow *lw, FileData *fd)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_select_by_fd(lw->vf, fd);
}

static void layout_list_sync_sort(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_sort_set(lw->vf, lw->sort_method, lw->sort_ascend);
}

GList *layout_selection_list(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return NULL;

	if (layout_image_get_collection(lw, NULL))
		{
		FileData *fd;

		fd = layout_image_get_fd(lw);
		if (fd) return g_list_append(NULL, file_data_ref(fd));
		return NULL;
		}

	if (lw->vf) return vf_selection_get_list(lw->vf);

	return NULL;
}

GList *layout_selection_list_by_index(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return NULL;

	if (lw->vf) return vf_selection_get_list_by_index(lw->vf);

	return NULL;
}

guint layout_selection_count(LayoutWindow *lw, gint64 *bytes)
{
	if (!layout_valid(&lw)) return 0;

	if (lw->vf) return vf_selection_count(lw->vf, bytes);

	return 0;
}

void layout_select_all(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_select_all(lw->vf);
}

void layout_select_none(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_select_none(lw->vf);
}

void layout_select_invert(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_select_invert(lw->vf);
}

void layout_mark_to_selection(LayoutWindow *lw, gint mark, MarkToSelectionMode mode)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_mark_to_selection(lw->vf, mark, mode);
}

void layout_selection_to_mark(LayoutWindow *lw, gint mark, SelectionToMarkMode mode)
{
	if (!layout_valid(&lw)) return;

	if (lw->vf) vf_selection_to_mark(lw->vf, mark, mode);

	layout_status_update_info(lw, NULL); /* osd in fullscreen mode */
}

/*
 *-----------------------------------------------------------------------------
 * access
 *-----------------------------------------------------------------------------
 */

const gchar *layout_get_path(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return NULL;
	return lw->dir_fd ? lw->dir_fd->path : NULL;
}

static void layout_sync_path(LayoutWindow *lw)
{
	if (!lw->dir_fd) return;

	if (lw->path_entry) gtk_entry_set_text(GTK_ENTRY(lw->path_entry), lw->dir_fd->path);
	if (lw->vd) vd_set_fd(lw->vd, lw->dir_fd);

	if (lw->vf) vf_set_fd(lw->vf, lw->dir_fd);
}

gint layout_set_path(LayoutWindow *lw, const gchar *path)
{
	FileData *fd;
	if (!path) return FALSE;
	fd = file_data_new_simple(path);
	gint ret = layout_set_fd(lw, fd);
	file_data_unref(fd);
	return ret;
}


gint layout_set_fd(LayoutWindow *lw, FileData *fd)
{
	gint have_file = FALSE;

	if (!layout_valid(&lw)) return FALSE;

	if (!fd || !isname(fd->path)) return FALSE;
	if (lw->dir_fd && fd == lw->dir_fd)
		{
		return TRUE;
		}

	if (isdir(fd->path))
		{
		if (lw->dir_fd)
			{
			file_data_unregister_real_time_monitor(lw->dir_fd);
			file_data_unref(lw->dir_fd);
			}
		lw->dir_fd = file_data_ref(fd);
		file_data_register_real_time_monitor(fd);
		}
	else
		{
		gchar *base;

		base = remove_level_from_path(fd->path);
		if (lw->dir_fd && strcmp(lw->dir_fd->path, base) == 0)
			{
			g_free(base);
			}
		else if (isdir(base))
			{
			if (lw->dir_fd)
				{
				file_data_unregister_real_time_monitor(lw->dir_fd);
				file_data_unref(lw->dir_fd);
				}
			lw->dir_fd = file_data_new_simple(base);
			file_data_register_real_time_monitor(lw->dir_fd);
			g_free(base);
			}
		else
			{
			g_free(base);
			return FALSE;
			}
		if (isfile(fd->path)) have_file = TRUE;
		}

	if (lw->path_entry) tab_completion_append_to_history(lw->path_entry, lw->dir_fd->path);
	layout_sync_path(lw);
	layout_list_sync_sort(lw);
	
	if (have_file)
		{
		gint row;

		row = layout_list_get_index(lw, fd);
		if (row >= 0)
			{
			layout_image_set_index(lw, row);
			}
		else
			{
			layout_image_set_fd(lw, fd);
			}
		}
	else if (!options->lazy_image_sync)
		{
		layout_image_set_index(lw, 0);
		}

	if (options->metadata.confirm_on_dir_change)
		metadata_write_queue_confirm(NULL, NULL);

	return TRUE;
}

static void layout_refresh_lists(LayoutWindow *lw)
{
	if (lw->vd) vd_refresh(lw->vd);

	if (lw->vf) vf_refresh(lw->vf);
}

void layout_refresh(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;

	DEBUG_1("layout refresh");

	layout_refresh_lists(lw);

	if (lw->image) layout_image_refresh(lw);
}

void layout_thumb_set(LayoutWindow *lw, gint enable)
{
	if (!layout_valid(&lw)) return;

	if (lw->options.show_thumbnails == enable) return;

	lw->options.show_thumbnails = enable;

	layout_util_sync_thumb(lw);
	layout_list_sync_thumb(lw);
}

void layout_marks_set(LayoutWindow *lw, gint enable)
{
	if (!layout_valid(&lw)) return;

	if (lw->options.show_marks == enable) return;

	lw->options.show_marks = enable;

//	layout_util_sync_marks(lw);
	layout_list_sync_marks(lw);
}

gint layout_thumb_get(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return FALSE;

	return lw->options.show_thumbnails;
}

gint layout_marks_get(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return FALSE;

	return lw->options.show_marks;
}

void layout_sort_set(LayoutWindow *lw, SortType type, gint ascend)
{
	if (!layout_valid(&lw)) return;
	if (lw->sort_method == type && lw->sort_ascend == ascend) return;

	lw->sort_method = type;
	lw->sort_ascend = ascend;

	if (lw->info_sort) gtk_label_set_text(GTK_LABEL(GTK_BIN(lw->info_sort)->child),
					      sort_type_get_text(type));
	layout_list_sync_sort(lw);
}

gint layout_sort_get(LayoutWindow *lw, SortType *type, gint *ascend)
{
	if (!layout_valid(&lw)) return FALSE;

	if (type) *type = lw->sort_method;
	if (ascend) *ascend = lw->sort_ascend;

	return TRUE;
}

gint layout_geometry_get(LayoutWindow *lw, gint *x, gint *y, gint *w, gint *h)
{
	if (!layout_valid(&lw)) return FALSE;

	gdk_window_get_root_origin(lw->window->window, x, y);
	gdk_drawable_get_size(lw->window->window, w, h);

	return TRUE;
}

gint layout_geometry_get_dividers(LayoutWindow *lw, gint *h, gint *v)
{
	if (!layout_valid(&lw)) return FALSE;

	if (lw->h_pane && GTK_PANED(lw->h_pane)->child1->allocation.x >= 0)
		{
		*h = GTK_PANED(lw->h_pane)->child1->allocation.width;
		}
	else if (h != &lw->options.main_window.hdivider_pos)
		{
		*h = lw->options.main_window.hdivider_pos;
		}

	if (lw->v_pane && GTK_PANED(lw->v_pane)->child1->allocation.x >= 0)
		{
		*v = GTK_PANED(lw->v_pane)->child1->allocation.height;
		}
	else if (v != &lw->options.main_window.vdivider_pos)
		{
		*v = lw->options.main_window.vdivider_pos;
		}

	return TRUE;
}

void layout_views_set(LayoutWindow *lw, DirViewType dir_view_type, FileViewType file_view_type)
{
	if (!layout_valid(&lw)) return;

	if (lw->options.dir_view_type == dir_view_type && lw->file_view_type == file_view_type) return;

	lw->options.dir_view_type = dir_view_type;
	lw->file_view_type = file_view_type;

	layout_style_set(lw, -1, NULL);
}

gint layout_views_get(LayoutWindow *lw, DirViewType *dir_view_type, FileViewType *file_view_type)
{
	if (!layout_valid(&lw)) return FALSE;

	*dir_view_type = lw->options.dir_view_type;
	*file_view_type = lw->file_view_type;

	return TRUE;
}

/*
 *-----------------------------------------------------------------------------
 * location utils
 *-----------------------------------------------------------------------------
 */

static gint layout_location_single(LayoutLocation l)
{
	return (l == LAYOUT_LEFT ||
		l == LAYOUT_RIGHT ||
		l == LAYOUT_TOP ||
		l == LAYOUT_BOTTOM);
}

static gint layout_location_vertical(LayoutLocation l)
{
	return (l & LAYOUT_TOP ||
		l & LAYOUT_BOTTOM);
}

static gint layout_location_first(LayoutLocation l)
{
	return (l & LAYOUT_TOP ||
		l & LAYOUT_LEFT);
}

static LayoutLocation layout_grid_compass(LayoutWindow *lw)
{
	if (layout_location_single(lw->dir_location)) return lw->dir_location;
	if (layout_location_single(lw->file_location)) return lw->file_location;
	return lw->image_location;
}

static void layout_location_compute(LayoutLocation l1, LayoutLocation l2,
				    GtkWidget *s1, GtkWidget *s2,
				    GtkWidget **d1, GtkWidget **d2)
{
	LayoutLocation l;

	l = l1 & l2;	/* get common compass direction */
	l = l1 - l;	/* remove it */

	if (layout_location_first(l))
		{
		*d1 = s1;
		*d2 = s2;
		}
	else
		{
		*d1 = s2;
		*d2 = s1;
		}
}

/*
 *-----------------------------------------------------------------------------
 * tools window (for floating/hidden)
 *-----------------------------------------------------------------------------
 */

gint layout_geometry_get_tools(LayoutWindow *lw, gint *x, gint *y, gint *w, gint *h, gint *divider_pos)
{
	if (!layout_valid(&lw)) return FALSE;

	if (!lw->tools || !GTK_WIDGET_VISIBLE(lw->tools))
		{
		/* use the stored values (sort of breaks success return value) */

		*divider_pos = lw->options.float_window.vdivider_pos;

		return FALSE;
		}

	gdk_window_get_root_origin(lw->tools->window, x, y);
	gdk_drawable_get_size(lw->tools->window, w, h);

	if (GTK_IS_VPANED(lw->tools_pane))
		{
		*divider_pos = GTK_PANED(lw->tools_pane)->child1->allocation.height;
		}
	else
		{
		*divider_pos = GTK_PANED(lw->tools_pane)->child1->allocation.width;
		}

	return TRUE;
}

static void layout_tools_geometry_sync(LayoutWindow *lw)
{
	layout_geometry_get_tools(lw, &options->layout.float_window.x, &options->layout.float_window.x,
				  &options->layout.float_window.w, &options->layout.float_window.h, &lw->options.float_window.vdivider_pos);
}

static void layout_tools_hide(LayoutWindow *lw, gint hide)
{
	if (!lw->tools) return;

	if (hide)
		{
		if (GTK_WIDGET_VISIBLE(lw->tools))
			{
			layout_tools_geometry_sync(lw);
			gtk_widget_hide(lw->tools);
			}
		}
	else
		{
		if (!GTK_WIDGET_VISIBLE(lw->tools))
			{
			gtk_widget_show(lw->tools);
			if (lw->vf) vf_refresh(lw->vf);
			}
		}

	lw->options.tools_hidden = hide;
}

static gint layout_tools_delete_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	LayoutWindow *lw = data;

	layout_tools_float_toggle(lw);

	return TRUE;
}

static void layout_tools_setup(LayoutWindow *lw, GtkWidget *tools, GtkWidget *files)
{
	GtkWidget *vbox;
	GtkWidget *w1, *w2;
	gint vertical;
	gint new_window = FALSE;

	vertical = (layout_location_single(lw->image_location) && !layout_location_vertical(lw->image_location)) ||
		   (!layout_location_single(lw->image_location) && layout_location_vertical(layout_grid_compass(lw)));
#if 0
	layout_location_compute(lw->dir_location, lw->file_location,
				tools, files, &w1, &w2);
#endif
	/* for now, tools/dir are always first in order */
	w1 = tools;
	w2 = files;

	if (!lw->tools)
		{
		GdkGeometry geometry;
		GdkWindowHints hints;

		lw->tools = window_new(GTK_WINDOW_TOPLEVEL, "tools", PIXBUF_INLINE_ICON_TOOLS, NULL, _("Tools"));
		g_signal_connect(G_OBJECT(lw->tools), "delete_event",
				 G_CALLBACK(layout_tools_delete_cb), lw);
		layout_keyboard_init(lw, lw->tools);

		if (options->layout.save_window_positions)
			{
			hints = GDK_HINT_USER_POS;
			}
		else
			{
			hints = 0;
			}

		geometry.min_width = DEFAULT_MINIMAL_WINDOW_SIZE;
		geometry.min_height = DEFAULT_MINIMAL_WINDOW_SIZE;
		geometry.base_width = TOOLWINDOW_DEF_WIDTH;
		geometry.base_height = TOOLWINDOW_DEF_HEIGHT;
		gtk_window_set_geometry_hints(GTK_WINDOW(lw->tools), NULL, &geometry,
					      GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE | hints);


		gtk_window_set_resizable(GTK_WINDOW(lw->tools), TRUE);
		gtk_container_set_border_width(GTK_CONTAINER(lw->tools), 0);

		new_window = TRUE;
		}
	else
		{
		layout_tools_geometry_sync(lw);
		/* dump the contents */
		gtk_widget_destroy(GTK_BIN(lw->tools)->child);
		}

	layout_actions_add_window(lw, lw->tools);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(lw->tools), vbox);
	gtk_widget_show(vbox);

	layout_status_setup(lw, vbox, TRUE);

	if (vertical)
		{
		lw->tools_pane = gtk_vpaned_new();
		}
	else
		{
		lw->tools_pane = gtk_hpaned_new();
		}
	gtk_box_pack_start(GTK_BOX(vbox), lw->tools_pane, TRUE, TRUE, 0);
	gtk_widget_show(lw->tools_pane);

	gtk_paned_pack1(GTK_PANED(lw->tools_pane), w1, FALSE, TRUE);
	gtk_paned_pack2(GTK_PANED(lw->tools_pane), w2, TRUE, TRUE);

	gtk_widget_show(tools);
	gtk_widget_show(files);

	if (new_window)
		{
		if (options->layout.save_window_positions)
			{
			gtk_window_set_default_size(GTK_WINDOW(lw->tools), options->layout.float_window.w, options->layout.float_window.h);
			gtk_window_move(GTK_WINDOW(lw->tools), options->layout.float_window.x, options->layout.float_window.y);
			}
		else
			{
			if (vertical)
				{
				gtk_window_set_default_size(GTK_WINDOW(lw->tools),
							    TOOLWINDOW_DEF_WIDTH, TOOLWINDOW_DEF_HEIGHT);
				}
			else
				{
				gtk_window_set_default_size(GTK_WINDOW(lw->tools),
							    TOOLWINDOW_DEF_HEIGHT, TOOLWINDOW_DEF_WIDTH);
				}
			}
		}

	if (!options->layout.save_window_positions)
		{
		if (vertical)
			{
			lw->options.float_window.vdivider_pos = MAIN_WINDOW_DIV_VPOS;
			}
		else
			{
			lw->options.float_window.vdivider_pos = MAIN_WINDOW_DIV_HPOS;
			}
		}

	gtk_paned_set_position(GTK_PANED(lw->tools_pane), lw->options.float_window.vdivider_pos);
}

/*
 *-----------------------------------------------------------------------------
 * glue (layout arrangement)
 *-----------------------------------------------------------------------------
 */

static void layout_grid_compute(LayoutWindow *lw,
				GtkWidget *image, GtkWidget *tools, GtkWidget *files,
				GtkWidget **w1, GtkWidget **w2, GtkWidget **w3)
{
	/* heh, this was fun */

	if (layout_location_single(lw->dir_location))
		{
		if (layout_location_first(lw->dir_location))
			{
			*w1 = tools;
			layout_location_compute(lw->file_location, lw->image_location, files, image, w2, w3);
			}
		else
			{
			*w3 = tools;
			layout_location_compute(lw->file_location, lw->image_location, files, image, w1, w2);
			}
		}
	else if (layout_location_single(lw->file_location))
		{
		if (layout_location_first(lw->file_location))
			{
			*w1 = files;
			layout_location_compute(lw->dir_location, lw->image_location, tools, image, w2, w3);
			}
		else
			{
			*w3 = files;
			layout_location_compute(lw->dir_location, lw->image_location, tools, image, w1, w2);
			}
		}
	else
		{
		/* image */
		if (layout_location_first(lw->image_location))
			{
			*w1 = image;
			layout_location_compute(lw->file_location, lw->dir_location, files, tools, w2, w3);
			}
		else
			{
			*w3 = image;
			layout_location_compute(lw->file_location, lw->dir_location, files, tools, w1, w2);
			}
		}
}

void layout_split_change(LayoutWindow *lw, ImageSplitMode mode)
{
	GtkWidget *image;
	gint i;

	for (i = 0; i < MAX_SPLIT_IMAGES; i++)
		{
		if (lw->split_images[i])
			{
			gtk_widget_hide(lw->split_images[i]->widget);
			if (lw->split_images[i]->widget->parent != lw->utility_box)
				gtk_container_remove(GTK_CONTAINER(lw->split_images[i]->widget->parent), lw->split_images[i]->widget);
			}
		}
	gtk_container_remove(GTK_CONTAINER(lw->utility_box), lw->split_image_widget);

	image = layout_image_setup_split(lw, mode);

	gtk_box_pack_start(GTK_BOX(lw->utility_box), image, TRUE, TRUE, 0);
	gtk_box_reorder_child(GTK_BOX(lw->utility_box), image, 0);
	gtk_widget_show(image);
}

static void layout_grid_setup(LayoutWindow *lw)
{
	gint priority_location;
	GtkWidget *h;
	GtkWidget *v;
	GtkWidget *w1, *w2, *w3;

	GtkWidget *image_sb; /* image together with sidebars in utility box */
	GtkWidget *tools;
	GtkWidget *files;

	layout_actions_setup(lw);
	layout_actions_add_window(lw, lw->window);

	lw->group_box = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(lw->main_box), lw->group_box, TRUE, TRUE, 0);
	gtk_widget_show(lw->group_box);

	priority_location = layout_grid_compass(lw);

	if (lw->utility_box)
		{
		image_sb = lw->utility_box;
		}
	else
		{
		GtkWidget *image; /* image or split images together */
		image = layout_image_setup_split(lw, lw->split_mode);
		image_sb = layout_bars_prepare(lw, image);
		}
	
	tools = layout_tools_new(lw);
	files = layout_list_new(lw);


	if (lw->options.tools_float || lw->options.tools_hidden)
		{
		gtk_box_pack_start(GTK_BOX(lw->group_box), image_sb, TRUE, TRUE, 0);
		gtk_widget_show(image_sb);

		layout_tools_setup(lw, tools, files);

		gtk_widget_grab_focus(lw->image->widget);

		return;
		}
	else if (lw->tools)
		{
		layout_tools_geometry_sync(lw);
		gtk_widget_destroy(lw->tools);
		lw->tools = NULL;
		lw->tools_pane = NULL;
		}

	layout_status_setup(lw, lw->group_box, FALSE);

	layout_grid_compute(lw, image_sb, tools, files, &w1, &w2, &w3);

	v = lw->v_pane = gtk_vpaned_new();

	h = lw->h_pane = gtk_hpaned_new();

	if (!layout_location_vertical(priority_location))
		{
		GtkWidget *tmp;

		tmp = v;
		v = h;
		h = tmp;
		}

	gtk_box_pack_start(GTK_BOX(lw->group_box), v, TRUE, TRUE, 0);

	if (!layout_location_first(priority_location))
		{
		gtk_paned_pack1(GTK_PANED(v), h, FALSE, TRUE);
		gtk_paned_pack2(GTK_PANED(v), w3, TRUE, TRUE);

		gtk_paned_pack1(GTK_PANED(h), w1, FALSE, TRUE);
		gtk_paned_pack2(GTK_PANED(h), w2, TRUE, TRUE);
		}
	else
		{
		gtk_paned_pack1(GTK_PANED(v), w1, FALSE, TRUE);
		gtk_paned_pack2(GTK_PANED(v), h, TRUE, TRUE);

		gtk_paned_pack1(GTK_PANED(h), w2, FALSE, TRUE);
		gtk_paned_pack2(GTK_PANED(h), w3, TRUE, TRUE);
		}

	gtk_widget_show(image_sb);
	gtk_widget_show(tools);
	gtk_widget_show(files);

	gtk_widget_show(v);
	gtk_widget_show(h);

	/* fix to have image pane visible when it is left and priority widget */
	if (lw->options.main_window.hdivider_pos == -1 &&
	    w1 == image_sb &&
	    !layout_location_vertical(priority_location) &&
	    layout_location_first(priority_location))
		{
		gtk_widget_set_size_request(image_sb, 200, -1);
		}

	gtk_paned_set_position(GTK_PANED(lw->h_pane), lw->options.main_window.hdivider_pos);
	gtk_paned_set_position(GTK_PANED(lw->v_pane), lw->options.main_window.vdivider_pos);

	gtk_widget_grab_focus(lw->image->widget);
}

void layout_style_set(LayoutWindow *lw, gint style, const gchar *order)
{
	FileData *dir_fd;

	if (!layout_valid(&lw)) return;

	if (style != -1)
		{
		LayoutLocation d, f, i;

		layout_config_parse(style, order, &d,  &f, &i);

		if (lw->dir_location == d &&
		    lw->file_location == f &&
		    lw->image_location == i) return;

		lw->dir_location = d;
		lw->file_location = f;
		lw->image_location = i;
		}

	/* remember state */

	layout_image_slideshow_stop(lw);
	layout_image_full_screen_stop(lw);

	dir_fd = lw->dir_fd;
	if (dir_fd) file_data_unregister_real_time_monitor(dir_fd);
	lw->dir_fd = NULL;

	/* lw->image is preserved together with lw->utility_box */
	if (lw->utility_box)
		{
		/* preserve utility_box (image + sidebars) to be reused later in layout_grid_setup */
		gtk_widget_hide(lw->utility_box);
		g_object_ref(lw->utility_box);
		gtk_container_remove(GTK_CONTAINER(lw->utility_box->parent), lw->utility_box);
		}

	layout_geometry_get_dividers(lw, &lw->options.main_window.hdivider_pos, &lw->options.main_window.vdivider_pos);

	/* clear it all */

	lw->h_pane = NULL;
	lw->v_pane = NULL;

	lw->toolbar = NULL;
	lw->path_entry = NULL;
	lw->dir_view = NULL;
	lw->vd = NULL;

	lw->file_view = NULL;
	lw->vf = NULL;

	lw->info_box = NULL;
	lw->info_progress_bar = NULL;
	lw->info_sort = NULL;
	lw->info_color = NULL;
	lw->info_status = NULL;
	lw->info_details = NULL;
	lw->info_zoom = NULL;

	if (lw->ui_manager) g_object_unref(lw->ui_manager);
	lw->ui_manager = NULL;
	lw->action_group = NULL;
	lw->action_group_external = NULL;

	gtk_container_remove(GTK_CONTAINER(lw->main_box), lw->group_box);
	lw->group_box = NULL;

	/* re-fill */

	layout_grid_setup(lw);
	layout_tools_hide(lw, lw->options.tools_hidden);

	layout_util_sync(lw);
	layout_status_update_all(lw);

	/* sync */

	if (image_get_fd(lw->image))
		{
		layout_set_fd(lw, image_get_fd(lw->image));
		}
	else
		{
		layout_set_fd(lw, dir_fd);
		}
	image_top_window_set_sync(lw->image, (lw->options.tools_float || lw->options.tools_hidden));

	/* clean up */

	file_data_unref(dir_fd);
}

void layout_styles_update(void)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		layout_style_set(lw, options->layout.style, options->layout.order);
		}
}

void layout_colors_update(void)
{
	GList *work;

	work = layout_window_list;
	while (work)
		{
		LayoutWindow *lw = work->data;
		work = work->next;

		if (!lw->image) continue;
		image_background_set_color(lw->image, options->image.use_custom_border_color ? &options->image.border_color : NULL);
		}
}

void layout_tools_float_toggle(LayoutWindow *lw)
{
	gint popped;

	if (!lw) return;

	if (!lw->options.tools_hidden)
		{
		popped = !lw->options.tools_float;
		}
	else
		{
		popped = TRUE;
		}

	if (lw->options.tools_float == popped)
		{
		if (popped && lw->options.tools_hidden)
			{
			layout_tools_float_set(lw, popped, FALSE);
			}
		}
	else
		{
		if (lw->options.tools_float)
			{
			layout_tools_float_set(lw, FALSE, FALSE);
			}
		else
			{
			layout_tools_float_set(lw, TRUE, FALSE);
			}
		}
}

void layout_tools_hide_toggle(LayoutWindow *lw)
{
	if (!lw) return;

	layout_tools_float_set(lw, lw->options.tools_float, !lw->options.tools_hidden);
}

void layout_tools_float_set(LayoutWindow *lw, gint popped, gint hidden)
{
	if (!layout_valid(&lw)) return;

	if (lw->options.tools_float == popped && lw->options.tools_hidden == hidden) return;

	if (lw->options.tools_float == popped && lw->options.tools_float && lw->tools)
		{
		layout_tools_hide(lw, hidden);
		return;
		}

	lw->options.tools_float = popped;
	lw->options.tools_hidden = hidden;

	layout_style_set(lw, -1, NULL);
}

gint layout_tools_float_get(LayoutWindow *lw, gint *popped, gint *hidden)
{
	if (!layout_valid(&lw)) return FALSE;

	*popped = lw->options.tools_float;
	*hidden = lw->options.tools_hidden;

	return TRUE;
}

void layout_toolbar_toggle(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return;
	if (!lw->toolbar) return;

	lw->options.toolbar_hidden = !lw->options.toolbar_hidden;

	if (lw->options.toolbar_hidden)
		{
		if (GTK_WIDGET_VISIBLE(lw->toolbar)) gtk_widget_hide(lw->toolbar);
		}
	else
		{
		if (!GTK_WIDGET_VISIBLE(lw->toolbar)) gtk_widget_show(lw->toolbar);
		}
}

gint layout_toolbar_hidden(LayoutWindow *lw)
{
	if (!layout_valid(&lw)) return TRUE;

	return lw->options.toolbar_hidden;
}

/*
 *-----------------------------------------------------------------------------
 * base
 *-----------------------------------------------------------------------------
 */

void layout_sync_options_with_current_state(LayoutWindow *lw)
{
	Histogram *histogram;
	if (!layout_valid(&lw)) return;

	lw->options.main_window.maximized =  window_maximized(lw->window);
	if (!lw->options.main_window.maximized)
		{
		layout_geometry_get(lw, &lw->options.main_window.x, &lw->options.main_window.y,
				    &lw->options.main_window.w, &lw->options.main_window.h);
		}

	layout_geometry_get_dividers(lw, &lw->options.main_window.hdivider_pos, &lw->options.main_window.vdivider_pos);

//	layout_sort_get(NULL, &options->file_sort.method, &options->file_sort.ascending);

	layout_geometry_get_tools(lw, &lw->options.float_window.x, &lw->options.float_window.y,
				  &lw->options.float_window.w, &lw->options.float_window.h, &lw->options.float_window.vdivider_pos);

	lw->options.image_overlay.state = image_osd_get(lw->image);
	histogram = image_osd_get_histogram(lw->image);
	
	lw->options.image_overlay.histogram_channel = histogram->histogram_channel;
	lw->options.image_overlay.histogram_mode = histogram->histogram_mode;

//	if (options->startup.restore_path && options->startup.use_last_path)
//		{
//		g_free(options->startup.path);
//		options->startup.path = g_strdup(layout_get_path(NULL));
//		}
}


void layout_close(LayoutWindow *lw)
{
	if (layout_window_list && layout_window_list->next)
		{
		layout_free(lw);
		}
	else
		{
		exit_program();
		}
}

void layout_free(LayoutWindow *lw)
{
	if (!lw) return;

	layout_window_list = g_list_remove(layout_window_list, lw);

	if (lw->exif_window) g_signal_handlers_disconnect_matched(G_OBJECT(lw->exif_window), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, lw);
		
	layout_bars_close(lw);

	gtk_widget_destroy(lw->window);
	
	if (lw->split_image_sizegroup) g_object_unref(lw->split_image_sizegroup);

	file_data_unregister_notify_func(layout_image_notify_cb, lw);

	if (lw->dir_fd)
		{
		file_data_unregister_real_time_monitor(lw->dir_fd);
		file_data_unref(lw->dir_fd);
		}

	string_list_free(lw->toolbar_actions);
	free_layout_options_content(&lw->options);
	g_free(lw);
}

static gint layout_delete_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	LayoutWindow *lw = data;

	layout_close(lw);
	return TRUE;
}

LayoutWindow *layout_new(FileData *dir_fd, LayoutOptions *lop)
{
	return layout_new_with_geometry(dir_fd, lop, NULL);
}

LayoutWindow *layout_new_with_geometry(FileData *dir_fd, LayoutOptions *lop,
				       const gchar *geometry)
{
	LayoutWindow *lw;
	GdkGeometry hint;
	GdkWindowHints hint_mask;
	Histogram *histogram;

	lw = g_new0(LayoutWindow, 1);

	if (lop)
		copy_layout_options(&lw->options, lop);
	else
		copy_layout_options(&lw->options, &options->layout);

	lw->sort_method = SORT_NAME;
	lw->sort_ascend = TRUE;

//	lw->options.tools_float = popped;
//	lw->options.tools_hidden = hidden;

	lw->utility_box = NULL;
	lw->bar_sort = NULL;
//	lw->bar_sort_enabled = options->panels.sort.enabled;

	lw->bar = NULL;
//	lw->bar_enabled = options->panels.info.enabled;

	lw->exif_window = NULL;
	/* default layout */

	layout_config_parse(lw->options.style, lw->options.order,
			    &lw->dir_location,  &lw->file_location, &lw->image_location);
	if (lw->options.dir_view_type >= VIEW_DIR_TYPES_COUNT) lw->options.dir_view_type = 0;
	if (lw->options.file_view_type >= VIEW_FILE_TYPES_COUNT) lw->options.file_view_type = 0;

	/* divider positions */

	if (!lw->options.save_window_positions)
		{
		lw->options.main_window.hdivider_pos = MAIN_WINDOW_DIV_HPOS;
		lw->options.main_window.vdivider_pos = MAIN_WINDOW_DIV_VPOS;
		lw->options.float_window.vdivider_pos = MAIN_WINDOW_DIV_VPOS;
		}

	/* window */

	lw->window = window_new(GTK_WINDOW_TOPLEVEL, GQ_APPNAME_LC, NULL, NULL, NULL);
	gtk_window_set_resizable(GTK_WINDOW(lw->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(lw->window), 0);

	if (lw->options.save_window_positions)
		{
		hint_mask = GDK_HINT_USER_POS;
		}
	else
		{
		hint_mask = 0;
		}

	hint.min_width = 32;
	hint.min_height = 32;
	hint.base_width = 0;
	hint.base_height = 0;
	gtk_window_set_geometry_hints(GTK_WINDOW(lw->window), NULL, &hint,
				      GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE | hint_mask);

	if (lw->options.save_window_positions)
		{
		gtk_window_set_default_size(GTK_WINDOW(lw->window), lw->options.main_window.w, lw->options.main_window.h);
//		if (!layout_window_list)
//			{
		gtk_window_move(GTK_WINDOW(lw->window), lw->options.main_window.x, lw->options.main_window.y);
		if (lw->options.main_window.maximized) gtk_window_maximize(GTK_WINDOW(lw->window));
//			}
		}
	else
		{
		gtk_window_set_default_size(GTK_WINDOW(lw->window), MAINWINDOW_DEF_WIDTH, MAINWINDOW_DEF_HEIGHT);
		}

	g_signal_connect(G_OBJECT(lw->window), "delete_event",
			 G_CALLBACK(layout_delete_cb), lw);

	layout_keyboard_init(lw, lw->window);

#ifdef HAVE_LIRC
	layout_image_lirc_init(lw);
#endif

	lw->main_box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(lw->window), lw->main_box);
	gtk_widget_show(lw->main_box);

	layout_grid_setup(lw);
	image_top_window_set_sync(lw->image, (lw->options.tools_float || lw->options.tools_hidden));

	layout_util_sync(lw);
	layout_status_update_all(lw);

	if (dir_fd)
		{
		layout_set_fd(lw, dir_fd);
		}
	else
		{
		GdkPixbuf *pixbuf;

		pixbuf = pixbuf_inline(PIXBUF_INLINE_LOGO);
		
		/* FIXME: the zoom value set here is the value, which is then copied again and again
		   in "Leave Zoom at previous setting" mode. This is not ideal.  */
		image_change_pixbuf(lw->image, pixbuf, 0.0, FALSE);
		g_object_unref(pixbuf);
		}

	if (geometry)
		{
		if (!gtk_window_parse_geometry(GTK_WINDOW(lw->window), geometry))
			{
			log_printf("%s", _("Invalid geometry\n"));
			}
		}

	gtk_widget_show(lw->window);
	layout_tools_hide(lw, lw->options.tools_hidden);

	image_osd_set(lw->image, lw->options.image_overlay.state);
	histogram = image_osd_get_histogram(lw->image);
	
	histogram->histogram_channel = lw->options.image_overlay.histogram_channel;
	histogram->histogram_mode = lw->options.image_overlay.histogram_mode;

	layout_window_list = g_list_append(layout_window_list, lw);

	file_data_register_notify_func(layout_image_notify_cb, lw, NOTIFY_PRIORITY_LOW);

	return lw;
}

void layout_write_attributes(LayoutOptions *layout, GString *outstr, gint indent)
{
	WRITE_INT(*layout, style);
	WRITE_CHAR(*layout, order);
	WRITE_UINT(*layout, dir_view_type);
	WRITE_UINT(*layout, file_view_type);
	WRITE_BOOL(*layout, show_marks);
	WRITE_BOOL(*layout, show_thumbnails);
	WRITE_BOOL(*layout, show_directory_date);
	WRITE_CHAR(*layout, home_path);
	WRITE_SEPARATOR();

	WRITE_BOOL(*layout, save_window_positions);
	WRITE_INT(*layout, main_window.x);
	WRITE_INT(*layout, main_window.y);
	WRITE_INT(*layout, main_window.w);
	WRITE_INT(*layout, main_window.h);
	WRITE_BOOL(*layout, main_window.maximized);
	WRITE_INT(*layout, main_window.hdivider_pos);
	WRITE_INT(*layout, main_window.vdivider_pos);
	WRITE_SEPARATOR();

	WRITE_INT(*layout, float_window.x);
	WRITE_INT(*layout, float_window.y);
	WRITE_INT(*layout, float_window.w);
	WRITE_INT(*layout, float_window.h);
	WRITE_INT(*layout, float_window.vdivider_pos);
	WRITE_SEPARATOR();

	WRITE_INT(*layout, properties_window.w);
	WRITE_INT(*layout, properties_window.h);
	WRITE_SEPARATOR();

	WRITE_BOOL(*layout, tools_float);
	WRITE_BOOL(*layout, tools_hidden);
	WRITE_BOOL(*layout, tools_restore_state);
	WRITE_SEPARATOR();

	WRITE_BOOL(*layout, toolbar_hidden);
	
	WRITE_UINT(*layout, image_overlay.state);
	WRITE_INT(*layout, image_overlay.histogram_channel);
	WRITE_INT(*layout, image_overlay.histogram_mode);
}


void layout_write_config(LayoutWindow *lw, GString *outstr, gint indent)
{
	layout_sync_options_with_current_state(lw);
	WRITE_STRING("<layout\n");
	layout_write_attributes(&lw->options, outstr, indent + 1);
	WRITE_STRING(">\n");

	bar_sort_write_config(lw->bar_sort, outstr, indent + 1);
	bar_write_config(lw->bar, outstr, indent + 1);
	
	layout_toolbar_write_config(lw, outstr, indent + 1);

	WRITE_STRING("</layout>\n");
}

void layout_load_attributes(LayoutOptions *layout, const gchar **attribute_names, const gchar **attribute_values)
{
	
	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		/* layout options */

		if (READ_INT(*layout, style)) continue;
		if (READ_CHAR(*layout, order)) continue;
		
		if (READ_UINT(*layout, dir_view_type)) continue;
		if (READ_UINT(*layout, file_view_type)) continue;
		if (READ_BOOL(*layout, show_marks)) continue;
		if (READ_BOOL(*layout, show_thumbnails)) continue;
		if (READ_BOOL(*layout, show_directory_date)) continue;
		if (READ_CHAR(*layout, home_path)) continue;

		/* window positions */

		if (READ_BOOL(*layout, save_window_positions)) continue;

		if (READ_INT(*layout, main_window.x)) continue;
		if (READ_INT(*layout, main_window.y)) continue;
		if (READ_INT(*layout, main_window.w)) continue;
		if (READ_INT(*layout, main_window.h)) continue;
		if (READ_BOOL(*layout, main_window.maximized)) continue;
		if (READ_INT(*layout, main_window.hdivider_pos)) continue;
		if (READ_INT(*layout, main_window.vdivider_pos)) continue;

		if (READ_INT(*layout, float_window.x)) continue;
		if (READ_INT(*layout, float_window.y)) continue;
		if (READ_INT(*layout, float_window.w)) continue;
		if (READ_INT(*layout, float_window.h)) continue;
		if (READ_INT(*layout, float_window.vdivider_pos)) continue;
	
		if (READ_INT(*layout, properties_window.w)) continue;
		if (READ_INT(*layout, properties_window.h)) continue;

		if (READ_BOOL(*layout, tools_float)) continue;
		if (READ_BOOL(*layout, tools_hidden)) continue;
		if (READ_BOOL(*layout, tools_restore_state)) continue;
		if (READ_BOOL(*layout, toolbar_hidden)) continue;

		if (READ_UINT(*layout, image_overlay.state)) continue;
		if (READ_INT(*layout, image_overlay.histogram_channel)) continue;
		if (READ_INT(*layout, image_overlay.histogram_mode)) continue;

		DEBUG_1("unknown attribute %s = %s", option, value);
		}

}

static void layout_config_commandline(LayoutOptions *lop, gchar **path)
{
	if (command_line->startup_blank)
		{
		*path = NULL;
		}
	else if (command_line->file)
		{
		*path = g_strdup(command_line->file);
		}
	else if (command_line->path)
		{
		*path = g_strdup(command_line->path);
		}
	else if (options->startup.restore_path && options->startup.path && isdir(options->startup.path))
		{
		*path = g_strdup(options->startup.path);
		}
	else
		{
		*path = get_current_dir();
		}
	
	if (command_line->tools_show)
		{
		lop->tools_float = FALSE;
		lop->tools_hidden = FALSE;
		}
	else if (command_line->tools_hide)
		{
		lop->tools_hidden = TRUE;
		}
}

LayoutWindow *layout_new_from_config(const gchar **attribute_names, const gchar **attribute_values, gboolean use_commandline)
{
	LayoutOptions lop;
	LayoutWindow *lw;
	gchar *path = NULL;
	
	memset(&lop, 0, sizeof(LayoutOptions));
	copy_layout_options(&lop, &options->layout);

	if (attribute_names) layout_load_attributes(&lop, attribute_names, attribute_values);
	
	if (use_commandline)
		{
		layout_config_commandline(&lop, &path);
		}
	else if (options->startup.restore_path && options->startup.path && isdir(options->startup.path))
		{
		path = g_strdup(options->startup.path);
		}
	else
		{
		path = get_current_dir();
		}

	lw = layout_new_with_geometry(NULL, &lop, use_commandline ? command_line->geometry : NULL);
	layout_sort_set(lw, options->file_sort.method, options->file_sort.ascending);
	layout_set_path(lw, path);

	if (use_commandline && command_line->startup_full_screen) layout_image_full_screen_start(lw);
	if (use_commandline && command_line->startup_in_slideshow) layout_image_slideshow_start(lw);


	g_free(path);
	free_layout_options_content(&lop);
	return lw;
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
