#include "exposed_funcs.h"

int lua_GoxCreateBox(lua_State* L) {
	int x, y, z;
	x = (int)lua_tonumber(L, 1);
	y = (int)lua_tonumber(L, 2);
	z = (int)lua_tonumber(L, 3);

	mesh_iterator_t it = mesh_get_iterator(goxel.image->active_layer->mesh, MESH_ITER_VOXELS);
	mesh_set_at(
		goxel.image->active_layer->mesh, // Mesh To Draw At
		&it, // Iterator
		(int[3]) { x, y, z }, // Position
		(uint8_t[4]) { 0xFF, 0xFF, 0xFF, 0xFF } // Color
	);

	printf("[C] - GoxCreateBox(%d, %d, %d);\n", x, y, z);
	return 3;
}

int lua_GoxRemoveBox(lua_State* L) {
	int x, y, z;
	x = (int)lua_tonumber(L, 1);
	y = (int)lua_tonumber(L, 2);
	z = (int)lua_tonumber(L, 3);

	mesh_iterator_t it = mesh_get_iterator(goxel.image->active_layer->mesh, MESH_ITER_VOXELS);
	mesh_clear_block(
		goxel.image->active_layer->mesh, // Mesh To Draw At
		&it, // Iterator
		(int[3]) { x, y, z } // Position
	);

	printf("[C] - GoxRemoveBox(%d, %d, %d);\n", x, y, z);
	return 3;
}

