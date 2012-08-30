/*
 * Geeqie
 * Copyright (C) 2008 - 2012 The Geeqie Team
 *
 * Authors: John Ellis, Vladimir Nadvornik, Laurent Monin
 *
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "uri_utils.h"

#include "filedata.h"
#include "ui_fileops.h"

gchar **uris_from_pathlist(GList *list)
{
	GList *work;
	guint i = 0;
	guint num = g_list_length(list);
	gchar **uris = g_new0(gchar *, num + 1);

	work = list;
	while (work)
		{
		const gchar *path = work->data;
		gchar *local_path = path_from_utf8(path);
		uris[i] = g_filename_to_uri(local_path, NULL, NULL);
		g_free(local_path);
		
		i++;
		work = work->next;
		}

	uris[i] = NULL;
	return uris;
}

gchar **uris_from_filelist(GList *list)
{
	GList *path_list = filelist_to_path_list(list);
	gchar **ret = uris_from_pathlist(path_list);
	string_list_free(path_list);
	return ret;
}

gboolean uri_selection_data_set_uris_from_filelist(GtkSelectionData *selection_data, GList *list)
{
	gchar **uris = uris_from_filelist(list);
	gboolean ret = gtk_selection_data_set_uris(selection_data, uris);
	if (!ret)
		{
		char *str = g_strjoinv("\r\n", uris);
		ret = gtk_selection_data_set_text(selection_data, str, -1);
		g_free(str);
		}

	g_strfreev(uris);
	return ret;
}

GList *uri_pathlist_from_uris(gchar **uris)
{
	GList *list = NULL;
	guint i = 0;

	while (uris[i])
		{
		gchar *local_path = g_filename_from_uri(uris[i], NULL, NULL);
		gchar *path = path_to_utf8(local_path);
		g_free(local_path);
		list = g_list_prepend(list, path);
		i++;
		}

	return g_list_reverse(list);
}



GList *uri_filelist_from_uris(gchar **uris)
{
	GList *path_list = uri_pathlist_from_uris(uris);
	GList *filelist = filelist_from_path_list(path_list);
	string_list_free(path_list);
	return filelist;
}

GList *uri_filelist_from_gtk_selection_data(GtkSelectionData *selection_data)
{
	gchar **uris = gtk_selection_data_get_uris(selection_data);
	GList *ret = uri_filelist_from_uris(uris);
	g_strfreev(uris);
	return ret;
}



/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
