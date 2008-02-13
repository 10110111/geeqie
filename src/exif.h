/*
 *  GQView
 *  (C) 2006 John Ellis
 *
 *  Authors:
 *    Support for Exif file format, originally written by Eric Swalens.    
 *    Modified by Quy Tonthat
 *    Reimplemented with generic data storage by John Ellis
 *

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __EXIF_H
#define __EXIF_H


/*
 *-----------------------------------------------------------------------------
 * Tag formats
 *-----------------------------------------------------------------------------
 */

#define EXIF_FORMAT_COUNT 13

typedef enum {
	EXIF_FORMAT_UNKNOWN		= 0,
	EXIF_FORMAT_BYTE_UNSIGNED	= 1,
	EXIF_FORMAT_STRING		= 2,
	EXIF_FORMAT_SHORT_UNSIGNED	= 3,
	EXIF_FORMAT_LONG_UNSIGNED	= 4,
	EXIF_FORMAT_RATIONAL_UNSIGNED	= 5,
	EXIF_FORMAT_BYTE		= 6,
	EXIF_FORMAT_UNDEFINED		= 7,
	EXIF_FORMAT_SHORT		= 8,
	EXIF_FORMAT_LONG		= 9,
	EXIF_FORMAT_RATIONAL		= 10,
	EXIF_FORMAT_FLOAT		= 11,
	EXIF_FORMAT_DOUBLE		= 12
} ExifFormatType;

/*
 *-----------------------------------------------------------------------------
 * Data storage
 *-----------------------------------------------------------------------------
 */

typedef struct _ExifItem ExifItem;

typedef struct _ExifData ExifData;

typedef struct _ExifRational ExifRational;
struct _ExifRational
{
	guint32 num;
	guint32 den;
};


/* enums useful for image manipulation */

typedef enum {
	EXIF_ORIENTATION_UNKNOWN	= 0,
	EXIF_ORIENTATION_TOP_LEFT	= 1,
	EXIF_ORIENTATION_TOP_RIGHT	= 2,
	EXIF_ORIENTATION_BOTTOM_RIGHT	= 3,
	EXIF_ORIENTATION_BOTTOM_LEFT	= 4,
	EXIF_ORIENTATION_LEFT_TOP	= 5,
	EXIF_ORIENTATION_RIGHT_TOP	= 6,
	EXIF_ORIENTATION_RIGHT_BOTTOM	= 7,
	EXIF_ORIENTATION_LEFT_BOTTOM	= 8
} ExifOrientationType;

typedef enum {
	EXIF_UNIT_UNKNOWN	= 0,
	EXIF_UNIT_NOUNIT	= 1,
	EXIF_UNIT_INCH		= 2,
	EXIF_UNIT_CENTIMETER	= 3
} ExifUnitType;


/*
 *-----------------------------------------------------------------------------
 * functions
 *-----------------------------------------------------------------------------
 */

ExifData *exif_read(FileData *fd, gint parse_color_profile);
void exif_free(ExifData *exif);

gchar *exif_get_data_as_text(ExifData *exif, const gchar *key);
gint exif_get_integer(ExifData *exif, const gchar *key, gint *value);
ExifRational *exif_get_rational(ExifData *exif, const gchar *key, gint *sign);
double exif_rational_to_double(ExifRational *r, gint sign);

ExifItem *exif_get_item(ExifData *exif, const gchar *key);
ExifItem *exif_get_first_item(ExifData *exif);
ExifItem *exif_get_next_item(ExifData *exif);

const char *exif_item_get_tag_name(ExifItem *item);
guint exif_item_get_tag_id(ExifItem *item);
guint exif_item_get_elements(ExifItem *item);
char *exif_item_get_data(ExifItem *item, guint *data_len);
const char *exif_item_get_description(ExifItem *item);
guint exif_item_get_format_id(ExifItem *item);
const char *exif_item_get_format_name(ExifItem *item, gint brief);
gchar *exif_item_get_data_as_text(ExifItem *item);
gint exif_item_get_integer(ExifItem *item, gint *value);
ExifRational *exif_item_get_rational(ExifItem *item, gint *sign);

const gchar *exif_get_description_by_key(const gchar *key);

gint format_raw_img_exif_offsets_fd(int fd, const gchar *path,
				    unsigned char *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset);


#endif

