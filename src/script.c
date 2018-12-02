#include "goxel.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

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
    luaL_loadstring(l, script);
    if (lua_pcall(l, 0, 0, 0)) {
        fprintf(stderr, "ERROR:\n%s\n", lua_tostring(l, -1));
        ret = -1;
    }
    lua_close(l);
    free(script);
    return ret;
}
