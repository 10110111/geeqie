
#include "main.h" 
#include "jpeg_parser.h"

gboolean jpeg_segment_find(guchar *data, guint size,
			    guchar app_marker, const gchar *magic, guint magic_len,
			    guint *seg_offset, guint *seg_length)
{
	guchar marker = 0;
	guint offset = 0;
	guint length = 0;

	while (marker != app_marker &&
	       marker != JPEG_MARKER_EOI)
		{
		offset += length;
		length = 2;

		if (offset + 2 >= size ||
		    data[offset] != JPEG_MARKER) return FALSE;

		marker = data[offset + 1];
		if (marker != JPEG_MARKER_SOI &&
		    marker != JPEG_MARKER_EOI)
			{
			if (offset + 4 >= size) return FALSE;
			length += ((guint)data[offset + 2] << 8) + data[offset + 3];
			}
		}

	if (marker == app_marker &&
	    offset + length < size &&
	    length >= 4 + magic_len &&
	    memcmp(data + offset + 4, magic, magic_len) == 0)
		{
		*seg_offset = offset + 4;
		*seg_length = length - 4;
		return TRUE;
		}

	return FALSE;
}
