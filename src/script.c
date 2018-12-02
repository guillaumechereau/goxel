#include "goxel.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

static int l_action_func(lua_State *l)
{
    action_t *a;
    const char *id = lua_tostring(l, lua_upvalueindex(1));
    a = action_get(id);
    assert(a);
    return action_exec_lua(a, l);
}

static int add_action(action_t *action, void *user)
{
    lua_State *l = user;
    lua_pushstring(l, action->id);
    lua_pushcclosure(l, l_action_func, 1);
    lua_setglobal(l, action->id);
    return 0;
}

static void add_globals(lua_State *l)
{
    actions_iter(add_action, l);

    // Create metatable for mesh.
    luaL_newmetatable(l, "mesh");

    lua_getglobal(l, "mesh_new");
    lua_setfield(l, -2, "__new");

    lua_getglobal(l, "mesh_delete");
    lua_setfield(l, -2, "__gc");
}

/*
 * Function: script_run
 * Run a lua script from a file.
 */
int script_run(const char *filename)
{
    lua_State *l;
    char *script;
    int ret = 0;

    script = read_file(filename, 0);
    if (!script) {
        LOG_E("Cannot read '%d'", filename);
        return -1;
    }
    l = luaL_newstate();
    luaL_openlibs(l);
    add_globals(l);
    luaL_loadstring(l, script);
    if (lua_pcall(l, 0, 0, 0)) {
        fprintf(stderr, "ERROR:\n%s\n", lua_tostring(l, -1));
        ret = -1;
    }
    lua_close(l);
    free(script);
    return ret;
}
