/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik, Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "debug.h"

#ifdef DEBUG

static gint debug_level = DEBUG_LEVEL_MIN;


gint get_debug_level(void)
{
	return debug_level;
}

void set_debug_level(gint new_level)
{
	debug_level = CLAMP(new_level, DEBUG_LEVEL_MIN, DEBUG_LEVEL_MAX);	
}

void debug_level_add(gint delta)
{
	set_debug_level(debug_level + delta);
}

gint required_debug_level(gint level)
{
	return (debug_level >= level);
}

static gint timeval_delta(struct timeval *result, struct timeval *x, struct timeval *y)
{
	if (x->tv_usec < y->tv_usec)
		{
		gint nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
		}

	if (x->tv_usec - y->tv_usec > 1000000)
		{
		gint nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	return x->tv_sec < y->tv_sec;
}

const gchar *get_exec_time(void)
{
	static gchar timestr[30];
	static struct timeval start_tv = {0, 0};
	static struct timeval previous = {0, 0};
	static gint started = 0;

	struct timeval tv = {0, 0};
	static struct timeval delta = {0, 0};

	gettimeofday(&tv, NULL);

	if (start_tv.tv_sec == 0) start_tv = tv;

	tv.tv_sec -= start_tv.tv_sec;
	if (tv.tv_usec >= start_tv.tv_usec)
		tv.tv_usec -= start_tv.tv_usec;
	else
		{
		tv.tv_usec += 1000000 - start_tv.tv_usec;
		tv.tv_sec -= 1;
		}

	if (started) timeval_delta(&delta, &tv, &previous);

	previous = tv;
	started = 1;

	g_snprintf(timestr, sizeof(timestr), "%5d.%06d (+%05d.%06d)", (int)tv.tv_sec, (int)tv.tv_usec, (int)delta.tv_sec, (int)delta.tv_usec);

	return timestr;
}

void init_exec_time(void)
{
	get_exec_time();
}

#endif
