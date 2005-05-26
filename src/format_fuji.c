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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "intl.h"

#include "format_fuji.h"
#include "format_raw.h"


gint format_raw_test_fuji(const void *data, const guint len,
			  guint *image_offset, guint *exif_offset)
{
	guint io;
	guint eo;

	if (len < 128 ||
	    memcmp(data, "FUJIFILM", 8) != 0)
		{
		return FALSE;
		}

	io = GUINT32_FROM_BE(*(guint32*)(data + 84));
	eo = *image_offset + 12;

	/* verify jpeg marker */
	if (memcmp(data + io, "\xff\xd8\xff\xe1", 4) != 0)
		{
		return FALSE;
		}

	if (image_offset) *image_offset = io;
	if (exif_offset) *exif_offset = eo;

	printf("raw Fuji format file\n");

	return TRUE;
}


