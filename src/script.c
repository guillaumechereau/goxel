#include "goxel.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

static int l_action_func(lua_State *l)
{
    action_t *a;
    const char *id = lua_tostring(l, lua_upvalueindex(1));
    a = action_get(id, false);
    if (!a) luaL_error(l, "Cannot find action %s", id);
    return action_exec_lua(a, l);
}

static int l_index(lua_State *l)
{
    const char *name, *key;
    char buf[128];
    action_t *action;

    lua_getmetatable(l, 1);
    lua_getfield(l, -1, "name");
    name = luaL_checkstring(l, -1);
    key = luaL_checkstring(l, 2);
    sprintf(buf, "%s_%s", name, key);
    action = action_get(buf, false);
    if (!action) luaL_error(l, "cannot find action %s", buf);
    lua_pushstring(l, action->id);
    lua_pushcclosure(l, l_action_func, 1);
    return 1;
}

static void add_globals(lua_State *l)
{
    // Create metatable for mesh.
    luaL_newmetatable(l, "Mesh");
    lua_pushstring(l, "mesh");
    lua_setfield(l, -2, "name");
    lua_pushcfunction(l, l_index);
    lua_setfield(l, -2, "__index");

    lua_pushstring(l, "mesh_new");
    lua_pushcclosure(l, l_action_func, 1);
    lua_setfield(l, -2, "new");

    lua_pushstring(l, "mesh_delete");
    lua_pushcclosure(l, l_action_func, 1);
    lua_setfield(l, -2, "__gc");

    lua_setglobal(l, "Mesh");
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
