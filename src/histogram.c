/*
 * Geeqie
 *
 * Author: Vladimir Nadvornik
 * based on a patch by Uwe Ohse
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "histogram.h"

#include "pixbuf_util.h"

#include <math.h>

/*
 *----------------------------------------------------------------------------
 * image histogram
 *----------------------------------------------------------------------------
 */

#define HISTOGRAM_SIZE 256

struct _Histogram {
	gulong histmap[HISTOGRAM_SIZE*4];
	gint histogram_chan;
	gint histogram_logmode;
};


Histogram *histogram_new()
{
	Histogram *histogram;

	histogram = g_new0(Histogram, 1);
	histogram->histogram_chan = HCHAN_RGB;
	histogram->histogram_logmode = 1;

	return histogram;
}

void histogram_free(Histogram *histogram)
{
	g_free(histogram);
}


gint histogram_set_channel(Histogram *histogram, gint chan)
{
	if (!histogram) return 0;
	histogram->histogram_chan = chan;
	return chan;
}

gint histogram_get_channel(Histogram *histogram)
{
	if (!histogram) return 0;
	return histogram->histogram_chan;
}

gint histogram_set_mode(Histogram *histogram, gint mode)
{
	if (!histogram) return 0;
	histogram->histogram_logmode = mode;
	return mode;
}

gint histogram_get_mode(Histogram *histogram)
{
	if (!histogram) return 0;
	return histogram->histogram_logmode;
}

const gchar *histogram_label(Histogram *histogram)
{
	const gchar *t1 = "";
	if (!histogram) return NULL;

	if (histogram->histogram_logmode)
			switch (histogram->histogram_chan)
			{
			case HCHAN_R:   t1 = _("logarithmical histogram on red"); break;
			case HCHAN_G:   t1 = _("logarithmical histogram on green"); break;
			case HCHAN_B:   t1 = _("logarithmical histogram on blue"); break;
			case HCHAN_VAL: t1 = _("logarithmical histogram on value"); break;
			case HCHAN_RGB: t1 = _("logarithmical histogram on RGB"); break;
			case HCHAN_MAX: t1 = _("logarithmical histogram on max value"); break;
			}
		else
			switch (histogram->histogram_chan)
			{
			case HCHAN_R:   t1 = _("linear histogram on red"); break;
			case HCHAN_G:   t1 = _("linear histogram on green"); break;
			case HCHAN_B:   t1 = _("linear histogram on blue"); break;
			case HCHAN_VAL: t1 = _("linear histogram on value"); break;
			case HCHAN_RGB: t1 = _("linear histogram on RGB"); break;
			case HCHAN_MAX: t1 = _("linear histogram on max value"); break;
			}
	return t1;
}

gulong histogram_read(Histogram *histogram, GdkPixbuf *imgpixbuf)
{
	gint w, h, i, j, srs, has_alpha;
	guchar *s_pix;

	if (!histogram) return 0;

	w = gdk_pixbuf_get_width(imgpixbuf);
	h = gdk_pixbuf_get_height(imgpixbuf);
	srs = gdk_pixbuf_get_rowstride(imgpixbuf);
	s_pix = gdk_pixbuf_get_pixels(imgpixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha(imgpixbuf);

	for (i = 0; i < HISTOGRAM_SIZE*4; i++) histogram->histmap[i] = 0;

	for (i = 0; i < h; i++)
		{
		guchar *sp = s_pix + (i * srs); /* 8bit */
		for (j = 0; j < w; j++)
			{
			histogram->histmap[sp[0] + 0 * HISTOGRAM_SIZE]++;
			histogram->histmap[sp[1] + 1 * HISTOGRAM_SIZE]++;
			histogram->histmap[sp[2] + 2 * HISTOGRAM_SIZE]++;
			if (histogram->histogram_chan == HCHAN_MAX)
				{
				guchar t = sp[0];
				if (sp[1]>t) t = sp[1];
				if (sp[2]>t) t = sp[2];
  				histogram->histmap[t + 3 * HISTOGRAM_SIZE]++;
				}
			else
  				histogram->histmap[3 * HISTOGRAM_SIZE + (sp[0]+sp[1]+sp[2])/3]++;
			sp += 3;
			if (has_alpha) sp++;
			}
		}

	return w*h;
}


gint histogram_draw(Histogram *histogram, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height)
{
	/* FIXME: use the coordinates correctly */
	gint i;
	gulong max = 0;
	double logmax;

	if (!histogram) return 0;

	for (i=0; i<1024; i++) {
		gint flag = 0;

		switch (histogram->histogram_chan)
		{
		case HCHAN_RGB: if ((i%4) != 3 ) flag = 1; break;
		case HCHAN_R: if ((i%4) == 0) flag = 1; break;
		case HCHAN_G: if ((i%4) == 1) flag = 1; break;
		case HCHAN_B: if ((i%4) == 2) flag = 1; break;
		case HCHAN_VAL: if ((i%4) == 3) flag = 1; break;
		case HCHAN_MAX: if ((i%4) == 3) flag = 1; break;
		}
		if (flag && histogram->histmap[i] > max) max = histogram->histmap[i];
	}

	logmax = log(max);
	for (i=0; i<width; i++)
		{
		gint j;
		glong v[4] = {0, 0, 0, 0};
		gint rplus = 0;
		gint gplus = 0;
		gint bplus = 0;
		gint ii = i * HISTOGRAM_SIZE / width;
		gint combine  = (HISTOGRAM_SIZE - 1) / width + 1;

		for (j = 0; j < combine; j++)
			{
			v[0] += histogram->histmap[ii + j + 0*HISTOGRAM_SIZE]; // r
			v[1] += histogram->histmap[ii + j + 1*HISTOGRAM_SIZE]; // g
			v[2] += histogram->histmap[ii + j + 2*HISTOGRAM_SIZE]; // b
			v[3] += histogram->histmap[ii + j + 3*HISTOGRAM_SIZE]; // value, max
			}

		for (j=0; j<4; j++)
			{
			gint r = rplus;
			gint g = gplus;
			gint b = bplus;
			gint max2 = 0;
			gint k;
			gulong pt;

			for (k=1; k<4; k++)
				if (v[k] > v[max2]) max2 = k;

			switch (max2)
			{
			case HCHAN_R: rplus = r = 255; break;
			case HCHAN_G: gplus = g = 255; break;
			case HCHAN_B: bplus = b = 255; break;
			}

			switch(histogram->histogram_chan)
			{
			case HCHAN_MAX: r = 0; b = 0; g = 0; break;
			case HCHAN_VAL: r = 0; b = 0; g = 0; break;
			case HCHAN_R: g = 0; b = 0; break;
			case HCHAN_G: r = 0; b = 0; break;
			case HCHAN_B: r = 0; g = 0; break;
			case HCHAN_RGB:
				if (r == 255 && g == 255 && b == 255) {
					r = 0;
					g = 0;
					b = 0;
				}
				break;
			}

			if (v[max2] == 0)
				pt = 0;
			else if (histogram->histogram_logmode)
				pt = ((float)log(v[max2])) / logmax * (height - 1);
			else
				pt = ((float)v[max2])/ max * (height - 1);
			if (histogram->histogram_chan >= HCHAN_RGB
			    || max2 == histogram->histogram_chan)
				pixbuf_draw_line(pixbuf,
					x, y, width, height,
					x + i, y + height, x + i, y + height-pt,
					r, g, b, 255);
			v[max2] = -1;
			}
		}
	return TRUE;
}
