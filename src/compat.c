/*
 * Geeqie
 * Copyright (C) 2008 - 2012 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik / Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "compat.h"

#if !GTK_CHECK_VERSION(2,24,0)
gint compat_gdk_window_get_width(GdkWindow *window)
{
	gint w, h;
	gdk_drawable_get_size(window, &w, &h);
	return w;
}

gint compat_gdk_window_get_height(GdkWindow *window)
{
	gint w, h;
	gdk_drawable_get_size(window, &w, &h);
	return h;
}
#endif

#if !GTK_CHECK_VERSION(2,22,0)
cairo_surface_t *compat_gdk_window_create_similar_surface (GdkWindow *window, cairo_content_t content, gint width, gint height)
{
	cairo_t *cr = gdk_cairo_create(window);
	cairo_surface_t *ws = cairo_get_target(cr);
	cairo_surface_t *ret = cairo_surface_create_similar(ws, content, width, height);
	cairo_destroy(cr);
	return ret;
}
#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
