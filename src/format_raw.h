/*
 *  GQView
 *  (C) 2005 John Ellis
 *
 *  Authors:
 *    Original version 2005 Lars Ellenberg, base on dcraw by David coffin.
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef __FORMAT_RAW_H
#define __FORMAT_RAW_H


typedef gint (* FormatRawParseFunc)(const void *data, const guint len,
				    guint *image_offset, guint *exif_offset);


gint format_raw_img_exif_offsets(const void *data, const guint len,
				 guint *image_offset, guint *exif_offset);
gint format_raw_img_exif_offsets_fd(int fd, const void *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset);


#endif

