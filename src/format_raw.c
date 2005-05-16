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

#include "format_raw.h"

static gint format_raw_test_canon(int fd, const void *data, const guint len,
				  guint *image_offset, guint *exif_offset)
{
	return FALSE;
}

static gint format_raw_test_fuji(int fd, const void *data, const guint len,
				  guint *image_offset, guint *exif_offset)
{
	if (len < 128 ||
	    memcmp(data, "FUJIFILM", 8) != 0)
		{
		return FALSE;
		}

	*image_offset = GUINT32_FROM_BE(*(guint32*)(data + 84));
	*exif_offset = *image_offset + 12;
printf("found a raw fuji file!\n");
	return TRUE;
}

static gint format_raw_test_nikon(int fd, const void *data, const guint len,
				  guint *image_offset, guint *exif_offset)
{
	return FALSE;
}


gint format_raw_img_exif_offsets(int fd, const void *data, const guint len,
				 guint *image_offset, guint *exif_offset)
{
	guint32 io = 0;
	guint32 eo = 0;
	gint found;

	if (fd < 0 && !data) return FALSE;
#if 0
	if (len < 512) return FALSE;
#endif

	found = format_raw_test_canon(fd, data, len, &io, &eo) ||
		format_raw_test_fuji (fd, data, len, &io, &eo) ||
		format_raw_test_nikon(fd, data, len, &io, &eo);

	if (!found ||
	    io >= len - 4 ||
	    eo >= len ||
	    memcmp(data + io, "\xff\xd8\xff\xe1", 4) != 0)	/* jpeg marker */
		{
		return FALSE;
		}

	if (image_offset) *image_offset = io;
	if (exif_offset) *exif_offset = eo;

	return TRUE;
}



