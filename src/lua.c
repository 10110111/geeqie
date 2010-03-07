/** \file
 * \short LUA implementation
 * \author Klaus Ethgen <Klaus@Ethgen.de>
 */

/*
 *  This file is a part of Geeqie project (http://geeqie.sourceforge.net/).
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

#include "config.h"

#ifdef HAVE_LUA

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <glib.h>

#include "glua.h"
#include "ui_fileops.h"

static lua_State *L; /** The LUA object needed for all operations (NOTE: That is
		       * a upper-case variable to match the documentation!) */

/**
 * \brief Initialize the lua interpreter.
 */
void lua_init(void)
{
	L = luaL_newstate();
	luaL_openlibs(L); /* Open all libraries for lua programms */
}

/**
 * \brief Call a lua function to get a single value.
 */
gchar *lua_callvalue(gchar *file, gchar *function)
{
	gint result;
	gchar *data = NULL;
	gchar *dir;
	gchar *path;

	if (file[0] == '\0')
		{
		result = luaL_dostring(L, function);
		}
	else
		{
		dir = g_build_filename(get_rc_dir(), "lua", NULL);
		path = g_build_filename(dir, file, NULL);
		result = luaL_dofile(L, path);
		g_free(path);
		g_free(dir);
		}

	if (result)
		{
		data = g_strdup_printf("Error running lua script: %s", lua_tostring(L, -1));
		return data;
		}
	data = g_strdup(lua_tostring(L, -1));
	return data;
}

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
