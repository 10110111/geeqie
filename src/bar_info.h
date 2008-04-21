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


#ifndef BAR_INFO_H
#define BAR_INFO_H


GtkWidget *bar_info_new(FileData *fd, gint metadata_only, GtkWidget *bounding_widget);
void bar_info_close(GtkWidget *bar);

void bar_info_set(GtkWidget *bar, FileData *fd);
gint bar_info_event(GtkWidget *bar, GdkEvent *event);

void bar_info_set_selection_func(GtkWidget *bar, GList *(*list_func)(gpointer data), gpointer data);
void bar_info_selection(GtkWidget *bar, gint count);

void bar_info_size_request(GtkWidget *bar, gint width);

void bar_info_maint_renamed(GtkWidget *bar, FileData *fd);

gint comment_write(FileData *fd, GList *keywords, const gchar *comment);

gint comment_read(FileData *fd, GList **keywords, gchar **comment);

GList *keyword_list_pull(GtkWidget *text_widget);
void keyword_list_push(GtkWidget *textview, GList *list);


#endif
