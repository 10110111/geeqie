/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include "image.h"

enum {
	TARGET_URI_LIST,
	TARGET_TEXT_PLAIN
};

static GtkTargetEntry file_drag_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "text/plain", 0, TARGET_TEXT_PLAIN }
};
static gint n_file_drag_types = 2;

static GtkTargetEntry file_drop_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
static gint n_file_drop_types = 1;

static GList *get_uri_file_list(gchar *data);

static void image_get_dnd_data(GtkWidget *widget, GdkDragContext *context,
			       gint x, gint y,
			       GtkSelectionData *selection_data, guint info,
			       guint time, gpointer data);
static void image_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data);
static void image_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data);

static void file_list_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data);
static void file_list_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data);
static void file_list_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data);

static void dir_list_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data);
static void dir_clist_set_highlight(gint set);
static void dir_list_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data);
static void dir_list_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data);

static GList *get_uri_file_list(gchar *data)
{
	GList *list = NULL;
	gint b, e;

	b = e = 0;

	while (data[b] != '\0')
		{
		while (data[e] != '\r' && data[e] != '\n' && data[e] != '\0') e++;
		if (!strncmp(data + b, "file:", 5))
			{
			b += 5;
			list = g_list_append(list, g_strndup(data + b, e - b));
			}
		while (data[e] == '\r' || data[e] == '\n') e++;
		b = e;
		}

	return list;
}

/*
 *-----------------------------------------------------------------------------
 * image drag and drop routines
 *-----------------------------------------------------------------------------
 */ 

static void image_get_dnd_data(GtkWidget *widget, GdkDragContext *context,
			       gint x, gint y,
			       GtkSelectionData *selection_data, guint info,
			       guint time, gpointer data)
{
	ImageWindow *imd = data;

	if (info == TARGET_URI_LIST)
		{
		GList *list = get_uri_file_list(selection_data->data);
		if (list)
			{
			gchar *path;

			path = list->data;

			if (imd == normal_image)
				{
				if (isfile(path))
					{
					gint row;
					gchar *dir = remove_level_from_path(path);
					if (strcmp(dir, current_path) != 0)
						filelist_change_to(dir);
					g_free(dir);
	
					row = find_file_in_list(path);
					if (row == -1)
						image_change_to(path);
					else
						file_image_change_to(row);
					}
				else if (isdir(path))
					{
					filelist_change_to(path);
					image_change_to(NULL);
					}
				}
			else
				{
				if (isfile(path))
					{
					image_area_set_image(imd, path, get_default_zoom(imd));
					}
				}

			if (debug)
				{
				GList *work = list;
				while (work)
					{
					printf("dropped: %s\n", (gchar *)(work->data));
					work = work->next;
					}
				}

			g_list_foreach(list, (GFunc)g_free, NULL);
			g_list_free(list);
			}
		}
}

static void image_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
			       GtkSelectionData *selection_data, guint info,
			       guint time, gpointer data)
{
	ImageWindow *imd = data;
	gchar *path = image_area_get_path(imd);

	if (path)
		{
		gchar *text = NULL;
		switch (info)
			{
			case TARGET_URI_LIST:
				text = g_strconcat("file:", path, "\r\n", NULL);
				break;
			case TARGET_TEXT_PLAIN:
				text = g_strdup(path);
				break;
			}
		if (text)
			{
			gtk_selection_data_set (selection_data, selection_data->target,
						8, text, strlen(text));
			g_free(text);
			}
		}
	else
		{
		gtk_selection_data_set (selection_data, selection_data->target,
					8, NULL, 0);
		}
}

static void image_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	ImageWindow *imd = data;
	if (context->action == GDK_ACTION_MOVE)
		{
		gint row = find_file_in_list(image_area_get_path(imd));
		if (row < 0) return;
		if (image_get_path() && strcmp(image_get_path(), image_area_get_path(imd)) == 0)
			{
			if (row < file_count() - 1)
				{
				file_next_image();
				}
			else
				{
				file_prev_image();
				}
			}
		filelist_refresh();
		}
}

void image_dnd_init(ImageWindow *imd)
{
	gtk_drag_source_set(imd->viewport, GDK_BUTTON2_MASK,
				file_drag_types, n_file_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	gtk_signal_connect(GTK_OBJECT(imd->viewport), "drag_data_get",
				image_set_dnd_data, imd);
	gtk_signal_connect(GTK_OBJECT(imd->viewport), "drag_end",
				image_drag_end, imd);

	gtk_drag_dest_set(imd->viewport,
			  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
			  file_drop_types, n_file_drop_types,
                          GDK_ACTION_COPY);
	gtk_signal_connect(GTK_OBJECT(imd->viewport), "drag_data_received",
				image_get_dnd_data, imd);
}

/*
 *-----------------------------------------------------------------------------
 * file list drag and drop routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void file_list_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data)
{
	gchar *uri_text = NULL;
	gchar *leader = "file:";
	gchar *sep = "\r\n";
	gint total;
	gint leader_l;
	gint sep_l = strlen(sep);
	gchar *ptr;

	switch (info)
		{
		case TARGET_URI_LIST:
			leader_l = strlen(leader);
			break;
		case TARGET_TEXT_PLAIN:
			leader_l = 0;
			break;
		default:
			return;
			break;
		}
	
	if (file_clicked_is_selected())
		{
		GList *list;
		GList *work;

		list = file_get_selected_list();
		if (!list) return;
		work = list;
		total = 0;

		/* compute length */

		while (work)
			{
			gchar *name = work->data;
			total += leader_l + strlen(name) + sep_l;
			work = work->next;
			}

		/* create list */
		uri_text = g_malloc(total + 1);
		ptr = uri_text;

		work = list;
		while (work)
			{
			gchar *name = work->data;
			if (leader_l > 0)
				{
				strcpy(ptr, leader);
				ptr += leader_l;
				}
			strcpy(ptr, name);
			ptr += strlen(name);
			strcpy(ptr, sep);
			ptr += sep_l;
			work = work->next;
			}
		ptr[0] = '\0';
		free_selected_list(list);
		}
	else
		{
		gchar *path = file_clicked_get_path();
		if (!path) return;
		switch (info)
			{
			case TARGET_URI_LIST:
				uri_text = g_strconcat("file:", path, "\r\n", NULL);
				break;
			case TARGET_TEXT_PLAIN:
				uri_text = g_strdup(path);
				break;
			}
		total = strlen(uri_text);
		g_free(path);
		}

	if (debug) printf(uri_text);

	gtk_selection_data_set (selection_data, selection_data->target,
					8, uri_text, total);
	g_free(uri_text);

	file_clist_highlight_unset();
}

static void file_list_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	file_clist_highlight_set();
}

static void file_list_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	file_clist_highlight_unset();

	if (context->action == GDK_ACTION_MOVE)
		{
		filelist_refresh();
		}
}

/*
 *-----------------------------------------------------------------------------
 * directory list drag and drop routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void dir_list_set_dnd_data(GtkWidget *widget, GdkDragContext *context,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data)
{
	gint row = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(dir_clist)));

	if (row >= 0)
		{
		gchar *name = gtk_clist_get_row_data(GTK_CLIST(dir_clist), row);
		gchar *new_path;
		gchar *text = NULL;

		if (strcmp(name, ".") == 0)
			new_path = g_strdup(current_path);
		else if (strcmp(name, "..") == 0)
			new_path = remove_level_from_path(current_path);
		else
			{
			if (strcmp(current_path, "/") == 0)
				new_path = g_strconcat(current_path, name, NULL);
			else
				new_path = g_strconcat(current_path, "/", name, NULL);
			}

		
		switch (info)
			{
			case TARGET_URI_LIST:
				text = g_strconcat("file:", new_path, "\r\n", NULL);
				break;
			case TARGET_TEXT_PLAIN:
				text = g_strdup(new_path);
				break;
			}
		if (text)
			{
			gtk_selection_data_set (selection_data, selection_data->target,
					8, text, strlen(text));
			g_free(text);
			}
		g_free(new_path);
		}
}

static void dir_clist_set_highlight(gint set)
{
	gint row = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(dir_clist)));
	if (set)
		{
		gtk_clist_set_background(GTK_CLIST(dir_clist), row,
			&GTK_WIDGET (dir_clist)->style->bg[GTK_STATE_SELECTED]);
		gtk_clist_set_foreground(GTK_CLIST(dir_clist), row,
			&GTK_WIDGET (dir_clist)->style->fg[GTK_STATE_SELECTED]);
		}
	else
		{
		gtk_clist_set_background(GTK_CLIST(dir_clist), row, NULL);
		gtk_clist_set_foreground(GTK_CLIST(dir_clist), row, NULL);
		}
}

static void dir_list_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	dir_clist_set_highlight(TRUE);
}

static void dir_list_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	dir_clist_set_highlight(FALSE);

	if (context->action == GDK_ACTION_MOVE)
		{
		filelist_refresh();
		}
}

/*
 *-----------------------------------------------------------------------------
 * drag and drop initialization (public)
 *-----------------------------------------------------------------------------
 */ 

void init_dnd()
{
	/* dir clist */
	gtk_drag_source_set(dir_clist, GDK_BUTTON2_MASK,
				file_drag_types, n_file_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	gtk_signal_connect(GTK_OBJECT(dir_clist), "drag_begin",
				dir_list_drag_begin, NULL);
	gtk_signal_connect(GTK_OBJECT(dir_clist), "drag_data_get",
				dir_list_set_dnd_data, NULL);
	gtk_signal_connect(GTK_OBJECT(dir_clist), "drag_end",
				dir_list_drag_end, NULL);

	/* file clist */
	gtk_drag_source_set(file_clist, GDK_BUTTON2_MASK,
				file_drag_types, n_file_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	gtk_signal_connect(GTK_OBJECT(file_clist), "drag_begin",
				file_list_drag_begin, NULL);
	gtk_signal_connect(GTK_OBJECT(file_clist), "drag_data_get",
				file_list_set_dnd_data, NULL);
	gtk_signal_connect(GTK_OBJECT(file_clist), "drag_end",
				file_list_drag_end, NULL);

	/* image */
	image_dnd_init(main_image);
}


