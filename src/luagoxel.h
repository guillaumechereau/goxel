/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * File: luagoxel.h
 * Some util functions to use lua from goxel.
 */

#ifndef LUAGOXEL_H
#define LUAGOXEL_H

#define LUA_USE_POSIX

#include <stdint.h>
#include <stdlib.h>

#ifndef HAS_SYSTEM
#   define HAS_SYSTEM 1
#endif
#if !HAS_SYSTEM
#   define system(x) -1
#endif

#include "../ext_src/lua/lauxlib.h"
#include "../ext_src/lua/lua.h"

void *luaG_checkpointer(lua_State *l, int idx, const char *type);

int luaG_checkpos(lua_State *l, int idx, int pos[3]);
int luaG_checkcolor(lua_State *l, int idx, uint8_t color[4]);
int luaG_checkaabb(lua_State *l, int idx, int aabb[2][3]);

void luaG_newintarray(lua_State *l, int n, const int *v);

#endif
