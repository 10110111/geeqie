/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

#define DEST_WIDTH 250
#define DEST_HEIGHT 100

typedef struct _Dest_Data Dest_Data;
struct _Dest_Data
{
	GtkWidget *d_clist;
	GtkWidget *f_clist;
	GtkWidget *entry;
	gchar *filter;
	gchar *path;
};

static void dest_free_data(GtkWidget *widget, gpointer data);
static gint dest_check_filter(gchar *filter, gchar *file);
static gint dest_sort_cb(void *a, void *b);
static void dest_populate(Dest_Data *dd, gchar *path);
static void dest_select_cb(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *bevent, gpointer data);

/*
 *-----------------------------------------------------------------------------
 * destination widget routines (private)
 *-----------------------------------------------------------------------------
 */ 

static void dest_free_data(GtkWidget *widget, gpointer data)
{
	Dest_Data *dd = data;
	g_free(dd->filter);
	g_free(dd->path);
	g_free(dd);
}

static gint dest_check_filter(gchar *filter, gchar *file)
{
	gchar *f_ptr = filter;
	gchar *file_ptr;
	gchar *strt_ptr;
	gint i;

	if (filter[0] == '*') return TRUE;
	while (f_ptr < filter + strlen(filter))
		{
		strt_ptr = f_ptr;
		i=0;
		while (*f_ptr != ';' && *f_ptr != '\0')
			{
			f_ptr++;
			i++;
			}
		f_ptr++;
		file_ptr = file + strlen(file) - i;
		if (!strncasecmp(file_ptr, strt_ptr, i)) return TRUE;
		}
	return FALSE;
}

static gint dest_sort_cb(void *a, void *b)
{
	return strcmp((gchar *)a, (gchar *)b);
}

static void dest_populate(Dest_Data *dd, gchar *path)
{
	DIR *dp;
	struct dirent *dir;
	struct stat ent_sbuf;
	gchar *buf[2];
	GList *path_list = NULL;
	GList *file_list = NULL;
	GList *list;

	buf[1] = NULL;

	if((dp = opendir(path))==NULL)
		{
		/* dir not found */
		return;
		}
	while ((dir = readdir(dp)) != NULL)
		{
		/* skips removed files */
		if (dir->d_ino > 0)
			{
			gchar *name = dir->d_name;
			gchar *filepath = g_strconcat(path, "/", name, NULL);
			if (stat(filepath,&ent_sbuf) >= 0 && S_ISDIR(ent_sbuf.st_mode))
				{
				path_list = g_list_prepend(path_list, g_strdup(name));
				}
			else if (dd->f_clist)
				{
				if (!dd->filter || (dd->filter && dest_check_filter(dd->filter, name)))
					file_list = g_list_prepend(file_list, g_strdup(name));
				}
			g_free(filepath);
			}
		}
	closedir(dp);

	path_list = g_list_sort(path_list, (GCompareFunc) dest_sort_cb);
	file_list = g_list_sort(file_list, (GCompareFunc) dest_sort_cb);

	gtk_clist_freeze(GTK_CLIST(dd->d_clist));
	gtk_clist_clear(GTK_CLIST(dd->d_clist));

	list = path_list;
	while (list)
		{
		gint row;
		gchar *filepath;
		if (strcmp(list->data, ".") == 0)
			{
			filepath = g_strdup(path);
			}
		else if (strcmp(list->data, "..") == 0)
			{
			gchar *p;
			filepath = g_strdup(path);
			p = filename_from_path(filepath);
			if (p - 1 != filepath) p--;
			p[0] = '\0';
			}
		else if (strcmp(path, "/") == 0)
			{
			filepath = g_strconcat("/", list->data, NULL);
			}
		else
			filepath = g_strconcat(path, "/", list->data, NULL);
		
		buf[0] = list->data;
		row = gtk_clist_append(GTK_CLIST(dd->d_clist),buf);
		gtk_clist_set_row_data_full(GTK_CLIST(dd->d_clist), row,
					    filepath, (GtkDestroyNotify) g_free);
		g_free(list->data);
		list = list->next;
		}

	g_list_free(path_list);

	gtk_clist_thaw(GTK_CLIST(dd->d_clist));

	if (dd->f_clist)
		{
		gtk_clist_freeze(GTK_CLIST(dd->f_clist));
		gtk_clist_clear(GTK_CLIST(dd->f_clist));

		list = file_list;
		while (list)
        	        {
			gint row;
			gchar *filepath;
			filepath = g_strconcat(path, "/", list->data, NULL);
		
			buf[0] = list->data;
			row = gtk_clist_append(GTK_CLIST(dd->f_clist),buf);
			gtk_clist_set_row_data_full(GTK_CLIST(dd->f_clist), row,
					    filepath, (GtkDestroyNotify) g_free);
			g_free(list->data);
			list = list->next;
			}

		g_list_free(file_list);

		gtk_clist_thaw(GTK_CLIST(dd->f_clist));
		}

	g_free(dd->path);
	dd->path = g_strdup(path);
}

static void dest_select_cb(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *bevent, gpointer data)
{
	Dest_Data *dd = data;
	gchar *path = g_strdup(gtk_clist_get_row_data(GTK_CLIST(clist), row));
	gtk_entry_set_text(GTK_ENTRY(dd->entry),path);

	if (clist == dd->d_clist) dest_populate(dd, path);
	g_free(path);
}

/*
 *-----------------------------------------------------------------------------
 * destination widget setup routines (public)
 *-----------------------------------------------------------------------------
 */ 

GtkWidget *destination_widget_new_with_files(gchar *path, gchar *filter, GtkWidget *entry)
{
	GtkWidget *vbox;
	Dest_Data *dd;
	GtkWidget *scrolled;

	dd = g_new0(Dest_Data, 1);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_widget_show(vbox);

	dd->entry = entry;
	gtk_object_set_data(GTK_OBJECT(dd->entry), "destination_data", dd);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	dd->d_clist=gtk_clist_new (1);
	gtk_clist_set_column_auto_resize(GTK_CLIST(dd->d_clist), 0, TRUE);
        gtk_signal_connect (GTK_OBJECT (dd->d_clist), "select_row",(GtkSignalFunc) dest_select_cb, dd);
	gtk_signal_connect(GTK_OBJECT(dd->d_clist), "destroy", (GtkSignalFunc) dest_free_data, dd);
	gtk_widget_set_usize(dd->d_clist, DEST_WIDTH, DEST_HEIGHT);
	gtk_container_add (GTK_CONTAINER (scrolled), dd->d_clist);
        gtk_widget_show (dd->d_clist);

	if (filter)
		{
		scrolled = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
		gtk_widget_show(scrolled);

		dd->f_clist=gtk_clist_new (1);
		gtk_clist_set_column_auto_resize(GTK_CLIST(dd->f_clist), 0, TRUE);
		gtk_widget_set_usize(dd->f_clist, DEST_WIDTH, DEST_HEIGHT);
	        gtk_signal_connect (GTK_OBJECT (dd->f_clist), "select_row",(GtkSignalFunc) dest_select_cb, dd);
		gtk_container_add (GTK_CONTAINER (scrolled), dd->f_clist);
	        gtk_widget_show (dd->f_clist);

		dd->filter = g_strdup(filter);
		}

	if (isdir(path))
		{
		dest_populate(dd, path);
		}
	else
		{
		gchar *buf = remove_level_from_path(path);
		if (isdir(buf))
			{
			dest_populate(dd, buf);
			}
		else
			{
			dest_populate(dd, homedir());
			}
		g_free(buf);
		}
	return vbox;
}

GtkWidget *destination_widget_new(gchar *path, GtkWidget *entry)
{
	return destination_widget_new_with_files(path, NULL, entry);
}

void destination_widget_sync_to_entry(GtkWidget *entry)
{
	Dest_Data *dd = gtk_object_get_data(GTK_OBJECT(entry), "destination_data");
	gchar *path;

	if (!dd) return;

	path = gtk_entry_get_text(GTK_ENTRY(entry));
	
	if (isdir(path) && strcmp(path, dd->path) != 0)
		{
		dest_populate(dd, path);
		}
	else
		{
		gchar *buf = remove_level_from_path(path);
		if (isdir(buf) && strcmp(buf, dd->path) != 0)
			{
			dest_populate(dd, buf);
			}
		g_free(buf);
		}
}

