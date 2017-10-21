#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stdint.h>

#define BLOCK_SIZE 16

typedef struct mesh mesh_t;
typedef struct block block_t;

enum {
    MESH_ITER_VOXELS                = 1 << 0,
    MESH_ITER_BLOCKS                = 1 << 1,
};

// Fast iterator of all the mesh voxel.
typedef struct {
    const mesh_t *mesh;
    const mesh_t *mesh2;
    // Current cached block and its position.
    // the block can be NULL if there is no block at this position.
    block_t *block;
    int block_pos[3];
    uint64_t block_id;

    int pos[3];
    float box[4][4];
    int bbox[2][3];

    int flags;
} mesh_iterator_t;
typedef mesh_iterator_t mesh_accessor_t;

mesh_t *mesh_new(void);
void mesh_clear(mesh_t *mesh);
void mesh_delete(mesh_t *mesh);
mesh_t *mesh_copy(const mesh_t *mesh);
void mesh_set(mesh_t *mesh, const mesh_t *other);
mesh_accessor_t mesh_get_accessor(const mesh_t *mesh);
void mesh_get_at(const mesh_t *mesh, mesh_iterator_t *it,
                 const int pos[3], uint8_t out[4]);
uint8_t mesh_get_alpha_at(const mesh_t *mesh, mesh_iterator_t *it,
                          const int pos[3]);
void mesh_set_at(mesh_t *mesh, mesh_iterator_t *it,
                 const int pos[3], const uint8_t v[4]);
void mesh_remove_empty_blocks(mesh_t *mesh);
bool mesh_is_empty(const mesh_t *mesh);

mesh_iterator_t mesh_get_iterator(const mesh_t *mesh, int flags);
// Return an iterator that follow a given box shape.
// if skip_empty is set to false, we get values even for the empty blocks.
mesh_iterator_t mesh_get_box_iterator(const mesh_t *mesh,
                                      const float box[4][4]);

mesh_iterator_t mesh_get_union_iterator(
        const mesh_t *m1, const mesh_t *m2, int flags);

int mesh_iter(mesh_iterator_t *it, int pos[3]);

uint64_t mesh_get_id(const mesh_t *mesh);
void *mesh_get_block_data(const mesh_t *mesh, mesh_accessor_t *accessor,
                          const int bpos[3], uint64_t *id);

// Maybe replace this with a generic mesh_copy_part function?
void mesh_copy_block(const mesh_t *src, const int src_pos[3],
                     mesh_t *dst, const int dst_pos[3]);

#endif // MESH_H
