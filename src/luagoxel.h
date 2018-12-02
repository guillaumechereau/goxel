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

#ifndef LUAGOXEL_H
#define LUAGOXEL_H

#include <stdint.h>

#include "lua.h"

/*
 * File: luagoxel.h
 * Some util functions to use lua from goxel.
 */

void *luaG_checkpointer(lua_State *l, int idx, const char *type);

int luaG_checkpos(lua_State *l, int idx, int pos[3]);
int luaG_checkcolor(lua_State *l, int idx, uint8_t color[4]);

#endif
