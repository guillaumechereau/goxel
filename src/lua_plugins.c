#include "goxel.h"

bool check_lua(lua_State* L, int r) {
    if (r != LUA_OK) {
        printf("[Lua] - %s\n", lua_tostring(L, -1));
        return false;
    }
    return true;
}

static void open_run_lua_plugin(void) {
    const char *filePath;
    filePath = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "lua\0*.lua\0", NULL, NULL);
    if (!filePath) return;

    check_lua(goxel.L_State, luaL_dofile(goxel.L_State, filePath));
}

ACTION_REGISTER(open_run_lua_plugin,
    .help = "Run Plugin",
    .cfunc = open_run_lua_plugin,
)
