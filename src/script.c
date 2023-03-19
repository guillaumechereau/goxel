/* Goxel 3D voxels editor
 *
 * copyright (c) 2023-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "goxel.h"
#include "script.h"

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"

int script_run(const char *filename, int argc, const char **argv)
{
    lua_State *l;
    char *script;
    int ret = 0, i;

    script = read_file(filename, 0);
    if (!script) {
        fprintf(stderr, "Cannot read '%s'\n", filename);
        return -1;
    }
    l = luaL_newstate();
    luaL_openlibs(l);
    // Put the arguments.
    lua_newtable(l);
    for (i = 0; i < argc; i++) {
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, argv[i]);
        lua_rawset(l, -3);
    }
    lua_setglobal(l, "arg");
    luaL_loadstring(l, script);
    if (lua_pcall(l, 0, 0, 0)) {
        fprintf(stderr, "ERROR:\n%s\n", lua_tostring(l, -1));
        ret = -1;
    }
    lua_close(l);
    free(script);
    return ret;
}
