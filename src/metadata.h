/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis, Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef METADATA_H
#define METADATA_H
gboolean metadata_write_queue_remove(FileData *fd);
gboolean metadata_write_queue_remove_list(GList *list);
gboolean metadata_write_perform(FileData *fd);


gint metadata_write(FileData *fd, GList *keywords, const gchar *comment);

gint metadata_read(FileData *fd, GList **keywords, gchar **comment);

void metadata_set(FileData *fd, GList *new_keywords, gchar *new_comment, gboolean append);
gboolean find_string_in_list(GList *list, const gchar *keyword);
GList *string_to_keywords_list(const gchar *text);

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
