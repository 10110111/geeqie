/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <math.h>

#ifdef HAVE_LCMS
/*** color support enabled ***/

#ifdef HAVE_LCMS_LCMS_H
  #include <lcms/lcms.h>
#else
  #include <lcms.h>
#endif
#endif

#include <glib.h>

#include "intl.h"

#include "main.h"
#include "exif.h"

#include "debug.h"
#include "filelist.h"
#include "format_raw.h"
#include "ui_fileops.h"


double exif_rational_to_double(ExifRational *r, gint sign)
{
	if (!r || r->den == 0.0) return 0.0;

	if (sign) return (double)((int)r->num) / (double)((int)r->den);
	return (double)r->num / r->den;
}

double exif_get_rational_as_double(ExifData *exif, const gchar *key)
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

static gchar *remove_common_prefix(gchar *s, gchar *t)
{
	gint i;

	if (!s || !t) return t;

	for (i = 0; s[i] && t[i] && s[i] == t[i]; i++)
		;
	if (!i)
		return t;
	if (s[i-1] == ' ' || !s[i])
		{
		while (t[i] == ' ')
			i++;
		return t + i;
		}
	return s;
}

static double get_crop_factor(ExifData *exif)
{
	double res_unit_tbl[] = {0.0, 25.4, 25.4, 10.0, 1.0, 0.001 };

	double xres = exif_get_rational_as_double(exif, "Exif.Photo.FocalPlaneXResolution");
	double yres = exif_get_rational_as_double(exif, "Exif.Photo.FocalPlaneYResolution");
	int res_unit;
	int w, h;
	double xsize, ysize, size, ratio;

	if (xres == 0.0 || yres == 0.0) return 0.0;

	if (!exif_get_integer(exif, "Exif.Photo.FocalPlaneResolutionUnit", &res_unit)) return 0.0;
	if (res_unit < 1 || res_unit > 5) return 0.0;

	if (!exif_get_integer(exif, "Exif.Photo.PixelXDimension", &w)) return 0.0;
	if (!exif_get_integer(exif, "Exif.Photo.PixelYDimension", &h)) return 0.0;

	xsize = w * res_unit_tbl[res_unit] / xres;
	ysize = h * res_unit_tbl[res_unit] / yres;

	ratio = xsize / ysize;

	if (ratio < 0.5 || ratio > 2.0) return 0.0; /* reasonable ratio */

	size = sqrt(xsize * xsize + ysize * ysize);

	if (size < 1.0 || size > 100.0) return 0.0; /* reasonable sensor size in mm */

	return sqrt(36*36+24*24) / size;

}

static gint remove_suffix(gchar *str, const gchar *suffix, gint suffix_len)
{
	gint str_len = strlen(str);
	
	if (suffix_len < 0) suffix_len = strlen(suffix);
	if (str_len < suffix_len) return FALSE;
	
	if (strcmp(str + str_len - suffix_len, suffix) != 0) return FALSE; 
	str[str_len - suffix_len] = '\0';
	
	return TRUE;
}

static gchar *exif_build_fCamera(ExifData *exif)
{
	gchar *text;
	gchar *make = exif_get_data_as_text(exif, "Exif.Image.Make");
	gchar *model = exif_get_data_as_text(exif, "Exif.Image.Model");
	gchar *software = exif_get_data_as_text(exif, "Exif.Image.Software");
	gchar *model2;
	gchar *software2;

	if (make)
		{
		g_strstrip(make);

		if (remove_suffix(make, " CORPORATION", 12)) { /* Nikon */ }
		else if (remove_suffix(make, " Corporation", 12)) { /* Pentax */ }
		else if (remove_suffix(make, " OPTICAL CO.,LTD", 16)) { /* OLYMPUS */ };
		}

	if (model)
		g_strstrip(model);

	if (software)
		{
		gint i;

		g_strstrip(software);
		
		/* remove superfluous spaces (pentax K100D) */
		for (i = 0; software[i]; i++)
			if (software[i] == ' ' && software[i + 1] == ' ')
				{
				gint j;

				for (j = 1; software[i + j] == ' '; j++);
				memmove(software + i + 1, software + i + j, strlen(software + i + j) + 1);
				}
		}

	model2 = remove_common_prefix(make, model);
	software2 = remove_common_prefix(model2, software);

	text = g_strdup_printf("%s%s%s%s%s%s", (make) ? make : "", (make && model2) ? " " : "",
					       (model2) ? model2 : "",
					       (software2 && (make || model2)) ? " (" : "",
					       (software2) ? software2 : "",
					       (software2 && (make || model2)) ? ")" : "");

	g_free(make);
	g_free(model);
	g_free(software);
	return text;
}

static gchar *exif_build_fDateTime(ExifData *exif)
{
	gchar *text = exif_get_data_as_text(exif, "Exif.Photo.DateTimeOriginal");
	gchar *subsec = NULL;

	if (text) subsec = exif_get_data_as_text(exif, "Exif.Photo.SubSecTimeOriginal");
	if (!text)
		{
		text = exif_get_data_as_text(exif, "Exif.Image.DateTime");
		if (text) subsec = exif_get_data_as_text(exif, "Exif.Photo.SubSecTime");
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

static gchar *exif_build_fShutterSpeed(ExifData *exif)
{
	ExifRational *r;

	r = exif_get_rational(exif, "Exif.Photo.ExposureTime", NULL);
	if (r && r->num && r->den)
		{
		double n = (double)r->den / (double)r->num;
		return g_strdup_printf("%s%.0fs", n > 1.0 ? "1/" : "",
						  n > 1.0 ? n : 1.0 / n);
		}
	r = exif_get_rational(exif, "Exif.Photo.ShutterSpeedValue", NULL);
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

static gchar *exif_build_fAperture(ExifData *exif)
{
	double n;

	n = exif_get_rational_as_double(exif, "Exif.Photo.FNumber");
	if (n == 0.0) n = exif_get_rational_as_double(exif, "Exif.Photo.ApertureValue");
	if (n == 0.0) return NULL;

	return g_strdup_printf("f/%.1f", n);
}

static gchar *exif_build_fExposureBias(ExifData *exif)
{
	ExifRational *r;
	gint sign;
	double n;

	r = exif_get_rational(exif, "Exif.Photo.ExposureBiasValue", &sign);
	if (!r) return NULL;

	n = exif_rational_to_double(r, sign);
	return g_strdup_printf("%+.1f", n);
}

static gchar *exif_build_fFocalLength(ExifData *exif)
{
	double n;

	n = exif_get_rational_as_double(exif, "Exif.Photo.FocalLength");
	if (n == 0.0) return NULL;
	return g_strdup_printf("%.0f mm", n);
}

static gchar *exif_build_fFocalLength35mmFilm(ExifData *exif)
{
	gint n;
	double f, c;

	if (exif_get_integer(exif, "Exif.Photo.FocalLengthIn35mmFilm", &n) && n != 0)
		{
		return g_strdup_printf("%d mm", n);
		}

	f = exif_get_rational_as_double(exif, "Exif.Photo.FocalLength");
	c = get_crop_factor(exif);

	if (f != 0.0 && c != 0.0)
		{
		return g_strdup_printf("%.0f mm", f * c);
		}

	return NULL;
}

static gchar *exif_build_fISOSpeedRating(ExifData *exif)
{
	gchar *text;

	text = exif_get_data_as_text(exif, "Exif.Photo.ISOSpeedRatings");
	/* kodak may set this instead */
	if (!text) text = exif_get_data_as_text(exif, "Exif.Photo.ExposureIndex");
	return text;
}

static gchar *exif_build_fSubjectDistance(ExifData *exif)
{
	ExifRational *r;
	gint sign;
	double n;

	r = exif_get_rational(exif, "Exif.Photo.SubjectDistance", &sign);
	if (!r) return NULL;

	if ((long)r->num == 0xffffffff) return g_strdup(_("infinity"));
	if ((long)r->num == 0) return g_strdup(_("unknown"));

	n = exif_rational_to_double(r, sign);
	if (n == 0.0) return _("unknown");
	return g_strdup_printf("%.3f m", n);
}

static gchar *exif_build_fFlash(ExifData *exif)
{
	/* grr, flash is a bitmask... */
	GString *string;
	gchar *text;
	gint n;
	gint v;

	if (!exif_get_integer(exif, "Exif.Photo.Flash", &n)) return NULL;

	/* Exif 2.1 only defines first 3 bits */
	if (n <= 0x07) return exif_get_data_as_text(exif, "Exif.Photo.Flash");

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

static gchar *exif_build_fResolution(ExifData *exif)
{
	ExifRational *rx, *ry;
	gchar *units;
	gchar *text;

	rx = exif_get_rational(exif, "Exif.Image.XResolution", NULL);
	ry = exif_get_rational(exif, "Exif.Image.YResolution", NULL);
	if (!rx || !ry) return NULL;

	units = exif_get_data_as_text(exif, "Exif.Image.ResolutionUnit");
	text = g_strdup_printf("%0.f x %0.f (%s/%s)", rx->den ? (double)rx->num / rx->den : 1.0,
						      ry->den ? (double)ry->num / ry->den : 1.0,
						      _("dot"), (units) ? units : _("unknown"));

	g_free(units);
	return text;
}

static gchar *exif_build_fColorProfile(ExifData *exif)
{
	const gchar *name = "";
	const gchar *source = "";
	unsigned char *profile_data;
	guint profile_len;

	profile_data = exif_get_color_profile(exif, &profile_len);
	if (!profile_data)
		{
		gint cs;
		gchar *interop_index;

		/* ColorSpace == 1 specifies sRGB per EXIF 2.2 */
		if (!exif_get_integer(exif, "Exif.Photo.ColorSpace", &cs)) cs = 0;
		interop_index = exif_get_data_as_text(exif, "Exif.Iop.InteroperabilityIndex");

		if (cs == 1)
			{
			name = _("sRGB");
			source = "ColorSpace";
			}
		else if (cs == 2 || (interop_index && !strcmp(interop_index, "R03")))
			{
			name = _("AdobeRGB");
			source = (cs == 2) ? "ColorSpace" : "Iop";
			}

		g_free(interop_index);
		}
	else
		{
		source = _("embedded");
#ifdef HAVE_LCMS

			{
			cmsHPROFILE profile;

			profile = cmsOpenProfileFromMem(profile_data, profile_len);
			if (profile)
				{
				name = cmsTakeProductName(profile);
				cmsCloseProfile(profile);
				}
			g_free(profile_data);
			}
#endif
		}
	if (name[0] == 0 && source[0] == 0) return NULL;
	return g_strdup_printf("%s (%s)", name, source);
}


/* List of custom formatted pseudo-exif tags */
#define EXIF_FORMATTED_TAG(name, label) { #name, label, exif_build##_##name }

ExifFormattedText ExifFormattedList[] = {
	EXIF_FORMATTED_TAG(fCamera,		N_("Camera")),
	EXIF_FORMATTED_TAG(fDateTime,		N_("Date")),
	EXIF_FORMATTED_TAG(fShutterSpeed,	N_("Shutter speed")),
	EXIF_FORMATTED_TAG(fAperture,		N_("Aperture")),
	EXIF_FORMATTED_TAG(fExposureBias,	N_("Exposure bias")),
	EXIF_FORMATTED_TAG(fISOSpeedRating,	N_("ISO sensitivity")),
	EXIF_FORMATTED_TAG(fFocalLength,	N_("Focal length")),
	EXIF_FORMATTED_TAG(fFocalLength35mmFilm,N_("Focal length 35mm")),
	EXIF_FORMATTED_TAG(fSubjectDistance,	N_("Subject distance")),
	EXIF_FORMATTED_TAG(fFlash,		N_("Flash")),
	EXIF_FORMATTED_TAG(fResolution,		N_("Resolution")),
	EXIF_FORMATTED_TAG(fColorProfile,	N_("Color profile")),
	{ NULL, NULL, NULL }
};

gchar *exif_get_formatted_by_key(ExifData *exif, const gchar *key, gint *key_valid)
{
	/* must begin with f, else not formatted */
	if (key[0] == 'f')
		{
		gint i;

		if (key_valid) *key_valid = TRUE;

		for (i = 0; ExifFormattedList[i].key; i++)
			if (strcmp(key, ExifFormattedList[i].key) == 0)
				return ExifFormattedList[i].build_func(exif);
		}

	if (key_valid) *key_valid = FALSE;
	return NULL;
}

const gchar *exif_get_description_by_key(const gchar *key)
{
	gint i;

	if (!key) return NULL;

	for (i = 0; ExifFormattedList[i].key; i++)
		if (strcmp(key, ExifFormattedList[i].key) == 0)
			return _(ExifFormattedList[i].description);

	return exif_get_tag_description_by_key(key);
}

gint exif_get_integer(ExifData *exif, const gchar *key, gint *value)
{
	ExifItem *item;

	item = exif_get_item(exif, key);
	return exif_item_get_integer(item, value);
}

ExifRational *exif_get_rational(ExifData *exif, const gchar *key, gint *sign)
{
	ExifItem *item;

	item = exif_get_item(exif, key);
	return exif_item_get_rational(item, sign);
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

ExifData *exif_read_fd(FileData *fd)
{
	gchar *sidecar_path = NULL;

	if (!fd) return NULL;

	if (filter_file_class(fd->extension, FORMAT_CLASS_RAWIMAGE))
		{
		GList *work;
		
		work = fd->parent ? fd->parent->sidecar_files : fd->sidecar_files;
		while (work)
			{
			FileData *sfd = work->data;
			work = work->next;
			if (strcasecmp(sfd->extension, ".xmp") == 0)
				{
				sidecar_path = sfd->path;
				break;
				}
			}
		}


	// FIXME: some caching would be nice
	return exif_read(fd->path, sidecar_path);
}



/* embedded icc in jpeg */


#define JPEG_MARKER		0xFF
#define JPEG_MARKER_SOI		0xD8
#define JPEG_MARKER_EOI		0xD9
#define JPEG_MARKER_APP1	0xE1
#define JPEG_MARKER_APP2	0xE2

/* jpeg container format:
     all data markers start with 0XFF
     2 byte long file start and end markers: 0xFFD8(SOI) and 0XFFD9(EOI)
     4 byte long data segment markers in format: 0xFFTTSSSSNNN...
       FF:   1 byte standard marker identifier
       TT:   1 byte data type
       SSSS: 2 bytes in Motorola byte alignment for length of the data.
	     This value includes these 2 bytes in the count, making actual
	     length of NN... == SSSS - 2.
       NNN.: the data in this segment
 */

gint exif_jpeg_segment_find(unsigned char *data, guint size,
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

gint exif_jpeg_parse_color(ExifData *exif, unsigned char *data, guint size)
{
	guint seg_offset = 0;
	guint seg_length = 0;
	guint chunk_offset[255];
	guint chunk_length[255];
	guint chunk_count = 0;

	/* For jpeg/jfif, ICC color profile data can be in more than one segment.
	   the data is in APP2 data segments that start with "ICC_PROFILE\x00\xNN\xTT"
	   NN = segment number for data
	   TT = total number of ICC segments (TT in each ICC segment should match)
	 */

	while (exif_jpeg_segment_find(data + seg_offset + seg_length,
				      size - seg_offset - seg_length,
				      JPEG_MARKER_APP2,
				      "ICC_PROFILE\x00", 12,
				      &seg_offset, &seg_length))
		{
		guchar chunk_num;
		guchar chunk_tot;

		if (seg_length < 14) return FALSE;

		chunk_num = data[seg_offset + 12];
		chunk_tot = data[seg_offset + 13];

		if (chunk_num == 0 || chunk_tot == 0) return FALSE;

		if (chunk_count == 0)
			{
			guint i;

			chunk_count = (guint)chunk_tot;
			for (i = 0; i < chunk_count; i++) chunk_offset[i] = 0;
			for (i = 0; i < chunk_count; i++) chunk_length[i] = 0;
			}

		if (chunk_tot != chunk_count ||
		    chunk_num > chunk_count) return FALSE;

		chunk_num--;
		chunk_offset[chunk_num] = seg_offset + 14;
		chunk_length[chunk_num] = seg_length - 14;
		}

	if (chunk_count > 0)
		{
		unsigned char *cp_data;
		guint cp_length = 0;
		guint i;

		for (i = 0; i < chunk_count; i++) cp_length += chunk_length[i];
		cp_data = g_malloc(cp_length);

		for (i = 0; i < chunk_count; i++)
			{
			if (chunk_offset[i] == 0)
				{
				/* error, we never saw this chunk */
				g_free(cp_data);
				return FALSE;
				}
			memcpy(cp_data, data + chunk_offset[i], chunk_length[i]);
			}
		DEBUG_1("Found embedded icc profile in jpeg");
		exif_add_jpeg_color_profile(exif, cp_data, cp_length);

		return TRUE;
		}

	return FALSE;
}
