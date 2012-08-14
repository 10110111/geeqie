/*
 * Geeqie
 * Copyright (C) 2008 - 2012 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik / Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "compat.h"

/* gtk_radio_action_set_current_value() replacement for GTK+ < 2.10 */
void radio_action_set_current_value(GtkRadioAction *action, gint current_value)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_radio_action_set_current_value(action, current_value);
#else
	GSList *group;
	gint value;

	group = gtk_radio_action_get_group(action);
	while (group)
		{
		action = GTK_RADIO_ACTION(group->data);
		g_object_get(G_OBJECT(action), "value", &value, NULL);
		if (value == current_value)
			{
			gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
			return;
			}
		group = g_slist_next(group);
		}
#endif
}

#if !GLIB_CHECK_VERSION(2, 14, 0)
static void hash_table_add(gpointer key, gpointer value, gpointer user_data)
{
	GList **list = user_data;
	*list = g_list_prepend(*list, key);
}
#endif

GList* hash_table_get_keys(GHashTable *hash_table)
{
#if GLIB_CHECK_VERSION(2, 14, 0)
	return g_hash_table_get_keys(hash_table);
#else
	GList *list = NULL;
	g_hash_table_foreach(hash_table, hash_table_add, &list);
	return list;
#endif
}

#if !GTK_CHECK_VERSION(2,24,0)
gint compat_gdk_window_get_width(GdkWindow *window)
{
	gint w, h;
	gdk_drawable_get_size(window, &w, &h);
	return w;
}

gint compat_gdk_window_get_height(GdkWindow *window)
{
	gint w, h;
	gdk_drawable_get_size(window, &w, &h);
	return h;
}
#endif

#if !GTK_CHECK_VERSION(2,22,0)
cairo_surface_t *compat_gdk_window_create_similar_surface (GdkWindow *window, cairo_content_t content, gint width, gint height)
{
	cairo_t *cr = gdk_cairo_create(window);
	cairo_surface_t *ws = cairo_get_target(cr); 
	cairo_surface_t *ret = cairo_surface_create_similar(ws, content, width, height);
	cairo_destroy(cr);
	return ret;
}
#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
