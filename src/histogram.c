/*
 * Geeqie
 * Copyright (C) 2008 - 2009 The Geeqie Team
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

#define HISTMAP_SIZE 256
typedef enum {
	HISTMAP_CHANNEL_R = 0,
	HISTMAP_CHANNEL_G,
	HISTMAP_CHANNEL_B,
	HISTMAP_CHANNEL_AVG,
	HISTMAP_CHANNEL_MAX,
	HISTMAP_CHANNELS
} HistMapChannels;

struct _HistMap {
	gulong histmap[HISTMAP_SIZE * HISTMAP_CHANNELS];
	gulong area;
};

struct _Histogram {
	gint histogram_chan;
	gint histogram_logmode;
};


Histogram *histogram_new(void)
{
	Histogram *histogram;

	histogram = g_new0(Histogram, 1);
	histogram->histogram_chan = options->histogram.last_channel_mode;
	histogram->histogram_logmode = options->histogram.last_log_mode;

	return histogram;
}

void histogram_free(Histogram *histogram)
{
	g_free(histogram);
}


gint histogram_set_channel(Histogram *histogram, gint chan)
{
	if (!histogram) return 0;
	options->histogram.last_channel_mode = histogram->histogram_chan = chan;
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
	options->histogram.last_log_mode = histogram->histogram_logmode = mode;
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

static HistMap *histmap_read(GdkPixbuf *imgpixbuf)
{
	gint w, h, i, j, srs, has_alpha, step;
	guchar *s_pix;

	HistMap *histmap;
	
	w = gdk_pixbuf_get_width(imgpixbuf);
	h = gdk_pixbuf_get_height(imgpixbuf);
	srs = gdk_pixbuf_get_rowstride(imgpixbuf);
	s_pix = gdk_pixbuf_get_pixels(imgpixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha(imgpixbuf);

	histmap = g_new0(HistMap, 1);

	step = 3 + !!(has_alpha);
	for (i = 0; i < h; i++)
		{
		guchar *sp = s_pix + (i * srs); /* 8bit */
		for (j = 0; j < w; j++)
			{
			guint avg = (sp[0] + sp[1] + sp[2]) / 3;
			guint max = sp[0];
			if (sp[1] > max) max = sp[1];
			if (sp[2] > max) max = sp[2];

			histmap->histmap[sp[0] * HISTMAP_CHANNELS + HISTMAP_CHANNEL_R]++;
			histmap->histmap[sp[1] * HISTMAP_CHANNELS + HISTMAP_CHANNEL_G]++;
			histmap->histmap[sp[2] * HISTMAP_CHANNELS + HISTMAP_CHANNEL_B]++;
			histmap->histmap[avg   * HISTMAP_CHANNELS + HISTMAP_CHANNEL_AVG]++;
			histmap->histmap[max   * HISTMAP_CHANNELS + HISTMAP_CHANNEL_MAX]++;
			sp += step;
			}
		}
	histmap->area = w * h;
	return histmap;
}

const HistMap *histmap_get(FileData *fd)
{
	if (fd->histmap) return fd->histmap;
	
	if (fd->pixbuf) 
		{
		fd->histmap = histmap_read(fd->pixbuf);
		return fd->histmap;
		}
	return NULL;
}

gint histogram_draw(Histogram *histogram, const HistMap *histmap, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height)
{
	/* FIXME: use the coordinates correctly */
	gint i;
	gulong max = 0;
	gdouble logmax;

	if (!histogram || !histmap) return 0;

	for (i = 0; i < 1024; i++) {
#if 0
		/* this is probably broken for MAX or VAL mode */
		gint flag = 0;

		switch (histogram->histogram_chan)
			{
			case HCHAN_RGB: if ((i % HISTMAP_CHANNELS) < 3) flag = 1; break;
			case HCHAN_R:   if ((i % HISTMAP_CHANNELS) == HISTMAP_CHANNEL_R) flag = 1; break;
			case HCHAN_G:   if ((i % HISTMAP_CHANNELS) == HISTMAP_CHANNEL_G) flag = 1; break;
			case HCHAN_B:   if ((i % HISTMAP_CHANNELS) == HISTMAP_CHANNEL_B) flag = 1; break;
			case HCHAN_VAL: if ((i % HISTMAP_CHANNELS) == HISTMAP_CHANNEL_AVG) flag = 1; break;
			case HCHAN_MAX: if ((i % HISTMAP_CHANNELS) == HISTMAP_CHANNEL_MAX) flag = 1; break;
			}
		if (flag && histmap->histmap[i] > max) max = histmap->histmap[i];
#else
		if (histmap->histmap[i] > max) max = histmap->histmap[i];
#endif
	}

	logmax = log(max);
	for (i = 0; i < width; i++)
		{
		gint j;
		glong v[4] = {0, 0, 0, 0};
		gint rplus = 0;
		gint gplus = 0;
		gint bplus = 0;
		gint ii = i * HISTMAP_SIZE / width;
		gint combine  = (HISTMAP_SIZE - 1) / width + 1;

		for (j = 0; j < combine; j++)
			{
			v[0] += histmap->histmap[(ii + j) * HISTMAP_CHANNELS + HISTMAP_CHANNEL_R]; // r
			v[1] += histmap->histmap[(ii + j) * HISTMAP_CHANNELS + HISTMAP_CHANNEL_G]; // g
			v[2] += histmap->histmap[(ii + j) * HISTMAP_CHANNELS + HISTMAP_CHANNEL_B]; // b
			v[3] += histmap->histmap[(ii + j) * HISTMAP_CHANNELS + 
			        ((histogram->histogram_chan == HCHAN_VAL) ? HISTMAP_CHANNEL_AVG : HISTMAP_CHANNEL_MAX)]; // value, max
			}
			
		for (j = 0; j < 4; j++)
			{
			v[j] /= combine;
			}

		for (j = 0; j < 4; j++)
			{
			gint max2 = 0;
			gint k;
		
			for (k = 1; k < 4; k++)
				if (v[k] > v[max2]) max2 = k;
			
			if (histogram->histogram_chan >= HCHAN_RGB
			    || max2 == histogram->histogram_chan)
			    	{
				gulong pt;
				gint r = rplus;
				gint g = gplus;
				gint b = bplus;

				switch (max2)
					{
					case HCHAN_R: rplus = r = 255; break;
					case HCHAN_G: gplus = g = 255; break;
					case HCHAN_B: bplus = b = 255; break;
					}

				switch (histogram->histogram_chan)
					{
					case HCHAN_RGB:
						if (r == 255 && g == 255 && b == 255)
							{
							r = 0; b = 0; g = 0;
							}
						break;
					case HCHAN_R:          b = 0; g = 0; break;
					case HCHAN_G:   r = 0; b = 0;        break;
					case HCHAN_B:   r = 0;        g = 0; break;
					case HCHAN_MAX:
					case HCHAN_VAL: r = 0; b = 0; g = 0; break;
					}
				
				if (v[max2] == 0)
					pt = 0;
				else if (histogram->histogram_logmode)
					pt = ((float)log(v[max2])) / logmax * (height - 1);
				else
					pt = ((float)v[max2])/ max * (height - 1);

				pixbuf_draw_line(pixbuf,
					x, y, width, height,
					x + i, y + height, x + i, y + height - pt,
					r, g, b, 255);
				}
			v[max2] = -1;
			}
		}

	return TRUE;
}

void histogram_notify_cb(FileData *fd, NotifyType type, gpointer data)
{
	if (type != NOTIFY_TYPE_INTERNAL && fd->histmap)
		{
		g_free(fd->histmap);
		fd->histmap = NULL;
		}
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
