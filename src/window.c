/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

#define MAINWINDOW_DEF_WIDTH 500
#define MAINWINDOW_DEF_HEIGHT 400
#define TOOLWINDOW_DEF_WIDTH 224
#define TOOLWINDOW_DEF_HEIGHT 450
#define LIST_DEFAULT_WIDTH -1
#define LIST_DEFAULT_HEIGHT 100

static GtkWidget *add_label(gchar *text, GtkWidget *box, gint start, gint size, gint expand);
static void info_area_create(GtkWidget *vbox);

static void toolwindow_create();
static void toolwindow_destroy();
static void toolwindow_show();

static void image_focus_paint(GtkWidget *widget);
static gint image_focus_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data);
static gint image_focus_in_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data);
static gint image_focus_out_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data);

static void create_tools(GtkAccelGroup *accel_grp, GtkTooltips *tooltips);
static void mainwindow_destroy_cb(GtkWidget *widget, gpointer data);

/*
 *-----------------------------------------------------------------------------
 * information widget creation routines (private)
 *-----------------------------------------------------------------------------
 */ 

static GtkWidget *add_label(gchar *text, GtkWidget *box, gint start, gint size, gint expand)
{
	GtkWidget *label;
	GtkWidget *frame;

	frame = gtk_frame_new (NULL);
	if (size)
		gtk_widget_set_usize (frame, size, -1);
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
	if (start)
		gtk_box_pack_start(GTK_BOX(box), frame, expand, expand, 0);
	else
		gtk_box_pack_end(GTK_BOX(box), frame, expand, expand, 0);
	gtk_widget_show(frame);

	label = gtk_label_new(text);
	gtk_container_add (GTK_CONTAINER (frame), label);
	gtk_widget_show(label);

	return label;
}

static void info_area_create(GtkWidget *vbox)
{
	gchar *status_text;
	gchar *details_text;
	gchar *zoom_text;
	gchar *buf;

	if (info_status)
		{
		gtk_label_get(GTK_LABEL(info_status), &buf);
		status_text = g_strdup(buf);

		gtk_label_get(GTK_LABEL(info_details), &buf);
		details_text = g_strdup(buf);

		gtk_label_get(GTK_LABEL(info_zoom), &buf);
		zoom_text = g_strdup(buf);
		}
	else
		{
		status_text = g_strdup("");
		details_text = g_strdup("GQview");
		zoom_text = g_strdup(":");
		}

	if (info_box)
		{
		gtk_widget_destroy(info_box);
		info_box = NULL;
		}

	if (vbox)
		{
		GtkWidget *hbox;
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);
			
		info_progress_bar = gtk_progress_bar_new();
		gtk_widget_set_usize(info_progress_bar,150,-1);
		gtk_box_pack_start (GTK_BOX (hbox), info_progress_bar, FALSE, FALSE, 0);
		gtk_widget_show(info_progress_bar);

		info_zoom = add_label(zoom_text, hbox, FALSE, 48, FALSE);

		info_status = add_label(status_text, vbox, TRUE, 0, FALSE);
		info_details = add_label(details_text, vbox, TRUE, 0, FALSE);
		}
	else
		{
		info_box = gtk_hbox_new(FALSE, 0);

		info_progress_bar = gtk_progress_bar_new();
		gtk_widget_set_usize(info_progress_bar,150,-1);
		gtk_box_pack_start (GTK_BOX (info_box), info_progress_bar, FALSE, FALSE, 0);
		gtk_widget_show(info_progress_bar);

		info_status = add_label(status_text, info_box, TRUE, 0, TRUE);
		info_details = add_label(details_text, info_box, TRUE, 0, TRUE);
		info_zoom = add_label(zoom_text, info_box, FALSE, 48, FALSE);

		gtk_widget_show(info_box);
		}

	image_set_labels(info_details, info_zoom);

	g_free(status_text);
	g_free(details_text);
	g_free(zoom_text);
}

/*
 *-----------------------------------------------------------------------------
 * tool window create/show/hide routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void toolwindow_destroy_cb(GtkWidget *widget, gpointer data)
{
	toolwindow_float();
}

static void toolwindow_create()
{
	GtkWidget *vbox;
	GtkAllocation req_size;

	toolwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (toolwindow), "delete_event",(GtkSignalFunc) toolwindow_destroy_cb, NULL);
	gtk_window_set_policy(GTK_WINDOW(toolwindow), TRUE, TRUE, FALSE);
	gtk_window_set_title (GTK_WINDOW (toolwindow), _("GQview Tools"));
	gtk_window_set_wmclass(GTK_WINDOW (toolwindow), "tools", "GQview");
	gtk_container_border_width (GTK_CONTAINER (toolwindow), 0);
	gtk_window_add_accel_group(GTK_WINDOW(toolwindow),mainwindow_accel_grp);

	gtk_signal_connect(GTK_OBJECT(toolwindow), "key_press_event", GTK_SIGNAL_FUNC(key_press_cb), NULL);

	if (save_window_positions)
		{
		gtk_window_set_default_size (GTK_WINDOW(toolwindow), float_window_w, float_window_h);
		gtk_widget_set_uposition(toolwindow, float_window_x, float_window_y);
		req_size.x = req_size.y = 0;
		req_size.width = float_window_w;
		req_size.height = float_window_h;
		}
	else
		{
		gtk_window_set_default_size (GTK_WINDOW(toolwindow), TOOLWINDOW_DEF_WIDTH, TOOLWINDOW_DEF_HEIGHT);
		req_size.x = req_size.y = 0;
		req_size.width = TOOLWINDOW_DEF_WIDTH;
		req_size.height = TOOLWINDOW_DEF_HEIGHT;
		}
	gtk_widget_size_allocate(toolwindow, &req_size);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(toolwindow), vbox);

	gtk_widget_realize(toolwindow);
	gtk_widget_realize(vbox);

	if (tool_vbox->parent)
		{
		gtk_widget_ref(tool_vbox);
		gtk_container_remove(GTK_CONTAINER(tool_vbox->parent), tool_vbox);
		gtk_box_pack_start(GTK_BOX(vbox), tool_vbox, TRUE, TRUE, 0);
		gtk_widget_unref(tool_vbox);
		}
	else
		{
		gtk_box_pack_start(GTK_BOX(vbox), tool_vbox, TRUE, TRUE, 0);
		}

	info_area_create(vbox);

	gtk_widget_show_all(vbox);
}

static void toolwindow_destroy()
{
	if (toolwindow && GTK_WIDGET_VISIBLE(toolwindow))
		{
		gdk_window_get_position (toolwindow->window, &float_window_x, &float_window_y);
		gdk_window_get_size(toolwindow->window, &float_window_w, &float_window_h);
		}

	info_area_create(NULL);

	gtk_widget_ref(tool_vbox);
	gtk_container_remove(GTK_CONTAINER(tool_vbox->parent), tool_vbox);
	gtk_box_pack_start(GTK_BOX(mainwindow_hbox), tool_vbox, FALSE, FALSE, 0);
	gtk_widget_unref(tool_vbox);

	gtk_box_pack_start(GTK_BOX(mainwindow_vbox), info_box, FALSE, FALSE, 0);
	gtk_widget_destroy(toolwindow);
	toolwindow = NULL;
}

static void toolwindow_show()
{
	gtk_widget_show(toolwindow);
	if (save_window_positions)
		gdk_window_move(toolwindow->window, float_window_x, float_window_y);

}

/*
 *-----------------------------------------------------------------------------
 * tool window hide/float routines (public)
 *-----------------------------------------------------------------------------
 */ 

void toolwindow_float()
{
	if (toolwindow)
		{
		if (GTK_WIDGET_VISIBLE(toolwindow))
			{
			toolwindow_destroy();
			tools_float = FALSE;
			tools_hidden = FALSE;
			}
		else
			{
			toolwindow_show();
			tools_float = TRUE;
			tools_hidden = FALSE;
			}
		}
	else
		{
		toolwindow_create();
		toolwindow_show();
		tools_float = TRUE;
		tools_hidden = FALSE;
		}
}

void toolwindow_hide()
{
	if (toolwindow)
		{
		if (GTK_WIDGET_VISIBLE(toolwindow))
			{
			gtk_widget_hide(toolwindow);
			gdk_window_get_position (toolwindow->window, &float_window_x, &float_window_y);
			gdk_window_get_size(toolwindow->window, &float_window_w, &float_window_h);
			tools_hidden = TRUE;
			}
		else
			{
			if (tools_float)
				toolwindow_show();
			else
				toolwindow_destroy();
			tools_hidden = FALSE;
			}
		}
	else
		{
		toolwindow_create();
		tools_hidden = TRUE;
		}
}

/*
 *-----------------------------------------------------------------------------
 * image viewport focus display (private)
 *-----------------------------------------------------------------------------
 */ 

static void image_focus_paint(GtkWidget *widget)
{
	gint width, height;
	gdk_window_get_size (widget->window, &width, &height);
	gdk_draw_rectangle (widget->window,
			    widget->style->black_gc,
			    FALSE,
			    0, 0, width - 1, height - 1);
}

static gint image_focus_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	image_focus_paint (widget);
	return TRUE;
}

static gint image_focus_in_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	gtk_signal_connect_after (GTK_OBJECT (widget), "draw",
			GTK_SIGNAL_FUNC (image_focus_expose), NULL);
	gtk_signal_connect (GTK_OBJECT (widget), "expose_event",
			GTK_SIGNAL_FUNC (image_focus_paint), NULL);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);

	gtk_widget_queue_draw (widget);
	return FALSE;
}

static gint image_focus_out_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
			GTK_SIGNAL_FUNC (image_focus_paint), NULL);
	gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
			GTK_SIGNAL_FUNC (image_focus_expose), NULL);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	gtk_widget_queue_clear (widget);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT(widget), GTK_SHADOW_IN);
	return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 * main window setup
 *-----------------------------------------------------------------------------
 */ 

static void create_tools(GtkAccelGroup *accel_grp, GtkTooltips *tooltips)
{
	GtkWidget *menu_bar;
	GtkWidget *button_bar;
	GtkWidget *vpaned;
	GtkWidget *scrolled;
	GtkWidget *tabcomp;

	tool_vbox = gtk_vbox_new(FALSE, 0);

	menu_bar = create_menu_bar(accel_grp);
	gtk_box_pack_start (GTK_BOX(tool_vbox), menu_bar,FALSE,FALSE,0);
	gtk_widget_show(menu_bar);

	button_bar = create_button_bar(tooltips);
	gtk_box_pack_start (GTK_BOX(tool_vbox), button_bar,FALSE,FALSE,0);
	gtk_widget_show(button_bar);

	/* path entry */
	tabcomp = tab_completion_new(&path_entry, mainwindow, NULL, path_entry_cb, NULL);
	tab_completion_add_tab_func(path_entry, path_entry_tab_cb, NULL);
	gtk_box_pack_start (GTK_BOX (tool_vbox), tabcomp, FALSE, FALSE, 0);
	gtk_widget_show (tabcomp);

        /* history button */
	history_menu = gtk_option_menu_new ();
	gtk_box_pack_start (GTK_BOX (tool_vbox), history_menu, FALSE, FALSE, 0);
	gtk_widget_show (history_menu);

	vpaned = gtk_vpaned_new ();
	gtk_paned_handle_size (GTK_PANED(vpaned), 10);
	gtk_paned_gutter_size (GTK_PANED(vpaned), 10);
	gtk_box_pack_start (GTK_BOX (tool_vbox), vpaned, TRUE, TRUE, 0);
	gtk_widget_show (vpaned);

	/* dir list */
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_paned_add1 (GTK_PANED(vpaned), scrolled);
	gtk_widget_show(scrolled);

	dir_clist=gtk_clist_new(1);
	gtk_signal_connect (GTK_OBJECT (dir_clist), "button_press_event",(GtkSignalFunc) dir_press_cb, NULL);
	gtk_signal_connect (GTK_OBJECT (dir_clist), "select_row",(GtkSignalFunc) dir_select_cb, NULL);
	gtk_clist_column_titles_passive (GTK_CLIST (dir_clist)); 
	gtk_widget_set_usize(dir_clist, LIST_DEFAULT_WIDTH, LIST_DEFAULT_HEIGHT);
	gtk_container_add (GTK_CONTAINER (scrolled), dir_clist);
	gtk_widget_show(dir_clist);

	/* file list */
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_paned_add2 (GTK_PANED(vpaned), scrolled);
	gtk_widget_show(scrolled);

	file_clist=gtk_clist_new(1);
	gtk_clist_set_selection_mode(GTK_CLIST(file_clist), GTK_SELECTION_EXTENDED);
	gtk_signal_connect (GTK_OBJECT (file_clist), "button_press_event",(GtkSignalFunc) file_press_cb, NULL);
	gtk_signal_connect (GTK_OBJECT (file_clist), "select_row",(GtkSignalFunc) file_select_cb, NULL);
	gtk_signal_connect_after (GTK_OBJECT (file_clist), "unselect_row",(GtkSignalFunc) file_unselect_cb, NULL);
	gtk_clist_column_titles_passive (GTK_CLIST (file_clist)); 
	gtk_widget_set_usize(file_clist, LIST_DEFAULT_WIDTH, LIST_DEFAULT_HEIGHT);
	gtk_container_add (GTK_CONTAINER (scrolled), file_clist);
	gtk_widget_show(file_clist);

	gtk_widget_show(tool_vbox);
}

static void mainwindow_destroy_cb(GtkWidget *widget, gpointer data)
{
	exit_gqview();
}

void create_main_window()
{
	GtkWidget *image_window;
	GtkAllocation req_size;
	GtkTooltips *tooltips;
	GdkColormap *colormap;
	static GdkColor tooltip_color = { 0, 0xffff, 0xf9f9, 0xcbcb }; /*255 249 203*/

	mainwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(mainwindow), TRUE, TRUE, FALSE);

	gtk_signal_connect (GTK_OBJECT (mainwindow), "delete_event",(GtkSignalFunc) mainwindow_destroy_cb, NULL);

	gtk_window_set_title(GTK_WINDOW (mainwindow), "GQview");
	gtk_window_set_wmclass(GTK_WINDOW (mainwindow), "gqview", "GQview");
	gtk_container_border_width (GTK_CONTAINER (mainwindow), 0);

	gtk_signal_connect(GTK_OBJECT(mainwindow), "key_press_event", GTK_SIGNAL_FUNC(key_press_cb), NULL);

	if (save_window_positions)
		{
		req_size.x = main_window_x;
		req_size.y = main_window_y;
		req_size.width = main_window_w;
		req_size.height = main_window_h;
		gtk_window_set_default_size (GTK_WINDOW(mainwindow), main_window_w, main_window_h);
		gtk_widget_set_uposition(mainwindow, main_window_x, main_window_y);

		}
	else
		{
		req_size.x = 0;
		req_size.y = 0;
		req_size.width = MAINWINDOW_DEF_WIDTH;
		req_size.height = MAINWINDOW_DEF_HEIGHT;
		gtk_window_set_default_size (GTK_WINDOW(mainwindow), MAINWINDOW_DEF_WIDTH, MAINWINDOW_DEF_HEIGHT);
		}

	gtk_widget_size_allocate(mainwindow, &req_size);

	gtk_widget_realize(mainwindow);

	mainwindow_accel_grp = gtk_accel_group_new ();
	gtk_window_add_accel_group(GTK_WINDOW(mainwindow),mainwindow_accel_grp);

	tooltips = gtk_tooltips_new();
	colormap = gdk_window_get_colormap (mainwindow->window);
	gdk_color_alloc (colormap, &tooltip_color);
	gtk_tooltips_set_colors(tooltips, &tooltip_color, &mainwindow->style->fg[GTK_STATE_NORMAL]);

	create_menu_popups();
	create_tools(mainwindow_accel_grp, tooltips);

	image_window = image_create();

	mainwindow_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (mainwindow), mainwindow_vbox);
	gtk_widget_show(mainwindow_vbox);

	mainwindow_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainwindow_vbox), mainwindow_hbox, TRUE, TRUE, 0);
	gtk_widget_show(mainwindow_hbox);

	if (tools_float || tools_hidden)
		{
		toolwindow_create();
		if (!tools_hidden)
			{
			toolwindow_show();
			}
		}
	else
		{
		info_area_create(NULL);
		gtk_box_pack_start(GTK_BOX(mainwindow_hbox), tool_vbox, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(mainwindow_vbox), info_box, FALSE, FALSE, 0);
		}

	gtk_box_pack_end(GTK_BOX(mainwindow_hbox), image_window, TRUE, TRUE, 0);
	gtk_widget_show_all(image_window);
	
	GTK_WIDGET_SET_FLAGS(main_image->viewport, GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(main_image->viewport), "focus_in_event", GTK_SIGNAL_FUNC(image_focus_in_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(main_image->viewport), "focus_out_event", GTK_SIGNAL_FUNC(image_focus_out_cb), NULL);

	gtk_widget_show(mainwindow);

	if (save_window_positions)
		gdk_window_move(mainwindow->window, main_window_x, main_window_y);
}

