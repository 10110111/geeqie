/*
 * Copyright (C) 2018 The Geeqie Team
 *
 * Author: Colin Clark
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

#include "main.h"
#include "print.h"

#include "filedata.h"
#include "image-load.h"
#include "ui_misc.h"
#include "ui_fileops.h"

#define PRINT_SETTINGS "print_settings" // filename save printer settings
#define PAGE_SETUP "page_setup" // filename save page setup

/* padding between objects */
#define PRINT_TEXT_PADDING 3.0

/* method to use when scaling down image data */
#define PRINT_MAX_INTERP GDK_INTERP_HYPER

typedef enum {
	TEXT_INFO_FILENAME = 1 << 0,
	TEXT_INFO_FILEDATE = 1 << 1,
	TEXT_INFO_FILESIZE = 1 << 2,
	TEXT_INFO_DIMENSIONS = 1 << 3,
	TEXT_INFO_FILEPATH = 1 << 4
} TextInfo;

/* reverse order is important */
typedef enum {
	FOOTER_2,
	FOOTER_1,
	HEADER_2,
	HEADER_1
} TextPosition;

typedef struct _PrintWindow PrintWindow;
struct _PrintWindow
{
	GtkWidget *vbox;
	GList *source_selection;

	TextInfo	text_fields;
	gint		 job_page;
	GtkTextBuffer *page_text;
	ImageLoader	*job_loader;

	GList *print_pixbuf_queue;
	gboolean job_render_finished;
	GSList *image_group;
	GSList *page_group;
};

static gint print_layout_page_count(PrintWindow *pw)
{
	gint images;

	images = g_list_length(pw->source_selection);

	if (images < 1 ) return 0;

	return images;
}

static gboolean print_job_render_image(PrintWindow *pw);

static void print_job_render_image_loader_done(ImageLoader *il, gpointer data)
{
	PrintWindow *pw = data;
	GdkPixbuf *pixbuf;

	pixbuf = image_loader_get_pixbuf(il);

	g_object_ref(pixbuf);
	pw->print_pixbuf_queue = g_list_append(pw->print_pixbuf_queue, pixbuf);

	image_loader_free(pw->job_loader);
	pw->job_loader = NULL;

	pw->job_page++;

	if (!print_job_render_image(pw))
		{
		pw->job_render_finished = TRUE;
		}
}

static gboolean print_job_render_image(PrintWindow *pw)
{
	FileData *fd = NULL;

	fd = g_list_nth_data(pw->source_selection, pw->job_page);
	if (!fd) return FALSE;

	image_loader_free(pw->job_loader);
	pw->job_loader = NULL;

	pw->job_loader = image_loader_new(fd);
	g_signal_connect(G_OBJECT(pw->job_loader), "done",
						(GCallback)print_job_render_image_loader_done, pw);

	if (!image_loader_start(pw->job_loader))
		{
		image_loader_free(pw->job_loader);
		pw->job_loader= NULL;
		}

	return TRUE;
}

static void print_text_field_set(PrintWindow *pw, TextInfo field, gboolean active)
{
	if (active)
		{
		pw->text_fields |= field;
		}
	else
		{
		pw->text_fields &= ~field;
		}
}

static void print_text_cb_name(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	print_text_field_set(pw, TEXT_INFO_FILENAME, active);
}

static void print_text_cb_path(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	print_text_field_set(pw, TEXT_INFO_FILEPATH, active);
}

static void print_text_cb_date(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	print_text_field_set(pw, TEXT_INFO_FILEDATE, active);
}

static void print_text_cb_size(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	print_text_field_set(pw, TEXT_INFO_FILESIZE, active);
}

static void print_text_cb_dims(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	print_text_field_set(pw, TEXT_INFO_DIMENSIONS, active);
}

static void print_set_font_cb(GtkWidget *widget, gpointer data)
{
	gpointer option;

	if (g_strcmp0(data, "Image text font") == 0)
		{
		option = options->printer.image_font;
		}
	else
		{
		option = options->printer.page_font;
		}

#if GTK_CHECK_VERSION(3,4,0)
	GtkWidget *dialog;
	char *font;
	PangoFontDescription *font_desc;

	dialog = gtk_font_chooser_dialog_new(data, GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dialog), option);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_CANCEL)
		{
		font_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(dialog));
		font = pango_font_description_to_string(font_desc);
		g_free(option);
		option = g_strdup(font);
		g_free(font);
		}

	gtk_widget_destroy(dialog);
#else
	const char *font;

	font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));
	option = g_strdup(font);
#endif
}

static gint set_toggle(GSList *list, TextPosition pos)
{
	GtkToggleButton *current_sel;
	GtkToggleButton *new_sel;
	gint new_pos = - 1;

	current_sel = g_slist_nth(list, pos)->data;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(current_sel)))
		{
		new_pos = (pos - 1);
		if (new_pos < 0)
			{
			new_pos = HEADER_1;
			}
		new_sel = g_slist_nth(list, new_pos)->data;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(new_sel), TRUE);
		}
	return new_pos;
}

static void image_text_position_h1_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->page_group, HEADER_1);
		if (new_set >= 0)
			{
			options->printer.page_text_position = new_set;
			}
		options->printer.image_text_position = HEADER_1;
		}
}

static void image_text_position_h2_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->page_group, HEADER_2);
		if (new_set >= 0)
			{
			options->printer.page_text_position = new_set;
			}
		options->printer.image_text_position = HEADER_2;
		}
}

static void image_text_position_f1_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->page_group, FOOTER_1);
		if (new_set >= 0)
			{
			options->printer.page_text_position = new_set;
			}
		options->printer.image_text_position = FOOTER_1;
		}
}

static void image_text_position_f2_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->page_group, FOOTER_2);
		if (new_set >= 0)
			{
			options->printer.page_text_position = new_set;
			}
		options->printer.image_text_position = FOOTER_2;
		}
}

static void page_text_position_h1_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->image_group, HEADER_1);
		if (new_set >= 0)
			{
			options->printer.image_text_position = new_set;
			}
		options->printer.page_text_position = HEADER_1;
		}
}

static void page_text_position_h2_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->image_group, HEADER_2);
		if (new_set >= 0)
			{
			options->printer.image_text_position = new_set;
			}
		options->printer.page_text_position = HEADER_2;
		}
}

static void page_text_position_f1_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->image_group, FOOTER_1);
		if (new_set >= 0)
			{
			options->printer.image_text_position = new_set;
			}
		options->printer.page_text_position = FOOTER_1;
		}
}

static void page_text_position_f2_cb(GtkWidget *widget, gpointer data)
{
	PrintWindow *pw = data;
	gint new_set;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		new_set = set_toggle(pw->image_group, FOOTER_2);
		if (new_set >= 0)
			{
			options->printer.image_text_position = new_set;
			}
		options->printer.page_text_position = FOOTER_2;
		}
}

static void print_text_menu(GtkWidget *box, PrintWindow *pw)
{
	GtkWidget *group;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *image_text_button;
	GtkWidget *page_text_button;
	GtkWidget *subgroup;
	GtkWidget *page_text_view;

	group = pref_group_new(box, FALSE, _("Image text"), GTK_ORIENTATION_VERTICAL);

	image_text_button = pref_checkbox_new_int(group, _("Show image text"),
										options->printer.show_image_text, &options->printer.show_image_text);

	subgroup = pref_box_new(group, FALSE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);

	pref_checkbox_link_sensitivity(image_text_button, subgroup);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(subgroup), hbox, FALSE, FALSE, 0);

	/* order is important */
	button1 = pref_radiobutton_new(hbox, NULL,  "Header 1",
							options->printer.image_text_position == HEADER_1,
							G_CALLBACK(image_text_position_h1_cb), pw);
	button1 = pref_radiobutton_new(hbox, button1,  "Header 2",
							options->printer.image_text_position == HEADER_2,
							G_CALLBACK(image_text_position_h2_cb), pw);
	button1 = pref_radiobutton_new(hbox, button1, "Footer 1",
							options->printer.image_text_position == FOOTER_1,
							G_CALLBACK(image_text_position_f1_cb), pw);
	button1 = pref_radiobutton_new(hbox, button1, "Footer 2",
							options->printer.image_text_position == FOOTER_2,
							G_CALLBACK(image_text_position_f2_cb), pw);
	gtk_widget_show(hbox);
	pw->image_group = (gtk_radio_button_get_group(GTK_RADIO_BUTTON(button1)));

	pref_checkbox_new(subgroup, _("Name"), (pw->text_fields & TEXT_INFO_FILENAME),
			  G_CALLBACK(print_text_cb_name), pw);
	pref_checkbox_new(subgroup, _("Path"), (pw->text_fields & TEXT_INFO_FILEPATH),
			  G_CALLBACK(print_text_cb_path), pw);
	pref_checkbox_new(subgroup, _("Date"), (pw->text_fields & TEXT_INFO_FILEDATE),
			  G_CALLBACK(print_text_cb_date), pw);
	pref_checkbox_new(subgroup, _("Size"), (pw->text_fields & TEXT_INFO_FILESIZE),
			  G_CALLBACK(print_text_cb_size), pw);
	pref_checkbox_new(subgroup, _("Dimensions"), (pw->text_fields & TEXT_INFO_DIMENSIONS),
			  G_CALLBACK(print_text_cb_dims), pw);

	hbox = pref_box_new(subgroup, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);

#if GTK_CHECK_VERSION(3,4,0)
	button = pref_button_new(NULL, GTK_STOCK_SELECT_FONT, _("Font"), FALSE,
				 G_CALLBACK(print_set_font_cb), "Image text font");
#else
	button = gtk_font_button_new();
	gtk_font_button_set_title(GTK_FONT_BUTTON(button), "Image text Font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(button), options->printer.image_font);
	g_signal_connect(G_OBJECT(button), "font-set",
				 G_CALLBACK(print_set_font_cb), "Image text font");
#endif
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	pref_spacer(group, PREF_PAD_GAP);

	group = pref_group_new(box, FALSE, _("Page text"), GTK_ORIENTATION_VERTICAL);

	page_text_button = pref_checkbox_new_int(group, _("Show page text"),
					  options->printer.show_page_text, &options->printer.show_page_text);

	subgroup = pref_box_new(group, FALSE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);
	pref_checkbox_link_sensitivity(page_text_button, subgroup);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(subgroup), hbox, FALSE, FALSE, 0);

	/* order is important */
	button2 = pref_radiobutton_new(hbox, NULL, "Header 1",
							options->printer.page_text_position == HEADER_1,
							G_CALLBACK(page_text_position_h1_cb), pw);
	button2 = pref_radiobutton_new(hbox, button2,  "Header 2",
							options->printer.page_text_position == HEADER_2,
							G_CALLBACK(page_text_position_h2_cb), pw);
	button2 = pref_radiobutton_new(hbox, button2, "Footer 1",
							options->printer.page_text_position == FOOTER_1,
							G_CALLBACK(page_text_position_f1_cb), pw);
	button2 = pref_radiobutton_new(hbox, button2, "Footer 2",
							options->printer.page_text_position == FOOTER_2,
							G_CALLBACK(page_text_position_f2_cb), pw);
	gtk_widget_show(hbox);
	pw->page_group = (gtk_radio_button_get_group(GTK_RADIO_BUTTON(button2)));

	GtkWidget *scrolled;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled, 50, 50);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(subgroup), scrolled, TRUE, TRUE, 5);
	gtk_widget_show(scrolled);

	page_text_view = gtk_text_view_new();
	pw->page_text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page_text_view ));
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(pw->page_text), options->printer.page_text, -1);
	g_object_ref(pw->page_text);

	gtk_widget_set_tooltip_markup(page_text_view, ("Text shown on each page of a single or multi-page print job"));
	gtk_container_add(GTK_CONTAINER(scrolled), page_text_view);
	gtk_widget_show(page_text_view);

	hbox = pref_box_new(subgroup, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);

#if GTK_CHECK_VERSION(3,4,0)
	button = pref_button_new(NULL, GTK_STOCK_SELECT_FONT, _("Font"), FALSE,
				 G_CALLBACK(print_set_font_cb), "Page text font");
#else
	button = gtk_font_button_new();
	gtk_font_button_set_title(GTK_FONT_BUTTON(button), "Page text Font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(button), options->printer.page_font);
	g_signal_connect(G_OBJECT(button), "font-set",
				 G_CALLBACK(print_set_font_cb), "Page text font");
#endif
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
}

static gboolean paginate_cb(GtkPrintOperation *operation,
									GtkPrintContext *context,
									gpointer data)
{
	PrintWindow *pw = data;

	if (pw->job_render_finished)
		{
		return TRUE;
		}
	else
		{
		return FALSE;
		}
}

static void draw_page(GtkPrintOperation *operation, GtkPrintContext *context,
									gint page_nr, gpointer data)
{
	PrintWindow *pw = data;
	FileData *fd;
	cairo_t *cr;
	gdouble context_width, context_height;
	gdouble pixbuf_image_width, pixbuf_image_height;
	gdouble width_offset;
	gdouble height_offset;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_scaled;
	PangoLayout *layout_image = NULL;
	PangoLayout *layout_page = NULL;
	PangoFontDescription *desc;
	GString *image_text = g_string_new(NULL);
	GString *page_text = g_string_new(NULL);
	PangoRectangle ink_rect, logical_rect;
	gdouble w, h, scale;
	gdouble image_text_width, image_text_height, page_text_width, page_text_height;
	gint image_y;
	gint incr_y;
	gdouble pango_height;
	gdouble pango_image_height;
	gdouble pango_page_height;
	GtkTextIter start, end;
	gchar *tmp;

	pixbuf = g_list_nth_data(pw->print_pixbuf_queue, page_nr);
	pixbuf_image_width = gdk_pixbuf_get_width(pixbuf);
	pixbuf_image_height = gdk_pixbuf_get_height(pixbuf);

	fd = g_list_nth_data(pw->source_selection, page_nr);

	if (options->printer.show_image_text)
		{
		if (pw->text_fields & TEXT_INFO_FILENAME)
			{
			image_text = g_string_append(image_text, g_strdup(fd->name));
			image_text = g_string_append(image_text, "\n");
			}
		if (pw->text_fields & TEXT_INFO_FILEDATE)
			{
			image_text = g_string_append(image_text, g_strdup(text_from_time(fd->date)));
			image_text = g_string_append(image_text, "\n");
			}
		if (pw->text_fields & TEXT_INFO_FILESIZE)
			{
			image_text = g_string_append(image_text, g_strdup(text_from_size(fd->size)));
			image_text = g_string_append(image_text, "\n");
			}
		if (pw->text_fields & TEXT_INFO_DIMENSIONS)
			{
			g_string_append_printf(image_text, "%d x %d", (gint)pixbuf_image_width,
												(gint)pixbuf_image_height);
			image_text = g_string_append(image_text, "\n");
			}
		if (pw->text_fields & TEXT_INFO_FILEPATH)
			{
			image_text = g_string_append(image_text, g_strdup(fd->path));
			image_text = g_string_append(image_text, "\n");
			}
		if (image_text->len > 0)
			{
			image_text = g_string_truncate(image_text, image_text->len - 1);
			}
		}

	if (options->printer.show_page_text)
		{
		gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(pw->page_text), &start, &end);

		tmp = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(pw->page_text), &start, &end, FALSE);
		page_text = g_string_append(page_text, tmp);

		g_free(tmp);
		}

	cr = gtk_print_context_get_cairo_context(context);
	context_width = gtk_print_context_get_width(context);
	context_height = gtk_print_context_get_height(context);

	pango_image_height = 0;
	pango_page_height = 0;
	image_text_width = 0;
	page_text_width = 0;

	if (image_text->len > 0)
		{
		layout_image = pango_cairo_create_layout(cr);

		pango_layout_set_text(layout_image, image_text->str, -1);
		desc = pango_font_description_from_string(options->printer.image_font);
		pango_layout_set_font_description(layout_image, desc);

		pango_layout_get_extents(layout_image, &ink_rect, &logical_rect);
		image_text_width = ((gdouble)logical_rect.width / PANGO_SCALE) ;
		image_text_height = ((gdouble)logical_rect.height / PANGO_SCALE);

		pango_layout_set_alignment(layout_image, PANGO_ALIGN_CENTER);
		pango_layout_set_text(layout_image, image_text->str, -1);

		pango_image_height = image_text_height + PRINT_TEXT_PADDING * 2;

		pango_font_description_free(desc);
		}

	if (page_text->len > 0)
		{
		layout_page = pango_cairo_create_layout(cr);

		pango_layout_set_text(layout_page, page_text->str, -1);
		desc = pango_font_description_from_string(options->printer.page_font);
		pango_layout_set_font_description(layout_page, desc);

		pango_layout_get_extents(layout_page, &ink_rect, &logical_rect);
		page_text_width = ((gdouble)logical_rect.width / PANGO_SCALE) ;
		page_text_height = ((gdouble)logical_rect.height / PANGO_SCALE);

		pango_layout_set_alignment(layout_page, PANGO_ALIGN_CENTER);
		pango_layout_set_text(layout_page, page_text->str, -1);

		pango_page_height = page_text_height + PRINT_TEXT_PADDING * 2;

		pango_font_description_free(desc);
		}

	pango_height = pango_image_height + pango_page_height;

	if ((context_width / pixbuf_image_width) < ((context_height - pango_height) / pixbuf_image_height))
		{
		w = context_width;
		scale = context_width / pixbuf_image_width;
		h = pixbuf_image_height * scale;
		height_offset = (context_height - (h + pango_height)) / 2;
		width_offset = 0;
		}
	else
		{
		h = context_height - pango_height ;
		scale = (context_height - pango_height) / pixbuf_image_height;
		w = pixbuf_image_width * scale;
		height_offset = 0;
		width_offset = (context_width - (pixbuf_image_width * scale)) / 2;
		}

	incr_y = height_offset + PRINT_TEXT_PADDING;

	if (options->printer.page_text_position == HEADER_1 && page_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (page_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_page);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_page_height;
		}

	if (options->printer.image_text_position == HEADER_1 && image_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (image_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_image);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_image_height;
		}

	if (options->printer.page_text_position == HEADER_2 && page_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (page_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_page);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_page_height;
		}

	if (options->printer.image_text_position == HEADER_2 && image_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (image_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_image);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_image_height;
		}

	image_y = incr_y;
	incr_y = incr_y + h + PRINT_TEXT_PADDING;

	if (options->printer.page_text_position == FOOTER_1 && page_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (page_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_page);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_page_height;
		}

	if (options->printer.image_text_position == FOOTER_1 && image_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (image_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_image);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_image_height;
		}

	if (options->printer.page_text_position == FOOTER_2 && page_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (page_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_page);

		incr_y = incr_y + PRINT_TEXT_PADDING + pango_page_height;
		}

	if (options->printer.image_text_position == FOOTER_2 && image_text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (image_text_width / 2) + width_offset, incr_y);
		pango_cairo_show_layout(cr, layout_image);
		}

	pixbuf_scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
	gdk_pixbuf_scale(pixbuf, pixbuf_scaled, 0, 0, w, h, 0, 0,  scale, scale, PRINT_MAX_INTERP);

	cairo_rectangle(cr, width_offset, image_y, w, h);

	gdk_cairo_set_source_pixbuf(cr, pixbuf_scaled, width_offset, image_y);

	cairo_fill(cr);

	if (image_text->len > 0)
		{
		g_object_unref(layout_image);
		g_string_free(image_text, TRUE);
		}
	if (page_text->len > 0)
		{
		g_object_unref(layout_page);
		g_string_free(page_text, TRUE);
		}

	g_object_unref(pixbuf_scaled);

	return;
}

static void begin_print(GtkPrintOperation *operation,
						GtkPrintContext *context,
						gpointer user_data)
{
	PrintWindow *pw = user_data;
	gint page_count;

	page_count = print_layout_page_count(pw);
	gtk_print_operation_set_n_pages (operation, page_count);

	print_job_render_image(pw);
}


GObject *option_tab_cb(GtkPrintOperation *operation, gpointer user_data)
{
	PrintWindow *pw = user_data;

	return G_OBJECT(pw->vbox);
}

static void print_pref_store(PrintWindow *pw)
{
	gchar *tmp;
	GtkTextIter start, end;

	options->printer.text_fields = pw->text_fields;

	gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(pw->page_text), &start, &end);
	tmp = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(pw->page_text), &start, &end, FALSE);
	g_free(options->printer.page_text);
	options->printer.page_text = g_strdup(tmp);
	g_free(tmp);
}

static void end_print_cb(GtkPrintOperation *operation,
								GtkPrintContext *context, gpointer data)
{
	PrintWindow *pw = data;
	GList *work;
	GdkPixbuf *pixbuf;
	gchar *path;
	GtkPrintSettings *print_settings;
	GtkPageSetup *page_setup;
	GError *error = NULL;

	print_settings = gtk_print_operation_get_print_settings(operation);
	path = g_build_filename(get_rc_dir(), PRINT_SETTINGS, NULL);

	gtk_print_settings_to_file(print_settings, path, &error);
	if (error)
		{
		log_printf("Error: Print settings save failed:\n%s", error->message);
		g_error_free(error);
		error = NULL;
		}
	g_free(path);
	g_object_unref(print_settings);

	page_setup = gtk_print_operation_get_default_page_setup(operation);
	path = g_build_filename(get_rc_dir(), PAGE_SETUP, NULL);

	gtk_page_setup_to_file(page_setup, path, &error);
	if (error)
		{
		log_printf("Error: Print page setup save failed:\n%s", error->message);
		g_error_free(error);
		error = NULL;
		}
	g_free(path);
	g_object_unref(page_setup);

	print_pref_store(pw);

	work = pw->print_pixbuf_queue;
	while (work)
		{
		pixbuf = work->data;
		if (pixbuf)
			{
			g_object_unref(pixbuf);
			}
		work = work->next;
		}
	g_list_free(pw->print_pixbuf_queue);
	g_object_unref(pw->page_text);
	g_free(pw);
}

void print_window_new(FileData *fd, GList *selection, GList *list, GtkWidget *parent)
{
	PrintWindow *pw;
	GtkWidget *vbox;
	GtkPrintOperation *operation;
	GtkPageSetup *page_setup;
	gchar *uri;
	const gchar *dir;
	GError *error = NULL;
	gchar *path;
	GtkPrintSettings *settings;

	pw = g_new0(PrintWindow, 1);

	pw->source_selection = file_data_process_groups_in_selection(selection, FALSE, NULL);
	pw->text_fields = options->printer.text_fields;

	if (print_layout_page_count(pw) == 0)
		{
		return;
		}

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PREF_PAD_BORDER);
	gtk_widget_show(vbox);

	print_text_menu(vbox, pw);
	pw->vbox = vbox;

	pw->print_pixbuf_queue = NULL;
	pw->job_render_finished = FALSE;
	pw->job_page = 0;

	operation = gtk_print_operation_new();
	settings = gtk_print_settings_new();

	gtk_print_operation_set_custom_tab_label(operation, "Options");
	gtk_print_operation_set_use_full_page(operation, TRUE);
	gtk_print_operation_set_unit(operation, GTK_UNIT_POINTS);
	gtk_print_operation_set_embed_page_setup(operation, TRUE);
	gtk_print_operation_set_allow_async (operation, TRUE);
	dir = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
	if (dir == NULL)
		{
		dir = g_get_home_dir();
		}

	uri = g_build_filename("file:/", dir, "geeqie-file.pdf", NULL);
	gtk_print_settings_set(settings, GTK_PRINT_SETTINGS_OUTPUT_URI, uri);
	g_free(uri);

	path = g_build_filename(get_rc_dir(), PRINT_SETTINGS, NULL);
	gtk_print_settings_load_file(settings, path, &error);
	if (error)
		{
		log_printf("Error: Printer settings load failed:\n%s", error->message);
		g_error_free(error);
		error = NULL;
		}
	gtk_print_operation_set_print_settings(operation, settings);
	g_free(path);

	page_setup = gtk_page_setup_new();
	path = g_build_filename(get_rc_dir(), PAGE_SETUP, NULL);
	gtk_page_setup_load_file(page_setup, path, &error);
	if (error)
		{
		log_printf("Error: Print page setup load failed:\n%s", error->message);
		g_error_free(error);
		error = NULL;
		}
	gtk_print_operation_set_default_page_setup(operation, page_setup);
	g_free(path);

	g_signal_connect (G_OBJECT (operation), "begin-print",
					G_CALLBACK (begin_print), pw);
	g_signal_connect (G_OBJECT (operation), "draw-page",
					G_CALLBACK (draw_page), pw);
	g_signal_connect (G_OBJECT (operation), "end-print",
					G_CALLBACK (end_print_cb), pw);
	g_signal_connect (G_OBJECT (operation), "create-custom-widget",
					G_CALLBACK (option_tab_cb), pw);
	g_signal_connect (G_OBJECT (operation), "paginate",
					G_CALLBACK (paginate_cb), pw);

	gtk_print_operation_set_n_pages(operation, print_layout_page_count(pw));

	gtk_print_operation_run(operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
												GTK_WINDOW (parent), &error);

	if (error)
		{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new(GTK_WINDOW (parent),
								GTK_DIALOG_DESTROY_WITH_PARENT,
								GTK_MESSAGE_ERROR,
								GTK_BUTTONS_CLOSE,
								"%s", error->message);
		g_error_free (error);

		g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

		gtk_widget_show (dialog);
		}

	g_object_unref(page_setup);
	g_object_unref(settings);
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
