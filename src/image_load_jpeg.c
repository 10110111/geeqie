
#include "main.h"
#include "image-load.h"
#include "image_load_jpeg.h"

#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>

typedef struct _ImageLoaderJpeg ImageLoaderJpeg;
struct _ImageLoaderJpeg {
	ImageLoaderBackendCbAreaUpdated area_updated_cb;
	ImageLoaderBackendCbSize size_cb;
	ImageLoaderBackendCbAreaPrepared area_prepared_cb;
	
	gpointer data;
	
	GdkPixbuf *pixbuf;
	guint requested_width;
	guint requested_height;
	
	gboolean abort;
	
};

/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	sigjmp_buf setjmp_buffer;
        GError **error;
};

/* explode gray image data from jpeg library into rgb components in pixbuf */
static void
explode_gray_into_buf (struct jpeg_decompress_struct *cinfo,
		       guchar **lines) 
{
	gint i, j;
	guint w;

	g_return_if_fail (cinfo != NULL);
	g_return_if_fail (cinfo->output_components == 1);
	g_return_if_fail (cinfo->out_color_space == JCS_GRAYSCALE);

	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	w = cinfo->output_width;
	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		guchar *from, *to;
		
		from = lines[i] + w - 1;
		to = lines[i] + (w - 1) * 3;
		for (j = w - 1; j >= 0; j--) {
			to[0] = from[0];
			to[1] = from[0];
			to[2] = from[0];
			to -= 3;
			from--;
		}
	}
}


static void
convert_cmyk_to_rgb (struct jpeg_decompress_struct *cinfo,
		     guchar **lines) 
{
	gint i, j;

	g_return_if_fail (cinfo != NULL);
	g_return_if_fail (cinfo->output_components == 4);
	g_return_if_fail (cinfo->out_color_space == JCS_CMYK);

	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		guchar *p;
		
		p = lines[i];
		for (j = 0; j < cinfo->output_width; j++) {
			int c, m, y, k;
			c = p[0];
			m = p[1];
			y = p[2];
			k = p[3];
			if (cinfo->saw_Adobe_marker) {
				p[0] = k*c / 255;
				p[1] = k*m / 255;
				p[2] = k*y / 255;
			}
			else {
				p[0] = (255 - k)*(255 - c) / 255;
				p[1] = (255 - k)*(255 - m) / 255;
				p[2] = (255 - k)*(255 - y) / 255;
			}
			p[3] = 255;
			p += 4;
		}
	}
}


static gpointer image_loader_jpeg_new(ImageLoaderBackendCbAreaUpdated area_updated_cb, ImageLoaderBackendCbSize size_cb, ImageLoaderBackendCbAreaPrepared area_prepared_cb, gpointer data)
{
        ImageLoaderJpeg *loader = g_new0(ImageLoaderJpeg, 1);
        
	loader->area_updated_cb = area_updated_cb;
	loader->size_cb = size_cb;
	loader->area_prepared_cb = area_prepared_cb;
	loader->data = data;
	return (gpointer) loader;
}

static void
fatal_error_handler (j_common_ptr cinfo)
{
	struct error_handler_data *errmgr;
        char buffer[JMSG_LENGTH_MAX];
        
	errmgr = (struct error_handler_data *) cinfo->err;
        
        /* Create the message */
        (* cinfo->err->format_message) (cinfo, buffer);

        /* broken check for *error == NULL for robustness against
         * crappy JPEG library
         */
        if (errmgr->error && *errmgr->error == NULL) {
                g_set_error (errmgr->error,
                             GDK_PIXBUF_ERROR,
                             cinfo->err->msg_code == JERR_OUT_OF_MEMORY 
			     ? GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY 
			     : GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Error interpreting JPEG image file (%s)"),
                             buffer);
        }
        
	siglongjmp (errmgr->setjmp_buffer, 1);

        g_assert_not_reached ();
}

static void
output_message_handler (j_common_ptr cinfo)
{
  /* This method keeps libjpeg from dumping crap to stderr */

  /* do nothing */
}

static gboolean image_loader_jpeg_load (gpointer loader, const guchar *buf, gsize count, GError **error)
{
	ImageLoaderJpeg *lj = (ImageLoaderJpeg *) loader;
	struct jpeg_decompress_struct cinfo;
	guchar *dptr;
	guint rowstride;

	struct error_handler_data jerr;
//	stdio_src_ptr src;

	/* setup error handler */
	cinfo.err = jpeg_std_error (&jerr.pub);
	jerr.pub.error_exit = fatal_error_handler;
        jerr.pub.output_message = output_message_handler;

        jerr.error = error;


	if (setjmp(jerr.setjmp_buffer)) 
		{
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		*/
		jpeg_destroy_decompress(&cinfo);
		return FALSE;
		}
	
	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, (unsigned char *)buf, count);


	jpeg_read_header(&cinfo, TRUE);

	lj->requested_width = cinfo.image_width;
	lj->requested_height = cinfo.image_height;
	lj->size_cb(loader, cinfo.image_width, cinfo.image_height, lj->data);
			
	cinfo.scale_num = 1;
	for (cinfo.scale_denom = 2; cinfo.scale_denom <= 8; cinfo.scale_denom *= 2) {
		jpeg_calc_output_dimensions(&cinfo);
		if (cinfo.output_width < lj->requested_width || cinfo.output_height < lj->requested_height) {
			cinfo.scale_denom /= 2;
			break;
		}
	}
	jpeg_calc_output_dimensions(&cinfo);

	jpeg_start_decompress(&cinfo);
	
	lj->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
				     cinfo.out_color_components == 4 ? TRUE : FALSE, 
				     8, cinfo.output_width, cinfo.output_height);

	lj->area_prepared_cb(loader, lj->data);
	rowstride = gdk_pixbuf_get_rowstride(lj->pixbuf);
	dptr = gdk_pixbuf_get_pixels(lj->pixbuf);
	      
	if (!lj->pixbuf) 
		{
		jpeg_destroy_decompress (&cinfo);
		return 0;
		}

	while (cinfo.output_scanline < cinfo.output_height && !lj->abort) 
		{
		guchar *lines[4];
		guchar **lptr;
		gint i;
		guint scanline = cinfo.output_scanline;

		lptr = lines;
		for (i = 0; i < cinfo.rec_outbuf_height; i++) 
			{
			*lptr++ = dptr;
			dptr += rowstride;
			}

		jpeg_read_scanlines (&cinfo, lines, cinfo.rec_outbuf_height);

		switch (cinfo.out_color_space) 
			{
			    case JCS_GRAYSCALE:
			      explode_gray_into_buf (&cinfo, lines);
			      break;
			    case JCS_RGB:
			      /* do nothing */
			      break;
			    case JCS_CMYK:
			      convert_cmyk_to_rgb (&cinfo, lines);
			      break;
			    default:
			      break;
			}
		lj->area_updated_cb(loader, 0, scanline, cinfo.output_width, cinfo.rec_outbuf_height, lj->data);
		}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	return TRUE;
}

static void image_loader_jpeg_set_size(gpointer loader, int width, int height)
{
	ImageLoaderJpeg *lj = (ImageLoaderJpeg *) loader;
	lj->requested_width = width;
	lj->requested_height = height;
}

static GdkPixbuf* image_loader_jpeg_get_pixbuf(gpointer loader)
{
	ImageLoaderJpeg *lj = (ImageLoaderJpeg *) loader;
	return lj->pixbuf;
}

static gchar* image_loader_jpeg_get_format_name(gpointer loader)
{
	return g_strdup("jpeg");
}
static gchar** image_loader_jpeg_get_format_mime_types(gpointer loader)
{
	static gchar *mime[] = {"image/jpeg", NULL};
	return g_strdupv(mime);
}

static gboolean image_loader_jpeg_close(gpointer loader, GError **error)
{
	return TRUE;
}

static void image_loader_jpeg_abort(gpointer loader)
{
	ImageLoaderJpeg *lj = (ImageLoaderJpeg *) loader;
	lj->abort = TRUE;
}

static void image_loader_jpeg_free(gpointer loader)
{
	ImageLoaderJpeg *lj = (ImageLoaderJpeg *) loader;
	if (lj->pixbuf) g_object_unref(lj->pixbuf);
	g_free(lj);
}


void image_loader_backend_set_jpeg(ImageLoaderBackend *funcs)
{
	funcs->loader_new = image_loader_jpeg_new;
	funcs->set_size = image_loader_jpeg_set_size;
	funcs->load = image_loader_jpeg_load;
	funcs->write = NULL;
	funcs->get_pixbuf = image_loader_jpeg_get_pixbuf;
	funcs->close = image_loader_jpeg_close;
	funcs->abort = image_loader_jpeg_abort;
	funcs->free = image_loader_jpeg_free;
	
	funcs->get_format_name = image_loader_jpeg_get_format_name;
	funcs->get_format_mime_types = image_loader_jpeg_get_format_mime_types;
}



