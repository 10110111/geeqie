/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

static gint filelist_click_row = -1;

static void update_progressbar(gfloat val);

static gint file_is_hidden(gchar *name);
static gint file_is_in_filter(gchar *name);
static void add_to_filter(gchar *text, gint add);

static gint sort_list_cb(void *a, void *b);
static void filelist_read(gchar *path);

static gint file_find_closest_unaccounted(gint row, gint count, GList *ignore_list);

static void history_menu_select_cb(GtkWidget *widget, gpointer data);
static gchar *truncate_hist_text(gchar *t, gint l);
static void filelist_set_history(gchar *path);

/*
 *-----------------------------------------------------------------------------
 * file status information (private)
 *-----------------------------------------------------------------------------
 */

static void update_progressbar(gfloat val)
{
	gtk_progress_bar_update (GTK_PROGRESS_BAR(info_progress_bar), val);
}

void update_status_label(gchar *text)
{
	gchar *buf;
	gint count;
	gchar *ss = "";

	if (text)
		{
		gtk_label_set(GTK_LABEL(info_status), text);
		return;
		}

	if (slideshow_is_running()) ss = _(" Slideshow");

	count = file_selection_count();
	if (count > 0)
		buf = g_strdup_printf(_("%d files (%d)%s"), file_count(), count, ss);
	else
		buf = g_strdup_printf(_("%d files%s"), file_count(), ss);

	gtk_label_set(GTK_LABEL(info_status), buf);
	g_free(buf);
}

/*
 *-----------------------------------------------------------------------------
 * file filtering
 *-----------------------------------------------------------------------------
 */

static gint file_is_hidden(gchar *name)
{
	if (name[0] != '.') return FALSE;
	if (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) return FALSE;
	return TRUE;
}

static gint file_is_in_filter(gchar *name)
{
	GList *work;
	if (!filename_filter || file_filter_disable) return TRUE;

	work = filename_filter;
	while (work)
		{
		gchar *filter = work->data;
		gint lf = strlen(filter);
		gint ln = strlen(name);
		if (ln >= lf)
			{
			if (strncasecmp(name + ln - lf, filter, lf) == 0) return TRUE;
			}
		work = work->next;
		}

	return FALSE;
}

static void add_to_filter(gchar *text, gint add)
{
	if (add) filename_filter = g_list_append(filename_filter, g_strdup(text));
}

void rebuild_file_filter()
{
	if (filename_filter)
		{
		g_list_foreach(filename_filter,(GFunc)g_free,NULL);
		g_list_free(filename_filter);
		filename_filter = NULL;
		}

	add_to_filter(".jpg", filter_include_jpg);
	add_to_filter(".jpeg", filter_include_jpg);
	add_to_filter(".xpm", filter_include_xpm);
	add_to_filter(".tif", filter_include_tif);
	add_to_filter(".tiff", filter_include_tif);
	add_to_filter(".gif", filter_include_gif);
	add_to_filter(".png", filter_include_png);
	add_to_filter(".ppm", filter_include_ppm);
	add_to_filter(".pgm", filter_include_pgm);
	add_to_filter(".pcx", filter_include_pcx);
	add_to_filter(".bmp", filter_include_bmp);

	if (custom_filter)
		{
		gchar *buf = g_strdup(custom_filter);
		gchar *pos_ptr_b;
		gchar *pos_ptr_e = buf;
		while(pos_ptr_e[0] != '\0')
			{
			pos_ptr_b = pos_ptr_e;
			while (pos_ptr_e[0] != ';' && pos_ptr_e[0] != '\0') pos_ptr_e++;
			if (pos_ptr_e[0] == ';')
				{
				pos_ptr_e[0] = '\0';
				pos_ptr_e++;
				}
			add_to_filter(pos_ptr_b, TRUE);
			}
		g_free(buf);
		}
}

/*
 *-----------------------------------------------------------------------------
 * load file list (private)
 *-----------------------------------------------------------------------------
 */

static gint sort_list_cb(void *a, void *b)
{
	return strcmp((gchar *)a, (gchar *)b);
}

static void filelist_read(gchar *path)
{
	DIR *dp;
	struct dirent *dir;
	struct stat ent_sbuf;

	if((dp = opendir(path))==NULL)
		{
		/* dir not found */
		return;
		}
	
	g_list_foreach(dir_list,(GFunc)g_free,NULL);
	g_list_free(dir_list);
	dir_list = NULL;

	g_list_foreach(file_list,(GFunc)g_free,NULL);
	g_list_free(file_list);
	file_list = NULL;

	while ((dir = readdir(dp)) != NULL)
		{
		/* skips removed files */
		if (dir->d_ino > 0)
			{
			gchar *name = dir->d_name;
			if (show_dot_files || !file_is_hidden(name))
				{
				gchar *filepath = g_strconcat(path, "/", name, NULL);
				if (stat(filepath,&ent_sbuf) >= 0 && S_ISDIR(ent_sbuf.st_mode))
					{
					dir_list = g_list_prepend(dir_list, g_strdup(name));
					}
				else
					{
					if (file_is_in_filter(name))
						file_list = g_list_prepend(file_list, g_strdup(name));
					}
				g_free(filepath);
				}
			}
		}

	closedir(dp);

	dir_list = g_list_sort(dir_list, (GCompareFunc) sort_list_cb);
	file_list = g_list_sort(file_list, (GCompareFunc) sort_list_cb);
}

/*
 *-----------------------------------------------------------------------------
 * file list utilities to retrieve information (public)
 *-----------------------------------------------------------------------------
 */

gint file_count()
{
	return g_list_length(file_list);
}

gint file_selection_count()
{
	gint count = 0;
	GList *work = GTK_CLIST(file_clist)->selection;
	while(work)
		{
		count++;
		if (debug) printf("s = %d\n", GPOINTER_TO_INT(work->data));
		work = work->next;
		}

	if (debug) printf("files selected = %d\n", count);

	return count;
}

gint find_file_in_list(gchar *path)
{
	GList *work = file_list;
	gchar *buf;
	gchar *name;
	gint count = -1;

	if (!path) return -1;

	buf = remove_level_from_path(path);
	if (strcmp(buf, current_path) != 0)
		{
		g_free(buf);
		return -1;
		}
	g_free(buf);

	name = filename_from_path(path);
	while(work)
		{
		count++;
		if (strcmp(name, work->data) == 0) return count;
		work = work->next;
		}

	return -1;
}

gchar *file_get_path(gint row)
{
	gchar *path = NULL;
	gchar *name = gtk_clist_get_row_data(GTK_CLIST(file_clist), row);

	if (name) path = g_strconcat(current_path, "/", name, NULL);

	return path;
}

gint file_is_selected(gint row)
{
	GList *work = GTK_CLIST(file_clist)->selection;

	while(work)
		{
		if (GPOINTER_TO_INT(work->data) == row) return TRUE;
		work = work->next;
		}

	return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 * utilities to retrieve list of selected files (public)
 *-----------------------------------------------------------------------------
 */

GList *file_get_selected_list()
{
	GList *list = NULL;
	GList *work = GTK_CLIST(file_clist)->selection;

	while(work)
		{
		gchar *name = gtk_clist_get_row_data(GTK_CLIST(file_clist),
			GPOINTER_TO_INT(work->data));
		list = g_list_prepend(list, g_strconcat(current_path, "/", name, NULL));
		work = work->next;
		}

	list = g_list_reverse(list);

	return list;
}

void free_selected_list(GList *list)
{
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

gint file_clicked_is_selected()
{
	return file_is_selected(filelist_click_row);
}

gchar *file_clicked_get_path()
{
	return file_get_path(filelist_click_row);
}

/*
 *-----------------------------------------------------------------------------
 * image change routines
 *-----------------------------------------------------------------------------
 */

void file_image_change_to(gint row)
{
	gtk_clist_unselect_all(GTK_CLIST(file_clist));
	gtk_clist_select_row(GTK_CLIST(file_clist), row, -1);
	if (gtk_clist_row_is_visible(GTK_CLIST(file_clist), row) != GTK_VISIBILITY_FULL)
		{
		gtk_clist_moveto(GTK_CLIST(file_clist), row, -1, 0.5, 0.0);
		}
}

void file_next_image()
{
	gint current;
	gint total;

	if (slideshow_is_running())
		{
		slideshow_next();
		return;
		}

	current = find_file_in_list(image_get_path());
	total = file_count();

	if (current >= 0)
		{
		if (current < total - 1)
			{
			file_image_change_to(current + 1);
			}
		}
	else
		{
		file_image_change_to(0);
		}
}

void file_prev_image()
{
	gint current;

	if (slideshow_is_running())
		{
		slideshow_prev();
		return;
		}

	current = find_file_in_list(image_get_path());

	if (current >= 0)
		{
		if (current > 0)
			{
			file_image_change_to(current - 1);
			}
		}
	else
		{
		file_image_change_to(file_count() - 1);
		}
}

void file_first_image()
{
	gint current = find_file_in_list(image_get_path());
	if (current != 0 && file_count() > 0)
		{
		file_image_change_to(0);
		}
}

void file_last_image()
{
	gint current = find_file_in_list(image_get_path());
	gint count = file_count();
	if (current != count - 1 && count > 0)
		{
		file_image_change_to(count - 1);
		}
}

/*
 *-----------------------------------------------------------------------------
 * file delete/rename update routines
 *-----------------------------------------------------------------------------
 */

static gint file_find_closest_unaccounted(gint row, gint count, GList *ignore_list)
{
	GList *list = NULL;
	GList *work;
	gint rev = row - 1;
	row ++;

	work = ignore_list;
	while(work)
		{
		gint f = find_file_in_list(work->data);
		if (f >= 0) list = g_list_append(list, GINT_TO_POINTER(f));
		work = work->next;
		}

	while(list)
		{
		gint c = TRUE;
		work = list;
		while(work && c)
			{
			gpointer p = work->data;
			work = work->next;
			if (row == GPOINTER_TO_INT(p))
				{
				row++;
				c = FALSE;
				}
			if (rev == GPOINTER_TO_INT(p))
				{
				rev--;
				c = FALSE;
				}
			if (!c) list = g_list_remove(list, p);
			}
		if (c && list)
			{
			g_list_free(list);
			list = NULL;
			}
		}
	if (row > count - 1)
		{
		if (rev < 0)
			return -1;
		else
			return rev;
		}
	else
		{
		return row;
		}
}

void file_is_gone(gchar *path, GList *ignore_list)
{
	GList *list;
	gchar *name;
	gint row;
	gint new_row = -1;
	row = find_file_in_list(path);
	if (row < 0) return;

	if (file_is_selected(row) /* && file_selection_count() == 1 */)
		{
		gint n = file_count();
		if (ignore_list)
			{
			new_row = file_find_closest_unaccounted(row, n, ignore_list);
			if (debug) printf("row = %d, closest is %d\n", row, new_row);
			}
		else
			{
			if (row + 1 < n)
				{
				new_row = row + 1;
				}
			else if (row > 0)
				{
				new_row = row - 1;
				}
			}
		gtk_clist_unselect_all(GTK_CLIST(file_clist));
		if (new_row >= 0)
			{
			gtk_clist_select_row(GTK_CLIST(file_clist), new_row, -1);
			file_image_change_to(new_row);
			}
		else
			{
			image_change_to(NULL);
			}
		}

	gtk_clist_remove(GTK_CLIST(file_clist), row);
	list = g_list_nth(file_list, row);
	name = list->data;
	file_list = g_list_remove(file_list, name);
	g_free(name);
	update_status_label(NULL);
}

void file_is_renamed(gchar *source, gchar *dest)
{
	gint row;
	gchar *source_base;
	gchar *dest_base;

	if (image_get_path() && !strcmp(source, image_get_path()))
		{
		image_set_path(dest);
		}

	row = find_file_in_list(source);
	if (row < 0) return;

	source_base = remove_level_from_path(source);
	dest_base = remove_level_from_path(dest);

	if (strcmp(source_base, dest_base) == 0)
		{
		gchar *name;
		gint n;
		GList *work = g_list_nth(file_list, row);
		name = work->data;
		file_list = g_list_remove(file_list, name);
		g_free(name);
		name = g_strdup(filename_from_path(dest));
		file_list = g_list_insert_sorted(file_list, name, (GCompareFunc) sort_list_cb);
		n = g_list_index(file_list, name);

		if (gtk_clist_get_cell_type(GTK_CLIST(file_clist), row, 0) != GTK_CELL_PIXTEXT)
			{
			gtk_clist_set_text (GTK_CLIST(file_clist), row, 0, name);
			}
		else
			{
			guint8 spacing = 0;
			GdkPixmap *pixmap = NULL;
			GdkBitmap *mask = NULL;
			gtk_clist_get_pixtext(GTK_CLIST(file_clist), row, 0,
				NULL, &spacing, &pixmap, &mask);
			gtk_clist_set_pixtext(GTK_CLIST(file_clist), row, 0,
				name, spacing, pixmap, mask);
			}

		gtk_clist_set_row_data(GTK_CLIST(file_clist), row, name);
		gtk_clist_row_move(GTK_CLIST(file_clist), row, n);
		}
	else
		{
		GList *work = g_list_nth(file_list, row);
		gchar *name = work->data;
		file_list = g_list_remove(file_list, name);
		gtk_clist_remove(GTK_CLIST(file_clist), row);
		g_free(name);
		update_status_label(NULL);
		}

	g_free(source_base);
	g_free(dest_base);
	
}

/*
 *-----------------------------------------------------------------------------
 * directory list callbacks
 *-----------------------------------------------------------------------------
 */

void dir_select_cb(GtkWidget *widget, gint row, gint col,
		   GdkEvent *event, gpointer data)
{
	gchar *name;
	gchar *new_path;
	name = gtk_clist_get_row_data (GTK_CLIST(dir_clist), row);
	if (strcmp(name, ".") == 0)
		{
		new_path = g_strdup(current_path);
		}
	else if (strcmp(name, "..") == 0)
		{
		new_path = remove_level_from_path(current_path);
		}
	else
		{
		if (strcmp(current_path, "/") == 0)
			new_path = g_strconcat(current_path, name, NULL);
		else
			new_path = g_strconcat(current_path, "/", name, NULL);
		}
	filelist_change_to(new_path);
	g_free(new_path);
}

void dir_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	gint row = -1;
	gint col = -1;

	gtk_clist_get_selection_info (GTK_CLIST (widget), bevent->x, bevent->y, &row, &col);

	if (bevent->button == 2)
		{
		gtk_object_set_user_data(GTK_OBJECT(dir_clist), GINT_TO_POINTER(row));
		}
}

/*
 *-----------------------------------------------------------------------------
 * file list callbacks
 *-----------------------------------------------------------------------------
 */

void file_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	gint row = -1;
	gint col = -1;

	gtk_clist_get_selection_info (GTK_CLIST (widget), bevent->x, bevent->y, &row, &col);
	if (row == -1 || col == -1)
		{
		filelist_click_row = -1;
		return;
		}

	filelist_click_row = row;

	if (bevent->button == 3)
		{
		file_clist_highlight_set();
		gtk_menu_popup (GTK_MENU(menu_file_popup), NULL, NULL, NULL, NULL,
				bevent->button, bevent->time);
		}
}

void file_select_cb(GtkWidget *widget, gint row, gint col,
		   GdkEvent *event, gpointer data)
{
	gchar *name;
	gchar *path;

	if (file_selection_count() != 1)
		{
		update_status_label(NULL);
		return;
		}

	name = gtk_clist_get_row_data(GTK_CLIST(file_clist), row);
	path = g_strconcat(current_path, "/", name, NULL);
	image_change_to(path);
	update_status_label(NULL);
	g_free(path);
}

void file_unselect_cb(GtkWidget *widget, gint row, gint col,
		   GdkEvent *event, gpointer data)
{
#if 0
	gchar *name;
	gchar *path;

	name = gtk_clist_get_row_data(GTK_CLIST(file_clist), row);
	path = g_strconcat(current_path, "/", name, NULL);

	if (strcmp(path, image_get_path()) == 0)
		{
		if (file_selection_count() > 0 && !file_is_selected(find_file_in_list(image_get_path())) )
			{
			gint new_row = GPOINTER_TO_INT(GTK_CLIST(file_clist)->selection->data);
			gchar *new_name = gtk_clist_get_row_data(GTK_CLIST(file_clist), new_row);
			gchar *new_path = g_strconcat(current_path, "/", new_name, NULL);
			image_change_to(new_path);
			g_free(new_path);
			}
		}
	g_free(path);
#endif
	update_status_label(NULL);
}

/*
 *-----------------------------------------------------------------------------
 * file list highlight utils
 *-----------------------------------------------------------------------------
 */

void file_clist_highlight_set()
{
	if (file_clicked_is_selected()) return;

	gtk_clist_set_background(GTK_CLIST(file_clist), filelist_click_row,
		&GTK_WIDGET (file_clist)->style->bg[GTK_STATE_PRELIGHT]);
	gtk_clist_set_foreground(GTK_CLIST(file_clist), filelist_click_row,
		&GTK_WIDGET (file_clist)->style->fg[GTK_STATE_PRELIGHT]);
}

void file_clist_highlight_unset()
{
	if (file_clicked_is_selected()) return;

	gtk_clist_set_background(GTK_CLIST(file_clist), filelist_click_row, NULL);
	gtk_clist_set_foreground(GTK_CLIST(file_clist), filelist_click_row, NULL);
}

/*
 *-----------------------------------------------------------------------------
 * path entry and history menu
 *-----------------------------------------------------------------------------
 */

void path_entry_tab_cb(gchar *newdir, gpointer data)
{
	gchar *new_path;
	gchar *buf;
	gint found = FALSE;

	new_path = g_strdup(newdir);
	parse_out_relatives(new_path);
	buf = remove_level_from_path(new_path);

	if (buf && current_path && strcmp(buf, current_path) == 0)
		{
		GList *work;
		gchar *part;

		part = filename_from_path(new_path);
		work = file_list;

		while(part && work)
			{
			gchar *name = work->data;
			work = work->next;

			if (strncmp(part, name, strlen(part)) == 0)
				{
				gint row = g_list_index(file_list, name);
				if (!gtk_clist_row_is_visible(GTK_CLIST(file_clist), row) != GTK_VISIBILITY_FULL)
					{
					gtk_clist_moveto(GTK_CLIST(file_clist), row, -1, 0.5, 0.0);
					}
				found = TRUE;
				break;
				}
			}
		}

	if (!found && new_path && current_path &&
	    strcmp(new_path, current_path) != 0 && isdir(new_path))
		{
		filelist_change_to(new_path);
		/* we are doing tab completion, add '/' back */
		gtk_entry_append_text(GTK_ENTRY(path_entry), "/");
		}

	g_free(buf);
	g_free(new_path);
}

void path_entry_cb(gchar *newdir, gpointer data)
{
	gchar *new_path = g_strdup(newdir);
	parse_out_relatives(new_path);
	if (isdir(new_path))
		filelist_change_to(new_path);
	else if (isfile(new_path))
		{
		gchar *path = remove_level_from_path(new_path);
		filelist_change_to(path);
		g_free(path);
		image_change_to(new_path);
		}
	g_free(new_path);
}

static void history_menu_select_cb(GtkWidget *widget, gpointer data)
{
	gchar *new_path = data;
	filelist_change_to(new_path);
}

static gchar *truncate_hist_text(gchar *t, gint l)
{
	gchar *tp;
	gchar *tbuf;
	if (l >= strlen(t)) return g_strdup(t);
	tp = t + strlen(t) - l;
	while (tp[0] != '/' && tp < t + strlen(t)) tp++;
                /* this checks to see if directory name is longer than l, if so
                 * reset the length of name to l, it's better to have a partial
                 * name than no name at all.
		 */
	if (tp >= t + strlen(t)) tp = t + strlen(t) - l;
	tbuf = g_strconcat("/...", tp, NULL);
	return tbuf;
}

static void filelist_set_history(gchar *path)
{
	static GList *history_list = NULL;
	gchar *buf;
	gchar *buf_ptr;
	GtkWidget *menu;
	GtkWidget *item;

	if (!path) return;

	gtk_entry_set_text(GTK_ENTRY(path_entry), current_path);

	if (history_list)
                {
                g_list_foreach(history_list, (GFunc)g_free, NULL);
                g_list_free(history_list);
                history_list = NULL;
                }

	menu = gtk_menu_new();

	buf = g_strdup(path);
	buf_ptr = buf + strlen(buf) - 1 ;
	while (buf_ptr > buf)
		{
		gchar *full_path;
		gchar *truncated;
		truncated = truncate_hist_text(buf, 32);

		full_path = g_strdup(buf);
		history_list = g_list_append(history_list, full_path);

		item = gtk_menu_item_new_with_label (truncated);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
			(GtkSignalFunc) history_menu_select_cb, full_path);

		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);

		g_free(truncated);

		while (buf_ptr[0] != '/' && buf_ptr > buf) buf_ptr--;
		buf_ptr[0] = '\0';
		}
	g_free(buf);

	item = gtk_menu_item_new_with_label ("/");

	gtk_signal_connect (GTK_OBJECT (item), "activate",
		(GtkSignalFunc) history_menu_select_cb, "/");

	gtk_menu_append (GTK_MENU (menu), item);
	gtk_widget_show (item);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(history_menu), menu);
}

/*
 *-----------------------------------------------------------------------------
 * list update routines (public)
 *-----------------------------------------------------------------------------
 */

static gint thumbs_running = 0;

void interrupt_thumbs()
{
        if (thumbs_running > 0) thumbs_running ++;
}

void filelist_populate_clist()
{
	GList *work;
	gint width;
	gint tmp_width;
	gint row;
	gchar *image_name = NULL;
	gchar *buf;

	gint row_p = 0;
	gchar *text;
	guint8 spacing;
	GdkPixmap *nopixmap;
	GdkBitmap *nomask;

	interrupt_thumbs();

	filelist_set_history(current_path);

	gtk_clist_freeze (GTK_CLIST (dir_clist));
	gtk_clist_clear (GTK_CLIST (dir_clist));

	width = 0;
	work = dir_list;
	while(work)
		{
		gchar *buf[2];
		buf[0] = work->data;
		buf[1] = NULL;
		row = gtk_clist_append(GTK_CLIST(dir_clist), buf);
		gtk_clist_set_row_data (GTK_CLIST(dir_clist), row, work->data);
		tmp_width = gdk_string_width(dir_clist->style->font, buf[0]);
		if (tmp_width > width) width = tmp_width;
		work = work->next;
		}

	gtk_clist_set_column_width(GTK_CLIST(dir_clist), 0, width);
	gtk_clist_thaw(GTK_CLIST (dir_clist));

	buf = remove_level_from_path(image_get_path());
	if (buf && strcmp(buf, current_path) == 0)
		{
		image_name = image_get_name();
		}
	g_free(buf);

	gtk_clist_freeze (GTK_CLIST (file_clist));

	if (!thumbnails_enabled)
		{
		gtk_clist_set_row_height (GTK_CLIST(file_clist),
			GTK_WIDGET(file_clist)->style->font->ascent +
			GTK_WIDGET(file_clist)->style->font->descent + 1);
		}
	else
		{
		gtk_clist_set_row_height (GTK_CLIST(file_clist), thumb_max_height + 2);
		maintain_thumbnail_dir(current_path, FALSE);
		}

	width = 0;
	work = file_list;

	while(work)
		{
		gint has_pixmap;
		gint match;
		gchar *name = work->data;
		gint done = FALSE;

		while (!done)
			{
			if (GTK_CLIST(file_clist)->rows > row_p)
				{
				if (gtk_clist_get_cell_type(GTK_CLIST(file_clist),row_p, 0) == GTK_CELL_PIXTEXT)
					{
					gtk_clist_get_pixtext(GTK_CLIST(file_clist), row_p, 0, &text, &spacing, &nopixmap, &nomask);
					has_pixmap = TRUE;
					}
				else
					{
					gtk_clist_get_text(GTK_CLIST(file_clist), row_p, 0, &text);
					has_pixmap = FALSE;
					}
				match = strcmp(name, text);
				}
			else
				{
				match = -1;
				}

			if (match < 0)
				{
				gchar *buf[2];
				buf[0] = name;
				buf[1] = NULL;
				row = gtk_clist_insert(GTK_CLIST(file_clist), row_p, buf);
				gtk_clist_set_row_data (GTK_CLIST(file_clist), row, name);
				if (thumbnails_enabled)
					gtk_clist_set_shift(GTK_CLIST(file_clist), row, 0, 0, 5 + thumb_max_width);
				done = TRUE;
				if (image_name && strcmp(name, image_name) == 0)
					gtk_clist_select_row(GTK_CLIST(file_clist), row, 0);
				}
			else if (match > 0)
				{
				gtk_clist_remove(GTK_CLIST(file_clist), row_p);
				}
			else
				{
				if (thumbnails_enabled && !has_pixmap)
					gtk_clist_set_shift(GTK_CLIST(file_clist), row_p, 0, 0, 5 + thumb_max_width);
				if (!thumbnails_enabled/* && has_pixmap*/)
					{
					gtk_clist_set_text(GTK_CLIST(file_clist), row_p, 0, name);
					gtk_clist_set_shift(GTK_CLIST(file_clist), row_p, 0, 0, 0);
					}
				gtk_clist_set_row_data (GTK_CLIST(file_clist), row_p, name);
				done = TRUE;
				}
			}
		row_p++;

		if (thumbnails_enabled)
			tmp_width = gdk_string_width(file_clist->style->font, name) + thumb_max_width + 5;
		else
			tmp_width = gdk_string_width(file_clist->style->font, name);
		if (tmp_width > width) width = tmp_width;
		work = work->next;
		}

	while (GTK_CLIST(file_clist)->rows > row_p)
		gtk_clist_remove(GTK_CLIST(file_clist), row_p);

	gtk_clist_set_column_width(GTK_CLIST(file_clist), 0, width);
	gtk_clist_thaw(GTK_CLIST (file_clist));

	if (thumbnails_enabled)
		{
		GList *done_list = NULL;
		gint past_run;
		gint finished = FALSE;
		gint j;
		gint count = 0;
		update_status_label(_("Loading thumbs..."));

		for (j = 0; j < GTK_CLIST(file_clist)->rows; j++)
			{
			done_list = g_list_prepend(done_list, GINT_TO_POINTER(FALSE));
			}

		/* load thumbs */

		while (!finished && done_list)
			{
			gint p = -1;
			gint r = -1;
			gint c = -1;
			gtk_clist_get_selection_info (GTK_CLIST(file_clist), 1, 1, &r, &c);
			if (r != -1)
				{
				work = g_list_nth(done_list, r);
				while (work)
					{
					if (gtk_clist_row_is_visible(GTK_CLIST(file_clist), r))
						{
						if (!GPOINTER_TO_INT(work->data))
							{
							work->data = GINT_TO_POINTER(TRUE);
							p = r;
							work = NULL;
							}
						else
							{
							r++;
							work = work->next;
							}
						}
					else
						{
						work = NULL;
						}
					}
				}
			if (p == -1)
				{
				work = done_list;
				r = 0;
				while(work && p == -1)
					{
					if (!GPOINTER_TO_INT(work->data))
						{
						p = r;
						work->data = GINT_TO_POINTER(TRUE);
						}
					else
						{
						r++;
						work = work->next;
						if (!work) finished = TRUE;
						}
					}
				}

			count++;

			if (!finished && gtk_clist_get_cell_type(GTK_CLIST(file_clist), p, 0) != GTK_CELL_PIXTEXT)
				{
				GdkPixmap *pixmap = NULL;
				GdkBitmap *mask = NULL;
				gchar *name;
				gchar *path;

				thumbs_running ++;
				past_run = thumbs_running;
				while(gtk_events_pending()) gtk_main_iteration();
				if (thumbs_running > past_run)
					{
					thumbs_running -= 2;
					update_progressbar(0.0);
					update_status_label(NULL);
					g_list_free(done_list);
					return;
					}
				thumbs_running --;

				name = gtk_clist_get_row_data(GTK_CLIST(file_clist), p);
				path = g_strconcat (current_path, "/", name, NULL);
				spacing = create_thumbnail(path, &pixmap, &mask);
				g_free(path);
				gtk_clist_set_pixtext (GTK_CLIST(file_clist), p, 0, name, spacing + 5, pixmap, mask);
				gtk_clist_set_shift(GTK_CLIST(file_clist), p, 0, 0, 0);

				update_progressbar((gfloat)(count) / GTK_CLIST(file_clist)->rows);
				}
			}
		update_progressbar(0.0);
		g_list_free(done_list);
		}

	update_status_label(NULL);
}

void filelist_refresh()
{
	filelist_read(current_path);
	filelist_populate_clist();
	filelist_click_row = -1;
}

void filelist_change_to(gchar *path)
{
	if (!isdir(path)) return;

	g_free(current_path);
	current_path = g_strdup(path);

	filelist_refresh();
}
