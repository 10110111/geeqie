/*
 * Copyright (C) 20019 - The Geeqie Team
 *
 * Author: Colin Clark
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "main.h"

#include "image-load.h"
#include "image_load_heif.h"

#ifdef HAVE_HEIF
#include <libheif/heif.h>

typedef struct _ImageLoaderHEIF ImageLoaderHEIF;
struct _ImageLoaderHEIF {
	ImageLoaderBackendCbAreaUpdated area_updated_cb;
	ImageLoaderBackendCbSize size_cb;
	ImageLoaderBackendCbAreaPrepared area_prepared_cb;
	gpointer data;
	GdkPixbuf *pixbuf;
	guint requested_width;
	guint requested_height;
	gboolean abort;
};

static void free_buffer(guchar *pixels, gpointer data)
{
	g_free (pixels);
}

static gboolean image_loader_heif_load(gpointer loader, const guchar *buf, gsize count, GError **error)
{
	ImageLoaderHEIF *ld = (ImageLoaderHEIF *) loader;
	struct heif_context* ctx;
	struct heif_image* img;
	struct heif_error error_code;
	struct heif_image_handle* handle;
	guint8* data;
	gint width, height;
	gint stride;
	gboolean alpha;

	ctx = heif_context_alloc();

	error_code = heif_context_read_from_memory_without_copy(ctx, buf, count, NULL);
	if (error_code.code)
		{
		log_printf("warning: heif reader error: %s\n", error_code.message);
		heif_context_free(ctx);
		return FALSE;
		}

	// get a handle to the primary image
	error_code = heif_context_get_primary_image_handle(ctx, &handle);
	if (error_code.code)
		{
		log_printf("warning: heif reader error: %s\n", error_code.message);
		heif_context_free(ctx);
		return FALSE;
		}

	// decode the image and convert colorspace to RGB, saved as 24bit interleaved
	error_code = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_24bit, NULL);
	if (error_code.code)
		{
		log_printf("warning: heif reader error: %s\n", error_code.message);
		heif_context_free(ctx);
		return FALSE;
		}

	data = heif_image_get_plane(img, heif_channel_interleaved, &stride);

	height = heif_image_get_height(img,heif_channel_interleaved);
	width = heif_image_get_width(img,heif_channel_interleaved);
	alpha = heif_image_handle_has_alpha_channel(handle);

	ld->pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, alpha, 8, width, height, stride, free_buffer, NULL);

	ld->area_updated_cb(loader, 0, 0, width, height, ld->data);

	heif_context_free(ctx);

	return TRUE;
}

static gpointer image_loader_heif_new(ImageLoaderBackendCbAreaUpdated area_updated_cb, ImageLoaderBackendCbSize size_cb, ImageLoaderBackendCbAreaPrepared area_prepared_cb, gpointer data)
{
	ImageLoaderHEIF *loader = g_new0(ImageLoaderHEIF, 1);
	loader->area_updated_cb = area_updated_cb;
	loader->size_cb = size_cb;
	loader->area_prepared_cb = area_prepared_cb;
	loader->data = data;
	return (gpointer) loader;
}

static void image_loader_heif_set_size(gpointer loader, int width, int height)
{
	ImageLoaderHEIF *ld = (ImageLoaderHEIF *) loader;
	ld->requested_width = width;
	ld->requested_height = height;
}

static GdkPixbuf* image_loader_heif_get_pixbuf(gpointer loader)
{
	ImageLoaderHEIF *ld = (ImageLoaderHEIF *) loader;
	return ld->pixbuf;
}

static gchar* image_loader_heif_get_format_name(gpointer loader)
{
	return g_strdup("heif");
}

static gchar** image_loader_heif_get_format_mime_types(gpointer loader)
{
	static gchar *mime[] = {"image/heic", NULL};
	return g_strdupv(mime);
}

static gboolean image_loader_heif_close(gpointer loader, GError **error)
{
	return TRUE;
}

static void image_loader_heif_abort(gpointer loader)
{
	ImageLoaderHEIF *ld = (ImageLoaderHEIF *) loader;
	ld->abort = TRUE;
}

static void image_loader_heif_free(gpointer loader)
{
	ImageLoaderHEIF *ld = (ImageLoaderHEIF *) loader;
	if (ld->pixbuf) g_object_unref(ld->pixbuf);
	g_free(ld);
}

void image_loader_backend_set_heif(ImageLoaderBackend *funcs)
{
	funcs->loader_new = image_loader_heif_new;
	funcs->set_size = image_loader_heif_set_size;
	funcs->load = image_loader_heif_load;
	funcs->write = NULL;
	funcs->get_pixbuf = image_loader_heif_get_pixbuf;
	funcs->close = image_loader_heif_close;
	funcs->abort = image_loader_heif_abort;
	funcs->free = image_loader_heif_free;
	funcs->get_format_name = image_loader_heif_get_format_name;
	funcs->get_format_mime_types = image_loader_heif_get_format_mime_types;
}

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
