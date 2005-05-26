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

#ifndef __FORMAT_RAW_FUJI_H
#define __FORMAT_RAW_FUJI_H


gint format_raw_test_fuji(const void *data, const guint len,
			  guint *image_offset, guint *exif_offset);


#define FORMAT_RAW_FUJI { "FUJIFILM", 8, "Fuji raw format", format_raw_test_fuji }


#endif

