#include "goxel.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"


static int l_goxel_call(lua_State *l)
{
    action_t *a;
    const char *id = lua_tostring(l, 1);
    lua_remove(l, 1);
    a = action_get(id, false);
    if (!a) luaL_error(l, "Cannot find action %s", id);
    return action_exec_lua(a, l);
}

static void add_globals(lua_State *l)
{
    const char *script;
    lua_pushcfunction(l, l_goxel_call);
    lua_setglobal(l, "goxel_call");
    script = assets_get("asset://data/other/script_header.lua", NULL);
    assert(script);
    luaL_loadstring(l, script);
    lua_pcall(l, 0, 0, 0);
}

/*
 * Function: script_run
 * Run a lua script from a file.
 */
int script_run(const char *filename, int argc, const char **argv)
{
    lua_State *l;
    char *script;
    int ret = 0, i;

    script = read_file(filename, 0);
    if (!script) {
        LOG_E("Cannot read '%d'", filename);
        return -1;
    }
    l = luaL_newstate();
    luaL_openlibs(l);
    add_globals(l);

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
