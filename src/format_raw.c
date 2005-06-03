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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <glib.h>

#include "intl.h"

#include "format_raw.h"

#include "format_canon.h"
#include "format_fuji.h"


typedef struct _FormatEntry FormatEntry;
struct _FormatEntry {
	const void *header_pattern;
	const guint header_length;
	const gchar *description;
	FormatRawParseFunc func_parse;
};


static FormatEntry format_list[] = {
	FORMAT_RAW_CANON,
	FORMAT_RAW_FUJI,
	{ NULL, 0, NULL, NULL }
};


static FormatEntry *format_raw_find(const void *data, const guint len)
{
	gint n;

	n = 0;
	while (format_list[n].header_pattern)
		{
		if (format_list[n].header_length <= len &&
		    memcmp(data, format_list[n].header_pattern, format_list[n].header_length) == 0)
			{
			return &format_list[n];
			}
		n++;
		}

	return NULL;
}

static gint format_raw_parse(FormatEntry *entry,
			     const void *data, const guint len,
			     guint *image_offset, guint *exif_offset)
{
	guint io = 0;
	guint eo = 0;
	gint found;

	if (!entry || !entry->func_parse) return FALSE;

	found = entry->func_parse(data, len, &io, &eo);

	if (!found ||
	    io >= len - 4 ||
	    eo >= len)
		{
		return FALSE;
		}

	if (image_offset) *image_offset = io;
	if (exif_offset) *exif_offset = eo;

	return TRUE;
}

gint format_raw_img_exif_offsets(const void *data, const guint len,
				 guint *image_offset, guint *exif_offset)
{
	FormatEntry *entry;

	if (!data || len < 1) return FALSE;

	entry = format_raw_find(data, len);

	if (!entry || !entry->func_parse) return FALSE;

	return format_raw_parse(entry, data, len, image_offset, exif_offset);
}


gint format_raw_img_exif_offsets_fd(int fd, const void *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset)
{
	FormatEntry *entry;
	void *map_data = NULL;
	size_t map_len = 0;
	struct stat st;
	gint success;

	if (!header_data || fd < 0) return FALSE;

	entry = format_raw_find(header_data, header_len);

	if (!entry || !entry->func_parse) return FALSE;

	if (fstat(fd, &st) == -1)
		{
		printf("Failed to stat file %d\n", fd);
		return FALSE;
		}
	map_len = st.st_size;
	map_data = mmap(0, map_len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_data == MAP_FAILED)
		{
		printf("Failed to mmap file %d\n", fd);
		return FALSE;
		}

	success = format_raw_parse(entry, map_data, map_len, image_offset, exif_offset);

	if (munmap(map_data, map_len) == -1)
		{
		printf("Failed to unmap file %d\n", fd);
		}

	if (success && image_offset)
		{
		if (lseek(fd, *image_offset, SEEK_SET) != *image_offset)
			{
			printf("Failed to seek to embedded image\n");

			*image_offset = 0;
			if (*exif_offset) *exif_offset = 0;
			success = FALSE;
			}
		}

	return success;
}


