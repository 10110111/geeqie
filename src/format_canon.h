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

#ifndef __FORMAT_RAW_CANON_H
#define __FORMAT_RAW_CANON_H


#include "exif.h"


gint format_raw_test_canon(const void *data, const guint len,
			   guint *image_offset, guint *exif_offset);


#define FORMAT_RAW_CANON { "II", 2, "Canon crw format", format_raw_test_canon }, \
			 { "\x49\x49\x2a\00", 4, "Canon cr2 format", format_raw_test_canon }


gint format_exif_makernote_canon_parse(ExifData *exif, unsigned char *tiff, int offset,
				       int size, int byte_order);


#endif

