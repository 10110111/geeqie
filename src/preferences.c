/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "preferences.h"

#include "cache_maint.h"
#include "debug.h"
#include "editors.h"
#include "filedata.h"
#include "filefilter.h"
#include "fullscreen.h"
#include "image.h"
#include "image-overlay.h"
#include "color-man.h"
#include "img-view.h"
#include "layout_config.h"
#include "layout_util.h"
#include "pixbuf_util.h"
#include "slideshow.h"
#include "trash.h"
#include "utilops.h"
#include "ui_fileops.h"
#include "ui_misc.h"
#include "ui_tabcomp.h"
#include "ui_utildlg.h"
#include "bar_exif.h"
#include "exif.h"

#include <math.h>


#define EDITOR_NAME_MAX_LENGTH 32
#define EDITOR_COMMAND_MAX_LENGTH 1024


typedef struct _ThumbSize ThumbSize;
struct _ThumbSize
{
	gint w;
	gint h;
};

static ThumbSize thumb_size_list[] =
{
	{ 24, 24 },
	{ 32, 32 },
	{ 48, 48 },
	{ 64, 64 },
	{ 96, 72 },
	{ 96, 96 },
	{ 128, 96 },
	{ 128, 128 },
	{ 160, 120 },
	{ 160, 160 },
	{ 192, 144 },
	{ 192, 192 },
	{ 256, 192 },
	{ 256, 256 }
};

enum {
	FE_ENABLE,
	FE_EXTENSION,
	FE_DESCRIPTION
};

/* config memory values */
static ConfOptions *c_options = NULL;


#ifdef DEBUG
static gint debug_c;
#endif

static GtkWidget *configwindow = NULL;
static GtkWidget *startup_path_entry;
static GtkListStore *filter_store = NULL;
static GtkWidget *editor_name_entry[GQ_EDITOR_SLOTS];
static GtkWidget *editor_command_entry[GQ_EDITOR_SLOTS];

static GtkWidget *layout_widget;

static GtkWidget *safe_delete_path_entry;

static GtkWidget *color_profile_input_file_entry[COLOR_PROFILE_INPUTS];
static GtkWidget *color_profile_input_name_entry[COLOR_PROFILE_INPUTS];
static GtkWidget *color_profile_screen_file_entry;

static GtkWidget *sidecar_ext_entry;


#define CONFIG_WINDOW_DEF_WIDTH		700
#define CONFIG_WINDOW_DEF_HEIGHT	500

/*
 *-----------------------------------------------------------------------------
 * option widget callbacks (private)
 *-----------------------------------------------------------------------------
 */

static void startup_path_set_current(GtkWidget *widget, gpointer data)
{
	gtk_entry_set_text(GTK_ENTRY(startup_path_entry), layout_get_path(NULL));
}

static void zoom_mode_original_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		c_options->image.zoom_mode = ZOOM_RESET_ORIGINAL;
}

static void zoom_mode_fit_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		c_options->image.zoom_mode = ZOOM_RESET_FIT_WINDOW;
}

static void zoom_mode_none_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		c_options->image.zoom_mode = ZOOM_RESET_NONE;
}

static void zoom_increment_cb(GtkWidget *spin, gpointer data)
{
	c_options->image.zoom_increment = (gint)(gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin)) * 10.0 + 0.01);
}

static void slideshow_delay_cb(GtkWidget *spin, gpointer data)
{
	c_options->slideshow.delay = (gint)(gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin)) *
				   (double)SLIDESHOW_SUBSECOND_PRECISION + 0.01);
}

/*
 *-----------------------------------------------------------------------------
 * sync progam to config window routine (private)
 *-----------------------------------------------------------------------------
 */

static void config_window_apply(void)
{
	const gchar *buf;
	gint new_style;
	gint i;
	gint refresh = FALSE;

	for (i = 0; i < GQ_EDITOR_SLOTS; i++)
		{
		if (i < GQ_EDITOR_GENERIC_SLOTS)
			{
			g_free(options->editor_name[i]);
			options->editor_name[i] = NULL;
			buf = gtk_entry_get_text(GTK_ENTRY(editor_name_entry[i]));
			if (buf && strlen(buf) > 0) options->editor_name[i] = g_strdup(buf);
			}

		g_free(options->editor_command[i]);
		options->editor_command[i] = NULL;
		buf = gtk_entry_get_text(GTK_ENTRY(editor_command_entry[i]));
		if (buf && strlen(buf) > 0) options->editor_command[i] = g_strdup(buf);
		}
	layout_edit_update_all();

	g_free(options->file_ops.safe_delete_path);
	options->file_ops.safe_delete_path = NULL;
	buf = gtk_entry_get_text(GTK_ENTRY(safe_delete_path_entry));
	if (buf && strlen(buf) > 0) options->file_ops.safe_delete_path = remove_trailing_slash(buf);

	if (options->file_filter.show_hidden_files != c_options->file_filter.show_hidden_files) refresh = TRUE;
	if (options->file_filter.show_dot_directory != c_options->file_filter.show_dot_directory) refresh = TRUE;
	if (options->file_sort.case_sensitive != c_options->file_sort.case_sensitive) refresh = TRUE;
	if (options->file_filter.disable != c_options->file_filter.disable) refresh = TRUE;

	options->startup.restore_path = c_options->startup.restore_path;
	options->startup.use_last_path = c_options->startup.use_last_path;
	g_free(options->startup.path);
	options->startup.path = NULL;
	buf = gtk_entry_get_text(GTK_ENTRY(startup_path_entry));
	if (buf && strlen(buf) > 0) options->startup.path = remove_trailing_slash(buf);

	options->file_ops.confirm_delete = c_options->file_ops.confirm_delete;
	options->file_ops.enable_delete_key = c_options->file_ops.enable_delete_key;
	options->file_ops.safe_delete_enable = c_options->file_ops.safe_delete_enable;
	options->file_ops.safe_delete_folder_maxsize = c_options->file_ops.safe_delete_folder_maxsize;
	options->layout.tools_restore_state = c_options->layout.tools_restore_state;
	options->layout.save_window_positions = c_options->layout.save_window_positions;
	options->image.zoom_mode = c_options->image.zoom_mode;
	options->image.zoom_2pass = c_options->image.zoom_2pass;
	options->image.fit_window_to_image = c_options->image.fit_window_to_image;
	options->image.limit_window_size = c_options->image.limit_window_size;
	options->image.zoom_to_fit_allow_expand = c_options->image.zoom_to_fit_allow_expand;
	options->image.max_window_size = c_options->image.max_window_size;
	options->image.limit_autofit_size = c_options->image.limit_autofit_size;
	options->image.max_autofit_size = c_options->image.max_autofit_size;
	options->progressive_key_scrolling = c_options->progressive_key_scrolling;
	options->thumbnails.max_width = c_options->thumbnails.max_width;
	options->thumbnails.max_height = c_options->thumbnails.max_height;
	options->thumbnails.enable_caching = c_options->thumbnails.enable_caching;
	options->thumbnails.cache_into_dirs = c_options->thumbnails.cache_into_dirs;
	options->thumbnails.fast = c_options->thumbnails.fast;
#if 0
	options->thumbnails.use_xvpics = c_options->thumbnails.use_xvpics;
#endif
	options->thumbnails.spec_standard = c_options->thumbnails.spec_standard;
	options->enable_metadata_dirs = c_options->enable_metadata_dirs;
	options->file_filter.show_hidden_files = c_options->file_filter.show_hidden_files;
	options->file_filter.show_dot_directory = c_options->file_filter.show_dot_directory;

	options->file_sort.case_sensitive = c_options->file_sort.case_sensitive;
	options->file_filter.disable = c_options->file_filter.disable;

	sidecar_ext_parse(gtk_entry_get_text(GTK_ENTRY(sidecar_ext_entry)), FALSE);

	options->slideshow.random = c_options->slideshow.random;
	options->slideshow.repeat = c_options->slideshow.repeat;
	options->slideshow.delay = c_options->slideshow.delay;

	options->mousewheel_scrolls = c_options->mousewheel_scrolls;

	options->file_ops.enable_in_place_rename = c_options->file_ops.enable_in_place_rename;

	options->collections.rectangular_selection = c_options->collections.rectangular_selection;

	options->image.tile_cache_max = c_options->image.tile_cache_max;

	options->image.read_buffer_size = c_options->image.read_buffer_size;
	options->image.idle_read_loop_count = c_options->image.idle_read_loop_count;

	options->thumbnails.quality = c_options->thumbnails.quality;
	options->image.zoom_quality = c_options->image.zoom_quality;

	options->image.zoom_increment = c_options->image.zoom_increment;

	options->image.enable_read_ahead = c_options->image.enable_read_ahead;

	if (options->image.use_custom_border_color != c_options->image.use_custom_border_color
	    || !gdk_color_equal(&options->image.border_color, &c_options->image.border_color))
		{
		options->image.use_custom_border_color = c_options->image.use_custom_border_color;
		options->image.border_color = c_options->image.border_color;
		layout_colors_update();
		view_window_colors_update();
		}

	options->fullscreen.screen = c_options->fullscreen.screen;
	options->fullscreen.clean_flip = c_options->fullscreen.clean_flip;
	options->fullscreen.disable_saver = c_options->fullscreen.disable_saver;
	options->fullscreen.above = c_options->fullscreen.above;
	options->image_overlay.common.show_at_startup = c_options->image_overlay.common.show_at_startup;
	if (c_options->image_overlay.common.template_string)
		{
		g_free(options->image_overlay.common.template_string);
		options->image_overlay.common.template_string = g_strdup(c_options->image_overlay.common.template_string);
		}

	options->update_on_time_change = c_options->update_on_time_change;
	options->image.exif_rotate_enable = c_options->image.exif_rotate_enable;

	options->duplicates_similarity_threshold = c_options->duplicates_similarity_threshold;

	options->tree_descend_subdirs = c_options->tree_descend_subdirs;

	options->open_recent_list_maxsize = c_options->open_recent_list_maxsize;
	options->dnd_icon_size = c_options->dnd_icon_size;
	
	if (options->show_copy_path != c_options->show_copy_path)
		{
		options->show_copy_path = c_options->show_copy_path;
		layout_copy_path_update_all();
		}

	options->save_metadata_in_image_file = c_options->save_metadata_in_image_file;

#ifdef DEBUG
	set_debug_level(debug_c);
#endif

#ifdef HAVE_LCMS
	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		g_free(options->color_profile.input_name[i]);
		options->color_profile.input_name[i] = NULL;
		buf = gtk_entry_get_text(GTK_ENTRY(color_profile_input_name_entry[i]));
		if (buf && strlen(buf) > 0) options->color_profile.input_name[i] = g_strdup(buf);

		g_free(options->color_profile.input_file[i]);
		options->color_profile.input_file[i] = NULL;
		buf = gtk_entry_get_text(GTK_ENTRY(color_profile_input_file_entry[i]));
		if (buf && strlen(buf) > 0) options->color_profile.input_file[i] = g_strdup(buf);
		}
	g_free(options->color_profile.screen_file);
	options->color_profile.screen_file = NULL;
	buf = gtk_entry_get_text(GTK_ENTRY(color_profile_screen_file_entry));
	if (buf && strlen(buf) > 0) options->color_profile.screen_file = g_strdup(buf);
#endif

	for (i = 0; ExifUIList[i].key; i++)
		{
		ExifUIList[i].current = ExifUIList[i].temp;
		}

	{
	gchar *layout_order = layout_config_get(layout_widget, &new_style);

	if (new_style != options->layout.style ||
	    (layout_order == NULL) != (options->layout.order == NULL) ||
	    !options->layout.order ||
	    strcmp(layout_order, options->layout.order) != 0)
		{
		if (refresh) filter_rebuild();
		refresh = FALSE;

		g_free(options->layout.order);
		options->layout.order = layout_order;
		layout_order = NULL; /* g_free() later */

		options->layout.style = new_style;

		layout_styles_update();
		}

	g_free(layout_order);
	}

	image_options_sync();

	if (refresh)
		{
		filter_rebuild();
		layout_refresh(NULL);
		}
}

/*
 *-----------------------------------------------------------------------------
 * config window main button callbacks (private)
 *-----------------------------------------------------------------------------
 */

static void config_window_close_cb(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(configwindow);
	configwindow = NULL;
	filter_store = NULL;
}

static gint config_window_delete(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	config_window_close_cb(NULL, NULL);
	return TRUE;
}

static void config_window_ok_cb(GtkWidget *widget, gpointer data)
{
	config_window_apply();
	config_window_close_cb(NULL, NULL);
}

static void config_window_apply_cb(GtkWidget *widget, gpointer data)
{
	config_window_apply();
}

/*
 *-----------------------------------------------------------------------------
 * config window setup (private)
 *-----------------------------------------------------------------------------
 */

static void exif_item_cb(GtkWidget *combo, gpointer data)
{
	gint *option = data;
	*option = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void exif_item(GtkWidget *table, gint column, gint row,
		      const gchar *text, gint option, gint *option_c)
{
	GtkWidget *combo;

	*option_c = option;

	pref_table_label(table, column, row, text, 0.0);

	combo = gtk_combo_box_new_text();

	/* note: the order is important, it must match the values of
	 * EXIF_UI_OFF, _IFSET, _ON */
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Never"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("If set"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Always"));

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), option);

	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(exif_item_cb), option_c);

	gtk_table_attach(GTK_TABLE(table), combo,
			 column + 1, column + 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show(combo);
}

static void quality_menu_cb(GtkWidget *combo, gpointer data)
{
	gint *option = data;

	switch (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)))
		{
		case 0:
		default:
			*option = GDK_INTERP_NEAREST;
			break;
		case 1:
			*option = GDK_INTERP_TILES;
			break;
		case 2:
			*option = GDK_INTERP_BILINEAR;
			break;
		case 3:
			*option = GDK_INTERP_HYPER;
			break;
		}
}

static void add_quality_menu(GtkWidget *table, gint column, gint row, const gchar *text,
			     gint option, gint *option_c)
{
	GtkWidget *combo;
	gint current = 0;

	*option_c = option;

	pref_table_label(table, column, row, text, 0.0);

	combo = gtk_combo_box_new_text();

	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Nearest (worst, but fastest)"));
	if (option == GDK_INTERP_NEAREST) current = 0;
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Tiles"));
	if (option == GDK_INTERP_TILES) current = 1;
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Bilinear"));
	if (option == GDK_INTERP_BILINEAR) current = 2;
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Hyper (best, but slowest)"));
	if (option == GDK_INTERP_HYPER) current = 3;

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);

	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(quality_menu_cb), option_c);

	gtk_table_attach(GTK_TABLE(table), combo, column + 1, column + 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show(combo);
}

#if 0
static void add_dither_menu(gint option, gint *option_c, gchar *text, GtkWidget *box)
{
	GtkWidget *hbox;
	GtkWidget *omenu;
	GtkWidget *menu;

	*option_c = option;

	hbox = pref_box_new(box, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
	pref_label_new(hbox, text);

	omenu = gtk_option_menu_new();
	menu = gtk_menu_new();

	add_menu_item(menu, _("None"), option_c, (gint)GDK_RGB_DITHER_NONE);
	add_menu_item(menu, _("Normal"), option_c, (gint)GDK_RGB_DITHER_NORMAL);
	add_menu_item(menu, _("Best"), option_c, (gint)GDK_RGB_DITHER_MAX);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), *option_c);

	gtk_box_pack_start(GTK_BOX(hbox), omenu, FALSE, FALSE, 0);
	gtk_widget_show(omenu);
}
#endif

static void thumb_size_menu_cb(GtkWidget *combo, gpointer data)
{
	gint n;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

	if (n >= 0 && n < sizeof(thumb_size_list) / sizeof(ThumbSize))
		{
		c_options->thumbnails.max_width = thumb_size_list[n].w;
		c_options->thumbnails.max_height = thumb_size_list[n].h;
		}
	else if (n > 0)
		{
		c_options->thumbnails.max_width = options->thumbnails.max_width;
		c_options->thumbnails.max_height = options->thumbnails.max_height;
		}
}

static void add_thumb_size_menu(GtkWidget *table, gint column, gint row, gchar *text)
{
	GtkWidget *combo;
	gint current;
	gint i;

	c_options->thumbnails.max_width = options->thumbnails.max_width;
	c_options->thumbnails.max_height = options->thumbnails.max_height;

	pref_table_label(table, column, row, text, 0.0);

	combo = gtk_combo_box_new_text();

	current = -1;
	for (i = 0; i < sizeof(thumb_size_list) / sizeof(ThumbSize); i++)
		{
		gint w, h;
		gchar *buf;

		w = thumb_size_list[i].w;
		h = thumb_size_list[i].h;

		buf = g_strdup_printf("%d x %d", w, h);
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), buf);
		g_free(buf);

		if (w == options->thumbnails.max_width && h == options->thumbnails.max_height) current = i;
		}

	if (current == -1)
		{
		gchar *buf;

		buf = g_strdup_printf("%s %d x %d", _("Custom"), options->thumbnails.max_width, options->thumbnails.max_height);
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), buf);
		g_free(buf);

		current = i;
		}

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(thumb_size_menu_cb), NULL);

	gtk_table_attach(GTK_TABLE(table), combo, column + 1, column + 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show(combo);
}

static void filter_store_populate(void)
{
	GList *work;

	if (!filter_store) return;

	gtk_list_store_clear(filter_store);

	work = filter_get_list();
	while (work)
		{
		FilterEntry *fe;
		GtkTreeIter iter;

		fe = work->data;
		work = work->next;

		gtk_list_store_append(filter_store, &iter);
		gtk_list_store_set(filter_store, &iter, 0, fe, -1);
		}
}

static void filter_store_ext_edit_cb(GtkCellRendererText *cell, gchar *path_str,
				     gchar *new_text, gpointer data)
{
	GtkWidget *model = data;
	FilterEntry *fe = data;
	GtkTreePath *tpath;
	GtkTreeIter iter;

	if (!new_text || strlen(new_text) < 1) return;

	tpath = gtk_tree_path_new_from_string(path_str);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, tpath);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &fe, -1);

	g_free(fe->extensions);
	fe->extensions = g_strdup(new_text);

	gtk_tree_path_free(tpath);
	filter_rebuild();
}

static void filter_store_desc_edit_cb(GtkCellRendererText *cell, gchar *path_str,
				      gchar *new_text, gpointer data)
{
	GtkWidget *model = data;
	FilterEntry *fe;
	GtkTreePath *tpath;
	GtkTreeIter iter;

	if (!new_text || strlen(new_text) < 1) return;

	tpath = gtk_tree_path_new_from_string(path_str);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, tpath);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &fe, -1);

	g_free(fe->description);
	fe->description = g_strdup(new_text);

	gtk_tree_path_free(tpath);
}

static void filter_store_enable_cb(GtkCellRendererToggle *renderer,
				   gchar *path_str, gpointer data)
{
	GtkWidget *model = data;
	FilterEntry *fe;
	GtkTreePath *tpath;
	GtkTreeIter iter;

	tpath = gtk_tree_path_new_from_string(path_str);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, tpath);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &fe, -1);

	fe->enabled = !fe->enabled;

	gtk_tree_path_free(tpath);
	filter_rebuild();
}

static void filter_set_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
			    GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	FilterEntry *fe;

	gtk_tree_model_get(tree_model, iter, 0, &fe, -1);

	switch(GPOINTER_TO_INT(data))
		{
		case FE_ENABLE:
			g_object_set(GTK_CELL_RENDERER(cell),
				     "active", fe->enabled, NULL);
			break;
		case FE_EXTENSION:
			g_object_set(GTK_CELL_RENDERER(cell),
				     "text", fe->extensions, NULL);
			break;
		case FE_DESCRIPTION:
			g_object_set(GTK_CELL_RENDERER(cell),
				     "text", fe->description, NULL);
			break;
		}
}

static void filter_add_cb(GtkWidget *widget, gpointer data)
{
	filter_add_unique("description", ".new", FORMAT_CLASS_IMAGE, TRUE);
	filter_store_populate();

	/* FIXME: implement the scroll to/select row stuff for tree view */
}

static void filter_remove_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *filter_view = data;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	FilterEntry *fe;

	if (!filter_store) return;
       	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(filter_view));
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) return;
	gtk_tree_model_get(GTK_TREE_MODEL(filter_store), &iter, 0, &fe, -1);
	if (!fe) return;

	filter_remove_entry(fe);
	filter_rebuild();
	filter_store_populate();
}

static void filter_default_ok_cb(GenericDialog *gd, gpointer data)
{
	filter_reset();
	filter_add_defaults();
	filter_rebuild();
	filter_store_populate();
}

static void dummy_cancel_cb(GenericDialog *gd, gpointer data)
{
	/* no op, only so cancel button appears */
}

static void filter_default_cb(GtkWidget *widget, gpointer data)
{
	GenericDialog *gd;

	gd = generic_dialog_new(_("Reset filters"),
				GQ_WMCLASS, "reset_filter", widget, TRUE,
				dummy_cancel_cb, NULL);
	generic_dialog_add_message(gd, GTK_STOCK_DIALOG_QUESTION, _("Reset filters"),
				   _("This will reset the file filters to the defaults.\nContinue?"));
	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL, filter_default_ok_cb, TRUE);
	gtk_widget_show(gd->dialog);
}

static void filter_disable_cb(GtkWidget* widget, gpointer data)
{
	GtkWidget *frame = data;

	gtk_widget_set_sensitive(frame,
				 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

static void editor_default_ok_cb(GenericDialog *gd, gpointer data)
{
	gint i;

	editor_reset_defaults();
	if (!configwindow) return;

	for (i = 0; i < GQ_EDITOR_SLOTS; i++)
		{
		if (i < GQ_EDITOR_GENERIC_SLOTS)
			gtk_entry_set_text(GTK_ENTRY(editor_name_entry[i]),
				   (options->editor_name[i]) ? options->editor_name[i] : "");
		gtk_entry_set_text(GTK_ENTRY(editor_command_entry[i]),
				   (options->editor_command[i]) ? options->editor_command[i] : "");
		}
}

static void editor_default_cb(GtkWidget *widget, gpointer data)
{
	GenericDialog *gd;

	gd = generic_dialog_new(_("Reset editors"),
				GQ_WMCLASS, "reset_filter", widget, TRUE,
				dummy_cancel_cb, NULL);
	generic_dialog_add_message(gd, GTK_STOCK_DIALOG_QUESTION, _("Reset editors"),
				   _("This will reset the edit commands to the defaults.\nContinue?"));
	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL, editor_default_ok_cb, TRUE);
	gtk_widget_show(gd->dialog);
}

static void editor_help_cb(GtkWidget *widget, gpointer data)
{
	help_window_show("editors");
}

static void safe_delete_view_cb(GtkWidget* widget, gpointer data)
{
	layout_set_path(NULL, gtk_entry_get_text(GTK_ENTRY(safe_delete_path_entry)));
}

static void safe_delete_clear_ok_cb(GenericDialog *gd, gpointer data)
{
	file_util_trash_clear();
}

static void safe_delete_clear_cb(GtkWidget* widget, gpointer data)
{
	GenericDialog *gd;
	GtkWidget *entry;
	gd = generic_dialog_new(_("Clear trash"),
				GQ_WMCLASS, "clear_trash", widget, TRUE,
				dummy_cancel_cb, NULL);
	generic_dialog_add_message(gd, GTK_STOCK_DIALOG_QUESTION, _("Clear trash"),
				    _("This will remove the trash contents."));
	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL, safe_delete_clear_ok_cb, TRUE);
	entry = gtk_entry_new();
	GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	if (options->file_ops.safe_delete_path) gtk_entry_set_text(GTK_ENTRY(entry), options->file_ops.safe_delete_path);
	gtk_box_pack_start(GTK_BOX(gd->vbox), entry, FALSE, FALSE, 0);
	gtk_widget_show(entry);
	gtk_widget_show(gd->dialog);
}

static void image_overlay_template_view_changed_cb(GtkWidget* widget, gpointer data)
{
	GtkWidget *pTextView;
	GtkTextBuffer* pTextBuffer;
	GtkTextIter iStart;
	GtkTextIter iEnd;

	pTextView = GTK_WIDGET(data);

	pTextBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pTextView));
	gtk_text_buffer_get_start_iter(pTextBuffer, &iStart);
	gtk_text_buffer_get_end_iter(pTextBuffer, &iEnd);

	if (c_options->image_overlay.common.template_string) g_free(c_options->image_overlay.common.template_string);
	c_options->image_overlay.common.template_string = gtk_text_buffer_get_text(pTextBuffer, &iStart, &iEnd, TRUE);
}

static void image_overlay_default_template_ok_cb(GenericDialog *gd, gpointer data)
{
	GtkTextView *text_view = data;
	GtkTextBuffer *buffer;

	set_default_image_overlay_template_string(options);
	if (!configwindow) return;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
	gtk_text_buffer_set_text(buffer, options->image_overlay.common.template_string, -1);
}

static void image_overlay_default_template_cb(GtkWidget *widget, gpointer data)
{
	GenericDialog *gd;

	gd = generic_dialog_new(_("Reset image overlay template string"),
				GQ_WMCLASS, "reset_image_overlay_template_string", widget, TRUE,
				dummy_cancel_cb, data);
	generic_dialog_add_message(gd, GTK_STOCK_DIALOG_QUESTION, _("Reset image overlay template string"),
				   _("This will reset the image overlay template string to the default.\nContinue?"));
	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL, image_overlay_default_template_ok_cb, TRUE);
	gtk_widget_show(gd->dialog);
}

static void image_overlay_help_cb(GtkWidget *widget, gpointer data)
{
	help_window_show("overlay");
}

/* general options tab */
static void config_tab_general(GtkWidget *notebook)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *subvbox;
	GtkWidget *group;
	GtkWidget *subgroup;
	GtkWidget *button;
	GtkWidget *tabcomp;
	GtkWidget *ct_button;
	GtkWidget *table;
	GtkWidget *spin;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PREF_PAD_BORDER);
	gtk_widget_show(vbox);
	label = gtk_label_new(_("General"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

	group = pref_group_new(vbox, FALSE, _("Startup"), GTK_ORIENTATION_VERTICAL);

	button = pref_checkbox_new_int(group, _("Restore folder on startup"),
				       options->startup.restore_path, &c_options->startup.restore_path);

	subvbox = pref_box_new(group, FALSE, GTK_ORIENTATION_VERTICAL, PREF_PAD_SPACE);
	pref_checkbox_link_sensitivity(button, subvbox);

	hbox = pref_box_new(subvbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);

	tabcomp = tab_completion_new(&startup_path_entry, options->startup.path, NULL, NULL);
	tab_completion_add_select_button(startup_path_entry, NULL, TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), tabcomp, TRUE, TRUE, 0);
	gtk_widget_show(tabcomp);

	button = pref_button_new(hbox, NULL, _("Use current"), FALSE,
				 G_CALLBACK(startup_path_set_current), NULL);

	button = pref_checkbox_new_int(subvbox, _("Use last path"),
				       options->startup.use_last_path, &c_options->startup.use_last_path);
	pref_checkbox_link_sensitivity_swap(button, hbox);


	group = pref_group_new(vbox, FALSE, _("Thumbnails"), GTK_ORIENTATION_VERTICAL);

	table = pref_table_new(group, 2, 2, FALSE, FALSE);
	add_thumb_size_menu(table, 0, 0, _("Size:"));
	add_quality_menu(table, 0, 1, _("Quality:"), options->thumbnails.quality, &c_options->thumbnails.quality);

	ct_button = pref_checkbox_new_int(group, _("Cache thumbnails"),
					  options->thumbnails.enable_caching, &c_options->thumbnails.enable_caching);

	subgroup = pref_box_new(group, FALSE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);
	pref_checkbox_link_sensitivity(ct_button, subgroup);

	button = pref_checkbox_new_int(subgroup, _("Use shared thumbnail cache"),
				       options->thumbnails.spec_standard, &c_options->thumbnails.spec_standard);

	subgroup = pref_box_new(subgroup, FALSE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);
	pref_checkbox_link_sensitivity_swap(button, subgroup);

	pref_checkbox_new_int(subgroup, _("Cache thumbnails into .thumbnails"),
			      options->thumbnails.cache_into_dirs, &c_options->thumbnails.cache_into_dirs);

#if 0
	pref_checkbox_new_int(subgroup, _("Use xvpics thumbnails when found (read only)"),
			      options->thumbnails.use_xvpics, &c_options->thumbnails.use_xvpics);
#endif

	pref_checkbox_new_int(group, _("Faster jpeg thumbnailing (may reduce quality)"),
			      options->thumbnails.fast, &c_options->thumbnails.fast);

	group = pref_group_new(vbox, FALSE, _("Slide show"), GTK_ORIENTATION_VERTICAL);

	c_options->slideshow.delay = options->slideshow.delay;
	spin = pref_spin_new(group, _("Delay between image change:"), _("seconds"),
			     SLIDESHOW_MIN_SECONDS, SLIDESHOW_MAX_SECONDS, 1.0, 1,
			     options->slideshow.delay ? (double)options->slideshow.delay / SLIDESHOW_SUBSECOND_PRECISION : 10.0,
			     G_CALLBACK(slideshow_delay_cb), NULL);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_ALWAYS);

	pref_checkbox_new_int(group, _("Random"), options->slideshow.random, &c_options->slideshow.random);
	pref_checkbox_new_int(group, _("Repeat"), options->slideshow.repeat, &c_options->slideshow.repeat);
}

/* image tab */
static void config_tab_image(GtkWidget *notebook)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *group;
	GtkWidget *button;
	GtkWidget *ct_button;
	GtkWidget *table;
	GtkWidget *spin;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PREF_PAD_BORDER);
	gtk_widget_show(vbox);
	label = gtk_label_new(_("Image"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

	group = pref_group_new(vbox, FALSE, _("Zoom"), GTK_ORIENTATION_VERTICAL);

#if 0
	add_dither_menu(dither_quality, &c_options->image.dither_quality, _("Dithering method:"), group);
#endif
	table = pref_table_new(group, 2, 1, FALSE, FALSE);
	add_quality_menu(table, 0, 0, _("Quality:"), options->image.zoom_quality, &c_options->image.zoom_quality);

	pref_checkbox_new_int(group, _("Two pass zooming"),
			      options->image.zoom_2pass, &c_options->image.zoom_2pass);

	pref_checkbox_new_int(group, _("Allow enlargement of image for zoom to fit"),
			      options->image.zoom_to_fit_allow_expand, &c_options->image.zoom_to_fit_allow_expand);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
	ct_button = pref_checkbox_new_int(hbox, _("Limit image size when autofitting (%):"),
					  options->image.limit_autofit_size, &c_options->image.limit_autofit_size);
	spin = pref_spin_new_int(hbox, NULL, NULL,
				 10, 150, 1,
				 options->image.max_autofit_size, &c_options->image.max_autofit_size);
	pref_checkbox_link_sensitivity(ct_button, spin);

	c_options->image.zoom_increment = options->image.zoom_increment;
	spin = pref_spin_new(group, _("Zoom increment:"), NULL,
			     0.1, 4.0, 0.1, 1, (double)options->image.zoom_increment / 10.0,
			     G_CALLBACK(zoom_increment_cb), NULL);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_ALWAYS);

	group = pref_group_new(vbox, FALSE, _("When new image is selected:"), GTK_ORIENTATION_VERTICAL);

	c_options->image.zoom_mode = options->image.zoom_mode;
	button = pref_radiobutton_new(group, NULL, _("Zoom to original size"),
				      (options->image.zoom_mode == ZOOM_RESET_ORIGINAL),
				      G_CALLBACK(zoom_mode_original_cb), NULL);
	button = pref_radiobutton_new(group, button, _("Fit image to window"),
				      (options->image.zoom_mode == ZOOM_RESET_FIT_WINDOW),
				      G_CALLBACK(zoom_mode_fit_cb), NULL);
	button = pref_radiobutton_new(group, button, _("Leave Zoom at previous setting"),
				      (options->image.zoom_mode == ZOOM_RESET_NONE),
				      G_CALLBACK(zoom_mode_none_cb), NULL);

	group = pref_group_new(vbox, FALSE, _("Appearance"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Custom border color"),
			      options->image.use_custom_border_color, &c_options->image.use_custom_border_color);

	pref_color_button_new(group, _("Border color"), &options->image.border_color,
			      G_CALLBACK(pref_color_button_set_cb), &c_options->image.border_color);

	group = pref_group_new(vbox, FALSE, _("Convenience"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Refresh on file change"),
			      options->update_on_time_change, &c_options->update_on_time_change);
	pref_checkbox_new_int(group, _("Preload next image"),
			      options->image.enable_read_ahead, &c_options->image.enable_read_ahead);
	pref_checkbox_new_int(group, _("Auto rotate image using Exif information"),
			      options->image.exif_rotate_enable, &c_options->image.exif_rotate_enable);
}

/* windows tab */
static void config_tab_windows(GtkWidget *notebook)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *group;
	GtkWidget *ct_button;
	GtkWidget *spin;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PREF_PAD_BORDER);
	gtk_widget_show(vbox);
	label = gtk_label_new(_("Windows"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

	group = pref_group_new(vbox, FALSE, _("State"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Remember window positions"),
			      options->layout.save_window_positions, &c_options->layout.save_window_positions);
	pref_checkbox_new_int(group, _("Remember tool state (float/hidden)"),
			      options->layout.tools_restore_state, &c_options->layout.tools_restore_state);

	group = pref_group_new(vbox, FALSE, _("Size"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Fit window to image when tools are hidden/floating"),
			      options->image.fit_window_to_image, &c_options->image.fit_window_to_image);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
	ct_button = pref_checkbox_new_int(hbox, _("Limit size when auto-sizing window (%):"),
					  options->image.limit_window_size, &c_options->image.limit_window_size);
	spin = pref_spin_new_int(hbox, NULL, NULL,
				 10, 150, 1,
				 options->image.max_window_size, &c_options->image.max_window_size);
	pref_checkbox_link_sensitivity(ct_button, spin);

	group = pref_group_new(vbox, FALSE, _("Layout"), GTK_ORIENTATION_VERTICAL);

	layout_widget = layout_config_new();
	layout_config_set(layout_widget, options->layout.style, options->layout.order);
	gtk_box_pack_start(GTK_BOX(group), layout_widget, FALSE, FALSE, 0);
	gtk_widget_show(layout_widget);
}

/* filtering tab */
static void config_tab_filtering(GtkWidget *notebook)
{
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *group;
	GtkWidget *button;
	GtkWidget *ct_button;
	GtkWidget *scrolled;
	GtkWidget *filter_view;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PREF_PAD_BORDER);
	gtk_widget_show(vbox);
	label = gtk_label_new(_("Filtering"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

	group = pref_box_new(vbox, FALSE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);

	pref_checkbox_new_int(group, _("Show hidden files or folders"),
			      options->file_filter.show_hidden_files, &c_options->file_filter.show_hidden_files);
	pref_checkbox_new_int(group, _("Show dot directory"),
			      options->file_filter.show_dot_directory, &c_options->file_filter.show_dot_directory);
	pref_checkbox_new_int(group, _("Case sensitive sort"),
			      options->file_sort.case_sensitive, &c_options->file_sort.case_sensitive);

	ct_button = pref_checkbox_new_int(group, _("Disable File Filtering"),
					  options->file_filter.disable, &c_options->file_filter.disable);


	group = pref_group_new(vbox, FALSE, _("Grouping sidecar extensions"), GTK_ORIENTATION_VERTICAL);

	sidecar_ext_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(sidecar_ext_entry), sidecar_ext_to_string());
	gtk_box_pack_start(GTK_BOX(group), sidecar_ext_entry, FALSE, FALSE, 0);
	gtk_widget_show(sidecar_ext_entry);

	group = pref_group_new(vbox, TRUE, _("File types"), GTK_ORIENTATION_VERTICAL);

	frame = pref_group_parent(group);
	g_signal_connect(G_OBJECT(ct_button), "toggled",
			 G_CALLBACK(filter_disable_cb), frame);
	gtk_widget_set_sensitive(frame, !options->file_filter.disable);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(group), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	filter_store = gtk_list_store_new(1, G_TYPE_POINTER);
	filter_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(filter_store));
	g_object_unref(filter_store);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(filter_view));
	gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection), GTK_SELECTION_SINGLE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(filter_view), FALSE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Filter"));
	gtk_tree_view_column_set_resizable(column, TRUE);

	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(filter_store_enable_cb), filter_store);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, filter_set_func,
						GINT_TO_POINTER(FE_ENABLE), NULL);

	renderer = gtk_cell_renderer_text_new();
	g_signal_connect(G_OBJECT(renderer), "edited",
			 G_CALLBACK(filter_store_ext_edit_cb), filter_store);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	g_object_set(G_OBJECT(renderer), "editable", (gboolean)TRUE, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer, filter_set_func,
						GINT_TO_POINTER(FE_EXTENSION), NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(filter_view), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Description"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	renderer = gtk_cell_renderer_text_new();
	g_signal_connect(G_OBJECT(renderer), "edited",
			 G_CALLBACK(filter_store_desc_edit_cb), filter_store);
	g_object_set(G_OBJECT(renderer), "editable", (gboolean)TRUE, NULL);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, filter_set_func,
						GINT_TO_POINTER(FE_DESCRIPTION), NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(filter_view), column);

	filter_store_populate();
	gtk_container_add(GTK_CONTAINER(scrolled), filter_view);
	gtk_widget_show(filter_view);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);

	button = pref_button_new(NULL, NULL, _("Defaults"), FALSE,
				 G_CALLBACK(filter_default_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_REMOVE, NULL, FALSE,
				 G_CALLBACK(filter_remove_cb), filter_view);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_ADD, NULL, FALSE,
				 G_CALLBACK(filter_add_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
}

/* editors tab */
static void config_tab_editors(GtkWidget *notebook)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *table;
	gint i;

	vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PREF_PAD_BORDER);
	gtk_widget_show(vbox);
	label = gtk_label_new(_("Editors"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

	table = pref_table_new(vbox, 3, 9, FALSE, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), PREF_PAD_GAP);

	label = pref_table_label(table, 0, 0, _("#"), 1.0);
	pref_label_bold(label, TRUE, FALSE);

	label = pref_table_label(table, 1, 0, _("Menu name"), 0.0);
	pref_label_bold(label, TRUE, FALSE);

	label = pref_table_label(table, 2, 0, _("Command Line"), 0.0);
	pref_label_bold(label, TRUE, FALSE);

	for (i = 0; i < GQ_EDITOR_SLOTS; i++)
		{
		GtkWidget *entry;

		if (i < GQ_EDITOR_GENERIC_SLOTS)
			{
			gchar *buf;

			buf = g_strdup_printf("%d", i+1);
			pref_table_label(table, 0, i+1, buf, 1.0);
			g_free(buf);
			entry = gtk_entry_new();
			gtk_entry_set_max_length(GTK_ENTRY(entry), EDITOR_NAME_MAX_LENGTH);
			gtk_widget_set_size_request(entry, 80, -1);
			if (options->editor_name[i])
				gtk_entry_set_text(GTK_ENTRY(entry), options->editor_name[i]);
			}
		else
			{
			entry = gtk_label_new(options->editor_name[i]);
			gtk_misc_set_alignment(GTK_MISC(entry), 0.0, 0.5);
			}

		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, i+1, i+2,
				 GTK_FILL | GTK_SHRINK, 0, 0, 0);
		gtk_widget_show(entry);
		editor_name_entry[i] = entry;

		entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), EDITOR_COMMAND_MAX_LENGTH);
		gtk_widget_set_size_request(entry, 160, -1);
		tab_completion_add_to_entry(entry, NULL, NULL);
		if (options->editor_command[i])
			gtk_entry_set_text(GTK_ENTRY(entry), options->editor_command[i]);
		gtk_table_attach(GTK_TABLE(table), entry, 2, 3, i+1, i+2,
				 GTK_FILL | GTK_EXPAND, 0, 0, 0);
		gtk_widget_show(entry);
		editor_command_entry[i] = entry;
		}

	hbox = pref_box_new(vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);

	button = pref_button_new(NULL, NULL, _("Defaults"), FALSE,
				 G_CALLBACK(editor_default_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_HELP, NULL, FALSE,
				 G_CALLBACK(editor_help_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
}

/* properties tab */
static void config_tab_properties(GtkWidget *notebook)
{
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *group;
	GtkWidget *table;
	GtkWidget *scrolled;
	GtkWidget *viewport;
	gint i;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled), PREF_PAD_BORDER);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	label = gtk_label_new(_("Properties"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled, label);
	gtk_widget_show(scrolled);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(scrolled), viewport);
	gtk_widget_show(viewport);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(viewport), vbox);
	gtk_widget_show(vbox);

	group = pref_group_new(vbox, FALSE, _("Exif"),
			       GTK_ORIENTATION_VERTICAL);


	pref_spacer(group, PREF_PAD_INDENT - PREF_PAD_SPACE);
	label = pref_label_new(group, _("What to show in properties dialog:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	table = pref_table_new(group, 2, 2, FALSE, FALSE);

	for (i = 0; ExifUIList[i].key; i++)
		{
		const gchar *title;

		title = exif_get_description_by_key(ExifUIList[i].key);
		exif_item(table, 0, i, title, ExifUIList[i].current,
			  &ExifUIList[i].temp);
		}
}

/* advanced entry tab */
static void config_tab_advanced(GtkWidget *notebook)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *group;
	GtkWidget *button;
	GtkWidget *tabcomp;
	GtkWidget *ct_button;
	GtkWidget *table;
	GtkWidget *spin;
	GtkWidget *scrolled;
	GtkWidget *viewport;
	GtkWidget *image_overlay_template_view;
	GtkTextBuffer *buffer;
	gint i;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled), PREF_PAD_BORDER);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	label = gtk_label_new(_("Advanced"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled, label);
	gtk_widget_show(scrolled);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(scrolled), viewport);
	gtk_widget_show(viewport);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(viewport), vbox);
	gtk_widget_show(vbox);

	group = pref_group_new(vbox, FALSE, _("Full screen"), GTK_ORIENTATION_VERTICAL);

	c_options->fullscreen.screen = options->fullscreen.screen;
	c_options->fullscreen.above = options->fullscreen.above;
	hbox = fullscreen_prefs_selection_new(_("Location:"), &c_options->fullscreen.screen, &c_options->fullscreen.above);
	gtk_box_pack_start(GTK_BOX(group), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	pref_checkbox_new_int(group, _("Smooth image flip"),
			      options->fullscreen.clean_flip, &c_options->fullscreen.clean_flip);
	pref_checkbox_new_int(group, _("Disable screen saver"),
			      options->fullscreen.disable_saver, &c_options->fullscreen.disable_saver);


	group = pref_group_new(vbox, FALSE, _("Overlay Screen Display"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Always show image overlay at startup"),
			      options->image_overlay.common.show_at_startup, &c_options->image_overlay.common.show_at_startup);
	pref_label_new(group, _("Image overlay template"));

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled, 200, 150);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(group), scrolled, TRUE, TRUE, 5);
	gtk_widget_show(scrolled);

	image_overlay_template_view = gtk_text_view_new();

#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_markup(image_overlay_template_view,
	_("<i>%name%</i> results in the filename of the picture.\n"
	  "Also available: <i>%collection%</i>, <i>%number%</i>, <i>%total%</i>, <i>%date%</i>,\n"
	  "<i>%size%</i> (filesize), <i>%width%</i>, <i>%height%</i>, <i>%res%</i> (resolution)\n"
	  "To access exif data use the exif name, e. g. <i>%formatted.Camera%</i> is the formatted camera name,\n"
	  "<i>%Exif.Photo.DateTimeOriginal%</i> the date of the original shot.\n"
	  "<i>%formatted.Camera:20</i> notation will truncate the displayed data to 20 characters and will add 3 dots at the end to denote the truncation.\n"
	  "If two or more variables are connected with the |-sign, it prints available variables with a separator.\n"
	  "<i>%formatted.ShutterSpeed%</i>|<i>%formatted.ISOSpeedRating%</i>|<i>%formatted.FocalLength%</i> could show \"1/20s - 400 - 80 mm\" or \"1/200 - 80 mm\",\n"
	  "if there's no ISO information in the Exif data.\n"
	  "If a line is empty, it is removed. This allows to add lines that totally disappear when no data is available.\n"
));
#endif
	gtk_container_add(GTK_CONTAINER(scrolled), image_overlay_template_view);
	gtk_widget_show(image_overlay_template_view);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);

	button = pref_button_new(NULL, NULL, _("Defaults"), FALSE,
				 G_CALLBACK(image_overlay_default_template_cb), image_overlay_template_view);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_HELP, NULL, FALSE,
				 G_CALLBACK(image_overlay_help_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(image_overlay_template_view));
	if (options->image_overlay.common.template_string) gtk_text_buffer_set_text(buffer, options->image_overlay.common.template_string, -1);
	g_signal_connect(G_OBJECT(buffer), "changed",
			 G_CALLBACK(image_overlay_template_view_changed_cb), image_overlay_template_view);

	group = pref_group_new(vbox, FALSE, _("Delete"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Confirm file delete"),
			      options->file_ops.confirm_delete, &c_options->file_ops.confirm_delete);
	pref_checkbox_new_int(group, _("Enable Delete key"),
			      options->file_ops.enable_delete_key, &c_options->file_ops.enable_delete_key);

	ct_button = pref_checkbox_new_int(group, _("Safe delete"),
					  options->file_ops.safe_delete_enable, &c_options->file_ops.safe_delete_enable);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
	pref_checkbox_link_sensitivity(ct_button, hbox);

	pref_spacer(hbox, PREF_PAD_INDENT - PREF_PAD_SPACE);
	pref_label_new(hbox, _("Folder:"));

	tabcomp = tab_completion_new(&safe_delete_path_entry, options->file_ops.safe_delete_path, NULL, NULL);
	tab_completion_add_select_button(safe_delete_path_entry, NULL, TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), tabcomp, TRUE, TRUE, 0);
	gtk_widget_show(tabcomp);

	hbox = pref_box_new(group, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_BUTTON_GAP);
	pref_checkbox_link_sensitivity(ct_button, hbox);

	pref_spacer(hbox, PREF_PAD_INDENT - PREF_PAD_GAP);
	spin = pref_spin_new_int(hbox, _("Maximum size:"), _("MB"),
				 0, 2048, 1, options->file_ops.safe_delete_folder_maxsize, &c_options->file_ops.safe_delete_folder_maxsize);
#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_markup(spin, _("Set to 0 for unlimited size"));
#endif
	button = pref_button_new(NULL, NULL, _("View"), FALSE,
				 G_CALLBACK(safe_delete_view_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_CLEAR, NULL, FALSE,
				 G_CALLBACK(safe_delete_clear_cb), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);


	group = pref_group_new(vbox, FALSE, _("Behavior"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Rectangular selection in icon view"),
			      options->collections.rectangular_selection, &c_options->collections.rectangular_selection);

	pref_checkbox_new_int(group, _("Descend folders in tree view"),
			      options->tree_descend_subdirs, &c_options->tree_descend_subdirs);

	pref_checkbox_new_int(group, _("In place renaming"),
			      options->file_ops.enable_in_place_rename, &c_options->file_ops.enable_in_place_rename);

	pref_checkbox_new_int(group, _("Show \"Copy path\" menu item which write the path of selected files to clipboard"),
			      options->show_copy_path, &c_options->show_copy_path);

	pref_spin_new_int(group, _("Open recent list maximum size"), NULL,
			  1, 50, 1, options->open_recent_list_maxsize, &c_options->open_recent_list_maxsize);
	
	pref_spin_new_int(group, _("Drag'n drop icon size"), NULL,
			  16, 256, 16, options->dnd_icon_size, &c_options->dnd_icon_size);

	group = pref_group_new(vbox, FALSE, _("Navigation"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Progressive keyboard scrolling"),
			      options->progressive_key_scrolling, &c_options->progressive_key_scrolling);
	pref_checkbox_new_int(group, _("Mouse wheel scrolls image"),
			      options->mousewheel_scrolls, &c_options->mousewheel_scrolls);

	group = pref_group_new(vbox, FALSE, _("Miscellaneous"), GTK_ORIENTATION_VERTICAL);

	pref_checkbox_new_int(group, _("Store metadata and cache files in source image's directory"),
			      options->enable_metadata_dirs, &c_options->enable_metadata_dirs);

	pref_checkbox_new_int(group, _("Store keywords and comments as XMP tags in image files"),
			      options->save_metadata_in_image_file, &c_options->save_metadata_in_image_file);

	pref_spin_new_int(group, _("Custom similarity threshold:"), NULL,
			  0, 100, 1, options->duplicates_similarity_threshold, &c_options->duplicates_similarity_threshold);

	group = pref_group_new(vbox, FALSE, _("Image loading and caching"), GTK_ORIENTATION_VERTICAL);

	pref_spin_new_int(group, _("Offscreen cache size (Mb per image):"), NULL,
			  0, 128, 1, options->image.tile_cache_max, &c_options->image.tile_cache_max);

	pref_spin_new_int(group, _("Image read buffer size (bytes):"), NULL,
			  IMAGE_LOADER_READ_BUFFER_SIZE_MIN, IMAGE_LOADER_READ_BUFFER_SIZE_MAX, 512,
			  options->image.read_buffer_size, &c_options->image.read_buffer_size);

	pref_spin_new_int(group, _("Image idle loop read count:"), NULL,
			  IMAGE_LOADER_IDLE_READ_LOOP_COUNT_MIN, IMAGE_LOADER_IDLE_READ_LOOP_COUNT_MAX, 1,
			  options->image.idle_read_loop_count, &c_options->image.idle_read_loop_count);


	group =  pref_group_new(vbox, FALSE, _("Color profiles"), GTK_ORIENTATION_VERTICAL);
#ifndef HAVE_LCMS
	gtk_widget_set_sensitive(pref_group_parent(group), FALSE);
#endif

	table = pref_table_new(group, 3, COLOR_PROFILE_INPUTS + 2, FALSE, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), PREF_PAD_GAP);

	label = pref_table_label(table, 0, 0, _("Type"), 0.0);
	pref_label_bold(label, TRUE, FALSE);

	label = pref_table_label(table, 1, 0, _("Menu name"), 0.0);
	pref_label_bold(label, TRUE, FALSE);

	label = pref_table_label(table, 2, 0, _("File"), 0.0);
	pref_label_bold(label, TRUE, FALSE);

	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		GtkWidget *entry;
		gchar *buf;

		buf = g_strdup_printf("Input %d:", i + COLOR_PROFILE_FILE);
		pref_table_label(table, 0, i + 1, buf, 1.0);
		g_free(buf);

		entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), EDITOR_NAME_MAX_LENGTH);
		gtk_widget_set_size_request(editor_name_entry[i], 30, -1);
		if (options->color_profile.input_name[i])
			{
			gtk_entry_set_text(GTK_ENTRY(entry), options->color_profile.input_name[i]);
			}
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, i + 1, i + 2,
				 GTK_FILL | GTK_EXPAND, 0, 0, 0);
		gtk_widget_show(entry);
		color_profile_input_name_entry[i] = entry;

		tabcomp = tab_completion_new(&entry, options->color_profile.input_file[i], NULL, NULL);
		tab_completion_add_select_button(entry, _("Select color profile"), FALSE);
		gtk_widget_set_size_request(entry, 160, -1);
		gtk_table_attach(GTK_TABLE(table), tabcomp, 2, 3, i + 1, i + 2,
				 GTK_FILL | GTK_EXPAND, 0, 0, 0);
		gtk_widget_show(tabcomp);
		color_profile_input_file_entry[i] = entry;
		}

	pref_table_label(table, 0, COLOR_PROFILE_INPUTS + 1, _("Screen:"), 1.0);
	tabcomp = tab_completion_new(&color_profile_screen_file_entry,
				     options->color_profile.screen_file, NULL, NULL);
	tab_completion_add_select_button(color_profile_screen_file_entry, _("Select color profile"), FALSE);
	gtk_widget_set_size_request(color_profile_screen_file_entry, 160, -1);
	gtk_table_attach(GTK_TABLE(table), tabcomp, 2, 3,
			 COLOR_PROFILE_INPUTS + 1, COLOR_PROFILE_INPUTS + 2,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(tabcomp);

#ifdef DEBUG
	group = pref_group_new(vbox, FALSE, _("Debugging"), GTK_ORIENTATION_VERTICAL);

	pref_spin_new_int(group, _("Debug level:"), NULL,
			  DEBUG_LEVEL_MIN, DEBUG_LEVEL_MAX, 1, get_debug_level(), &debug_c);
#endif
}

/* Main preferences window */
static void config_window_create(void)
{
	GtkWidget *win_vbox;
	GtkWidget *hbox;
	GtkWidget *notebook;
	GtkWidget *button;
	GtkWidget *ct_button;

	if (!c_options) c_options = init_options(NULL);

	configwindow = window_new(GTK_WINDOW_TOPLEVEL, "preferences", PIXBUF_INLINE_ICON_CONFIG, NULL, _("Preferences"));
	gtk_window_set_type_hint(GTK_WINDOW(configwindow), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(configwindow), "delete_event",
			 G_CALLBACK(config_window_delete), NULL);
	gtk_window_set_default_size(GTK_WINDOW(configwindow), CONFIG_WINDOW_DEF_WIDTH, CONFIG_WINDOW_DEF_HEIGHT);
	gtk_window_set_resizable(GTK_WINDOW(configwindow), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(configwindow), PREF_PAD_BORDER);

	win_vbox = gtk_vbox_new(FALSE, PREF_PAD_SPACE);
	gtk_container_add(GTK_CONTAINER(configwindow), win_vbox);
	gtk_widget_show(win_vbox);

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), PREF_PAD_BUTTON_GAP);
	gtk_box_pack_end(GTK_BOX(win_vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = pref_button_new(NULL, GTK_STOCK_OK, NULL, FALSE,
				 G_CALLBACK(config_window_ok_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	ct_button = button;

	button = pref_button_new(NULL, GTK_STOCK_APPLY, NULL, FALSE,
				 G_CALLBACK(config_window_apply_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_CANCEL, NULL, FALSE,
				 G_CALLBACK(config_window_close_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	if (!generic_dialog_get_alternative_button_order(configwindow))
		{
		gtk_box_reorder_child(GTK_BOX(hbox), ct_button, -1);
		}

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(win_vbox), notebook, TRUE, TRUE, 0);

	config_tab_general(notebook);
	config_tab_image(notebook);
	config_tab_windows(notebook);
	config_tab_filtering(notebook);
	config_tab_editors(notebook);
	config_tab_properties(notebook);
	config_tab_advanced(notebook);

	gtk_widget_show(notebook);

	gtk_widget_show(configwindow);
}

/*
 *-----------------------------------------------------------------------------
 * config window show (public)
 *-----------------------------------------------------------------------------
 */

void show_config_window(void)
{
	if (configwindow)
		{
		gtk_window_present(GTK_WINDOW(configwindow));
		return;
		}

	config_window_create();
}

/*
 *-----------------
 * about window
 *-----------------
 */

static GtkWidget *about = NULL;

static gint about_delete_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	gtk_widget_destroy(about);
	about = NULL;

	return TRUE;
}

static void about_window_close(GtkWidget *widget, gpointer data)
{
	if (!about) return;

	gtk_widget_destroy(about);
	about = NULL;
}

static void about_credits_cb(GtkWidget *widget, gpointer data)
{
	help_window_show("credits");
}

void show_about_window(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *button;
	GdkPixbuf *pixbuf;

	gchar *buf;

	if (about)
		{
		gtk_window_present(GTK_WINDOW(about));
		return;
		}

	about = window_new(GTK_WINDOW_TOPLEVEL, "about", NULL, NULL, _("About"));
	gtk_window_set_type_hint(GTK_WINDOW(about), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(about), "delete_event",
			 G_CALLBACK(about_delete_cb), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(about), PREF_PAD_BORDER);

	vbox = gtk_vbox_new(FALSE, PREF_PAD_SPACE);
	gtk_container_add(GTK_CONTAINER(about), vbox);
	gtk_widget_show(vbox);

	pixbuf = pixbuf_inline(PIXBUF_INLINE_LOGO);
	button = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	buf = g_strdup_printf(_("%s %s\n\nCopyright (c) 2006 John Ellis\nCopyright (c) %s The Geeqie Team\nwebsite: %s\nemail: %s\n\nReleased under the GNU General Public License"),
			      GQ_APPNAME,
			      VERSION,
			      "2008",
			      GQ_WEBSITE,
			      GQ_EMAIL_ADDRESS);
	label = gtk_label_new(buf);
	g_free(buf);

	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), PREF_PAD_BUTTON_GAP);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = pref_button_new(NULL, NULL, _("Credits..."), FALSE,
				 G_CALLBACK(about_credits_cb), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_show(button);

	button = pref_button_new(NULL, GTK_STOCK_CLOSE, NULL, FALSE,
				 G_CALLBACK(about_window_close), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	gtk_widget_show(about);
}
