/*
 * Geeqie
 * Copyright (C) 2008 - 2012 The Geeqie Team
 *
 * Author: John Ellis, Vladimir Nadvornik, Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef URI_UTILS_H
#define URI_UTILS_H

GList *uri_filelist_from_uris(gchar **uris);
gchar **uris_from_pathlist(GList *list);
gchar **uris_from_filelist(GList *list);
GList *uri_pathlist_from_uris(gchar **uris);
gboolean uri_selection_data_set_uris_from_filelist(GtkSelectionData *selection_data, GList *list);
GList *uri_filelist_from_gtk_selection_data(GtkSelectionData *selection_data);

#endif /* URI_UTILS_H */
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
