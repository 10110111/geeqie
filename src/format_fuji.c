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

#include "exif.h"


/*
 *-----------------------------------------------------------------------------
 * Raw (RAF) embedded jpeg extraction for Fujifilm
 *-----------------------------------------------------------------------------
 */


gint format_fuji_raw(const void *data, const guint len,
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


/*
 *-----------------------------------------------------------------------------
 * EXIF Makernote for Fujifilm
 *-----------------------------------------------------------------------------
 */

static ExifTextList FujiTagSharpness[] = {
	{ 1,	"soft" },
	{ 2,	"soft" },
	{ 3,	"normal" },
	{ 4,	"hard" },
	{ 5,	"hard" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagWhiteBalance[]= {
	{ 0,	"auto" },
	{ 256,	"daylight" },
	{ 512,	"cloudy" },
	{ 768,	"daylight color-fluorescence" },
	{ 769,	"daywhite color-fluorescence" },
	{ 770,	"white-fluorescence" },
	{ 1024,	"incandescent" },
	{ 3840,	"custom" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagColorTone[]= {
	{ 0,	"normal" },
	{ 256,	"high" },
	{ 512,	"low" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagFlashMode[]= {
	{ 0,	"auto" },
	{ 1,	"on" },
	{ 2,	"off" },
	{ 3,	"red-eye reduction" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagOffOn[]= {
	{ 0,	"off" },
	{ 1,	"on" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagFocusMode[]= {
	{ 0,	"auto" },
	{ 1,	"manual" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagPictureMode[]= {
	{ 0,	"auto" },
	{ 1,	"portrait" },
	{ 2,	"landscape" },
	{ 4,	"sports" },
	{ 5,	"night" },
	{ 6,	"program AE" },
	{ 256,	"aperture priority AE" },
	{ 512,	"shutter priority AE" },
	{ 768,	"manual" },
	EXIF_TEXT_LIST_END
};

static ExifTextList FujiTagNoYes[]= {
	{ 0,	"no" },
	{ 1,	"yes" },
	EXIF_TEXT_LIST_END
};

#if 0
static ExifTextList FujiTag[]= {
	{ ,	"" },
	{ ,	"" },
	EXIF_TEXT_LIST_END
};
#endif


static ExifMarker FujiExifMarkersList[] = {
{ 0x1000,	EXIF_FORMAT_STRING, 8,		"MkN.Fuji.Quality",	"Quality",	NULL },
{ 0x1001,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.Sharpness",	"Sharpness",	FujiTagSharpness },
{ 0x1002,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.WhiteBalance","White balance",FujiTagWhiteBalance },
{ 0x1003,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.Color",	"Color",	FujiTagColorTone },
{ 0x1004,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.Tone",	"Tone",		FujiTagColorTone },
{ 0x1010,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.FlashMode",	"Flash mode",	FujiTagFlashMode },
{ 0x1011,	EXIF_FORMAT_RATIONAL, 1,	"MkN.Fuji.FlashStrength", "Flash strength", NULL },
{ 0x1020,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.Macro",	"Macro",	FujiTagOffOn },
{ 0x1021,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.FocusMode",	"Focus mode",	FujiTagFocusMode },
{ 0x1030,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.SlowSync",	"Slow synchro",	FujiTagOffOn },
{ 0x1031,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.PictureMode",	"Picture mode",	FujiTagPictureMode },
{ 0x1100,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.ContTake/Bracket",
							"Continuous / Auto bracket",	FujiTagOffOn },
{ 0x1300,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.BlurWarning",	"Blue warning",	FujiTagNoYes },
{ 0x1301,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.FocusWarning","Focus warning",FujiTagNoYes },
{ 0x1302,	EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Fuji.AEWarning",	"AE warning",	FujiTagNoYes },
EXIF_MARKER_LIST_END
};



gint format_fuji_makernote(ExifData *exif, unsigned char *tiff, guint offset,
			   guint size, ExifByteOrder byte_order)
{
	unsigned char *data;
	guint ifdstart;

	if (offset + 8 + 4 >= size) return FALSE;

	data = tiff + offset;
	if (memcmp(data, "FUJIFILM", 8) != 0) return FALSE;

	ifdstart = exif_byte_get_int32(data + 8, EXIF_BYTE_ORDER_INTEL);
	if (offset + ifdstart >= size) return FALSE;

	if (exif_parse_IFD_table(exif, tiff + offset, ifdstart, size - offset,
				 EXIF_BYTE_ORDER_INTEL, FujiExifMarkersList) != 0)
		{
		return FALSE;
		}

	return TRUE;
}

