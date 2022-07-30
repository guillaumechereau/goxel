#include "goxel.h"

// Free All The VMs
void ReleaseAllLuaVMs();

// Creates A New Lua VM & Pushes it Into Lua VM's Arrays, if array is full returns -1;
lua_State* NewLuaVM();
