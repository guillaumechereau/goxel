/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stdint.h>

#define BLOCK_SIZE 16

/* Type: mesh_t
 * Opaque type that represents a mesh.
 */
typedef struct mesh mesh_t;

typedef struct block block_t;

/* Enum: MESH_ITER
 * Some flags that can be used to modify the behavior of the iteration
 * function.
 *
 * MESH_ITER_VOXELS - Iter on the voxels (default if zero).
 * MESH_ITER_BLOCKS - Iter on the blocks: the iterator return successive
 *                    blocks positions.
 * MESH_ITER_INCLUDES_NEIGHBORS - Also yield one position for each
 *                                neighbor of the voxels.
 * MESH_ITER_SKIP_EMPTY - Don't yield empty voxels/blocks.
 */
enum {
    MESH_ITER_VOXELS                = 1 << 0,
    MESH_ITER_BLOCKS                = 1 << 1,
    MESH_ITER_INCLUDES_NEIGHBORS    = 1 << 2,
    MESH_ITER_SKIP_EMPTY            = 1 << 3,
};

/* Type mesh_iterator_t
 * Fast iterator of all the mesh voxels.
 *
 * This struct can be used when we want to make a lot of successive accesses
 * to the same mesh.
 *
 * It's also the struct used as an iterator into the mesh voxels.
 *
 * You don't need to care about the attributes, just create them with
 * <mesh_get_accessor>, <mesh_get_iterator>, <mesh_get_box_iterator> or
 * <mesh_get_union_iterator>.
 */
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

/*
 * Function: mesh_get_block_aabb
 * Compute the AABB box of a given block of the mesh.
 */
static inline void mesh_get_block_aabb(const int pos[3], int aabb[2][3])
{
    aabb[0][0] = pos[0];
    aabb[0][1] = pos[1];
    aabb[0][2] = pos[2];
    aabb[1][0] = pos[0] + BLOCK_SIZE;
    aabb[1][1] = pos[1] + BLOCK_SIZE;
    aabb[1][2] = pos[2] + BLOCK_SIZE;
}

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
 * Function: mesh_clear_block
 * Set to zero all the voxels in a given block.
 */
void mesh_clear_block(mesh_t *mesh, mesh_iterator_t *it, const int pos[3]);

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
 *           the mesh.  If the mesh is empty, this will contain all zero.
 *
 * Returns:
 *   true if the mesh is not empty.
 */
bool mesh_get_bbox(const mesh_t *mesh, int bbox[2][3], bool exact);

/*
 * Function: mesh_get_iterator
 * Return an iterator that yield all the voxels of the mesh.
 *
 * Parameters:
 *   mesh  - The mesh we want to iterate.
 *   flags - Any of the <MESH_ITER> enum.
 */
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
 *   The key, if the mesh input is NULL, returns zero.
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

typedef struct {
    int       nb_meshes;
    int       nb_blocks;
    uint64_t  mem;
} mesh_global_stats_t;

void mesh_get_global_stats(mesh_global_stats_t *stats);

#endif // MESH_H
