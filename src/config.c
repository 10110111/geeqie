/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

/* config memory values */
static gint startup_path_enable_c;
static gint confirm_delete_c;
static gint restore_tool_c;
static gint save_window_positions_c;
static gint zoom_mode_c;
static gint fit_window_c;
static gint limit_window_size_c;
static gint max_window_size_c;
static gint progressive_key_scrolling_c;
static gint thumb_max_width_c;
static gint thumb_max_height_c;
static gint enable_thumb_caching_c;
static gint use_xvpics_thumbnails_c;
static gint show_dot_files_c;
static gint file_filter_disable_c;
static gint filter_include_jpg_c;
static gint filter_include_xpm_c;
static gint filter_include_tif_c;
static gint filter_include_gif_c;
static gint filter_include_png_c;
static gint filter_include_ppm_c;
static gint filter_include_pgm_c;
static gint filter_include_pcx_c;
static gint filter_include_bmp_c;
static gint slideshow_delay_c;
static gint slideshow_random_c;
static gint slideshow_repeat_c;

static GtkWidget *configwindow = NULL;
static GtkWidget *startup_path_entry;
static GtkWidget *custom_filter_entry;
static GtkWidget *editor_name_entry[8];
static GtkWidget *editor_command_entry[8];

static void startup_path_set_current(GtkWidget *widget, gpointer data);
static void slideshow_delay_cb(GtkObject *adj, gpointer data);
static void zoom_mode_original_cb(GtkWidget *widget, gpointer data);
static void zoom_mode_fit_cb(GtkWidget *widget, gpointer data);
static void zoom_mode_none_cb(GtkWidget *widget, gpointer data);
static void max_window_size_cb(GtkObject *adj, gpointer data);
static void thumb_size_48_selected(GtkWidget *w, gpointer data);
static void thumb_size_64_selected(GtkWidget *w, gpointer data);
static void thumb_size_85_selected(GtkWidget *w, gpointer data);
static void thumb_size_100_selected(GtkWidget *w, gpointer data);

static void config_window_apply();
static void config_window_close_cb(GtkWidget *widget, gpointer data);
static void config_window_destroy(GtkWidget *w, GdkEvent *event, gpointer data);
static void config_window_ok_cb(GtkWidget *widget, gpointer data);
static void config_window_save_cb(GtkWidget *widget, gpointer data);

static void check_button_cb(GtkWidget *widget, gpointer data);
static void add_check_button(gint option, gint *option_c, gchar *text, GtkWidget *box);
static void config_window_create(gint start_tab);

/*
 *-----------------------------------------------------------------------------
 * option widget callbacks (private)
 *-----------------------------------------------------------------------------
 */ 

static void startup_path_set_current(GtkWidget *widget, gpointer data)
{
	gtk_entry_set_text(GTK_ENTRY(startup_path_entry), current_path);
}

static void slideshow_delay_cb(GtkObject *adj, gpointer data)
{
	slideshow_delay_c = (gint)GTK_ADJUSTMENT(adj)->value;
}

static void zoom_mode_original_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		zoom_mode_c = ZOOM_RESET_ORIGINAL;
}

static void zoom_mode_fit_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		zoom_mode_c = ZOOM_RESET_FIT_WINDOW;
}

static void zoom_mode_none_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		zoom_mode_c = ZOOM_RESET_NONE;
}

static void max_window_size_cb(GtkObject *adj, gpointer data)
{
	max_window_size_c = (gint)GTK_ADJUSTMENT(adj)->value;
}

static void thumb_size_48_selected(GtkWidget *w, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (w)->active)
		{
		thumb_max_width_c = 48;
		thumb_max_height_c = 48;
		}
}

static void thumb_size_64_selected(GtkWidget *w, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (w)->active)
		{
		thumb_max_width_c = 64;
		thumb_max_height_c = 64;
		}
}

static void thumb_size_85_selected(GtkWidget *w, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (w)->active)
		{
		thumb_max_width_c = 85;
		thumb_max_height_c = 64;
		}
}

static void thumb_size_100_selected(GtkWidget *w, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (w)->active)
		{
		thumb_max_width_c = 100;
		thumb_max_height_c = 100;
		}
}

/*
 *-----------------------------------------------------------------------------
 * sync progam to config window routine (private)
 *-----------------------------------------------------------------------------
 */ 

static void config_window_apply()
{
	gchar *buf;
	gint i;
	gint refresh = FALSE;

	for(i=0; i<8; i++)
		{
		g_free(editor_name[i]);
		editor_name[i] = NULL;
		buf = gtk_entry_get_text(GTK_ENTRY(editor_name_entry[i]));
		if (buf && strlen(buf) > 0) editor_name[i] = g_strdup(buf);

		g_free(editor_command[i]);
		editor_command[i] = NULL;
		buf = gtk_entry_get_text(GTK_ENTRY(editor_command_entry[i]));
		if (buf && strlen(buf) > 0) editor_command[i] = g_strdup(buf);
		}
	update_edit_menus(mainwindow_accel_grp);

	g_free(startup_path);
	startup_path = NULL;
	buf = gtk_entry_get_text(GTK_ENTRY(startup_path_entry));
	if (buf && strlen(buf) > 0) startup_path = remove_trailing_slash(buf);

	buf = gtk_entry_get_text(GTK_ENTRY(custom_filter_entry));
	if ((buf && strlen(buf) > 0) != (custom_filter != NULL)) refresh = TRUE;
	if ((buf && strlen(buf) > 0 && custom_filter) && strcmp(buf, custom_filter) != 0) refresh = TRUE;
	g_free(custom_filter);
	custom_filter = NULL;
	if (buf && strlen(buf) > 0) custom_filter = g_strdup(buf);

	if (show_dot_files != show_dot_files_c) refresh = TRUE;
	if (file_filter_disable != file_filter_disable_c) refresh = TRUE;
	if (filter_include_jpg != filter_include_jpg_c) refresh = TRUE;
	if (filter_include_xpm != filter_include_xpm_c) refresh = TRUE;
	if (filter_include_tif != filter_include_tif_c) refresh = TRUE;
	if (filter_include_gif != filter_include_gif_c) refresh = TRUE;
	if (filter_include_png != filter_include_png_c) refresh = TRUE;
	if (filter_include_ppm != filter_include_ppm_c) refresh = TRUE;
	if (filter_include_pgm != filter_include_pgm_c) refresh = TRUE;
	if (filter_include_pcx != filter_include_pcx_c) refresh = TRUE;
	if (filter_include_bmp != filter_include_bmp_c) refresh = TRUE;

	startup_path_enable = startup_path_enable_c;
	confirm_delete = confirm_delete_c;
	restore_tool = restore_tool_c;
	save_window_positions = save_window_positions_c;
	zoom_mode = zoom_mode_c;
	fit_window = fit_window_c;
	limit_window_size = limit_window_size_c;
	max_window_size = max_window_size_c;
	progressive_key_scrolling = progressive_key_scrolling_c;
	thumb_max_width = thumb_max_width_c;
	thumb_max_height = thumb_max_height_c;
	enable_thumb_caching = enable_thumb_caching_c;
	use_xvpics_thumbnails = use_xvpics_thumbnails_c;
	show_dot_files = show_dot_files_c;
	file_filter_disable = file_filter_disable_c;
	filter_include_jpg = filter_include_jpg_c;
	filter_include_xpm = filter_include_xpm_c;
	filter_include_tif = filter_include_tif_c;
	filter_include_gif = filter_include_gif_c;
	filter_include_png = filter_include_png_c;
	filter_include_ppm = filter_include_ppm_c;
	filter_include_pgm = filter_include_pgm_c;
	filter_include_pcx = filter_include_pcx_c;
	filter_include_bmp = filter_include_bmp_c;

	slideshow_random = slideshow_random_c;
	slideshow_repeat = slideshow_repeat_c;
	slideshow_delay = slideshow_delay_c;

	if (refresh)
		{
		rebuild_file_filter();
		filelist_refresh();
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
}

static void config_window_destroy(GtkWidget *w, GdkEvent *event, gpointer data)
{
	config_window_close_cb(NULL, NULL);
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

static void check_button_cb(GtkWidget *widget, gpointer data)
{
	gint *value_ptr = data;
	*value_ptr = GTK_TOGGLE_BUTTON (widget)->active;
}

static void add_check_button(gint option, gint *option_c, gchar *text, GtkWidget *box)
{
	GtkWidget *button;
	*option_c = option;
	button = gtk_check_button_new_with_label (text);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(button), option);
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) check_button_cb, option_c);
	gtk_widget_show(button);
}

static void config_window_create(gint start_tab)
{
	GtkWidget *win_vbox;
	GtkWidget *hbox;
	GtkWidget *notebook;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *button;
	GtkWidget *tabcomp;
	GtkWidget *radiobuttongroup;
	GtkWidget *table;
	GtkObject *adj;
	GtkWidget *spin;
	GdkImlibImage* im;
	GdkPixmap *pixmap;
	gchar buf[255];
	gint i;

	configwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (configwindow), "delete_event",(GtkSignalFunc) config_window_destroy, NULL);
	gtk_window_set_policy (GTK_WINDOW (configwindow), FALSE, FALSE, FALSE);
	gtk_window_set_title (GTK_WINDOW (configwindow), _("GQview configuration"));
	gtk_window_set_wmclass(GTK_WINDOW (configwindow), "config", "GQview");
	gtk_container_border_width (GTK_CONTAINER (configwindow), 5);

	win_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(configwindow), win_vbox);
	gtk_widget_show(win_vbox);

	hbox = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_end(GTK_BOX(win_vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_with_label(_("Ok"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) config_window_ok_cb, NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 20);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Apply"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) config_window_apply_cb, NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 20);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Cancel"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) config_window_close_cb, NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 20);
	gtk_widget_show(button);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX(win_vbox), notebook, TRUE, TRUE, 0);

	/* general options tab */

	frame = gtk_frame_new(NULL);
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_widget_show(frame);
	label = gtk_label_new(_("General"));
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), frame, label);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER(frame),vbox);
	gtk_widget_show(vbox);

	frame = gtk_frame_new(_("Initial directory"));
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame),vbox1);
	gtk_widget_show(vbox1);

	add_check_button(startup_path_enable, &startup_path_enable_c,
			 _("On startup, change to this directory:"), vbox1);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_widget_realize(configwindow);

	tabcomp = tab_completion_new(&startup_path_entry, configwindow, startup_path, NULL, NULL);
	gtk_box_pack_start(GTK_BOX(hbox), tabcomp, TRUE, TRUE, 0);
	gtk_widget_show(tabcomp);

	button = gtk_button_new_with_label (_("Use current"));
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) startup_path_set_current, NULL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	add_check_button(confirm_delete, &confirm_delete_c,
			 _("Confirm file delete"), vbox);
	add_check_button(restore_tool, &restore_tool_c,
			 _("Remember tool state (float/hidden)"), vbox);
	add_check_button(save_window_positions, &save_window_positions_c,
			 _("Remember window positions"), vbox);

	frame = gtk_frame_new(_("Slide show"));
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame),vbox1);
	gtk_widget_show(vbox1);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Delay before image change (seconds):"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	slideshow_delay_c = slideshow_delay;
	adj = gtk_adjustment_new((float)slideshow_delay_c, 1.0, 1200.0, 1, 1, 1);
        spin = gtk_spin_button_new( GTK_ADJUSTMENT(adj), 1, 0 );
        gtk_box_pack_start( GTK_BOX(hbox), spin, FALSE, FALSE, 5);
        gtk_signal_connect( GTK_OBJECT(adj),"value_changed",GTK_SIGNAL_FUNC(slideshow_delay_cb), NULL);
        gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON(spin),GTK_UPDATE_ALWAYS );
        gtk_widget_show(spin);
	
	add_check_button(slideshow_random, &slideshow_random_c,
			 _("Random"), vbox1);
	add_check_button(slideshow_repeat, &slideshow_repeat_c,
			 _("Repeat"), vbox1);

	/* image tab */

	frame = gtk_frame_new(NULL);
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_widget_show(frame);
	label = gtk_label_new(_("Image"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER(frame),vbox);
	gtk_widget_show(vbox);
	
	frame = gtk_frame_new(_("When new image is selected:"));
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame),vbox1);
	gtk_widget_show(vbox1);

	zoom_mode_c = zoom_mode;
	radiobuttongroup = gtk_radio_button_new_with_label (NULL, _("Zoom to original size"));
	if (zoom_mode == ZOOM_RESET_ORIGINAL) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(radiobuttongroup), 1);
	gtk_signal_connect (GTK_OBJECT(radiobuttongroup),"clicked",(GtkSignalFunc) zoom_mode_original_cb, NULL);
	gtk_box_pack_start(GTK_BOX(vbox1), radiobuttongroup, FALSE, FALSE, 0);
	gtk_widget_show(radiobuttongroup);

	button = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(radiobuttongroup)),_("Fit image to window"));
	if (zoom_mode == ZOOM_RESET_FIT_WINDOW) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(button), 1);
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) zoom_mode_fit_cb, NULL);
	gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(radiobuttongroup)),_("Leave Zoom at previous setting"));
	if (zoom_mode == ZOOM_RESET_NONE) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(button), 1);
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) zoom_mode_none_cb, NULL);
	gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	add_check_button(fit_window, &fit_window_c,
			 _("Fit window to image when tools are hidden/floating"), vbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	add_check_button(limit_window_size, &limit_window_size_c,
			 _("Limit size when auto-sizing window"), hbox);

	max_window_size_c = max_window_size;
	adj = gtk_adjustment_new((float)max_window_size_c, 10.0, 150.0, 1, 1, 1);
        spin = gtk_spin_button_new( GTK_ADJUSTMENT(adj), 1, 0 );
        gtk_box_pack_start( GTK_BOX(hbox), spin, FALSE, FALSE, 5);
        gtk_signal_connect( GTK_OBJECT(adj),"value_changed",GTK_SIGNAL_FUNC(max_window_size_cb), NULL);
        gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON(spin),GTK_UPDATE_ALWAYS );
        gtk_widget_show(spin);

	frame = gtk_frame_new(_("Thumbnails"));
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame),vbox1);
	gtk_widget_show(vbox1);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	thumb_max_width_c = thumb_max_width;
	thumb_max_height_c = thumb_max_height;

	label = gtk_label_new(_("Size:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	radiobuttongroup = gtk_radio_button_new_with_label (NULL,"48x48");
	gtk_box_pack_start(GTK_BOX(hbox), radiobuttongroup, FALSE, FALSE, 0);
	if (thumb_max_width_c == 48 && thumb_max_height_c == 48) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(radiobuttongroup), 1);
	gtk_signal_connect (GTK_OBJECT(radiobuttongroup),"clicked",(GtkSignalFunc) thumb_size_48_selected, NULL);
	gtk_widget_show(radiobuttongroup);

	button = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(radiobuttongroup)),"64x64");
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (thumb_max_width_c == 64 && thumb_max_height_c == 64) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(button), 1);
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) thumb_size_64_selected, NULL);
	gtk_widget_show(button);

	button = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(radiobuttongroup)),"85x64");
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (thumb_max_width_c == 85 && thumb_max_height_c == 64) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(button), 1);
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) thumb_size_85_selected, NULL);
	gtk_widget_show(button);

	button = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(radiobuttongroup)),"100x100");
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (thumb_max_width_c == 100 && thumb_max_height_c == 100) gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(button), 1);
	gtk_signal_connect (GTK_OBJECT(button),"clicked",(GtkSignalFunc) thumb_size_100_selected, NULL);
	gtk_widget_show(button);

	add_check_button(enable_thumb_caching, &enable_thumb_caching_c,
			 _("Cache thumbnails"), vbox1);
	add_check_button(use_xvpics_thumbnails, &use_xvpics_thumbnails_c,
			 _("Use xvpics thumbnails when found (read only)"), vbox1);

	add_check_button(progressive_key_scrolling, &progressive_key_scrolling_c,
			 _("Progressive keyboard scrolling"), vbox);

	/* filtering tab */

	frame = gtk_frame_new(NULL);
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_widget_show(frame);
	label = gtk_label_new(_("Filtering"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER(frame),vbox);
	gtk_widget_show(vbox);

	add_check_button(show_dot_files, &show_dot_files_c,
			 _("Show entries that begin with a dot"), vbox);
	add_check_button(file_filter_disable, &file_filter_disable_c,
			 _("Disable File Filtering"), vbox);

	frame = gtk_frame_new(_("Include files of type:"));
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame),vbox1);
	gtk_widget_show(vbox1);

	hbox = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(hbox), vbox2,FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	add_check_button(filter_include_jpg, &filter_include_jpg_c,
			 "JPG / JPEG", vbox2);
	add_check_button(filter_include_xpm, &filter_include_xpm_c,
			 "XPM", vbox2);
	add_check_button(filter_include_tif, &filter_include_tif_c,
			 "TIF / TIFF", vbox2);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(hbox), vbox2,FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	add_check_button(filter_include_gif, &filter_include_gif_c,
			 "GIF", vbox2);
	add_check_button(filter_include_png, &filter_include_png_c,
			 "PNG", vbox2);
	add_check_button(filter_include_ppm, &filter_include_ppm_c,
			 "PPM", vbox2);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(hbox), vbox2,FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	add_check_button(filter_include_pgm, &filter_include_pgm_c,
			 "PGM", vbox2);
	add_check_button(filter_include_pcx, &filter_include_pcx_c,
			 "PCX", vbox2);
	add_check_button(filter_include_bmp, &filter_include_bmp_c,
			 "BMP", vbox2);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Custom file types:"));
	gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	custom_filter_entry = gtk_entry_new();
	if (custom_filter) gtk_entry_set_text(GTK_ENTRY(custom_filter_entry), custom_filter);
	gtk_box_pack_start(GTK_BOX(vbox1),custom_filter_entry,FALSE,FALSE,0);
	gtk_widget_show(custom_filter_entry);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("format: [.foo;.bar]"));
	gtk_box_pack_end(GTK_BOX(hbox),label,FALSE,FALSE,5);
	gtk_widget_show(label);

	/* editor entry tab */

	frame = gtk_frame_new(NULL);
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_widget_show(frame);
	label = gtk_label_new(_("External Editors"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

	vbox = gtk_vbox_new(FALSE,0);
	gtk_container_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER(frame),vbox);
	gtk_widget_show(vbox);

	table=gtk_table_new(3,9,FALSE);
	gtk_container_add (GTK_CONTAINER(vbox),table);
	gtk_widget_show(table);

	label = gtk_label_new(_("#"));
	gtk_table_attach_defaults(GTK_TABLE (table),label, 0, 1, 0, 1);
	gtk_widget_show(label);
	label = gtk_label_new(_("Menu name"));
	gtk_table_attach_defaults(GTK_TABLE (table),label, 1, 2, 0, 1);
	gtk_widget_show(label);
	label = gtk_label_new(_("Command Line"));
	gtk_table_attach_defaults(GTK_TABLE (table),label, 2, 3, 0, 1);
	gtk_widget_show(label);

	for (i=0; i<8; i++)
		{
		sprintf(buf,"%d",i+1);
		label = gtk_label_new(buf);
		gtk_table_attach_defaults(GTK_TABLE (table),label, 0, 1, i+1, i+2);
		gtk_widget_show(label);

		editor_name_entry[i] = gtk_entry_new_with_max_length(32);
		gtk_widget_set_usize(editor_name_entry[i],80,-1);
		if (editor_name[i]) gtk_entry_set_text(GTK_ENTRY(editor_name_entry[i]),editor_name[i]);
		gtk_table_attach_defaults(GTK_TABLE (table),editor_name_entry[i],1,2,i+1,i+2);
		gtk_widget_show(editor_name_entry[i]);

		editor_command_entry[i] = gtk_entry_new_with_max_length(255);
		gtk_widget_set_usize(editor_command_entry[i],160,-1);
		tab_completion_add_to_entry(editor_command_entry[i], NULL, NULL);
		if (editor_command[i]) gtk_entry_set_text(GTK_ENTRY(editor_command_entry[i]), editor_command[i]);
		gtk_table_attach_defaults(GTK_TABLE (table),editor_command_entry[i],2,3,i+1,i+2);
		gtk_widget_show(editor_command_entry[i]);
		}

	/* about tab */

	frame = gtk_frame_new(NULL);
	gtk_container_border_width (GTK_CONTAINER (frame), 5);
	gtk_widget_show(frame);
	label = gtk_label_new(_("About"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame),vbox);
	gtk_widget_show(vbox);

	im = gdk_imlib_create_image_from_data((char *)logo, NULL, logo_width, logo_height);
	gdk_imlib_render(im, logo_width, logo_height);
	pixmap = gdk_imlib_move_image(im);
	gdk_imlib_destroy_image(im);
	
	button=gtk_pixmap_new(pixmap, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
	gtk_widget_show (button);

	sprintf(buf, _("GQview %s\n\nCopyright (c) 2000 by John Ellis\nhttp://gqview.sorceforge.net\nor http://gqview.netpedia.net\ngqview@email.com\n\nReleased under the GNU Public License"), VERSION);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	gtk_notebook_set_page (GTK_NOTEBOOK(notebook), start_tab);
	gtk_widget_show(notebook);

	gtk_widget_show(configwindow);
}

/*
 *-----------------------------------------------------------------------------
 * config/about window show (public)
 *-----------------------------------------------------------------------------
 */ 

void show_config_window()
{
	if (configwindow) return;
	config_window_create(0);
}

void show_about_window()
{
	if (configwindow) return;
	config_window_create(4);
}

