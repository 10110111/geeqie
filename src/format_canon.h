/*
 *  GQView
 *  (C) 2005 John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 *
 *
 * Code to add support for Canon CR2 and CRW files, version 0.2
 *
 * Developed by Daniel M. German, dmgerman at uvic.ca 
 *
 * you can find the sources for this patch at http://turingmachine.org/~dmg/libdcraw/gqview/
 *
 */

#ifndef __FORMAT_CANON_H
#define __FORMAT_CANON_H


#include "exif.h"


gint format_canon_raw(const void *data, const guint len,
		      guint *image_offset, guint *exif_offset);


#define FORMAT_RAW_CANON { "II", 2, "Canon crw format", format_canon_raw }, \
			 { "\x49\x49\x2a\00", 4, "Canon cr2 format", format_canon_raw }


gint format_canon_makernote(ExifData *exif, unsigned char *tiff, guint offset,
			    guint size, ExifByteOrder byte_order);

#define FORMAT_EXIF_CANON { FORMAT_EXIF_MATCH_MAKE, "Canon", 5, format_canon_makernote }


#endif

