/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include <gdk/gdkkeysyms.h> /* for key values */

#include "tabcomp.xpm"

/* ----------------------------------------------------------------
   Tab completion routines, can be connected to any gtkentry widget
   using the tab_completion_add_to_entry() function.
   Use remove_trailing_slash() to strip the trailing '/'.
   ----------------------------------------------------------------*/

typedef struct _TabCompData TabCompData;
struct _TabCompData
{
	GtkWidget *entry;
	gchar *dir_path;
	GList *file_list;
	void (*enter_func)(gchar *, gpointer);
	void (*tab_func)(gchar *, gpointer);
	gpointer enter_data;
	gpointer tab_data;

	GtkWidget *combo;
	gint has_history;
	gchar *history_key;
	gint history_levels;
};

typedef struct _HistoryData HistoryData;
struct _HistoryData
{
	gchar *key;
	GList *list;
};

static GList *history_list = NULL;

static void tab_completion_free_list(TabCompData *td);
static void tab_completion_read_dir(TabCompData *td, gchar *path);
static void tab_completion_destroy(GtkWidget *widget, gpointer data);
static void tab_completion_emit_enter_signal(TabCompData *td);
static void tab_completion_emit_tab_signal(TabCompData *td);
static gint tab_completion_do(TabCompData *td);
static gint tab_completion_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void tab_completion_button_pressed(GtkWidget *widget, gpointer data);
static GtkWidget *tab_completion_create_complete_button(GtkWidget *window, GtkWidget *entry);

static void history_list_free(HistoryData *hd);
static HistoryData *history_list_find_by_key(const gchar* key);
static gchar *history_list_find_last_path_by_key(const gchar* key);
static void history_list_free_key(const gchar *key);
static void history_list_add_to_key(const gchar *key, const gchar *path, gint max);

static void history_list_free(HistoryData *hd)
{
	GList *work;

	if (!hd) return;

	work = hd->list;
	while(work)
		{
		g_free(work->data);
		work = work->next;
		}

	g_free(hd->key);
	g_free(hd);
}

static HistoryData *history_list_find_by_key(const gchar* key)
{
	GList *work = history_list;
	while(work)
		{
		HistoryData *hd = work->data;
		if (strcmp(hd->key, key) == 0) return hd;
		work = work->next;
		}
	return NULL;
}

static gchar *history_list_find_last_path_by_key(const gchar* key)
{
	HistoryData *hd;
	hd = history_list_find_by_key(key);
	if (!hd || !hd->list) return NULL;

	return hd->list->data;
}

static void history_list_free_key(const gchar *key)
{
	HistoryData *hd;
	hd = history_list_find_by_key(key);
	if (!hd) return;

	history_list = g_list_remove(history_list, hd);
	history_list_free(hd);
}

static void history_list_add_to_key(const gchar *key, const gchar *path, gint max)
{
	HistoryData *hd;
	GList *work;

	hd = history_list_find_by_key(key);
	if (!hd)
		{
		hd = g_new(HistoryData, 1);
		hd->key = g_strdup(key);
		hd->list = NULL;
		history_list = g_list_prepend(history_list, hd);
		}

	/* if already in the list, simply move it to the top */
	work = hd->list;
	while(work)
		{
		gchar *buf = work->data;
		work = work->next;
		if (strcmp(buf, path) == 0)
			{
			hd->list = g_list_remove(hd->list, buf);
			hd->list = g_list_prepend(hd->list, buf);
			return;
			}
		}

	hd->list = g_list_prepend(hd->list, g_strdup(path));

	if (max > 0)
		{
		while(hd->list && g_list_length(hd->list) > max)
			{
			GList *work = g_list_last(hd->list);
			gchar *buf = work->data;
			hd->list = g_list_remove(hd->list, buf);
			g_free(buf);
			}
		}
}

static void tab_completion_free_list(TabCompData *td)
{
	GList *list;

	g_free(td->dir_path);
	td->dir_path = NULL;

	list = td->file_list;

	while(list)
		{
		g_free(list->data);
		list = list->next;
		}

	g_list_free(td->file_list);
	td->file_list = NULL;
}

static void tab_completion_read_dir(TabCompData *td, gchar *path)
{
        DIR *dp;
        struct dirent *dir;
        GList *list = NULL;

	tab_completion_free_list(td);

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
			if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
				{
				list = g_list_prepend(list, g_strdup(name));
				}
                        }
		}
        closedir(dp);

	td->dir_path = g_strdup(path);
	td->file_list = list;
}

static void tab_completion_destroy(GtkWidget *widget, gpointer data)
{
	TabCompData *td = data;
	tab_completion_free_list(td);
	g_free(td->history_key);
	g_free(td);
}

static void tab_completion_emit_enter_signal(TabCompData *td)
{
	gchar *text;
	if (!td->enter_func) return;

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(td->entry)));

	if (text[0] == '~')
		{
		gchar *t = text;
		text = g_strconcat(homedir(), t + 1, NULL);
		g_free(t);
		}

	td->enter_func(text, td->enter_data);
	g_free(text);
}

static void tab_completion_emit_tab_signal(TabCompData *td)
{
	gchar *text;
	if (!td->tab_func) return;

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(td->entry)));

	if (text[0] == '~')
		{
		gchar *t = text;
		text = g_strconcat(homedir(), t + 1, NULL);
		g_free(t);
		}

	td->tab_func(text, td->tab_data);
	g_free(text);
}

static gint tab_completion_do(TabCompData *td)
{
	gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(td->entry));
	gchar *entry_file;
	gchar *entry_dir;
	gchar *ptr;
	gint home_exp = FALSE;

	/* home dir expansion */
	if (entry_text[0] == '~')
		{
		entry_dir = g_strconcat(homedir(), entry_text + 1, NULL);
		home_exp = TRUE;
		}
	else
		{
		entry_dir = g_strdup(entry_text);
		}

	entry_file = filename_from_path(entry_text);

	if (isfile(entry_dir))
		{
		if (home_exp)
			{
			gtk_entry_set_text(GTK_ENTRY(td->entry), entry_dir);
			gtk_entry_set_position(GTK_ENTRY(td->entry), strlen(entry_dir));
			}
		g_free(entry_dir);
		return home_exp;
		}
	if (isdir(entry_dir) && strcmp(entry_file, ".") != 0 && strcmp(entry_file, "..") != 0)
		{
		ptr = entry_dir + strlen(entry_dir) - 1;
		if (ptr[0] == '/')
			{
			if (home_exp)
				{
				gtk_entry_set_text(GTK_ENTRY(td->entry), entry_dir);
				gtk_entry_set_position(GTK_ENTRY(td->entry), strlen(entry_dir));
				}
			g_free(entry_dir);
			return home_exp;
			}
		else
			{
			gchar *buf = g_strconcat(entry_dir, "/", NULL);
			gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
			gtk_entry_set_position(GTK_ENTRY(td->entry), strlen(buf));
			g_free(buf);
			g_free(entry_dir);
			return TRUE;
			}
		}

	ptr = filename_from_path(entry_dir);
	if (ptr > entry_dir) ptr--;
	ptr[0] = '\0';

	if (strlen(entry_dir) == 0)
		{
		g_free(entry_dir);
		entry_dir = g_strdup("/");
		}

	if (isdir(entry_dir))
		{
		GList *list;
		GList *poss = NULL;
		gint l = strlen(entry_file);

		if (!td->dir_path || !td->file_list || strcmp(td->dir_path, entry_dir) != 0)
			{
			tab_completion_read_dir(td, entry_dir);
			}

		if (strcmp(entry_dir, "/") == 0) entry_dir[0] = '\0';

		list = td->file_list;
		while(list)
			{
			gchar *file = list->data;
			if (strncmp(entry_file, file, l) == 0)
				{
				poss = g_list_prepend(poss, file);
				}
			list = list->next;
			}

		if (poss)
			{
			if (!poss->next)
				{
				gchar *file = poss->data;
				gchar *buf;

				buf = g_strconcat(entry_dir, "/", file, NULL);

				if (isdir(buf))
					{
					g_free(buf);
					buf = g_strconcat(entry_dir, "/", file, "/", NULL);
					}
				gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
				gtk_entry_set_position(GTK_ENTRY(td->entry), strlen(buf));
				g_free(buf);
				g_list_free(poss);
				g_free(entry_dir);
				return TRUE;
				}
			else
				{
				gint c = strlen(entry_file);
				gint done = FALSE;
				gchar *test_file = poss->data;

				while (!done)
					{
					list = poss;
					if (!list) done = TRUE;
					while(list && !done)
						{
						gchar *file = list->data;
						if (strlen(file) < c || strncmp(test_file, file, c) != 0)
							{
							done = TRUE;
							}
						list = list->next;
						}
					c++;
					}
				c -= 2;
				if (c > 0)
					{
					gchar *file;
					gchar *buf;
					file = g_strdup(test_file);
					file[c] = '\0';
					buf = g_strconcat(entry_dir, "/", file, NULL);
					gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
					gtk_entry_set_position(GTK_ENTRY(td->entry), strlen(buf));
					g_free(file);
					g_free(buf);
					g_list_free(poss);
					g_free(entry_dir);
					return TRUE;
					}
				}
			g_list_free(poss);
			}
		}

	g_free(entry_dir);

	return FALSE;
}

static gint tab_completion_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	TabCompData *td = data;
	gint stop_signal = FALSE;

	switch (event->keyval)
		{
                case GDK_Tab:
			if (tab_completion_do(td))
				{
				tab_completion_emit_tab_signal(td);
				}
			stop_signal = TRUE;
			break;
		case GDK_Return:
			tab_completion_emit_enter_signal(td);
			stop_signal = TRUE;
			break;
		default:
			break;
		}

	if (stop_signal)
		{
		if (stop_signal) gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
		return TRUE;
		}

	return FALSE;
}

static void tab_completion_button_pressed(GtkWidget *widget, gpointer data)
{
	TabCompData *td;
	GtkWidget *entry = data;

	td = gtk_object_get_data(GTK_OBJECT(entry), "tab_completion_data");

	if (!td) return;

	if (!GTK_WIDGET_HAS_FOCUS(entry))
		{
		gtk_widget_grab_focus(entry);
		}

	if (tab_completion_do(td))
		{
		tab_completion_emit_tab_signal(td);
		}
}

static GtkWidget *tab_completion_create_complete_button(GtkWidget *window, GtkWidget *entry)
{
	GtkWidget *button;
	GtkWidget *icon;
	GdkPixmap *pixmap = NULL;
	GdkBitmap *mask = NULL;
	GtkStyle *style;

	style = gtk_widget_get_style(window);
	pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
		&style->bg[GTK_STATE_NORMAL], (gchar **)tabcomp_xpm);

	button = gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc) tab_completion_button_pressed, entry);
	
	icon = gtk_pixmap_new(pixmap, mask);
	gtk_container_add(GTK_CONTAINER(button), icon);
	gtk_widget_show(icon);

	return button;
}

/*
 *----------------------------------------------------------------------------
 * public interface
 *----------------------------------------------------------------------------
 */

GtkWidget *tab_completion_new_with_history(GtkWidget **entry, GtkWidget *window, gchar *text,
					   const gchar *history_key, gint max_levels,
					   void (*enter_func)(gchar *, gpointer), gpointer data)
{
	GtkWidget *combo;
	GtkWidget *button;
	HistoryData *hd;
	TabCompData *td;

	combo = gtk_combo_new();
	gtk_combo_set_use_arrows(GTK_COMBO(combo), FALSE);

	button = tab_completion_create_complete_button(window, GTK_COMBO(combo)->entry);
	gtk_box_pack_start(GTK_BOX(combo), button, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(combo), button, 1);
	gtk_widget_show(button);
	
	tab_completion_add_to_entry(GTK_COMBO(combo)->entry, enter_func, data);

	td = gtk_object_get_data(GTK_OBJECT(GTK_COMBO(combo)->entry), "tab_completion_data");
	if (!td) return; /* this should never happen! */

	td->combo = combo;
	td->has_history = TRUE;
	td->history_key = g_strdup(history_key);
	td->history_levels = max_levels;

	hd = history_list_find_by_key(td->history_key);
	if (hd && hd->list)
		{
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), hd->list);
		}

	if (text) gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), text);

	if (entry) *entry = GTK_COMBO(combo)->entry;
	return combo;
}

gchar *tab_completion_set_to_last_history(GtkWidget *entry)
{
	TabCompData *td = gtk_object_get_data(GTK_OBJECT(entry), "tab_completion_data");
	gchar *buf;

	if (!td || !td->has_history) return NULL;

	buf = history_list_find_last_path_by_key(td->history_key);
	if (buf)
		{
		gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
		}

	return buf;
}

void tab_completion_append_to_history(GtkWidget *entry, gchar *path)
{
	TabCompData *td = gtk_object_get_data(GTK_OBJECT(entry), "tab_completion_data");
	HistoryData *hd;

	if (!td || !td->has_history) return;

	history_list_add_to_key(td->history_key, path, td->history_levels);
	hd = history_list_find_by_key(td->history_key);
	if (hd && hd->list)
		{
		gtk_combo_set_popdown_strings(GTK_COMBO(td->combo), hd->list);
		}
}

GtkWidget *tab_completion_new(GtkWidget **entry, GtkWidget *window, gchar *text,
			      void (*enter_func)(gchar *, gpointer), gpointer data)
{
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *newentry;

	hbox = gtk_hbox_new(FALSE, 0);

	newentry = gtk_entry_new();
	if (text) gtk_entry_set_text(GTK_ENTRY(newentry), text);
	gtk_box_pack_start(GTK_BOX(hbox), newentry, TRUE, TRUE, 0);
	gtk_widget_show(newentry);

	button = tab_completion_create_complete_button(window, newentry);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	tab_completion_add_to_entry(newentry, enter_func, data);

	if (entry) *entry = newentry;
	return hbox;
}

void tab_completion_add_to_entry(GtkWidget *entry, void (*enter_func)(gchar *, gpointer), gpointer data)
{
	TabCompData *td;
	if (!entry)
		{
		printf("Tab completion error: entry != NULL\n");
		return;
		}

	td = g_new0(TabCompData, 1);
	td->entry = entry;
	td->dir_path = NULL;
	td->file_list = NULL;
	td->enter_func = enter_func;
	td->enter_data = data;
	td->tab_func = NULL;
	td->tab_data = NULL;

	td->has_history = FALSE;
	td->history_key = NULL;
	td->history_levels = 0;

	gtk_object_set_data(GTK_OBJECT(td->entry), "tab_completion_data", td);

	gtk_signal_connect(GTK_OBJECT(entry), "key_press_event",
			(GtkSignalFunc) tab_completion_key_pressed, td);
	gtk_signal_connect(GTK_OBJECT(entry), "destroy",
			(GtkSignalFunc) tab_completion_destroy, td);
}

void tab_completion_add_tab_func(GtkWidget *entry, void (*tab_func)(gchar *, gpointer), gpointer data)
{
	TabCompData *td = gtk_object_get_data(GTK_OBJECT(entry), "tab_completion_data");

	if (!td) return;

	td->tab_func = tab_func;
	td->tab_data = data;
}

gchar *remove_trailing_slash(gchar *path)
{
	gchar *ret;
	gint l;
	if (!path) return NULL;

	ret = g_strdup(path);
	l = strlen(ret);
	if (l > 1 && ret[l - 1] == '/') ret[l - 1] = '\0';

	return ret;
}

