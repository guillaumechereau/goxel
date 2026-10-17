#ifndef GLTF_MESH_H
#define GLTF_MESH_H

#include "volume.h"
#include "volume_utils.h"

// Includes a one-texel gutter on each side of every atlas patch.
#define GLTF_ATLAS_SIZE 4096

typedef struct gltf_mesh_part {
    struct gltf_mesh_part *next;
    volume_mesh_t *mesh;
    uint8_t *texture; // RGB; NULL in vertex-color mode.
    int texture_size[2];
} gltf_mesh_part_t;

// Greedy, axis-aligned meshes, split into bounded atlas pages in texture mode.
// One texel per surface voxel; use nearest filtering without mipmaps.
// Returns NULL for an empty volume. Free the list with gltf_mesh_parts_free.
// atlas_size must be a power of two in [4, GLTF_ATLAS_SIZE].
gltf_mesh_part_t *gltf_generate_greedy_meshes(
        const volume_t *volume, bool vertex_color, float simplify,
        int atlas_size);

void gltf_mesh_parts_free(gltf_mesh_part_t *parts);

#endif // GLTF_MESH_H
