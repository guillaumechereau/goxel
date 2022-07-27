#include "goxel.h"

static void open_run_lua_plugin(void) {
    const char *path;
    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "lua\0*.lua\0", NULL, NULL);
    if (!path) return;

    luaL_dofile(goxel.L_State, path);
}

ACTION_REGISTER(open_run_lua_plugin,
    .help = "Run Plugin",
    .cfunc = open_run_lua_plugin,
)
