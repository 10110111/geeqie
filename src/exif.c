/*
 *  GQView
 *  (C) 2004 John Ellis
 *
 *  Authors:
 *    Support for Exif file format, originally written by Eric Swalens.    
 *    Modified by Quy Tonthat
 *
 *    Reimplemented with generic data storage by John Ellis (Nov 2003)
 *
 *  The tags were added with information from the FREE document:
 *     http://www.ba.wakwak.com/~tsuruzoh/Computer/Digicams/exif-e.html
 *
 *  For the official Exif Format, please refer to:
 *     http://www.exif.org
 *     http://www.exif.org/specifications.html (PDF spec sheets)
 *
 *  Notes:
 *     Additional tag formats should be added to the proper
 *     location in ExifKnownMarkersList[].
 *
 *     Human readable ouput (that needs additional processing of data to
 *     be useable) can be defined by adding a key to ExifFormattedList[],
 *     then handling that tag in the function exif_get_formatted_by_key().
 *     The human readable formatted keys must begin with the character 'f'.
 *
 *  Unsupported at this time:
 *     IFD1 (thumbnail)
 *     MakerNote
 *     GPSInfo
 *
 *  TODO:
 *     Convert data to useable form in the ??_as_text function for:
 *        ComponentsConfiguration
 *        UserComment (convert this to UTF-8?)
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>   /* stdint.h is not available on all systems... */
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <math.h>
 
#include <glib.h>

#include "intl.h"

#include "exif.h"

#include "format_raw.h"
#include "ui_fileops.h"


/*
 *-----------------------------------------------------------------------------
 * Tag formats
 *-----------------------------------------------------------------------------
 */

ExifFormatAttrib ExifFormatList[] = {
	{ EXIF_FORMAT_UNKNOWN,		1, "unknown",	"unknown" },
	{ EXIF_FORMAT_BYTE_UNSIGNED,	1, "ubyte",	"unsigned byte" },
	{ EXIF_FORMAT_STRING,		1, "string",	"string" },
	{ EXIF_FORMAT_SHORT_UNSIGNED,	2, "ushort",	"unsigned short" },
	{ EXIF_FORMAT_LONG_UNSIGNED,	4, "ulong",	"unsigned long" },
	{ EXIF_FORMAT_RATIONAL_UNSIGNED,8, "urational",	"unsigned rational" },
	{ EXIF_FORMAT_BYTE,		1, "byte",	"byte" },
	{ EXIF_FORMAT_UNDEFINED,	1, "undefined",	"undefined" },
	{ EXIF_FORMAT_SHORT,		2, "sshort",	"signed short" },
	{ EXIF_FORMAT_LONG,		4, "slong",	"signed long" },
	{ EXIF_FORMAT_RATIONAL,		8, "srational",	"signed rational" },
	{ EXIF_FORMAT_FLOAT,		4, "float",	"float" },
	{ EXIF_FORMAT_DOUBLE,		8, "double",	"double" },
	{ -1, 0, NULL }
};

/* tags that are special, or need special treatment */
#define TAG_EXIFOFFSET          0x8769


/*
 *-----------------------------------------------------------------------------
 * Data
 *-----------------------------------------------------------------------------
 */

#define EXIF_TEXT_LIST_END { -1, NULL }

static ExifTextList ExifOrientationList[] = {
	{ EXIF_ORIENTATION_UNKNOWN,	N_("unknown") },
	{ EXIF_ORIENTATION_TOP_LEFT,	N_("top left") },
	{ EXIF_ORIENTATION_TOP_RIGHT,	N_("top right") },
	{ EXIF_ORIENTATION_BOTTOM_RIGHT,N_("bottom right") },
	{ EXIF_ORIENTATION_BOTTOM_LEFT,	N_("bottom left") },
	{ EXIF_ORIENTATION_LEFT_TOP,	N_("left top") },
	{ EXIF_ORIENTATION_RIGHT_TOP,	N_("right top") },
	{ EXIF_ORIENTATION_RIGHT_BOTTOM,N_("right bottom") },
	{ EXIF_ORIENTATION_LEFT_BOTTOM,	N_("left bottom") },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifUnitList[] = {
	{ EXIF_UNIT_UNKNOWN,	N_("unknown") },
	{ EXIF_UNIT_NOUNIT,	"" },
	{ EXIF_UNIT_INCH,	N_("inch") },
	{ EXIF_UNIT_CENTIMETER,	N_("centimeter") },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifYCbCrPosList[] = {
	{ 1,	"center" },
	{ 2,	"datum" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifMeteringModeList[] = {
	{ 0,	N_("unknown") },
	{ 1,	N_("average") },
	{ 2,	N_("center weighted") },
	{ 3,	N_("spot") },
	{ 4,	N_("multi-spot") },
	{ 5,	N_("multi-segment") },
	{ 6,	N_("partial") },
	{ 255,	N_("other") },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifExposureProgramList[] = {
	{ 0,	N_("not defined") },
	{ 1,	N_("manual") },
	{ 2,	N_("normal") },
	{ 3,	N_("aperture") },
	{ 4,	N_("shutter") },
	{ 5,	N_("creative") },
	{ 6,	N_("action") },
	{ 7,	N_("portrait") },
	{ 8,	N_("landscape") },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifLightSourceList[] = {
	{ 0,	N_("unknown") },
	{ 1,	N_("daylight") },
	{ 2,	N_("fluorescent") },
	{ 3,	N_("tungsten (incandescent)") },
	{ 4,	N_("flash") },
	{ 9,	"fine weather" },
	{ 10,	"cloudy weather" },
	{ 11,	"shade" },
	{ 12,	"daylight fluorescent" },
	{ 13,	"day white fluorescent" },
	{ 14,	"cool white fluorescent" },
	{ 15,	"while fluorescent" },
	{ 17,	"standard light A" },
	{ 18,	"standard light B" },
	{ 19,	"standard light C" },
	{ 20,	"D55" },
	{ 21,	"D65" },
	{ 22,	"D75" },
	{ 23,	"D50" },
	{ 24,	"ISO studio tungsten" },
	{ 255,	N_("other") },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifFlashList[] = {
	{ 0,	N_("no") },
	{ 1,	N_("yes") },
	{ 5,	N_("yes, not detected by strobe") },
	{ 7,	N_("yes, detected by strobe") },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifColorSpaceList[] = {
	{ 1,	"sRGB" },
	{ 65535,"uncalibrated" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSensorList[] = {
	{ 1,	"not defined" },
	{ 2,	"1 chip color area" },
	{ 2,	"2 chip color area" },
	{ 4,	"3 chip color area" },
	{ 5,	"color sequential area" },
	{ 7,	"trilinear" },
	{ 8,	"color sequential linear" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSourceList[] = {
	{ 3,	"digital still camera" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSceneList[] = {
	{ 1,	"direct photo" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifCustRenderList[] = {
	{ 0,	"normal" },
	{ 1,	"custom" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifExposureModeList[] = {
	{ 0,	"auto" },
	{ 1,	"manual" },
	{ 2,	"auto bracket" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifWhiteBalanceList[] = {
	{ 0,	"auto" },
	{ 1,	"manual" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSceneCaptureList[] = {
	{ 0,	"standard" },
	{ 1,	"landscape" },
	{ 2,	"portrait" },
	{ 3,	"night scene" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifGainControlList[] = {
	{ 0,	"none" },
	{ 1,	"low gain up" },
	{ 2,	"high gain up" },
	{ 3,	"low gain down" },
	{ 4,	"high gain down" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifContrastList[] = {
	{ 0,	"normal" },
	{ 1,	"soft" },
	{ 2,	"hard" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSaturationList[] = {
	{ 0,	"normal" },
	{ 1,	"low" },
	{ 2,	"high" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSharpnessList[] = {
	{ 0,	"normal" },
	{ 1,	"soft" },
	{ 2,	"hard" },
	EXIF_TEXT_LIST_END
};

static ExifTextList ExifSubjectRangeList[] = {
	{ 0,	"unknown" },
	{ 1,	"macro" },
	{ 2,	"close" },
	{ 3,	"distant" },
	EXIF_TEXT_LIST_END
};

ExifMarker ExifKnownMarkersList[] = {
{ 0x010e, EXIF_FORMAT_STRING, -1,		"ImageDescription",	N_("Image description"), NULL },
{ 0x010f, EXIF_FORMAT_STRING, -1,		"Make",			"Camera make", NULL },
{ 0x0110, EXIF_FORMAT_STRING, -1,		"Model",		"Camera model", NULL },
{ 0x0112, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Orientation",		N_("Orientation"), ExifOrientationList },
{ 0x011a, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"XResolution",		"X resolution", NULL },
{ 0x011b, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"YResolution",		"Y Resolution", NULL },
{ 0x0128, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"ResolutionUnit",	"Resolution units", ExifUnitList },
{ 0x0131, EXIF_FORMAT_STRING, -1, 		"Software",		"Firmware", NULL },
{ 0x0132, EXIF_FORMAT_STRING, 20,		"DateTime",		N_("Date"), NULL },
{ 0x013e, EXIF_FORMAT_RATIONAL_UNSIGNED, 2,	"WhitePoint",		"White point", NULL },
{ 0x013f, EXIF_FORMAT_RATIONAL_UNSIGNED, 6,	"PrimaryChromaticities","Primary chromaticities", NULL },
{ 0x0211, EXIF_FORMAT_RATIONAL_UNSIGNED, 3,	"YCbCrCoefficients",	"YCbCy coefficients", NULL },
{ 0x0213, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"YCbCrPositioning",	"YCbCr positioning", ExifYCbCrPosList },
{ 0x0214, EXIF_FORMAT_RATIONAL_UNSIGNED, 6, 	"ReferenceBlackWhite",	"Black white reference", NULL },
{ 0x8298, EXIF_FORMAT_STRING, -1,		"Copyright",		N_("Copyright"), NULL },
{ 0x8769, EXIF_FORMAT_LONG_UNSIGNED, 1,		"ExifOffset",		"SubIFD Exif offset", NULL },
	/* subIFD follows */
{ 0x829a, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"ExposureTime",		"Exposure time (seconds)", NULL },
{ 0x829d, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"FNumber",		"FNumber", NULL },
{ 0x8822, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"ExposureProgram",	N_("Exposure program"), ExifExposureProgramList },
{ 0x8824, EXIF_FORMAT_STRING, -1,		"SpectralSensitivity",	"Spectral Sensitivity", NULL },
{ 0x8827, EXIF_FORMAT_SHORT_UNSIGNED, -1,	"ISOSpeedRatings",	N_("ISO sensitivity"), NULL },
{ 0x8828, EXIF_FORMAT_UNDEFINED, -1,		"OECF",			"Optoelectric conversion factor", NULL },
{ 0x9000, EXIF_FORMAT_UNDEFINED, 4,		"ExifVersion",		"Exif version", NULL },
{ 0x9003, EXIF_FORMAT_STRING, 20,		"DateTimeOriginal",	N_("Date original"), NULL },
{ 0x9004, EXIF_FORMAT_STRING, 20,		"DateTimeDigitized",	N_("Date digitized"), NULL },
{ 0x9101, EXIF_FORMAT_UNDEFINED, -1,		"ComponentsConfiguration","Pixel format", NULL },
{ 0x9102, EXIF_FORMAT_RATIONAL_UNSIGNED,1,	"CompressedBitsPerPixel","Compression ratio", NULL },
{ 0x9201, EXIF_FORMAT_RATIONAL, 1,		"ShutterSpeedValue",	N_("Shutter speed"), NULL },
{ 0x9202, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"ApertureValue",	N_("Aperture"), NULL },
{ 0x9203, EXIF_FORMAT_RATIONAL, 1,		"BrightnessValue",	"Brightness", NULL },
{ 0x9204, EXIF_FORMAT_RATIONAL, 1,		"ExposureBiasValue",	N_("Exposure bias"), NULL },
{ 0x9205, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"MaxApertureValue",	"Maximum aperture", NULL },
{ 0x9206, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"SubjectDistance",	N_("Subject distance"), NULL },
{ 0x9207, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MeteringMode",		N_("Metering mode"), ExifMeteringModeList },
{ 0x9208, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"LightSource",		N_("Light source"), ExifLightSourceList },
{ 0x9209, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Flash",		N_("Flash"), ExifFlashList },
{ 0x920a, EXIF_FORMAT_RATIONAL_UNSIGNED, 1, 	"FocalLength",		N_("Focal length"), NULL },
{ 0x9214, EXIF_FORMAT_SHORT_UNSIGNED, -1,	"SubjectArea",		"Subject area", NULL },
{ 0x927c, EXIF_FORMAT_UNDEFINED, -1,		"MakerNote",		"MakerNote", NULL },
{ 0x9286, EXIF_FORMAT_UNDEFINED, -1, 		"UserComment",		"UserComment", NULL },
{ 0x9290, EXIF_FORMAT_STRING, -1,		"SubsecTime",		"Subsecond time", NULL },
{ 0x9291, EXIF_FORMAT_STRING, -1,		"SubsecTimeOriginal",	"Subsecond time original", NULL },
{ 0x9292, EXIF_FORMAT_STRING, -1,		"SubsecTimeDigitized",	"Subsecond time digitized", NULL },
{ 0xa000, EXIF_FORMAT_UNDEFINED, 4,		"FlashPixVersion",	"FlashPix version", NULL },
{ 0xa001, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"ColorSpace",		"Colorspace", ExifColorSpaceList },
	/* ExifImageWidth, ExifImageHeight can also be unsigned short */
{ 0xa002, EXIF_FORMAT_LONG_UNSIGNED, 1,		"ExifImageWidth",	N_("Width"), NULL },
{ 0xa003, EXIF_FORMAT_LONG_UNSIGNED, 1,		"ExifImageHeight",	N_("Height"), NULL },
{ 0xa004, EXIF_FORMAT_STRING, -1,		"RelatedSoundFile",	"Audio data", NULL },
{ 0xa005, EXIF_FORMAT_LONG_UNSIGNED, 1,		"ExifInteroperabilityOffset", "ExifR98 extension", NULL },
{ 0xa20b, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"FlashEnergy",		"Flash strength", NULL },
{ 0xa20c, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"SpatialFrequencyResponse","Spatial frequency response", NULL },
{ 0xa20e, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"FocalPlaneXResolution", "X Pixel density", NULL },
{ 0xa20f, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"FocalPlaneYResolution", "Y Pixel density", NULL },
{ 0xa210, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"FocalPlaneResolutionUnit", "Pixel density units", ExifUnitList },
{ 0x0214, EXIF_FORMAT_SHORT_UNSIGNED, 2,	"SubjectLocation",	"Subject location", NULL },
{ 0xa215, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"ExposureIndex",	N_("ISO sensitivity"), NULL },
{ 0xa217, EXIF_FORMAT_SHORT_UNSIGNED, -1,	"SensingMethod",	"Sensor type", ExifSensorList },
{ 0xa300, EXIF_FORMAT_UNDEFINED, 1,		"FileSource",		"Source type", ExifSourceList },
{ 0xa301, EXIF_FORMAT_UNDEFINED, 1,		"SceneType",		"Scene type", ExifSceneList },
{ 0xa302, EXIF_FORMAT_UNDEFINED, -1,		"CFAPattern",		"Color filter array pattern", NULL },
	/* tags a4xx were added for Exif 2.2 (not just these - some above, as well) */
{ 0xa401, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"CustomRendered",	"Render process", ExifCustRenderList },
{ 0xa402, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"ExposureMode",		"Exposure mode", ExifExposureModeList },
{ 0xa403, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"WhiteBalance",		"White balance", ExifWhiteBalanceList },
{ 0xa404, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"DigitalZoomRatio",	"Digital zoom ratio", NULL },
{ 0xa405, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"FocalLength35mmFilm",	"Focal length (35mm)", NULL },
{ 0xa406, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"SceneCapturetype",	"Scene capture type", ExifSceneCaptureList },
{ 0xa407, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"GainControl",		"Gain control", ExifGainControlList },
{ 0xa408, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Contrast",		"Contrast", ExifContrastList },
{ 0xa409, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Saturation",		"Saturation", ExifSaturationList },
{ 0xa40a, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Sharpness",		"Sharpness", ExifSharpnessList },
{ 0xa40b, EXIF_FORMAT_UNDEFINED, -1,		"DeviceSettingDescription","Device setting", NULL },
{ 0xa40c, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"SubjectDistanceRange",	"Subject range", ExifSubjectRangeList },
{ 0xa420, EXIF_FORMAT_STRING, -1,		"ImageUniqueID",	"Image serial number", NULL },
	/* place known, but undocumented or lesser used tags here */
{ 0x00fe, EXIF_FORMAT_LONG_UNSIGNED, 1,		"NewSubfileType",	NULL, NULL },
{ 0x00ff, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"SubfileType",		NULL, NULL },
{ 0x012d, EXIF_FORMAT_SHORT_UNSIGNED, 3,	"TransferFunction",	NULL, NULL },
{ 0x013b, EXIF_FORMAT_STRING, -1,		"Artist",		"Artist", NULL },
{ 0x013d, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Predictor",		NULL, NULL },
{ 0x0142, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"TileWidth",		NULL, NULL },
{ 0x0143, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"TileLength",		NULL, NULL },
{ 0x0144, EXIF_FORMAT_LONG_UNSIGNED, -1,	"TileOffsets",		NULL, NULL },
{ 0x0145, EXIF_FORMAT_SHORT_UNSIGNED, -1,	"TileByteCounts",	NULL, NULL },
{ 0x014a, EXIF_FORMAT_LONG_UNSIGNED, -1,	"SubIFDs",		NULL, NULL },
{ 0x015b, EXIF_FORMAT_UNDEFINED, -1,		"JPEGTables",		NULL, NULL },
{ 0x828d, EXIF_FORMAT_SHORT_UNSIGNED, 2,	"CFARepeatPatternDim",	NULL, NULL },
{ 0x828e, EXIF_FORMAT_BYTE_UNSIGNED, -1,	"CFAPattern",		NULL, NULL },
{ 0x828f, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"BatteryLevel",		NULL, NULL },
{ 0x83bb, EXIF_FORMAT_LONG_UNSIGNED, -1,	"IPTC/NAA",		NULL, NULL },
{ 0x8773, EXIF_FORMAT_UNDEFINED, -1,		"InterColorProfile",	NULL, NULL },
{ 0x8825, EXIF_FORMAT_LONG_UNSIGNED, 1,		"GPSInfo",		"SubIFD GPS offset", NULL },
{ 0x8829, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"Interlace",		NULL, NULL },
{ 0x882a, EXIF_FORMAT_SHORT, 1,			"TimeZoneOffset",	NULL, NULL },
{ 0x882b, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"SelfTimerMode",	NULL, NULL },
{ 0x920b, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"FlashEnergy",		NULL, NULL },
{ 0x920c, EXIF_FORMAT_UNDEFINED, -1,		"SpatialFrequencyResponse", NULL, NULL },
{ 0x920d, EXIF_FORMAT_UNDEFINED, -1,		"Noise",		NULL, NULL },
{ 0x9211, EXIF_FORMAT_LONG_UNSIGNED, 1,		"ImageNumber",		NULL, NULL },
{ 0x9212, EXIF_FORMAT_STRING, 1,		"SecurityClassification", NULL, NULL },
{ 0x9213, EXIF_FORMAT_STRING, -1,		"ImageHistory",		NULL, NULL },
{ 0x9215, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"ExposureIndex",	NULL, NULL },
{ 0x9216, EXIF_FORMAT_BYTE_UNSIGNED, 4,		"TIFF/EPStandardID",	NULL, NULL },

	/* end is marked by 0 tag */
{ 0x0000, EXIF_FORMAT_UNKNOWN, 0, NULL, NULL, NULL }
};

ExifMarker ExifUnknownMarkersList[] = {
{ 0x0000, EXIF_FORMAT_UNKNOWN, 0,		"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_BYTE_UNSIGNED, -1,	"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_STRING, -1,		"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_SHORT_UNSIGNED, -1,	"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_LONG_UNSIGNED, -1,	"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_RATIONAL_UNSIGNED, -1,	"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_BYTE, -1,			"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_UNDEFINED, -1,		"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_SHORT, -1,		"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_LONG, -1,			"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_RATIONAL, -1,		"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_FLOAT, -1,		"unknown",	NULL, NULL },
{ 0x0000, EXIF_FORMAT_DOUBLE, -1,		"unknown",	NULL, NULL },
};

/* human readable key list */

ExifFormattedText ExifFormattedList[] = {
	{ "fCamera",		N_("Camera") },
	{ "fDateTime",		N_("Date") },
	{ "fShutterSpeed",	N_("Shutter speed") },
	{ "fAperture",		N_("Aperture") },
	{ "fExposureBias",	N_("Exposure bias") },
	{ "fISOSpeedRating",	N_("ISO sensitivity") },
	{ "fFocalLength",	N_("Focal length") },
	{ "fSubjectDistance",	N_("Subject distance") },
	{ "fFlash",		N_("Flash") },
	{ "fResolution",	N_("Resolution") },
	{ NULL, NULL }
};


/*
 *-----------------------------------------------------------------------------
 * misc
 *-----------------------------------------------------------------------------
 */

#define BYTE_ORDER_INTEL	1
#define BYTE_ORDER_MOTOROLA	2


#define MARKER_UNKNOWN		0x00
#define MARKER_SOI		0xD8
#define MARKER_APP1		0xE1

/* These data structs are packed to make sure the
 * byte alignment matches the on-disk data format.
 */
typedef struct __attribute__((packed)) {
	char		byte_order[2];
	uint16_t	magic;
	uint32_t	IFD_offset;
} TIFFHeader;
 
typedef struct __attribute__((packed)) {
	uint16_t	tag;
	uint16_t	format;
	uint32_t	nb;
	uint32_t	data;
} IFDEntry;


static ExifMarker *exif_marker_from_tag(uint16_t tag);
static int parse_IFD_table(ExifData *exif, unsigned char *tiff, int offset,
			   int size, int byte_order);

/*
 *-----------------------------------------------------------------------------
 * ExifItem
 *-----------------------------------------------------------------------------
 */

static ExifItem *exif_item_new(ExifFormatType format, unsigned int tag, unsigned int elements, ExifMarker *marker)
{
	ExifItem *item;

	item = g_new0(ExifItem, 1);
	item->format = format;
	item->tag = tag;
	item->marker = marker;
	item->elements = elements;
	item->data = NULL;
	item->data_len = 0;

	switch (format)
		{
		case EXIF_FORMAT_UNKNOWN:
			/* unknown, data is NULL */
			return item;
			break;
		case EXIF_FORMAT_BYTE_UNSIGNED:
			item->data_len = sizeof(char) * elements;
			break;
		case EXIF_FORMAT_STRING:
			item->data_len = sizeof(char) * elements;
			break;
		case EXIF_FORMAT_SHORT_UNSIGNED:
			item->data_len = sizeof(unsigned short int) * elements;
			break;
		case EXIF_FORMAT_LONG_UNSIGNED:
			item->data_len = sizeof(unsigned long int) * elements;
			break;
		case EXIF_FORMAT_RATIONAL_UNSIGNED:
			item->data_len = sizeof(ExifRational) * elements;
			break;
		case EXIF_FORMAT_BYTE:
			item->data_len = sizeof(char) * elements;
			break;
		case EXIF_FORMAT_UNDEFINED:
			item->data_len = sizeof(char) * elements;
			break;
		case EXIF_FORMAT_SHORT:
			item->data_len = sizeof(short int) * elements;
			break;
		case EXIF_FORMAT_LONG:
			item->data_len = sizeof(long int) * elements;
			break;
		case EXIF_FORMAT_RATIONAL:
			item->data_len = sizeof(ExifRational) * elements;
			break;
		case EXIF_FORMAT_FLOAT:
			item->data_len = sizeof(float) * elements;
			break;
		case EXIF_FORMAT_DOUBLE:
			item->data_len = sizeof(double) * elements;
			break;
		}

	item->data = g_malloc0(item->data_len);

	return item;
}

static void exif_item_free(ExifItem *item)
{
	if (!item) return;

	g_free(item->data);
	g_free(item);
}

const char *exif_item_get_tag_name(ExifItem *item)
{
	if (!item || !item->marker) return NULL;
	return item->marker->key;
}

const char *exif_item_get_description(ExifItem *item)
{
	if (!item || !item->marker) return NULL;
	return _(item->marker->description);
}

const char *exif_item_get_format_name(ExifItem *item, gint brief)
{
	if (!item || !item->marker) return NULL; 
	return (brief) ? ExifFormatList[item->format].short_name : ExifFormatList[item->format].description;
}


#define UNDEFINED_TEXT_BYTE_COUNT 16

static GString *string_append_raw_bytes(GString *string, gpointer data, gint ne)
{
	gint i;

	for (i = 0 ; i < ne && i < UNDEFINED_TEXT_BYTE_COUNT; i++)
		{
		unsigned char c = ((char *)data)[i];
		if (c < 32 || c > 127) c = '.';
		g_string_append_printf(string, "%c", c);
		}
	string = g_string_append(string, " : ");
	for (i = 0 ; i < ne && i < UNDEFINED_TEXT_BYTE_COUNT; i++)
		{
		const gchar *spacer;
		if (i > 0)
			{
			if (i%8 == 0)
				{
				spacer = " - ";
				}
			else
				{
				spacer = " ";
				}
			}
		else
			{
			spacer = "";
			}
		g_string_append_printf(string, "%s%02x", spacer, ((char *)data)[i]);
		}
	if (i >= UNDEFINED_TEXT_BYTE_COUNT) g_string_append_printf(string, " (%d bytes)", ne);

	return string;
}

static gchar *text_list_find_value(ExifTextList *list, gint value)
{
	gchar *result = NULL;
	gint i;

	i = 0;
	while (!result && list[i].value >= 0)
		{
		if (value == list[i].value) result = g_strdup(_(list[i].description));
		i++;
		}
	if (!result) result = g_strdup_printf("%d (%s)", value, _("unknown"));

	return result;
}

/*
 *-------------------------------------------------------------------
 * byte size utils
 *-------------------------------------------------------------------
 */

static uint16_t get_int16(unsigned char *f, int bo)
{
	if (bo == BYTE_ORDER_INTEL)
		return *f + (*(f+1)<<8);
	else
		return ((*f)<<8) + *(f+1);
}

#if 0
/* not used ? */
static uint32_t get_int32(unsigned char *f, int bo)
{
	if (bo == BYTE_ORDER_INTEL)
		return get_int16(f, BYTE_ORDER_INTEL) + (get_int16(f+2, BYTE_ORDER_INTEL)<<16);
	else
		return (get_int16(f, BYTE_ORDER_MOTOROLA)<<16) + get_int16(f+2, BYTE_ORDER_MOTOROLA);
}
#endif

static uint16_t swab_int16(uint16_t n, int bo)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	if (bo == BYTE_ORDER_MOTOROLA)
#else
	if (bo == BYTE_ORDER_INTEL)
#endif
		return n>>8 | n<<8 ;
	else
		return n;
}

static uint32_t swab_int32(uint32_t n, int bo)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	if (bo == BYTE_ORDER_MOTOROLA)
#else
	if (bo == BYTE_ORDER_INTEL)
#endif
		return n<<24 | n>>24 | (n & 0xFF0000)>>8 | (n & 0xFF00)<<8;
	else
		return n;
}

/*
 *-------------------------------------------------------------------
 * marker utils
 *-------------------------------------------------------------------
 */

static int get_marker_size(unsigned char *f)
{
	/* Size is always in Motorola byte order */
	return get_int16(f+2, BYTE_ORDER_MOTOROLA);
}

static int goto_next_marker(unsigned char **f, int *size, int *marker)
{
	int marker_size = 2;

	*marker = MARKER_UNKNOWN;

	/* It is safe to access the marker and its size since we have checked
	 * the SOI and this function guaranties the whole next marker is
	 * available
	 */
	if (*(*f+1) != MARKER_SOI)
		{
		marker_size += get_marker_size(*f);
		}

	*size -= marker_size;

	/* size should be at least 4, so we can read the marker and its size
	 * and check data are actually available
	 */
	if (*size < 4) return -1;

	/* Jump to the next marker and be sure it begins with 0xFF
	 */
	*f += marker_size;
	if (**f != 0xFF) return -1;

	if (get_marker_size(*f)+2 > *size) return -1;

	*marker = *(*f+1);

	return 0;
}

/*
 *-------------------------------------------------------------------
 * IFD utils
 *-------------------------------------------------------------------
 */

static ExifMarker *exif_marker_from_tag(uint16_t tag)
{
	static int len = sizeof(ExifKnownMarkersList)/sizeof(ExifMarker) - 1;
	int i = 0;

	while (i < len && ExifKnownMarkersList[i].tag != tag)
		{
		i++;
		}

	return (i >= len ? NULL : &ExifKnownMarkersList[i]);
}

static void rational_from_data(ExifRational *r, void *src, int byte_order)
{
	r->num = swab_int32(*(uint32_t*)src, byte_order);
	r->den = swab_int32(*(uint32_t*)(src + sizeof(uint32_t)), byte_order);
}

static void exif_item_copy_data(ExifItem *item, void *src, int len, ExifFormatType src_format, int byte_order)
{
	int bs;
	int ne;
	gpointer dest;
	int i;

	bs = ExifFormatList[item->format].size;
	ne = item->elements;
	dest = item->data;

	if (!dest || len > item->data_len)
		{
		printf("exif tag %s data size mismatch\n", exif_item_get_tag_name(item));
		return;
		}

	switch (item->format)
		{
		case EXIF_FORMAT_UNKNOWN:
			break;
		case EXIF_FORMAT_BYTE_UNSIGNED:
		case EXIF_FORMAT_BYTE:
		case EXIF_FORMAT_UNDEFINED:
			memcpy(dest, src, len);
			break;
		case EXIF_FORMAT_STRING:
			memcpy(dest, src, len);
			/* string is NULL terminated, make sure this is true */
			if (((char *)dest)[len - 1] != '\0') ((char *)dest)[len - 1] = '\0';
			break;
		case EXIF_FORMAT_SHORT_UNSIGNED:
		case EXIF_FORMAT_SHORT:
			for (i = 0; i < ne; i++)
				{
				((short *)dest)[i] = swab_int16(*(uint16_t*)(src + i * bs), byte_order);
				}
			break;
		case EXIF_FORMAT_LONG_UNSIGNED:
		case EXIF_FORMAT_LONG:
			if (src_format == EXIF_FORMAT_SHORT_UNSIGNED ||
			    src_format == EXIF_FORMAT_SHORT)
				{
				/* a short fits into a long, so allow it */
				int ss;

				ss = ExifFormatList[src_format].size;
				for (i = 0; i < ne; i++)
					{
					((long *)dest)[i] = (long)swab_int16(*(uint16_t*)(src + i * ss), byte_order);
					}
				}
			else
				{
				for (i = 0; i < ne; i++)
					{
					((long *)dest)[i] = swab_int32(*(uint32_t*)(src + i * bs), byte_order);
					}
				}
			break;
		case EXIF_FORMAT_RATIONAL_UNSIGNED:
		case EXIF_FORMAT_RATIONAL:
			for (i = 0; i < ne; i++)
				{
				rational_from_data(&((ExifRational *)dest)[i], src + i * bs, byte_order);
				}
			break;
		case EXIF_FORMAT_FLOAT:
			for (i = 0; i < ne; i++)
				{
				((float *)dest)[i] = swab_int32(*(uint32_t*)(src + i * bs), byte_order);
				}
			break;
		case EXIF_FORMAT_DOUBLE:
			for (i = 0; i < ne; i++)
				{
				ExifRational r;

				rational_from_data(&r, src + i * bs, byte_order);
				if (r.den) ((double *)dest)[i] = (double)r.num / r.den;
				}
			break;
		}
}

static int parse_IFD_entry(ExifData *exif, unsigned char *tiff, int offset,
			   int size, int byte_order)
{
	IFDEntry *ent = (IFDEntry*)(tiff+offset);
	uint32_t swabed_data;
	void *data;
	int data_len;
	ExifMarker *marker;
	ExifItem *item;

	ent->tag = swab_int16(ent->tag, byte_order);
	ent->format = swab_int16(ent->format, byte_order);
	ent->nb = swab_int32(ent->nb, byte_order);
	swabed_data = swab_int32(ent->data, byte_order);

	/* Check tag type. If it does not match, either the format is wrong,
	 * either it is a unknown tag; so it is not really an error.
	 */
	marker = exif_marker_from_tag(ent->tag);
	if (!marker)
		{
		if (ent->format > EXIF_FORMAT_DOUBLE)
			{
			printf("warning: exif tag 0x%4x has invalid format %d\n", ent->tag, ent->format);
			return 0;
			}
		/* allow non recognized tags to be displayed */
		marker = &ExifUnknownMarkersList[ent->format];
		}
	if (marker->format != ent->format)
		{
		/* Some cameras got mixed up signed/unsigned_rational
		 * eg KODAK DC4800 on object_distance tag
		 *
		 * FIXME: what exactly is this test trying to do?
		 * ok, so this test is to allow the case of swapped signed/unsigned mismatch to leak through?
		 */
		if ( !(marker->format == EXIF_FORMAT_RATIONAL_UNSIGNED && ent->format == EXIF_FORMAT_RATIONAL) &&
		     !(marker->format == EXIF_FORMAT_RATIONAL && ent->format == EXIF_FORMAT_RATIONAL_UNSIGNED) &&
			/* short fits into a long so allow this mismatch
			 * as well (some tags allowed to be unsigned short _or_ unsigned long)
			 */
		     !(marker->format == EXIF_FORMAT_LONG_UNSIGNED && ent->format == EXIF_FORMAT_SHORT_UNSIGNED) )
			{
			if (ent->format <= EXIF_FORMAT_DOUBLE)
				{
				printf("warning: exif tag %s format mismatch, found %s exif spec requests %s\n",
					marker->key, ExifFormatList[ent->format].short_name, ExifFormatList[marker->format].short_name);
				}
			else
				{
				printf("warning: exif tag %s format mismatch, found unknown id %d exif spec requests %d (%s)\n",
					marker->key, ent->format, marker->format, ExifFormatList[marker->format].short_name);
				}
			return 0;
			}
		}

	/* Where is the data, is it available?
	 */
	if (marker->components > 0 && marker->components != ent->nb)
		{
		printf("warning: exif tag %s has %d elements, exif spec requests %d\n", marker->key, ent->nb, marker->components);
		}
	data_len = ExifFormatList[marker->format].size * ent->nb;
	if (data_len > sizeof(ent->data))
		{
		if (size < swabed_data+data_len)
			{
			printf("warning: exif tag %s will overrun IFD segment, ignored.\n", marker->key);
			return -1;
			}
		data = (void*)tiff + swabed_data;
		}
	else
		{
		data = (void*)(&(ent->data));
		}

	item = exif_item_new(marker->format, ent->tag, ent->nb, marker);
	exif_item_copy_data(item, data, data_len, ent->format, byte_order);
	exif->items = g_list_prepend(exif->items, item);

	if (item->tag == TAG_EXIFOFFSET)
		{
		parse_IFD_table(exif, tiff, swabed_data, size, byte_order);
		}

	return 0;
}

static int parse_IFD_table(ExifData *exif, unsigned char *tiff, int offset,
			   int size, int byte_order)
{
	int i, nb_entries;

	/* We should be able to read number of entries in IFD0) */
	if (size < offset+2) return -1;

	nb_entries = get_int16(tiff+offset, byte_order);

	/* Entries and next IFD offset must be readable */
	if (size < offset+nb_entries*12+4) return -1;

	for (i=0; i<nb_entries; ++i)
		{
		parse_IFD_entry(exif, tiff, offset+2+i*sizeof(IFDEntry), size, byte_order);
		}

	return 0;
}

/*
 *-------------------------------------------------------------------
 * file formats
 *-------------------------------------------------------------------
 */

static int parse_TIFF(ExifData *exif, unsigned char *tiff, int size)
{
	int byte_order, offset=0;

	if (size < sizeof(TIFFHeader))
		{
		return -1;
		}

	if (strncmp(((TIFFHeader*)tiff)->byte_order, "II", 2) == 0)
		{
		byte_order = BYTE_ORDER_INTEL;
		}
	else if (strncmp(((TIFFHeader*)tiff)->byte_order, "MM", 2) == 0)
		{
		byte_order = BYTE_ORDER_MOTOROLA;
		}
	else
		{
		return -1;
		}

	if (swab_int16(((TIFFHeader*)tiff)->magic, byte_order) != 0x002A)
		{
		return -1;
		}

	offset = swab_int32(((TIFFHeader*)tiff)->IFD_offset, byte_order);

	return parse_IFD_table(exif, tiff, offset, size, byte_order);
}

static int parse_JPEG(ExifData *exif, unsigned char *f, int size)
{
	int marker, marker_size;

	if (size<2 || *f!=0xFF || *(f+1)!=MARKER_SOI)
		{
		return -2;
		}

	do {
		if (goto_next_marker(&f, &size, &marker) == -1)
			{
			break;
			}
	} while (marker != MARKER_APP1);

	if (marker != MARKER_APP1)
		{
		return -2;
		}

	marker_size = get_marker_size(f)-2;
		
	if (marker_size<6 || strncmp((char*)f+4, "Exif\0\0", 6)!=0)
		{
		return -2;
		}

	return parse_TIFF(exif, f+10, marker_size-6);
}

static gint map_file(const gchar *path, void **mapping, int *size)
{
	int fd;
	struct stat fs;

	if ((fd = open(path, O_RDONLY)) == -1)
		{
		perror(path);
		return -1;
		}

	if (fstat(fd, &fs) == -1)
		{
		perror(path);
		close(fd);
		return -1;
		}

	*size = fs.st_size;

	if ((*mapping = mmap(0, *size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		{
		perror(path);
		close(fd);
		return -1;
		}

	close(fd);
	return 0;
}

static gint unmap_file(void *mapping, int size)
{
	if (munmap(mapping, size) == -1)
		{
		perror("munmap");
		return -1;
		}

	return 0;
}

void exif_free(ExifData *exif)
{
	GList *work;

	if (!exif) return;

	work = exif->items;
	while (work)
		{
		ExifItem *item = work->data;
		work = work->next;
		exif_item_free(item);
		}

	g_list_free(exif->items);
	g_free(exif);
}

ExifData *exif_read(const gchar *path)
{
	ExifData *exif;
	void *f;
	int size, res;
	gchar *pathl;

	if (!path) return NULL;

	pathl = path_from_utf8(path);
	if (map_file(pathl, &f, &size) == -1)
		{
		g_free(pathl);
		return NULL;
		}
	g_free(pathl);

	exif = g_new0(ExifData, 1);
	exif->items = NULL;

	if ((res = parse_JPEG(exif, (unsigned char *)f, size)) == -2)
		{
		res = parse_TIFF(exif, (unsigned char *)f, size);
		}

	if (res != 0)
		{
		guint32 offset = 0;
		
		if (format_raw_img_exif_offsets(f, size, NULL, &offset))
			{
			res = parse_TIFF(exif, (unsigned char*)f + offset, size - offset);
			}
		}

	if (res != 0)
		{
		exif_free(exif);
		exif = NULL;
		}

	unmap_file(f, size);

	if (exif) exif->items = g_list_reverse(exif->items);

#if 0
	exif_write_data_list(exif, stdout, TRUE);
	exif_write_data_list(exif, stdout, FALSE);
#endif

	return exif;
}

ExifItem *exif_get_item(ExifData *exif, const gchar *key)
{
	GList *work;

	if (!key) return NULL;

	work = exif->items;
	while (work)
 		{
		ExifItem *item;

		item = work->data;
		work = work->next;
		if (item->marker->key && strcmp(key, item->marker->key) == 0) return item;
                }
	return NULL;
}

gchar *exif_item_get_data_as_text(ExifItem *item)
{
	ExifMarker *marker;
	gpointer data;
	GString *string;
	gchar *text;
	gint ne;
	gint i;

	if (!item) return NULL;

	marker = item->marker;
	if (!marker) return NULL;

	data = item->data;
	ne = item->elements;
	string = g_string_new("");
	switch (item->format)
		{
		case EXIF_FORMAT_UNKNOWN:
			break;
		case EXIF_FORMAT_BYTE_UNSIGNED:
		case EXIF_FORMAT_BYTE:
		case EXIF_FORMAT_UNDEFINED:
			if (ne == 1 && marker->list)
				{
				gchar *result;
				unsigned char val;

				if (item->format == EXIF_FORMAT_BYTE_UNSIGNED ||
				    item->format == EXIF_FORMAT_UNDEFINED)
					{
					val = ((unsigned char *)data)[0];
					}
				else
					{
					val = (unsigned char)(((signed char *)data)[0]);
					}

				result = text_list_find_value(marker->list, (unsigned short)val);
				string = g_string_append(string, result);
				g_free(result);
				}
			else
				{
				string = string_append_raw_bytes(string, data, ne);
				}
			break;
		case EXIF_FORMAT_STRING:
			string = g_string_append(string, (gchar *)(item->data));
			break;
		case EXIF_FORMAT_SHORT_UNSIGNED:
			if (ne == 1 && marker->list)
				{
				gchar *result;

				result = text_list_find_value(marker->list, ((unsigned short *)data)[0]);
				string = g_string_append(string, result);
				g_free(result);
				}
			else for (i = 0; i < ne; i++)
				{
				g_string_append_printf(string, "%s%hd", (i > 0) ? ", " : "",
							((unsigned short *)data)[i]);
				}
			break;
		case EXIF_FORMAT_LONG_UNSIGNED:
			for (i = 0; i < ne; i++)
				{
				g_string_append_printf(string, "%s%ld", (i > 0) ? ", " : "",
							((unsigned long *)data)[i]);
				}
			break;
		case EXIF_FORMAT_RATIONAL_UNSIGNED:
			for (i = 0; i < ne; i++)
				{
				ExifRational *r;

				r = &((ExifRational *)data)[i];
				g_string_append_printf(string, "%s%ld/%ld", (i > 0) ? ", " : "",
							(unsigned long)r->num, (unsigned long)r->den);
				}
			break;
		case EXIF_FORMAT_SHORT:
			for (i = 0; i < ne; i++)
				{
				g_string_append_printf(string, "%s%hd", (i > 0) ? ", " : "",
							((short *)data)[i]);
				}
			break;
		case EXIF_FORMAT_LONG:
			for (i = 0; i < ne; i++)
				{
				g_string_append_printf(string, "%s%ld", (i > 0) ? ", " : "",
							((long *)data)[i]);
				}
			break;
		case EXIF_FORMAT_RATIONAL:
			for (i = 0; i < ne; i++)
				{
				ExifRational *r;

				r = &((ExifRational *)data)[i];
				g_string_append_printf(string, "%s%ld/%ld", (i > 0) ? ", " : "",
							(long)r->num, (long)r->den);
				}
			break;
		case EXIF_FORMAT_FLOAT:
			for (i = 0; i < ne; i++)
				{
				g_string_append_printf(string, "%s%f", (i > 0) ? ", " : "",
							((float *)data)[i]);
				}
			break;
		case EXIF_FORMAT_DOUBLE:
			for (i = 0; i < ne; i++)
				{
				g_string_append_printf(string, "%s%f", (i > 0) ? ", " : "",
							((double *)data)[i]);
				}
			break;
		}

	text = g_strdup(string->str);
	g_string_free(string, TRUE);

	return text;
}

gint exif_item_get_integer(ExifItem *item, gint *value)
{
	if (!item) return FALSE;

	switch (item->format)
		{
		case EXIF_FORMAT_SHORT:
			*value = (gint)(((short *)(item->data))[0]);
			return TRUE;
			break;
		case EXIF_FORMAT_SHORT_UNSIGNED:
			*value = (gint)(((unsigned short *)(item->data))[0]);
			return TRUE;
			break;
		case EXIF_FORMAT_LONG:
			*value = (gint)(((long *)(item->data))[0]);
			return TRUE;
			break;
		case EXIF_FORMAT_LONG_UNSIGNED:
			/* FIXME: overflow possible */
			*value = (gint)(((unsigned long *)(item->data))[0]);
			return TRUE;
		default:
			/* all other type return FALSE */
			break;
		}
	return FALSE;
}

gint exif_get_integer(ExifData *exif, const gchar *key, gint *value)
{
	ExifItem *item;

	item = exif_get_item(exif, key);
	return exif_item_get_integer(item, value);
}

ExifRational *exif_item_get_rational(ExifItem *item, gint *sign)
{
	if (!item) return NULL;

	if (item->format == EXIF_FORMAT_RATIONAL ||
	    item->format == EXIF_FORMAT_RATIONAL_UNSIGNED)
		{
		if (sign) *sign = (item->format == EXIF_FORMAT_RATIONAL);
		return &((ExifRational *)(item->data))[0];
		}

	return NULL;
}

ExifRational *exif_get_rational(ExifData *exif, const gchar *key, gint *sign)
{
	ExifItem *item;

	item = exif_get_item(exif, key);
	return exif_item_get_rational(item, sign);
}

double exif_rational_to_double(ExifRational *r, gint sign)
{
	if (!r || r->den == 0.0) return 0.0;

	if (sign) return (double)((int)r->num) / (double)((int)r->den);
	return (double)r->num / r->den;
}

static double exif_get_rational_as_double(ExifData *exif, const gchar *key)
{
	ExifRational *r;
	gint sign;

	r = exif_get_rational(exif, key, &sign);
	return exif_rational_to_double(r, sign);
}

static GString *append_comma_text(GString *string, const gchar *text)
{
	string = g_string_append(string, ", ");
	string = g_string_append(string, text);

	return string;
}

static gchar *exif_get_formatted_by_key(ExifData *exif, const gchar *key, gint *key_valid)
{
	/* must begin with f, else not formatted */
	if (key[0] != 'f')
		{
		if (key_valid) *key_valid = FALSE;
		return NULL;
		}

	if (key_valid) *key_valid = TRUE;

	if (strcmp(key, "fCamera") == 0)
		{
		gchar *text;
		gchar *make = exif_get_data_as_text(exif, "Make");
		gchar *model = exif_get_data_as_text(exif, "Model");
		gchar *software = exif_get_data_as_text(exif, "Software");

		text = g_strdup_printf("%s%s%s%s%s%s", (make) ? make : "", ((make) && (model)) ? " " : "",
						       (model) ? model : "",
						       (software) ? " (" : "",
						       (software) ? software : "",
						       (software) ? ")" : "");

		g_free(make);
		g_free(model);
		g_free(software);
		return text;
		}
	if (strcmp(key, "fDateTime") == 0)
		{
		gchar *text = exif_get_data_as_text(exif, "DateTimeOriginal");
		gchar *subsec = NULL;
		if (text) subsec = exif_get_data_as_text(exif, "SubsecTimeOriginal");
		if (!text)
			{
			text = exif_get_data_as_text(exif, "DateTime");
			if (text) subsec = exif_get_data_as_text(exif, "SubsecTime");
			}
		if (subsec)
			{
			gchar *tmp = text;
			text = g_strconcat(tmp, ".", subsec, NULL);
			g_free(tmp);
			g_free(subsec);
			}
		return text;
		}
	if (strcmp(key, "fShutterSpeed") == 0)
		{
		ExifRational *r;

		r = exif_get_rational(exif, "ExposureTime", NULL);
		if (r && r->num && r->den)
			{
			double n = (double)r->den / (double)r->num;
			return g_strdup_printf("%s%.0fs", n > 1.0 ? "1/" : "",
							  n > 1.0 ? n : 1.0 / n);
			}
		r = exif_get_rational(exif, "ShutterSpeedValue", NULL);
		if (r && r->num  && r->den)
			{
			double n = pow(2.0, exif_rational_to_double(r, TRUE));

			/* Correct exposure time to avoid values like 1/91s (seen on Minolta DImage 7) */
			if (n > 1.0 && (int)n - ((int)(n/10))*10 == 1) n--;

			return g_strdup_printf("%s%.0fs", n > 1.0 ? "1/" : "",
							  n > 1.0 ? floor(n) : 1.0 / n);	
			}
		return NULL;
		}
	if (strcmp(key, "fAperture") == 0)
		{
		double n;

		n = exif_get_rational_as_double(exif, "FNumber");
		if (n == 0.0) n = exif_get_rational_as_double(exif, "ApertureValue");
		if (n == 0.0) return NULL;

		return g_strdup_printf("f/%.1f", n);
		}
	if (strcmp(key, "fExposureBias") == 0)
		{
		ExifRational *r;
		gint sign;
		double n;

		r = exif_get_rational(exif, "ExposureBiasValue", &sign);
		if (!r) return NULL;

		n = exif_rational_to_double(r, sign);
		return g_strdup_printf("%+.1f", n);
		}
	if (strcmp(key, "fFocalLength") == 0)
		{
		double n;

		n = exif_get_rational_as_double(exif, "FocalLength");
		if (n == 0.0) return NULL;
		return g_strdup_printf("%.2f mm", n);
		}
	if (strcmp(key, "fISOSpeedRating") == 0)
		{
		gchar *text;

		text = exif_get_data_as_text(exif, "ISOSpeedRatings");
		/* kodak may set this instead */
		if (!text) text = exif_get_data_as_text(exif, "ExposureIndex");
		return text;
		}
	if (strcmp(key, "fSubjectDistance") == 0)
		{
		ExifRational *r;
		gint sign;
		double n;

		r = exif_get_rational(exif, "SubjectDistance", &sign);
		if (!r) return NULL;

		if ((long)r->num == 0xffffffff) return g_strdup(_("infinity"));
		if ((long)r->num == 0) return g_strdup(_("unknown"));

		n = exif_rational_to_double(r, sign);
		if (n == 0.0) return _("unknown");
		return g_strdup_printf("%.3f m", n);
		}
	if (strcmp(key, "fFlash") == 0)
		{
		/* grr, flash is a bitmask... */
		GString *string;
		gchar *text;
		gint n;
		gint v;

		if (!exif_get_integer(exif, "Flash", &n)) return NULL;

		/* Exif 2.1 only defines first 3 bits */
		if (n <= 0x07) return g_strdup(text_list_find_value(ExifFlashList, n));

		/* must be Exif 2.2 */
		string = g_string_new("");

		/* flash fired (bit 0) */
		string = g_string_append(string, (n & 0x01) ? _("yes") : _("no"));

		/* flash mode (bits 3, 4) */
		v = (n >> 3) & 0x03;
		if (v) string = append_comma_text(string, _("mode:"));
		switch (v)
			{
			case 1:
				string = g_string_append(string, _("on"));
				break;
			case 2:
				string = g_string_append(string, _("off"));
				break;
			case 3:
				string = g_string_append(string, _("auto"));
				break;
			}

		/* return light (bits 1, 2) */
		v = (n >> 1) & 0x03;
		if (v == 2) string = append_comma_text(string, _("not detected by strobe"));
		if (v == 3) string = append_comma_text(string, _("detected by strobe"));

		/* we ignore flash function (bit 5) */

		/* red-eye (bit 6) */
		if ((n >> 5) & 0x01) string = append_comma_text(string, _("red-eye reduction"));

		text = string->str;
		g_string_free(string, FALSE);
		return text;
		}
	if (strcmp(key, "fResolution") == 0)
		{
		ExifRational *rx, *ry;
		gchar *units;
		gchar *text;

		rx = exif_get_rational(exif, "XResolution", NULL);
		ry = exif_get_rational(exif, "YResolution", NULL);
		if (!rx || !ry) return NULL;

		units = exif_get_data_as_text(exif, "ResolutionUnit");
		text = g_strdup_printf("%0.f x %0.f (%s/%s)", rx->den ? (double)rx->num / rx->den : 1.0,
							      ry->den ? (double)ry->num / ry->den : 1.0,
							      _("dot"), (units) ? units : _("unknown"));

		g_free(units);
		return text;
		}

	if (key_valid) *key_valid = FALSE;
	return NULL;
}

gchar *exif_get_data_as_text(ExifData *exif, const gchar *key)
{
	ExifItem *item;
	gchar *text;
	gint key_valid;

	if (!key) return NULL;

	text = exif_get_formatted_by_key(exif, key, &key_valid);
	if (key_valid) return text;

	item = exif_get_item(exif, key);
	if (item) return exif_item_get_data_as_text(item);

	return NULL;
}

const gchar *exif_get_description_by_key(const gchar *key)
{
	gint i;

	if (!key) return NULL;

	i = 0;
	while (ExifFormattedList[i].key != NULL)
		{
		if (strcmp(key, ExifFormattedList[i].key) == 0) return _(ExifFormattedList[i].description);
		i++;
		}

	i = 0;
	while (ExifKnownMarkersList[i].tag > 0)
		{
		if (strcmp(key, ExifKnownMarkersList[i].key) == 0) return _(ExifKnownMarkersList[i].description);
		i++;
		}

	return NULL;
}

static void exif_write_item(FILE *f, ExifItem *item)
{
	gchar *text;

	text = exif_item_get_data_as_text(item);
	if (text)
		{
		fprintf(f, "%4x %9s %30s %s\n", item->tag, ExifFormatList[item->format].short_name,
			exif_item_get_tag_name(item), text);
		}
	g_free(text);
}

void exif_write_data_list(ExifData *exif, FILE *f, gint human_readable_list)
{
	if (!f || !exif) return;

	fprintf(f, " tag   format                             key value\n");
	fprintf(f, "----------------------------------------------------\n");

	if (human_readable_list)
		{
		gint i;

		i = 0;
		while (ExifFormattedList[i].key)
			{
			gchar *text;

			text = exif_get_formatted_by_key(exif, ExifFormattedList[i].key, NULL);
			if (text)
				{
				fprintf(f, "     %9s %30s %s\n", "string", ExifFormattedList[i].key, text);
				}
			i++;
			}
		}
	else
		{
		GList *work;

		work = exif->items;
		while (work)
			{
			ExifItem *item;

			item = work->data;
			work = work->next;

			exif_write_item(f, item);
			}
		}
	fprintf(f, "----------------------------------------------------\n");
}

