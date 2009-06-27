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

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
