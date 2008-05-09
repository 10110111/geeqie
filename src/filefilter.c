/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "filefilter.h"

#include "cache.h"
#include "debug.h"
#include "rcfile.h"
#include "secure_save.h"
#include "thumb_standard.h"
#include "ui_fileops.h"


/*
 *-----------------------------------------------------------------------------
 * file filtering
 *-----------------------------------------------------------------------------
 */

static GList *filter_list = NULL;
static GList *extension_list = NULL;
static GList *sidecar_ext_list = NULL;

static GList *file_class_extension_list[FILE_FORMAT_CLASSES];


gint ishidden(const gchar *name)
{
	if (name[0] != '.') return FALSE;
	if (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) return FALSE;
	return TRUE;
}

static FilterEntry *filter_entry_new(const gchar *key, const gchar *description,
				     const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	FilterEntry *fe;

	fe = g_new0(FilterEntry, 1);
	fe->key = g_strdup(key);
	fe->description = g_strdup(description);
	fe->extensions = g_strdup(extensions);
	fe->enabled = enabled;
	fe->file_class = file_class;

	return fe;
}

static void filter_entry_free(FilterEntry *fe)
{
	if (!fe) return;

	g_free(fe->key);
	g_free(fe->description);
	g_free(fe->extensions);
	g_free(fe);
}

GList *filter_get_list(void)
{
	return filter_list;
}

void filter_remove_entry(FilterEntry *fe)
{
	if (!g_list_find(filter_list, fe)) return;

	filter_list = g_list_remove(filter_list, fe);
	filter_entry_free(fe);
}

static gint filter_key_exists(const gchar *key)
{
	GList *work;

	if (!key) return FALSE;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;

		if (strcmp(fe->key, key) == 0) return TRUE;
		}

	return FALSE;
}

void filter_add(const gchar *key, const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	filter_list = g_list_append(filter_list, filter_entry_new(key, description, extensions, file_class, enabled));
}

void filter_add_unique(const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	gchar *key;
	gint n;

	key = g_strdup("user0");
	n = 1;
	while (filter_key_exists(key))
		{
		g_free(key);
		if (n > 999) return;
		key = g_strdup_printf("user%d", n);
		n++;
		}

	filter_add(key, description, extensions, file_class, enabled);
	g_free(key);
}

static void filter_add_if_missing(const gchar *key, const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	GList *work;

	if (!key) return;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;
		if (fe->key && strcmp(fe->key, key) == 0)
			{
			if (fe->file_class == FORMAT_CLASS_UNKNOWN)
				fe->file_class = file_class;	/* for compatibility */
			return;
			}
		}

	filter_add(key, description, extensions, file_class, enabled);
}

void filter_reset(void)
{
	GList *work;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;
		filter_entry_free(fe);
		}

	g_list_free(filter_list);
	filter_list = NULL;
}

void filter_add_defaults(void)
{
	GSList *list, *work;

	list = gdk_pixbuf_get_formats();
	work = list;
	while (work)
		{
		GdkPixbufFormat *format;
		gchar *name;
		gchar *desc;
		gchar **extensions;
		GString *filter = NULL;
		gint i;

		format = work->data;
		work = work->next;

		name = gdk_pixbuf_format_get_name(format);
		desc = gdk_pixbuf_format_get_description(format);
		extensions = gdk_pixbuf_format_get_extensions(format);

		i = 0;
		while (extensions[i])
			{
			if (!filter)
				{
				filter = g_string_new(".");
				filter = g_string_append(filter, extensions[i]);
				}
			else
				{
				filter = g_string_append(filter, ";.");
				filter = g_string_append(filter, extensions[i]);
				}
			i++;
			}

		DEBUG_1("loader reported [%s] [%s] [%s]", name, desc, filter->str);

		filter_add_if_missing(name, desc, filter->str, FORMAT_CLASS_IMAGE, TRUE);

		g_free(name);
		g_free(desc);
		g_strfreev(extensions);
		g_string_free(filter, TRUE);
		}
	g_slist_free(list);

	/* add defaults even if gdk-pixbuf does not have them, but disabled */
	filter_add_if_missing("jpeg", "JPEG group", ".jpg;.jpeg;.jpe", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("png", "Portable Network Graphic", ".png", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("tiff", "Tiff", ".tif;.tiff", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("pnm", "Packed Pixel formats", ".pbm;.pgm;.pnm;.ppm", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("gif", "Graphics Interchange Format", ".gif", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("xbm", "X bitmap", ".xbm", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("xpm", "X pixmap", ".xpm", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("bmp", "Bitmap", ".bmp", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("ico", "Icon file", ".ico;.cur", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("ras", "Raster", ".ras", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("svg", "Scalable Vector Graphics", ".svg", FORMAT_CLASS_IMAGE, FALSE);

	/* non-image files that might be desirable to show */
	filter_add_if_missing("xmp", "XMP sidecar", ".xmp", FORMAT_CLASS_META, TRUE);
	filter_add_if_missing("gqv", GQ_APPNAME " image collection", ".gqv", FORMAT_CLASS_META, TRUE);

	/* These are the raw camera formats with embedded jpeg/exif.
	 * (see format_raw.c and/or exiv2.cc)
	 */
	filter_add_if_missing("arw", "Sony raw format", ".arw;.srf;.sr2", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("crw", "Canon raw format", ".crw;.cr2", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("kdc", "Kodak raw format", ".kdc;.dcr", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("raf", "Fujifilm raw format", ".raf", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("mef", "Mamiya raw format", ".mef;.mos", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("mrw", "Minolta raw format", ".mrw", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("nef", "Nikon raw format", ".nef", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("orf", "Olympus raw format", ".orf", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("pef", "Pentax or Samsung raw format", ".pef;.ptx", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("dng", "Adobe Digital Negative raw format", ".dng", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("x3f", "Sigma raw format", ".x3f", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("raw", "Panasonic raw format", ".raw", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("r3d", "Red raw format", ".r3d", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("3fr", "Hasselblad raw format", ".3fr", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("erf", "Epson raw format", ".erf", FORMAT_CLASS_RAWIMAGE, TRUE);
}

GList *filter_to_list(const gchar *extensions)
{
	GList *list = NULL;
	const gchar *p;

	if (!extensions) return NULL;

	p = extensions;
	while (*p != '\0')
		{
		const gchar *b;
		gint l = 0;

		b = p;
		while (*p != '\0' && *p != ';')
			{
			p++;
			l++;
			}
		list = g_list_append(list, g_strndup(b, l));
		if (*p == ';') p++;
		}

	return list;
}

void filter_rebuild(void)
{
	GList *work;
	gint i;

	string_list_free(extension_list);
	extension_list = NULL;

	for (i = 0; i < FILE_FORMAT_CLASSES; i++)
		{
		string_list_free(file_class_extension_list[i]);
		file_class_extension_list[i] = NULL;
		}

	work = filter_list;
	while (work)
		{
		FilterEntry *fe;

		fe = work->data;
		work = work->next;

		if (fe->enabled)
			{
			GList *ext;

			ext = filter_to_list(fe->extensions);
			if (ext) extension_list = g_list_concat(extension_list, ext);

			if (fe->file_class >= 0 && fe->file_class < FILE_FORMAT_CLASSES)
				{
				ext = filter_to_list(fe->extensions);
				if (ext) file_class_extension_list[fe->file_class] = g_list_concat(file_class_extension_list[fe->file_class], ext);
				}
			else
				{
				printf("WARNING: invalid file class %d\n", fe->file_class);
				}
			}
		}
}

gint filter_name_exists(const gchar *name)
{
	GList *work;
	gint ln;

	if (!extension_list || options->file_filter.disable) return TRUE;

	ln = strlen(name);
	work = extension_list;
	while (work)
		{
		gchar *filter = work->data;
		gint lf = strlen(filter);

		if (ln >= lf)
			{
			/* FIXME: utf8 */
			if (strncasecmp(name + ln - lf, filter, lf) == 0) return TRUE;
			}
		work = work->next;
		}

	return FALSE;
}

gint filter_file_class(const gchar *name, FileFormatClass file_class)
{
	GList *work;
	gint ln;

	if (file_class < 0 || file_class >= FILE_FORMAT_CLASSES)
		{
		printf("WARNING: invalid file class %d\n", file_class);
		return FALSE;
		}

	ln = strlen(name);
	work = file_class_extension_list[file_class];
	while (work)
		{
		gchar *filter = work->data;
		gint lf = strlen(filter);

		if (ln >= lf)
			{
			/* FIXME: utf8 */
			if (strncasecmp(name + ln - lf, filter, lf) == 0) return TRUE;
			}
		work = work->next;
		}

	return FALSE;
}

void filter_write_list(SecureSaveInfo *ssi)
{
	GList *work;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;

		gchar *extensions = escquote_value(fe->extensions);
		gchar *description = escquote_value(fe->description);

		secure_fprintf(ssi, "file_filter.ext: \"%s%s\" %s %s %d\n",
			       (fe->enabled) ? "" : "#",
			       fe->key, extensions, description, fe->file_class);
		g_free(extensions);
		g_free(description);
		}
}

void filter_parse(const gchar *text)
{
	const gchar *p;
	gchar *key;
	gchar *ext;
	gchar *desc;
	gint enabled = TRUE;
	gint file_class;

	if (!text || text[0] != '"') return;

	key = quoted_value(text, &p);
	if (!key) return;

	ext = quoted_value(p, &p);
	desc = quoted_value(p, &p);

	file_class = strtol(p, NULL, 10);

	if (file_class < 0 || file_class >= FILE_FORMAT_CLASSES) file_class = FORMAT_CLASS_UNKNOWN;

	if (key && key[0] == '#')
		{
		gchar *tmp;
		tmp = g_strdup(key + 1);
		g_free(key);
		key = tmp;

		enabled = FALSE;
		}

	if (key && strlen(key) > 0 && ext) filter_add(key, desc, ext, file_class, enabled);

	g_free(key);
	g_free(ext);
	g_free(desc);
}

/*
 *-----------------------------------------------------------------------------
 * sidecar extension list
 *-----------------------------------------------------------------------------
 */

GList *sidecar_ext_get_list(void)
{
	return sidecar_ext_list;
}

void sidecar_ext_parse(const gchar *text, gint quoted)
{
	GList *work;
	gchar *value;

	work = sidecar_ext_list;
	while (work)
		{
		gchar *ext = work->data;
		work = work->next;
		g_free(ext);
		}
	g_list_free(sidecar_ext_list);
	sidecar_ext_list = NULL;

	if (quoted)
		value = quoted_value(text, NULL);
	else
		value = g_strdup(text);

	if (value == NULL) return;

	sidecar_ext_list = filter_to_list(value);

	g_free(value);
}

void sidecar_ext_write(SecureSaveInfo *ssi)
{
	secure_fprintf(ssi, "sidecar.ext: \"%s\"\n", sidecar_ext_to_string());
}

gchar *sidecar_ext_to_string(void)
{
	GList *work;
	GString *str = g_string_new("");

	work = sidecar_ext_list;
	while (work)
		{
		gchar *ext = work->data;
		work = work->next;
		g_string_append(str, ext);
		if (work) g_string_append(str, ";");
		}
	return g_string_free(str, FALSE);
}

void sidecar_ext_add_defaults(void)
{
	sidecar_ext_parse(".jpg;.cr2;.nef;.crw;.xmp", FALSE);
}
