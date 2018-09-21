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

/* method to use when scaling down image data */
#define PRINT_MAX_INTERP GDK_INTERP_HYPER

typedef enum {
	TEXT_INFO_FILENAME = 1 << 0,
	TEXT_INFO_FILEDATE = 1 << 1,
	TEXT_INFO_FILESIZE = 1 << 2,
	TEXT_INFO_DIMENSIONS = 1 << 3,
	TEXT_INFO_FILEPATH = 1 << 4
} TextInfo;

typedef struct _PrintWindow PrintWindow;
struct _PrintWindow
{
	GtkWidget *vbox;
	GList *source_selection;

	TextInfo	text_fields;
	gint		 job_page;
	ImageLoader	*job_loader;

	GList *print_pixbuf_queue;
	gboolean job_render_finished;
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
#if GTK_CHECK_VERSION(3,4,0)
	GtkWidget *dialog;
	char *font;
	PangoFontDescription *font_desc;

	dialog = gtk_font_chooser_dialog_new("Printer Font", GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dialog), options->printer.font);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_CANCEL)
		{
		font_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(dialog));
		font = pango_font_description_to_string(font_desc);
		g_free(options->printer.font);
		options->printer.font = g_strdup(font);
		g_free(font);
		}

	gtk_widget_destroy(dialog);
#else
	const char *font;

	font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));
	options->printer.font = g_strdup(font);
#endif
}

static void print_text_menu(GtkWidget *box, PrintWindow *pw)
{
	GtkWidget *group;
	GtkWidget *hbox;
	GtkWidget *button;

	group = pref_group_new(box, FALSE, _("Show"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new(group, _("Name"), (pw->text_fields & TEXT_INFO_FILENAME),
			  G_CALLBACK(print_text_cb_name), pw);
	pref_checkbox_new(group, _("Path"), (pw->text_fields & TEXT_INFO_FILEPATH),
			  G_CALLBACK(print_text_cb_path), pw);
	pref_checkbox_new(group, _("Date"), (pw->text_fields & TEXT_INFO_FILEDATE),
			  G_CALLBACK(print_text_cb_date), pw);
	pref_checkbox_new(group, _("Size"), (pw->text_fields & TEXT_INFO_FILESIZE),
			  G_CALLBACK(print_text_cb_size), pw);
	pref_checkbox_new(group, _("Dimensions"), (pw->text_fields & TEXT_INFO_DIMENSIONS),
			  G_CALLBACK(print_text_cb_dims), pw);

	group = pref_group_new(box, FALSE, _("Font"), GTK_ORIENTATION_VERTICAL);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);

#if GTK_CHECK_VERSION(3,4,0)
	button = pref_button_new(NULL, GTK_STOCK_SELECT_FONT, _("Font"), FALSE,
				 G_CALLBACK(print_set_font_cb), pw);
#else
	button = gtk_font_button_new();
	gtk_font_button_set_title(GTK_FONT_BUTTON(button), "Printer Font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(button), options->printer.font);
	g_signal_connect(G_OBJECT(button), "font-set",
				 G_CALLBACK(print_set_font_cb),NULL);
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

/* Returns the "depth" of a layout, that is the distance from the
 * top of the layout to the baseline of the first line in the
 * layout. */
int get_layout_depth(PangoLayout *layout)
{
  PangoLayoutLine *layout_line = pango_layout_get_line(layout,0);
  PangoRectangle rect;

  pango_layout_line_get_extents(layout_line, NULL, &rect);

  return PANGO_ASCENT(rect);
}

static void draw_page(GtkPrintOperation *operation, GtkPrintContext *context,
									gint page_nr, gpointer data)
{
	FileData *fd;
	PrintWindow *pw = data;
	cairo_t *cr;
	gdouble width, height;
	gdouble width_pixbuf_image, height_pixbuf_image;
	gdouble width_offset;
	gdouble height_offset;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_scaled;
	PangoLayout *layout;
	PangoFontDescription *desc;
	GString *text = g_string_new(NULL);
	PangoRectangle ink_rect, logical_rect;
	gdouble depth;
	gdouble text_padding;
	gdouble x, y, w, h, scale;
	gdouble pango_height;

	pixbuf = g_list_nth_data(pw->print_pixbuf_queue, page_nr);
	width_pixbuf_image = gdk_pixbuf_get_width(pixbuf);
	height_pixbuf_image = gdk_pixbuf_get_height(pixbuf);

	fd = g_list_nth_data(pw->source_selection, page_nr);

	if (pw->text_fields & TEXT_INFO_FILENAME)
		{
		text = g_string_append(text, g_strdup(fd->name));
		text = g_string_append(text, "\n");
		}
	if (pw->text_fields & TEXT_INFO_FILEDATE)
		{
		text = g_string_append(text, g_strdup(text_from_time(fd->date)));
		text = g_string_append(text, "\n");
		}
	if (pw->text_fields & TEXT_INFO_FILESIZE)
		{
		text = g_string_append(text, g_strdup(text_from_size(fd->size)));
		text = g_string_append(text, "\n");
		}
	if (pw->text_fields & TEXT_INFO_DIMENSIONS)
		{
		g_string_append_printf(text, "%d x %d", (gint)width_pixbuf_image,
											(gint)height_pixbuf_image);
		text = g_string_append(text, "\n");
		}
	if (pw->text_fields & TEXT_INFO_FILEPATH)
		{
		text = g_string_append(text, g_strdup(fd->path));
		text = g_string_append(text, "\n");
		}

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);

	if (text->len > 0)
		{
		text = g_string_truncate(text, text->len - 1);

		layout = pango_cairo_create_layout(cr);

		pango_layout_set_text(layout, text->str, -1);
		desc = pango_font_description_from_string(options->printer.font);
		pango_layout_set_font_description(layout, desc);

		pango_layout_get_extents(layout, &ink_rect, &logical_rect);
		x = ((gdouble)logical_rect.width / PANGO_SCALE) ;
		y = ((gdouble)logical_rect.height / PANGO_SCALE);

		pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
		pango_layout_set_text(layout, text->str, -1);

		depth = (gdouble)get_layout_depth(layout);
		text_padding = depth / 2 / PANGO_SCALE ;

		pango_height = y + text_padding * 2;

		}
	else
		{
		pango_height = 0;
		depth = 0;
		text_padding = 0;
		x = 0;
		y = 0;
		}

	if ((width / width_pixbuf_image) < ((height - pango_height) / height_pixbuf_image))
		{
		w = width;
		scale = width / width_pixbuf_image;
		h = height_pixbuf_image * scale;
		height_offset = (height - (h + pango_height)) / 2;
		width_offset = 0;
		}
	else
		{
		h = height - pango_height ;
		scale = (height - pango_height) / height_pixbuf_image;
		w = width_pixbuf_image * scale;
		height_offset = 0;
		width_offset = (width - (width_pixbuf_image * scale)) / 2;
		}

	if (text->len > 0)
		{
		cairo_move_to(cr, (w / 2) - (x / 2) + width_offset, h + height_offset + text_padding);
		pango_cairo_show_layout(cr, layout);
		}

	pixbuf_scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
	gdk_pixbuf_scale(pixbuf, pixbuf_scaled, 0, 0, w, h, 0, 0,  scale, scale, PRINT_MAX_INTERP);

	cairo_rectangle(cr, width_offset, height_offset, w, h);

	gdk_cairo_set_source_pixbuf(cr, pixbuf_scaled, width_offset, height_offset);
	cairo_fill(cr);

	if (text->len > 0)
		{
		g_object_unref(layout);
		g_string_free(text, TRUE);
		pango_font_description_free(desc);
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
	options->printer.text_fields = pw->text_fields;
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
