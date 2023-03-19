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

#include "file_format.h"
#include "script.h"

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "luaautoc/lautoc.h"

#define STR(x) #x

static const char *HEADER = STR(

Mesh = {}

function Mesh:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    o.data = C('mesh_new')
    return o
end

function Mesh:set_at(pos, color)
    C('_mesh_set_at', self.data, pos, color)
end

function Mesh:save(path, format)
    C('_mesh_save', self.data, path, format)
end

);

typedef struct {
    int x;
    int y;
    int z;
} ivec3_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} irgba_t;

static void _mesh_set_at(mesh_t *mesh, ivec3_t pos, irgba_t color)
{
    mesh_set_at(mesh, NULL, (int*)&pos, (uint8_t*)&color);
}

static int _mesh_save(mesh_t *mesh, const char *path, const char *format)
{
    const file_format_t *f;
    int err;
    image_t *img;

    f = file_format_for_path(path, format, "w");
    if (!f) {
        fprintf(stderr, "Cannot find format for %s\n", path);
        return -1;
    }

    img = image_new();
    mesh_set(img->active_layer->mesh, mesh);
    err = f->export_func(img, path);
    if (err) return err;
    image_delete(img);
    return 0;
}

luaA_function_declare(mesh_new, mesh_t*);
luaA_function_declare(_mesh_set_at, void, mesh_t*, ivec3_t, irgba_t);
luaA_function_declare(_mesh_save, int, mesh_t*, const char *, const char *);

int C(lua_State *L) {
    return luaA_call_name(L, lua_tostring(L, 1));
}

static int luaA_push_ptr(lua_State *L, luaA_Type t, const void *c_in)
{
    lua_pushlightuserdata(L, *(void**)c_in);
    return 1;
}

static void luaA_to_ptr(lua_State *L, luaA_Type t, void *c_out, int index)
{
    void** p = (void**)c_out;
    *p = lua_touserdata(L, index);
}

static int luaA_push_ivec3(lua_State *L, luaA_Type t, const void *c_in)
{
    return 0;
}

static void luaA_to_ivec3(lua_State *L, luaA_Type t, void *c_out, int index)
{
    int i;
    ivec3_t* v = (ivec3_t*)c_out;

    luaL_checktype(L, index, LUA_TTABLE);
    lua_pushvalue(L, index);
    for (i = 0; i < 3; i++) {
        lua_pushinteger(L, i + 1);
        lua_gettable(L, -2);
        ((int*)v)[i] = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static int luaA_push_irgba(lua_State *L, luaA_Type t, const void *c_in)
{
    return 0;
}

static void luaA_to_irgba(lua_State *L, luaA_Type t, void *c_out, int index)
{
    int i;
    irgba_t* v = (irgba_t*)c_out;

    luaL_checktype(L, index, LUA_TTABLE);
    lua_pushvalue(L, index);
    for (i = 0; i < 4; i++) {
        lua_pushinteger(L, i + 1);
        lua_gettable(L, -2);
        ((uint8_t*)v)[i] = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}


static void bind_goxel(lua_State *L)
{
    luaL_loadstring(L, HEADER);
    lua_pcall(L, 0, 0, 0);

    luaA_open(L);
    lua_register(L, "C", C);
    luaA_function_register(L, mesh_new, mesh_t*);
    luaA_function_register(L, _mesh_set_at, void, mesh_t*, ivec3_t, irgba_t);
    luaA_function_register(L, _mesh_save, int, mesh_t*,
                           const char*, const char*);
    luaA_conversion(L, mesh_t*, luaA_push_ptr, luaA_to_ptr);
    luaA_conversion(L, ivec3_t, luaA_push_ivec3, luaA_to_ivec3);
    luaA_conversion(L, irgba_t, luaA_push_irgba, luaA_to_irgba);
}

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
    bind_goxel(l);

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
    luaA_close(l);
    lua_close(l);
    free(script);
    return ret;
}
