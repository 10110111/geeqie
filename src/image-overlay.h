/*
 * GQview
 * (C) 2004 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef IMAGE_OVERLAY_H
#define IMAGE_OVERLAY_H


gint image_overlay_info_enable(ImageWindow *imd);
void image_overlay_info_disable(ImageWindow *imd, gint id);

void image_overlay_update(ImageWindow *imd, gint id);


#endif


