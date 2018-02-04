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
    MESH_ITER_INCLUDES_NEIGHBORS    = 1 << 2,
    MESH_ITER_SKIP_EMPTY            = 1 << 3,
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

/*
 * Function: mesh_new
 * Create a new mesh.
 */
mesh_t *mesh_new(void);

/*
 * Function: mesh_delete
 * Delete a mesh.
 */
void mesh_delete(mesh_t *mesh);

/* Function: mesh_clear
 *
 * Clear all the voxels from a mesh.
 */
void mesh_clear(mesh_t *mesh);

mesh_t *mesh_copy(const mesh_t *mesh);

void mesh_set(mesh_t *mesh, const mesh_t *other);

mesh_accessor_t mesh_get_accessor(const mesh_t *mesh);
void mesh_get_at(const mesh_t *mesh, mesh_iterator_t *it,
                 const int pos[3], uint8_t out[4]);
uint8_t mesh_get_alpha_at(const mesh_t *mesh, mesh_iterator_t *it,
                          const int pos[3]);

/*
 * Function: mesh_set_at
 *
 * Set a single voxel value in a mesh.
 *
 * Inputs:
 *   mesh - The mesh.
 *   it   - Optional mesh iterator.  Successive access to the same mesh
 *          using the iterator are optimized.
 *   pos  - Position of the voxel.
 *   v    - Value to set.
 */
void mesh_set_at(mesh_t *mesh, mesh_iterator_t *it,
                 const int pos[3], const uint8_t v[4]);

// XXX: we should remove this one I guess.
void mesh_remove_empty_blocks(mesh_t *mesh, bool fast);

/*
 * Function: mesh_is_empty
 *
 * Test whether a mesh has no voxel.
 *
 * Returns:
 *   true if the mesh is empty, false otherwise.
 */
bool mesh_is_empty(const mesh_t *mesh);

/*
 * Function: mesh_get_bbox
 *
 * Get the bounding box of a mesh.
 *
 * Inputs:
 *   mesh   - The mesh
 *   exact  - If true, compute the exact bounding box.  If false, returns
 *            an approximation that might be slightly bigger than the
 *            actual box, but faster to compute.
 *
 * Outputs:
 *   bbox  - The bounding box as the bottom left and top right corner of
 *           the mesh.  If the mesh is empty, this will contain an invalid
 *           box.
 *
 * Returns:
 *   true if the mesh is not empty.
 */
bool mesh_get_bbox(const mesh_t *mesh, int bbox[2][3], bool exact);

mesh_iterator_t mesh_get_iterator(const mesh_t *mesh, int flags);
// Return an iterator that follow a given box shape.
mesh_iterator_t mesh_get_box_iterator(const mesh_t *mesh,
                                      const float box[4][4],
                                      int flags);

mesh_iterator_t mesh_get_union_iterator(
        const mesh_t *m1, const mesh_t *m2, int flags);

int mesh_iter(mesh_iterator_t *it, int pos[3]);

/*
 * Function: mesh_get_key
 *
 * Return a value that is guarantied to be different for different meshes.
 *
 * This key can be used for quickly testing if two meshes are the same.
 *
 * Note that two meshes with the same key are guarantied to have the same
 * content, but two meshes with different key could still have the same
 * content: this is not an actual hash!
 *
 * Inputs:
 *   mesh - The mesh.
 *
 * Return:
 *   The key.
 *
 */
uint64_t mesh_get_key(const mesh_t *mesh);

void *mesh_get_block_data(const mesh_t *mesh, mesh_accessor_t *accessor,
                          const int bpos[3], uint64_t *id);

// Maybe replace this with a generic mesh_copy_part function?
void mesh_copy_block(const mesh_t *src, const int src_pos[3],
                     mesh_t *dst, const int dst_pos[3]);

void mesh_read(const mesh_t *mesh,
               const int pos[3], const int size[3],
               uint8_t *data);

#endif // MESH_H
