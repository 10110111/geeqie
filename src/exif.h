/*
 *  GQView
 *  (C) 2004 John Ellis
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

typedef struct _ExifFormatAttrib ExifFormatAttrib;
struct _ExifFormatAttrib
{
	ExifFormatType type;
	int size;
	const char *short_name;
	const char *description;
};

/* the list of known tag data formats */
extern ExifFormatAttrib ExifFormatList[];


/*
 *-----------------------------------------------------------------------------
 * Data storage
 *-----------------------------------------------------------------------------
 */

typedef struct _ExifData ExifData;
struct _ExifData
{
	GList *items;	/* list of (ExifItem *) */
};

typedef struct _ExifRational ExifRational;
struct _ExifRational
{
	unsigned long int num;
	unsigned long int den;
};


typedef struct _ExifItem ExifItem;
typedef struct _ExifMarker ExifMarker;
typedef struct _ExifTextList ExifTextList;

struct _ExifItem
{
	ExifFormatType format;
	int tag;
	ExifMarker *marker;
	int elements;
	gpointer data;
	int data_len;
};

struct _ExifMarker
{
	int		tag;
	ExifFormatType	format;
	int		components;
	char		*key;
	char		*description;
	ExifTextList	*list;
};

struct _ExifTextList
{
	int value;
	const char* description;
};


typedef struct _ExifFormattedText ExifFormattedText;
struct _ExifFormattedText
{
	const char *key;
	const char *description;
};


/*
 *-----------------------------------------------------------------------------
 * Data
 *-----------------------------------------------------------------------------
 */

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


/* the known exif tags list */
extern ExifMarker ExifKnownMarkersList[];

/* the unknown tags utilize this generic list */
extern ExifMarker ExifUnknownMarkersList[];

/* the list of specially formatted keys, for human readable output */
extern ExifFormattedText ExifFormattedList[];


/*
 *-----------------------------------------------------------------------------
 * functions
 *-----------------------------------------------------------------------------
 */

ExifData *exif_read(const gchar *path);
void exif_free(ExifData *exif);

gchar *exif_get_data_as_text(ExifData *exif, const gchar *key);
gint exif_get_integer(ExifData *exif, const gchar *key, gint *value);
ExifRational *exif_get_rational(ExifData *exif, const gchar *key, gint *sign);
double exif_rational_to_double(ExifRational *r, gint sign);

ExifItem *exif_get_item(ExifData *exif, const gchar *key);

const char *exif_item_get_tag_name(ExifItem *item);
const char *exif_item_get_description(ExifItem *item);
const char *exif_item_get_format_name(ExifItem *item, gint brief);
gchar *exif_item_get_data_as_text(ExifItem *item);
gint exif_item_get_integer(ExifItem *item, gint *value);
ExifRational *exif_item_get_rational(ExifItem *item, gint *sign);

const gchar *exif_get_description_by_key(const gchar *key);

/* usually for debugging to stdout */
void exif_write_data_list(ExifData *exif, FILE *f, gint human_readable_list);


#endif
