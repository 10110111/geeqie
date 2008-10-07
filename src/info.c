/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "info.h"

#include "bar_info.h"
#include "bar_exif.h"
#include "dnd.h"
#include "filedata.h"
#include "image.h"
#include "image-load.h"
#include "pixbuf-renderer.h"
#include "ui_fileops.h"
#include "ui_misc.h"
#include "uri_utils.h"
#include "window.h"

#include <pwd.h>
#include <grp.h>


#define IMAGE_SIZE_W 200
#define IMAGE_SIZE_H 200


typedef struct _TabData TabData;
struct _TabData
{
	void (*func_free)(gpointer data);
	void (*func_sync)(InfoData *id, gpointer data);
	void (*func_image)(InfoData *id, gpointer data);
	gpointer data;
	TabData *(*func_new)(InfoData *id);
	GtkWidget *child;
};

typedef struct _InfoTabsPos InfoTabsPos;
struct _InfoTabsPos
{
	TabData *(*func)(InfoData *id);
	guint pos;
};

static GList *info_tabs_pos_list = NULL;

static void notebook_set_tab_reorderable(GtkNotebook *notebook, GtkWidget *child, gboolean reorderable)
{
#if GTK_CHECK_VERSION(2,10,0)
	gtk_notebook_set_tab_reorderable(notebook, child, reorderable);
#endif
}

/*
 *-------------------------------------------------------------------
 * table utils
 *-------------------------------------------------------------------
 */

GtkWidget *table_add_line(GtkWidget *table, gint x, gint y,
			  const gchar *description, const gchar *text)
{
	GtkWidget *label;

	if (!text) text = "";

	label = pref_table_label(table, x, y, description, 1.0);
	pref_label_bold(label, TRUE, FALSE);

	label = pref_table_label(table, x + 1, y, text, 0.0);
	return label;
}

/*
 *-------------------------------------------------------------------
 * EXIF tab
 *-------------------------------------------------------------------
 */

static void info_tab_exif_image(InfoData *id, gpointer data)
{
	GtkWidget *bar = data;
	FileData *fd;

	if (id->image->unknown)
		{
		fd = NULL;
		}
	else
		{
		fd = id->image->image_fd;
		}

	bar_exif_set(bar, fd);
}

static void info_tab_exif_sync(InfoData *id, gpointer data)
{
	GtkWidget *bar = data;

	bar_exif_set(bar, NULL);
}

static TabData *info_tab_exif_new(InfoData *id)
{
	TabData *td;
	GtkWidget *bar;
	GtkWidget *label;

	bar = bar_exif_new(FALSE, NULL, FALSE, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(bar), PREF_PAD_BORDER);

	label = gtk_label_new(_("Exif"));
	gtk_notebook_append_page(GTK_NOTEBOOK(id->notebook), bar, label);
	notebook_set_tab_reorderable(GTK_NOTEBOOK(id->notebook), bar, TRUE);
	gtk_widget_show(bar);

	/* register */
	td = g_new0(TabData, 1);
	td->func_free = NULL;
	td->func_sync = info_tab_exif_sync;
	td->func_image = info_tab_exif_image;
	td->data = bar;
	td->func_new = info_tab_exif_new;
	td->child = bar;

	return td;
}

/*
 *-------------------------------------------------------------------
 * file attributes tab
 *-------------------------------------------------------------------
 */

typedef struct _InfoTabMeta InfoTabMeta;
struct _InfoTabMeta
{
	GtkWidget *bar_info;
};

static void info_tab_meta_free(gpointer data)
{
	InfoTabMeta *tab = data;

	g_free(tab);
}

static void info_tab_meta_sync(InfoData *id, gpointer data)
{
	InfoTabMeta *tab = data;

	bar_info_set(tab->bar_info, id->fd);
}

static GList *info_tab_meta_list_cb(gpointer data)
{
	InfoData *id = data;

	return filelist_copy(id->list);
}

static TabData *info_tab_meta_new(InfoData *id)
{
	TabData *td;
	InfoTabMeta *tab;
	GtkWidget *label;

	tab = g_new0(InfoTabMeta, 1);

	tab->bar_info = bar_info_new(NULL, TRUE, NULL);
	bar_info_set_selection_func(tab->bar_info, info_tab_meta_list_cb, id);
	bar_info_selection(tab->bar_info, g_list_length(id->list) - 1);

	gtk_container_set_border_width(GTK_CONTAINER(tab->bar_info), PREF_PAD_BORDER);

	label = gtk_label_new(_("Keywords"));
	gtk_notebook_append_page(GTK_NOTEBOOK(id->notebook), tab->bar_info, label);
	notebook_set_tab_reorderable(GTK_NOTEBOOK(id->notebook), tab->bar_info, TRUE);
	gtk_widget_show(tab->bar_info);

	/* register */
	td = g_new0(TabData, 1);
	td->func_free = info_tab_meta_free;
	td->func_sync = info_tab_meta_sync;
	td->func_image = NULL;
	td->data = tab;
	td->func_new = info_tab_meta_new;
	td->child = tab->bar_info;

	return td;
}

/*
 *-------------------------------------------------------------------
 * general tab
 *-------------------------------------------------------------------
 */

typedef struct _InfoTabGeneral InfoTabGeneral;
struct _InfoTabGeneral
{
	GtkWidget *label_file_time;
	GtkWidget *label_file_size;
	GtkWidget *label_dimensions;
	GtkWidget *label_transparent;
	GtkWidget *label_image_size;
	GtkWidget *label_compression;
	GtkWidget *label_mime_type;

	GtkWidget *label_user;
	GtkWidget *label_group;
	GtkWidget *label_perms;

	gint compression_done;
	gint64 byte_size;
};

static void info_tab_general_image(InfoData *id, gpointer data)
{
	InfoTabGeneral *tab = data;
	gchar *buf;
	guint mem_size;
	gint has_alpha;
	GdkPixbuf *pixbuf;
	gint width, height;

	if (id->image->unknown) return;

	image_get_image_size(id->image, &width, &height);

	buf = g_strdup_printf("%d x %d", width, height);
	gtk_label_set_text(GTK_LABEL(tab->label_dimensions), buf);
	g_free(buf);

	pixbuf = image_get_pixbuf(id->image);
	if (pixbuf)
		{
		has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
		}
	else
		{
		has_alpha = FALSE;
		}
	gtk_label_set_text(GTK_LABEL(tab->label_transparent), has_alpha ? _("yes") : _("no"));

	mem_size = width * height * ((has_alpha) ? 4 : 3);
	buf = text_from_size_abrev(mem_size);
	gtk_label_set_text(GTK_LABEL(tab->label_image_size), buf);
	g_free(buf);

	if (!tab->compression_done && mem_size > 0)
		{
		buf = g_strdup_printf("%.1f%%", (gdouble)tab->byte_size / mem_size * 100.0);
		gtk_label_set_text(GTK_LABEL(tab->label_compression), buf);
		g_free(buf);

		tab->compression_done = TRUE;
		}

	buf = image_loader_get_format(id->image->il);
	if (buf)
	gtk_label_set_text(GTK_LABEL(tab->label_mime_type), buf);
	g_free(buf);
}

static gchar *mode_number(mode_t m)
{
	gint mb, mu, mg, mo;

	mb = mu = mg = mo = 0;

	if (m & S_ISUID) mb |= 4;
	if (m & S_ISGID) mb |= 2;
	if (m & S_ISVTX) mb |= 1;

	if (m & S_IRUSR) mu |= 4;
	if (m & S_IWUSR) mu |= 2;
	if (m & S_IXUSR) mu |= 1;

	if (m & S_IRGRP) mg |= 4;
	if (m & S_IWGRP) mg |= 2;
	if (m & S_IXGRP) mg |= 1;

	if (m & S_IROTH) mo |= 4;
	if (m & S_IWOTH) mo |= 2;
	if (m & S_IXOTH) mo |= 1;

	return g_strdup_printf("%d%d%d%d", mb, mu, mg, mo);
}

static void info_tab_general_sync_perm(InfoTabGeneral *tab, InfoData *id)
{
	struct stat st;

	if (!stat_utf8(id->fd->path, &st))
		{
		gtk_label_set_text(GTK_LABEL(tab->label_user), "");
		gtk_label_set_text(GTK_LABEL(tab->label_group), "");
		gtk_label_set_text(GTK_LABEL(tab->label_perms), "");
		}
	else
		{
		struct passwd *user;
		struct group *grp;
		gchar pbuf[12];
		gchar *pmod;
		gchar *buf;

		user = getpwuid(st.st_uid);
		gtk_label_set_text(GTK_LABEL(tab->label_user), (user) ? user->pw_name : "");

		grp = getgrgid(st.st_gid);
		gtk_label_set_text(GTK_LABEL(tab->label_group), (grp) ? grp->gr_name : "");

		pbuf[0] = (st.st_mode & S_IRUSR) ? 'r' : '-';
		pbuf[1] = (st.st_mode & S_IWUSR) ? 'w' : '-';
		pbuf[2] = (st.st_mode & S_IXUSR) ? 'x' : '-';
		pbuf[3] = (st.st_mode & S_IRGRP) ? 'r' : '-';
		pbuf[4] = (st.st_mode & S_IWGRP) ? 'w' : '-';
		pbuf[5] = (st.st_mode & S_IXGRP) ? 'x' : '-';
		pbuf[6] = (st.st_mode & S_IROTH) ? 'r' : '-';
		pbuf[7] = (st.st_mode & S_IWOTH) ? 'w' : '-';
		pbuf[8] = (st.st_mode & S_IXOTH) ? 'x' : '-';
		pbuf[9] = '\0';

		pmod = mode_number(st.st_mode);
		buf = g_strdup_printf("%s (%s)", pbuf, pmod);
		gtk_label_set_text(GTK_LABEL(tab->label_perms), buf);
		g_free(buf);
		g_free(pmod);
		}
}

static void info_tab_general_sync(InfoData *id, gpointer data)
{
	InfoTabGeneral *tab = data;
	gchar *buf;

	gtk_label_set_text(GTK_LABEL(tab->label_file_time), text_from_time(id->fd->date));

	tab->byte_size = id->fd->size;

	buf = text_from_size(tab->byte_size);
	gtk_label_set_text(GTK_LABEL(tab->label_file_size), buf);
	g_free(buf);

	gtk_label_set_text(GTK_LABEL(tab->label_dimensions), "");
	gtk_label_set_text(GTK_LABEL(tab->label_transparent), "");
	gtk_label_set_text(GTK_LABEL(tab->label_image_size), "");

	gtk_label_set_text(GTK_LABEL(tab->label_compression), "");
	gtk_label_set_text(GTK_LABEL(tab->label_mime_type), "");

	info_tab_general_sync_perm(tab, id);

	tab->compression_done = FALSE;
}

static void info_tab_general_free(gpointer data)
{
	InfoTabGeneral *tab = data;

	g_free(tab);
}

static TabData *info_tab_general_new(InfoData *id)
{
	TabData *td;
	InfoTabGeneral *tab;
	GtkWidget *table;
	GtkWidget *label;

	tab = g_new0(InfoTabGeneral, 1);

	table = pref_table_new(NULL, 2, 11, FALSE, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), PREF_PAD_BORDER);

	tab->label_file_time = table_add_line(table, 0, 0, _("File date:"), NULL);
	tab->label_file_size = table_add_line(table, 0, 1, _("File size:"), NULL);

	tab->label_dimensions = table_add_line(table, 0, 2, _("Dimensions:"), NULL);
	tab->label_transparent = table_add_line(table, 0, 3, _("Transparent:"), NULL);
	tab->label_image_size = table_add_line(table, 0, 4, _("Image size:"), NULL);

	tab->label_compression = table_add_line(table, 0, 5, _("Compress ratio:"), NULL);
	tab->label_mime_type = table_add_line(table, 0, 6, _("File type:"), NULL);

	tab->label_user = table_add_line(table, 0, 7, _("Owner:"), NULL);
	tab->label_group = table_add_line(table, 0, 8, _("Group:"), NULL);
	tab->label_perms = table_add_line(table, 0, 9, "", NULL);

	label = gtk_label_new(_("General"));
	gtk_notebook_append_page(GTK_NOTEBOOK(id->notebook), table, label);
	notebook_set_tab_reorderable(GTK_NOTEBOOK(id->notebook), table, TRUE);
	gtk_widget_show(table);

	/* register */
	td = g_new0(TabData, 1);
	td->func_free = info_tab_general_free;
	td->func_sync = info_tab_general_sync;
	td->func_image = info_tab_general_image;
	td->data = tab;
	td->func_new = info_tab_general_new;
	td->child = table;

	return td;
}

/*
 *-------------------------------------------------------------------
 * tabs
 *-------------------------------------------------------------------
 */

static void info_tabs_sync(InfoData *id, gint image)
{
	GList *work;

	work = id->tab_list;
	while (work)
		{
		TabData *td = work->data;
		work = work->next;

		if (image)
			{
			if (td->func_image) td->func_image(id, td->data);
			}
		else
			{
			if (td->func_sync) td->func_sync(id, td->data);
			}
		}
}

static void info_tabs_free(InfoData *id)
{
	GList *work;

	work = id->tab_list;
	while (work)
		{
		TabData *td = work->data;
		work = work->next;

		if (td->func_free) td->func_free(td->data);
		g_free(td);
		}
	g_list_free(id->tab_list);
	id->tab_list = NULL;
}

static InfoTabsPos *info_tabs_pos_new(gpointer func, gint pos)
{
	InfoTabsPos *t = g_new0(InfoTabsPos, 1);
	t->func = func;
	t->pos = pos;

	return t;
}

static void info_tabs_pos_list_append(gpointer func)
{
	static gint pos = 0;

	info_tabs_pos_list = g_list_append(info_tabs_pos_list, info_tabs_pos_new(func, pos++));
}

static gint compare_info_tabs_pos(gconstpointer a, gconstpointer b)
{
	InfoTabsPos *ta = (InfoTabsPos *) a;
	InfoTabsPos *tb = (InfoTabsPos *) b;

	if (ta->pos > tb->pos) return 1;
	return -1;
}

static gpointer info_tab_new_funcs[] = {
	info_tab_general_new,
	info_tab_meta_new,
	info_tab_exif_new,
};

gchar *info_tab_default_order(void)
{
	guint i;
	static gchar str[G_N_ELEMENTS(info_tab_new_funcs) + 1];

	for (i = 0; i < G_N_ELEMENTS(info_tab_new_funcs); i++)
		str[i] = i + '1';
	str[i] = '\0';

	return str;
}

static void info_tab_get_order_string(gchar **dest)
{
	GList *work;
	gchar str[G_N_ELEMENTS(info_tab_new_funcs) + 1];

	g_assert(dest);

	if (!info_tabs_pos_list)
		return;
	
	memset(str, 0, G_N_ELEMENTS(info_tab_new_funcs) + 1);

	work = info_tabs_pos_list;
	while (work)
		{
		guint i;
		InfoTabsPos *t = work->data;
		work = work->next;

		for (i = 0; i < G_N_ELEMENTS(info_tab_new_funcs); i++)
			{
			if (t->func == info_tab_new_funcs[i])
				{
				g_assert(t->pos >= 0 && t->pos < G_N_ELEMENTS(info_tab_new_funcs));
				str[t->pos] = i + '1';
				}
			}
		}
	
	if (strlen(str) != G_N_ELEMENTS(info_tab_new_funcs)) return;

	g_free(*dest);
	*dest = g_strdup(str);
}

static void info_tabs_init(InfoData *id)
{
	GList *work;

	if (!info_tabs_pos_list)
		{
		guint count = 0;
		guint i;
		gchar *order = options->properties.tabs_order;
		
		for (i = 0; i < strlen(order); i++)
			{
			guint n = order[i] - '1';

			if (n >= G_N_ELEMENTS(info_tab_new_funcs)) break;
			count++;
			}

		if (count != G_N_ELEMENTS(info_tab_new_funcs))
			order = info_tab_default_order();

		for (i = 0; i < strlen(order); i++)
			{
			guint n = order[i] - '1';

			if (n >= G_N_ELEMENTS(info_tab_new_funcs)) continue;
			if (g_list_find(info_tabs_pos_list, info_tab_new_funcs[n])) continue;
			info_tabs_pos_list_append(info_tab_new_funcs[n]);
			}
		}
	else
		info_tabs_pos_list = g_list_sort(info_tabs_pos_list, compare_info_tabs_pos);

	info_tab_get_order_string(&options->properties.tabs_order);

	work = info_tabs_pos_list;
	while (work)
		{
		InfoTabsPos *t = work->data;
		work = work->next;

		id->tab_list = g_list_append(id->tab_list, t->func(id));
		}
}

/*
 *-------------------------------------------------------------------
 * sync
 *-------------------------------------------------------------------
 */

static void info_window_sync(InfoData *id, FileData *fd)
{

	if (!fd) return;

	gtk_entry_set_text(GTK_ENTRY(id->name_entry), fd->name);

	if (id->label_count)
		{
		gchar *buf;
		buf = g_strdup_printf(_("Image %d of %d"),
				      g_list_index(id->list, (gpointer)fd) + 1,
				      g_list_length(id->list));
		gtk_label_set_text(GTK_LABEL(id->label_count), buf);
		g_free(buf);
		}

	info_tabs_sync(id, FALSE);

	id->updated = FALSE;
	image_change_fd(id->image, fd, 0.0);
}

static void info_notebook_reordered_cb(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer data)
{
	InfoData *id = data;
	GList *work;

	/* Save current tabs position to be able to restore them later. */
	work = id->tab_list;
	while (work)
		{
		GList *tabpos;
		TabData *td = work->data;
		gint pos = gtk_notebook_page_num(GTK_NOTEBOOK(id->notebook), GTK_WIDGET(td->child));
		work = work->next;

		tabpos = info_tabs_pos_list;
		while (tabpos)
			{
			InfoTabsPos *t = tabpos->data;
			tabpos = tabpos->next;

			if (t->func == td->func_new)
				{
				t->pos = pos;
				break;
				}
			}
		}
	
	info_tabs_pos_list = g_list_sort(info_tabs_pos_list, compare_info_tabs_pos);
	info_tab_get_order_string(&options->properties.tabs_order);
}

/*
 *-------------------------------------------------------------------
 * drag n drop (dropping not supported!)
 *-------------------------------------------------------------------
 */

static void info_window_dnd_data_set(GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *selection_data, guint info,
				     guint time, gpointer data)
{
	InfoData *id = data;
	FileData *fd;

	fd = image_get_fd(id->image);
	if (fd)
		{
		gchar *text;
		gint len;
		GList *list;
		gint plain_text;

		switch (info)
			{
			case TARGET_URI_LIST:
				plain_text = FALSE;
				break;
			case TARGET_TEXT_PLAIN:
			default:
				plain_text = TRUE;
				break;
			}
		list = g_list_append(NULL, fd);
		text = uri_text_from_filelist(list, &len, plain_text);
		g_list_free(list);

		gtk_selection_data_set(selection_data, selection_data->target,
				       8, (guchar *)text, len);
		g_free(text);
		}
}

static void info_window_dnd_init(InfoData *id)
{
	ImageWindow *imd;

	imd = id->image;

	gtk_drag_source_set(imd->pr, GDK_BUTTON2_MASK,
			    dnd_file_drag_types, dnd_file_drag_types_count,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(imd->pr), "drag_data_get",
			 G_CALLBACK(info_window_dnd_data_set), id);

#if 0
	gtk_drag_dest_set(imd->pr,
			  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			  dnd_file_drop_types, dnd_file_drop_types_count,
			  GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(imd->pr), "drag_data_received",
			 G_CALLBACK(info_window_dnd_data_get), id);
#endif
}

/*
 *-------------------------------------------------------------------
 * base window
 *-------------------------------------------------------------------
 */

static void info_window_image_update_cb(ImageWindow *imd, gpointer data)
{
	InfoData *id = data;

	/* only do this once after when loading a new image,
	 * for tabs that depend on image data (exif)
	 * Subsequent updates are ignored, as the image
	 * should not really changed if id->updated is TRUE.
	 */

	if (id->updated) return;
	if (imd->unknown) return;

	info_tabs_sync(id, TRUE);
	id->updated = TRUE;
}

static void info_window_back_cb(GtkWidget *widget, gpointer data)
{
	InfoData *id = data;
	GList *work;

	work = g_list_find(id->list, (gpointer)id->fd);
	if (!work || !work->prev) return;

	work = work->prev;
	id->fd = work->data;

	info_window_sync(id, id->fd);

	gtk_widget_set_sensitive(id->button_back, (work->prev != NULL));
	gtk_widget_set_sensitive(id->button_next, TRUE);
}

static void info_window_next_cb(GtkWidget *widget, gpointer data)
{
	InfoData *id = data;
	GList *work;

	work = g_list_find(id->list, (gpointer)id->fd);
	if (!work || !work->next) return;

	work = work->next;
	id->fd = work->data;

	info_window_sync(id, id->fd);

	gtk_widget_set_sensitive(id->button_next, (work->next != NULL));
	gtk_widget_set_sensitive(id->button_back, TRUE);
}

static void info_window_image_button_cb(ImageWindow *imd, GdkEventButton *event, gpointer data)
{
	if (event->button == MOUSE_BUTTON_LEFT)
		{
		info_window_next_cb(NULL, data);
		}
	else if (event->button == MOUSE_BUTTON_MIDDLE || event->button == MOUSE_BUTTON_RIGHT)
		{
		info_window_back_cb(NULL, data);
		}
}

static void info_window_image_scroll_cb(ImageWindow *imd, GdkEventScroll *event, gpointer data)
{
	if (event->direction == GDK_SCROLL_UP)
		{
		info_window_back_cb(NULL, data);
		}
	else if (event->direction == GDK_SCROLL_DOWN)
		{
		info_window_next_cb(NULL, data);
		}
}

static void info_window_close(InfoData *id)
{
	gdk_drawable_get_size(id->window->window, &options->layout.properties_window.w, &options->layout.properties_window.h);
	options->layout.properties_window.w = MAX(options->layout.properties_window.w, DEF_PROPERTY_WIDTH);
	options->layout.properties_window.h = MAX(options->layout.properties_window.h, DEF_PROPERTY_HEIGHT);

	gtk_widget_destroy(id->window);
}

static void info_window_close_cb(GtkWidget *widget, gpointer data)
{
	InfoData *id = data;

	info_window_close(id);
}

static gint info_window_delete_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	InfoData *id = data;

	info_window_close(id);
	return TRUE;
}

static void info_window_destroy_cb(GtkWidget *widget, gpointer data)
{
	InfoData *id = data;

	info_tabs_free(id);
	filelist_free(id->list);
	g_free(id);
}

void info_window_new(FileData *fd, GList *list, GtkWidget *parent)
{
	InfoData *id;
	GtkWidget *main_vbox;
	GtkWidget *paned;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *label;
	GdkGeometry geometry;
	static gboolean run_once = FALSE;

	if (!fd && !list) return;

	run_once = TRUE;

	if (!list)
		{
		list = g_list_append(NULL, file_data_ref(fd));
		}

	id = g_new0(InfoData, 1);

	id->list = list;
	id->fd = (FileData *)id->list->data;
	id->updated = FALSE;

	id->window = window_new(GTK_WINDOW_TOPLEVEL, "properties", NULL, NULL, _("Image properties"));
	gtk_window_set_type_hint(GTK_WINDOW(id->window), GDK_WINDOW_TYPE_HINT_DIALOG);
	id->parent = parent;
	if (GTK_IS_WINDOW(id->parent)) {
		gtk_window_set_keep_above(GTK_WINDOW(id->window), TRUE);
#if 0
		/* work, but behavior is not that great */
		gtk_window_set_transient_for(GTK_WINDOW(id->window), GTK_WINDOW(id->parent));
#endif
	}
	gtk_window_set_resizable(GTK_WINDOW(id->window), TRUE);

	geometry.min_width = DEFAULT_MINIMAL_WINDOW_SIZE;
	geometry.min_height = DEFAULT_MINIMAL_WINDOW_SIZE;
	geometry.base_width = DEF_PROPERTY_WIDTH;
	geometry.base_height = DEF_PROPERTY_HEIGHT;
	gtk_window_set_geometry_hints(GTK_WINDOW(id->window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);

	if (options->layout.save_window_positions || run_once)
		gtk_window_set_default_size(GTK_WINDOW(id->window), options->layout.properties_window.w, options->layout.properties_window.h);
	else
		gtk_window_set_default_size(GTK_WINDOW(id->window), DEF_PROPERTY_WIDTH, DEF_PROPERTY_HEIGHT);

	gtk_container_set_border_width(GTK_CONTAINER(id->window), PREF_PAD_BORDER);

	g_signal_connect(G_OBJECT(id->window), "delete_event",
			 G_CALLBACK(info_window_delete_cb), id);
	g_signal_connect(G_OBJECT(id->window), "destroy",
			 G_CALLBACK(info_window_destroy_cb), id);

	paned = gtk_hpaned_new();
	gtk_container_add(GTK_CONTAINER(id->window), paned);
	gtk_widget_show(paned);

	id->image = image_new(FALSE);
	image_set_update_func(id->image, info_window_image_update_cb, id);

	image_set_button_func(id->image, info_window_image_button_cb, id);
	image_set_scroll_func(id->image, info_window_image_scroll_cb, id);

	gtk_widget_set_size_request(id->image->widget, IMAGE_SIZE_W, IMAGE_SIZE_H);
	gtk_paned_pack1(GTK_PANED(paned), id->image->widget, FALSE, TRUE);
	gtk_widget_show(id->image->widget);

	main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_paned_pack2(GTK_PANED(paned), main_vbox, TRUE, TRUE);
	gtk_widget_show(main_vbox);

	hbox = pref_box_new(main_vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
	label = pref_label_new(hbox, _("Filename:"));
	pref_label_bold(label, TRUE, FALSE);

	id->name_entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(id->name_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), id->name_entry, TRUE, TRUE, 0);
	gtk_widget_show(id->name_entry);

	id->notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(id->notebook), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(main_vbox), id->notebook, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(id->notebook), "page-reordered",
			 G_CALLBACK(info_notebook_reordered_cb), id);

	gtk_widget_show(id->notebook);

	pref_spacer(main_vbox, PREF_PAD_GAP);

	hbox = pref_box_new(main_vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_GAP);

	id->button_back = pref_button_new(hbox, GTK_STOCK_GO_BACK, NULL, TRUE,
					  G_CALLBACK(info_window_back_cb), id);
	gtk_widget_set_sensitive(id->button_back, FALSE);

	id->button_next = pref_button_new(hbox, GTK_STOCK_GO_FORWARD, NULL, TRUE,
					  G_CALLBACK(info_window_next_cb), id);
	gtk_widget_set_sensitive(id->button_next, (id->list->next != NULL));

	if (id->list->next)
		{
		id->label_count = pref_label_new(hbox, "");
		}

	button = pref_button_new(NULL, GTK_STOCK_CLOSE, NULL, FALSE,
				 G_CALLBACK(info_window_close_cb), id);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* set up tabs */

	info_tabs_init(id);

	/* fill it */

	info_window_sync(id, id->fd);

	/* finish */

	info_window_dnd_init(id);

	gtk_widget_show(id->window);
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
