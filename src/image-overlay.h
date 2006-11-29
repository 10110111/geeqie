/*
 * GQview
 * (C) 2006 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef IMAGE_OVERLAY_H
#define IMAGE_OVERLAY_H


void image_osd_set(ImageWindow *imd, gint info, gint status);
gint image_osd_get(ImageWindow *imd, gint *info, gint *status);

void image_osd_update(ImageWindow *imd);


#endif


