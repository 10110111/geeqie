
#include "main.h"
#include "image-load.h"
#include "image_load_gdk.h"


static gchar* image_loader_gdk_get_format_name(GObject *loader)
{
	return gdk_pixbuf_format_get_name(gdk_pixbuf_loader_get_format(GDK_PIXBUF_LOADER(loader)));
}
static gchar** image_loader_gdk_get_format_mime_types(GObject *loader)
{
	return gdk_pixbuf_format_get_mime_types(gdk_pixbuf_loader_get_format(GDK_PIXBUF_LOADER(loader)));
}

static gpointer image_loader_gdk_new(GCallback area_updated_cb, GCallback size_cb, GCallback area_prepared_cb, gpointer data)
{
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        
	g_signal_connect(G_OBJECT(loader), "area_updated", area_updated_cb, data);
	g_signal_connect(G_OBJECT(loader), "size_prepared", size_cb, data);
	g_signal_connect(G_OBJECT(loader), "area_prepared", area_prepared_cb, data);
	return (gpointer) loader;
}

void image_loader_backend_set_default(ImageLoaderBackend *funcs)
{
	funcs->loader_new = image_loader_gdk_new;
	funcs->set_size = gdk_pixbuf_loader_set_size;
	funcs->write = gdk_pixbuf_loader_write;
	funcs->get_pixbuf = gdk_pixbuf_loader_get_pixbuf;
	funcs->close = gdk_pixbuf_loader_close;
	
	funcs->get_format_name = image_loader_gdk_get_format_name;
	funcs->get_format_mime_types = image_loader_gdk_get_format_mime_types;
}



