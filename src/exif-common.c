/*
 *  GQView
 *  (C) 2006 John Ellis
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
 
#include <glib.h>

#include "intl.h"

#include "gqview.h"
#include "exif.h"

#include "format_raw.h"
#include "ui_fileops.h"


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

static ExifTextList ExifFlashList[] = {
	{ 0,	N_("no") },
	{ 1,	N_("yes") },
	{ 5,	N_("yes, not detected by strobe") },
	{ 7,	N_("yes, detected by strobe") },
	EXIF_TEXT_LIST_END
};


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


gchar *exif_get_formatted_by_key(ExifData *exif, const gchar *key, gint *key_valid)
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
		gchar *make = exif_get_data_as_text(exif, "Exif.Image.Make");
		gchar *model = exif_get_data_as_text(exif, "Exif.Image.Model");
		gchar *software = exif_get_data_as_text(exif, "Exif.Image.Software");

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
	if (strcmp(key, "fShutterSpeed") == 0)
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
	if (strcmp(key, "fAperture") == 0)
		{
		double n;

		n = exif_get_rational_as_double(exif, "Exif.Photo.FNumber");
		if (n == 0.0) n = exif_get_rational_as_double(exif, "Exif.Photo.ApertureValue");
		if (n == 0.0) return NULL;

		return g_strdup_printf("f/%.1f", n);
		}
	if (strcmp(key, "fExposureBias") == 0)
		{
		ExifRational *r;
		gint sign;
		double n;

		r = exif_get_rational(exif, "Exif.Photo.ExposureBiasValue", &sign);
		if (!r) return NULL;

		n = exif_rational_to_double(r, sign);
		return g_strdup_printf("%+.1f", n);
		}
	if (strcmp(key, "fFocalLength") == 0)
		{
		double n;

		n = exif_get_rational_as_double(exif, "Exif.Photo.FocalLength");
		if (n == 0.0) return NULL;
		return g_strdup_printf("%.2f mm", n);
		}
	if (strcmp(key, "fISOSpeedRating") == 0)
		{
		gchar *text;

		text = exif_get_data_as_text(exif, "Exif.Photo.ISOSpeedRatings");
		/* kodak may set this instead */
		if (!text) text = exif_get_data_as_text(exif, "Exif.Photo.ExposureIndex");
		return text;
		}
	if (strcmp(key, "fSubjectDistance") == 0)
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
	if (strcmp(key, "fFlash") == 0)
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
	if (strcmp(key, "fResolution") == 0)
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

	if (key_valid) *key_valid = FALSE;
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

ExifData *exif_read_fd(FileData *fd, gint parse_color_profile)
{
	GList *work = fd->parent ? fd->parent->sidecar_files : fd->sidecar_files;
	gchar *sidecar_path = NULL;

	if (strcasecmp(fd->extension, ".cr2") == 0 || // FIXME: list of formats that can have xmp sidecar, make it configurable
	    strcasecmp(fd->extension, ".nef") == 0)
		{
		while(work)
			{
			FileData *sfd = work->data;
			work = work->next;
			printf("sfd %s\n", sfd->path);
			if (strcasecmp(sfd->extension, ".xmp") == 0)
				{
				sidecar_path = sfd->path;
				break;
				}
			}
		}


	// FIXME: some caching would be nice
	return exif_read(fd->path, sidecar_path, parse_color_profile);
}
