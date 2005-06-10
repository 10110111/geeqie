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


#include "exif.h"


typedef enum {
	FORMAT_RAW_MATCH_MAGIC,
	FORMAT_RAW_MATCH_TIFF_MAKE
} FormatRawMatchType;

typedef gint (* FormatRawParseFunc)(unsigned char *data, const guint len,
				    guint *image_offset, guint *exif_offset);

gint format_raw_img_exif_offsets(unsigned char *data, const guint len,
				 guint *image_offset, guint *exif_offset);
gint format_raw_img_exif_offsets_fd(int fd, const gchar *path,
				    unsigned char *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset);


typedef enum {
	FORMAT_EXIF_MATCH_MAKE,
	FORMAT_EXIF_MATCH_MAKERNOTE
} FormatExifMatchType;

typedef gint (* FormatExifParseFunc)(ExifData *exif, unsigned char *tiff, guint offset,
				    guint size, ExifByteOrder bo);

gint format_exif_makernote_parse(ExifData *exif, unsigned char *tiff, guint offset,
				 guint size, ExifByteOrder bo);


#endif

