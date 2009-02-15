/*
 * Geeqie
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: Vladimir Nadvornik
 * based on a patch by Uwe Ohse
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

/* Note: The order is important */
#define HCHAN_R 0
#define HCHAN_G 1
#define HCHAN_B 2
#define HCHAN_RGB 3
#define HCHAN_VAL 4
#define HCHAN_MAX 5
#define HCHAN_COUNT (HCHAN_MAX+1)


Histogram *histogram_new(void);
void histogram_free(Histogram *histogram);
gint histogram_set_channel(Histogram *histogram, gint chan);
gint histogram_get_channel(Histogram *histogram);
gint histogram_set_mode(Histogram *histogram, gint mode);
gint histogram_get_mode(Histogram *histogram);
const gchar *histogram_label(Histogram *histogram);
const HistMap *histmap_get(FileData *fd);
gint histogram_draw(Histogram *histogram, const HistMap *histmap, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height);

void histogram_notify_cb(FileData *fd, NotifyType type, gpointer data);

#endif /* HISTOGRAM_H */
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
