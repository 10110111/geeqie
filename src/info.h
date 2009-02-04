/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef INFO_H
#define INFO_H

#define DEF_PROPERTY_WIDTH  600
#define DEF_PROPERTY_HEIGHT 400


typedef struct _InfoData InfoData;
struct _InfoData
{
	GtkWidget *window;
	GtkWidget *parent;

	ImageWindow *image;

	GList *list;

	FileData *fd;

	GtkWidget *notebook;
	GtkWidget *name_entry;

	GtkWidget *button_next;
	GtkWidget *button_back;
	GtkWidget *label_count;

	GList *tab_list;

	gint updated;
};


void info_window_new(FileData *fd, GList *list, GtkWidget *parent);

GtkWidget *table_add_line(GtkWidget *table, gint x, gint y,
			  const gchar *description, const gchar *text);

gchar *info_tab_default_order(void);

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
