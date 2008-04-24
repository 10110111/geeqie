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

#endif
