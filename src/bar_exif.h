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


#ifndef BAR_EXIF_H
#define BAR_EXIF_H

#define EXIF_UI_OFF     0
#define EXIF_UI_IFSET   1
#define EXIF_UI_ON      2

typedef struct _ExifUI ExifUI;
struct _ExifUI {
	gint         current;
	gint         temp;
	gint         default_value;
	const gchar *key;
};
extern ExifUI ExifUIList[];


GtkWidget *bar_exif_new(gint show_title, FileData *fd, gint advanced, GtkWidget *bounding_widget);
void bar_exif_close(GtkWidget *bar);

void bar_exif_set(GtkWidget *bar, FileData *fd);

gint bar_exif_is_advanced(GtkWidget *bar);


/* these are exposed for when duplication of the exif bar's text is needed */

const gchar **bar_exif_key_list;
const gint bar_exif_key_count;


#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
