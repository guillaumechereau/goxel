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

#include "luagoxel.h"

#include "goxel.h"

#include "lauxlib.h"

void *luaG_checkpointer(lua_State *l, int narg, const char *type)
{
    void **p;
    p = luaL_checkudata(l, narg, type);
    return p ? *p : NULL;
}

int luaG_checkpos(lua_State *l, int idx, int pos[3])
{
    int i;
    luaL_checktype(l, idx, LUA_TTABLE);
    lua_pushvalue(l, idx);
    for (i = 0; i < 3; i++) {
        lua_pushinteger(l, i + 1);
        lua_gettable(l, -2);
        pos[i] = luaL_checkinteger(l, -1);
        lua_pop(l, 1);
    }
    lua_pop(l, 1);
    return 0;
}

int luaG_checkcolor(lua_State *l, int idx, uint8_t color[4])
{
    int i;
    luaL_checktype(l, idx, LUA_TTABLE);
    lua_pushvalue(l, idx);
    for (i = 0; i < 4; i++) {
        lua_pushinteger(l, i + 1);
        lua_gettable(l, -2);
        color[i] = luaL_checkinteger(l, -1);
        lua_pop(l, 1);
    }
    lua_pop(l, 1);
    return 0;
}
