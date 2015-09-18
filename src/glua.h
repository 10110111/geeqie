/** \file
 * \short LUA implementation
 * \author Klaus Ethgen <Klaus@Ethgen.de>
 */

/*
 *  This file is a part of Geeqie project (http://www.geeqie.org/).
 *  Copyright (C) 2008 - 2010 The Geeqie Team
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 */

#ifndef __GLUA_H
#define __GLUA_H

#ifdef HAVE_LUA

#include <glib.h>
#include "main.h"

void lua_init(void);

gchar *lua_callvalue(FileData *fd, const gchar *file, const gchar *function);

#endif
#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
