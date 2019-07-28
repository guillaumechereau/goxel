/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "mesh.h"
#include "uthash.h"
#include <assert.h>
#include <limits.h>
#include <math.h>

#define min(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; \
      })

#define max(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a > _b ? _a : _b; \
      })

// Flags for the iterator/accessor status.
enum {
    MESH_ITER_FINISHED                  = 1 << 9,
    MESH_ITER_BOX                       = 1 << 10,
    MESH_ITER_MESH2                     = 1 << 11,
};

typedef struct block_data block_data_t;
struct block_data
{
    int         ref;
    uint64_t    id;
    uint8_t     voxels[BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE][4]; // RGBA voxels.
};

struct block
{
    UT_hash_handle  hh;     // The hash table of pos -> blocks in a mesh.
    block_data_t    *data;
    int             pos[3];
    uint64_t        id;
};

struct mesh
{
    block_t *blocks;
    int *ref;   // Used to implement copy on write of the blocks.
    uint64_t key; // Two meshes with the same key have the same value.
};

static uint64_t g_uid = 2; // Global id counter.

static mesh_global_stats_t g_global_stats = {};

#define N BLOCK_SIZE

#define vec3_copy(a, b) do {b[0] = a[0]; b[1] = a[1]; b[2] = a[2];} while (0)
#define vec3_equal(a, b) (b[0] == a[0] && b[1] == a[1] && b[2] == a[2])

#define BLOCK_ITER(x, y, z) \
    for (z = 0; z < N; z++) \
        for (y = 0; y < N; y++) \
            for (x = 0; x < N; x++)

#define DATA_AT(d, x, y, z) (d->voxels[x + y * N + z * N * N])
#define BLOCK_AT(c, x, y, z) (DATA_AT(c->data, x, y, z))

static void mat4_mul_vec4(float mat[4][4], const float v[4], float out[4])
{
    float ret[4] = {0};
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ret[i] += mat[j][i] * v[j];
        }
    }
    memcpy(out, ret, sizeof(ret));
}

static void box_get_bbox(float box[4][4], int bbox[2][3])
{
    const float vertices[8][4] = {
        {-1, -1, +1, 1},
        {+1, -1, +1, 1},
        {+1, +1, +1, 1},
        {-1, +1, +1, 1},
        {-1, -1, -1, 1},
        {+1, -1, -1, 1},
        {+1, +1, -1, 1},
        {-1, +1, -1, 1}};
    int i;
    int ret[2][3] = {{INT_MAX, INT_MAX, INT_MAX},
                     {INT_MIN, INT_MIN, INT_MIN}};
    float p[4];
    for (i = 0; i < 8; i++) {
        mat4_mul_vec4(box, vertices[i], p);
        ret[0][0] = min(ret[0][0], (int)floor(p[0]));
        ret[0][1] = min(ret[0][1], (int)floor(p[1]));
        ret[0][2] = min(ret[0][2], (int)floor(p[2]));
        ret[1][0] = max(ret[1][0], (int)ceil(p[0]));
        ret[1][1] = max(ret[1][1], (int)ceil(p[1]));
        ret[1][2] = max(ret[1][2], (int)ceil(p[2]));
    }
    memcpy(bbox, ret, sizeof(ret));
}

static void bbox_intersection(int a[2][3], int b[2][3], int out[2][3])
{
    int i;
    for (i = 0; i < 3; i++) {
        out[0][i] = max(a[0][i], b[0][i]);
        out[1][i] = min(a[1][i], b[1][i]);
    }
}

static block_data_t *get_empty_data(void)
{
    static block_data_t *data = NULL;
    if (!data) {
        data = calloc(1, sizeof(*data));
        data->ref = 1;
        data->id = 0;
    }
    return data;
}

static bool block_is_empty(const block_t *block, bool fast)
{
    int x, y, z;
    if (!block) return true;
    if (block->data->id == 0) return true;
    if (fast) return false;

    BLOCK_ITER(x, y, z) {
        if (BLOCK_AT(block, x, y, z)[3]) return false;
    }
    return true;
}

static block_t *block_new(const int pos[3])
{
    block_t *block = calloc(1, sizeof(*block));
    memcpy(block->pos, pos, sizeof(block->pos));
    block->data = get_empty_data();
    block->data->ref++;
    block->id = g_uid++;
    return block;
}

static void block_delete(block_t *block)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        g_global_stats.nb_blocks--;
        g_global_stats.mem -= sizeof(*block->data);
    }
    free(block);
}

static block_t *block_copy(const block_t *other)
{
    block_t *block = malloc(sizeof(*block));
    *block = *other;
    memset(&block->hh, 0, sizeof(block->hh));
    block->data->ref++;
    block->id = g_uid++;
    return block;
}

static void block_set_data(block_t *block, block_data_t *data)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        g_global_stats.nb_blocks--;
        g_global_stats.mem -= sizeof(*block->data);
    }
    block->data = data;
    data->ref++;
}

// Copy the data if there are any other blocks having reference to it.
static void block_prepare_write(block_t *block)
{
    if (block->data->ref == 1) {
        block->data->id = ++g_uid;
        return;
    }
    block->data->ref--;
    block_data_t *data;
    data = calloc(1, sizeof(*block->data));
    memcpy(data->voxels, block->data->voxels, N * N * N * 4);
    data->ref = 1;
    block->data = data;
    block->data->id = ++g_uid;

    g_global_stats.nb_blocks++;
    g_global_stats.mem += sizeof(*block->data);
}

static void block_get_at(const block_t *block, const int pos[3],
                         uint8_t out[4])
{
    int x, y, z;
    if (!block) {
        memset(out, 0, 4);
        return;
    }
    x = pos[0] - block->pos[0];
    y = pos[1] - block->pos[1];
    z = pos[2] - block->pos[2];
    assert(x >= 0 && x < N);
    assert(y >= 0 && y < N);
    assert(z >= 0 && z < N);
    memcpy(out, BLOCK_AT(block, x, y, z), 4);
}

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
bool mesh_get_bbox(const mesh_t *mesh, int bbox[2][3], bool exact)
{
    block_t *block;
    int ret[2][3] = {{INT_MAX, INT_MAX, INT_MAX},
                     {INT_MIN, INT_MIN, INT_MIN}};
    int pos[3];
    mesh_iterator_t iter;
    bool empty = false;

    if (!exact) {
        for (block = mesh->blocks; block; block = block->hh.next) {
            if (block_is_empty(block, true)) continue;
            ret[0][0] = min(ret[0][0], block->pos[0]);
            ret[0][1] = min(ret[0][1], block->pos[1]);
            ret[0][2] = min(ret[0][2], block->pos[2]);
            ret[1][0] = max(ret[1][0], block->pos[0] + N);
            ret[1][1] = max(ret[1][1], block->pos[1] + N);
            ret[1][2] = max(ret[1][2], block->pos[1] + N);
        }
    } else {
        iter = mesh_get_iterator(mesh, MESH_ITER_SKIP_EMPTY);
        while (mesh_iter(&iter, pos)) {
            if (!mesh_get_alpha_at(mesh, &iter, pos)) continue;
            ret[0][0] = min(ret[0][0], pos[0]);
            ret[0][1] = min(ret[0][1], pos[1]);
            ret[0][2] = min(ret[0][2], pos[2]);
            ret[1][0] = max(ret[1][0], pos[0] + 1);
            ret[1][1] = max(ret[1][1], pos[1] + 1);
            ret[1][2] = max(ret[1][2], pos[2] + 1);
        }
    }
    empty = ret[0][0] >= ret[1][0];
    if (empty) memset(ret, 0, sizeof(ret));
    memcpy(bbox, ret, sizeof(ret));
    return !empty;
}

static void mesh_prepare_write(mesh_t *mesh)
{
    block_t *blocks, *block, *new_block;
    assert(*mesh->ref > 0);
    mesh->key = g_uid++;
    if (*mesh->ref == 1)
        return;
    (*mesh->ref)--;
    mesh->ref = calloc(1, sizeof(*mesh->ref));
    *mesh->ref = 1;
    blocks = mesh->blocks;
    mesh->blocks = NULL;
    for (block = blocks; block; block = block->hh.next) {
        block->id = g_uid++; // Invalidate all accessors.
        new_block = block_copy(block);
        HASH_ADD(hh, mesh->blocks, pos, sizeof(new_block->pos), new_block);
    }
    g_global_stats.nb_meshes++;
}

static block_t *mesh_add_block(mesh_t *mesh, const int pos[3]);

static void mesh_add_neighbors_blocks(mesh_t *mesh)
{
    const int POS[6][3] = {
        {0, 0, -1}, {0, 0, +1},
        {0, -1, 0}, {0, +1, 0},
        {-1, 0, 0}, {+1, 0, 0},
    };
    int i, p[3] = {};
    uint64_t key = mesh->key;
    block_t *block, *tmp, *other;

    mesh_prepare_write(mesh);
    HASH_ITER(hh, mesh->blocks, block, tmp) {
        if (block_is_empty(block, true)) continue;
        for (i = 0; i < 6; i++) {
            p[0] = block->pos[0] + POS[i][0] * N;
            p[1] = block->pos[1] + POS[i][1] * N;
            p[2] = block->pos[2] + POS[i][2] * N;
            HASH_FIND(hh, mesh->blocks, p, 3 * sizeof(int), other);
            if (!other) mesh_add_block(mesh, p);
        }
    }
    // Adding empty blocks shouldn't change the key of the mesh.
    mesh->key = key;
}

void mesh_remove_empty_blocks(mesh_t *mesh, bool fast)
{
    block_t *block, *tmp;
    uint64_t key = mesh->key;
    mesh_prepare_write(mesh);
    HASH_ITER(hh, mesh->blocks, block, tmp) {
        if (block_is_empty(block, false)) {
            HASH_DEL(mesh->blocks, block);
            assert(mesh->blocks != block);
            block_delete(block);
        }
    }
    // Empty blocks shouldn't change the key of the mesh.
    mesh->key = key;
}

bool mesh_is_empty(const mesh_t *mesh)
{
    return mesh->blocks == NULL;
}

mesh_t *mesh_new(void)
{
    mesh_t *mesh;
    mesh = calloc(1, sizeof(*mesh));
    mesh->ref = calloc(1, sizeof(*mesh->ref));
    mesh->key = 1; // Empty mesh key.
    *mesh->ref = 1;
    g_global_stats.nb_meshes++;
    return mesh;
}

mesh_iterator_t mesh_get_iterator(const mesh_t *mesh, int flags)
{
    return (mesh_iterator_t){
        .mesh = mesh,
        .flags = flags,
    };
}

mesh_iterator_t mesh_get_union_iterator(
        const mesh_t *m1, const mesh_t *m2, int flags)
{
    return (mesh_iterator_t){
        .mesh = m1,
        .mesh2 = m2,
        .flags = flags,
    };
}

mesh_iterator_t mesh_get_box_iterator(const mesh_t *mesh,
                                      const float box[4][4], int flags)
{
    int mesh_bbox[2][3];
    mesh_iterator_t iter = {
        .mesh = mesh,
        .flags = MESH_ITER_BOX | MESH_ITER_VOXELS | flags,
    };
    memcpy(iter.box, box, sizeof(iter.box));
    box_get_bbox(iter.box, iter.bbox);

    if (flags & MESH_ITER_SKIP_EMPTY) {
        mesh_get_bbox(mesh, mesh_bbox, false);
        bbox_intersection(mesh_bbox, iter.bbox, iter.bbox);
    }

    return iter;
}

mesh_accessor_t mesh_get_accessor(const mesh_t *mesh)
{
    return (mesh_accessor_t){0};
}


void mesh_clear(mesh_t *mesh)
{
    assert(mesh);
    block_t *block, *tmp;
    mesh_prepare_write(mesh);
    HASH_ITER(hh, mesh->blocks, block, tmp) {
        HASH_DEL(mesh->blocks, block);
        assert(mesh->blocks != block);
        block_delete(block);
    }
    mesh->key = 1; // Empty mesh key.
    mesh->blocks = NULL;
}

void mesh_delete(mesh_t *mesh)
{
    block_t *block, *tmp;
    if (!mesh) return;
    (*mesh->ref)--;
    if (*mesh->ref == 0) {
        HASH_ITER(hh, mesh->blocks, block, tmp) {
            HASH_DEL(mesh->blocks, block);
            assert(mesh->blocks != block);
            block_delete(block);
        }
        free(mesh->ref);
        g_global_stats.nb_meshes--;
    }
    free(mesh);
}

mesh_t *mesh_copy(const mesh_t *other)
{
    mesh_t *mesh = calloc(1, sizeof(*mesh));
    mesh->blocks = other->blocks;
    mesh->ref = other->ref;
    mesh->key = other->key;
    (*mesh->ref)++;
    return mesh;
}

void mesh_set(mesh_t *mesh, const mesh_t *other)
{
    block_t *block, *tmp;
    assert(mesh && other);
    if (mesh->blocks == other->blocks) return; // Already the same.
    (*mesh->ref)--;
    if (*mesh->ref == 0) {
        HASH_ITER(hh, mesh->blocks, block, tmp) {
            HASH_DEL(mesh->blocks, block);
            assert(mesh->blocks != block);
            block_delete(block);
        }
        free(mesh->ref);
        g_global_stats.nb_meshes--;
    }
    mesh->blocks = other->blocks;
    mesh->ref = other->ref;
    mesh->key = other->key;
    (*mesh->ref)++;
}

static uint64_t get_block_id(const block_t *block)
{
    return block ? block->id : 1;
}

static block_t *mesh_get_block_at(const mesh_t *mesh, const int pos[3],
                                  mesh_accessor_t *it)
{
    block_t *block;
    int p[3] = {};
    p[0] = pos[0] & ~(int)(N - 1);
    p[1] = pos[1] & ~(int)(N - 1);
    p[2] = pos[2] & ~(int)(N - 1);
    if (!it) {
        HASH_FIND(hh, mesh->blocks, p, sizeof(p), block);
        return block;
    }

    if (    it->block_id && it->block_id == get_block_id(it->block) &&
            vec3_equal(it->block_pos, p)) {
        return it->block;
    }
    HASH_FIND(hh, mesh->blocks, p, sizeof(p), block);
    it->block = block;
    it->block_id = get_block_id(block);
    vec3_copy(p, it->block_pos);
    return block;
}

static block_t *mesh_add_block(mesh_t *mesh, const int pos[3])
{
    block_t *block;
    assert(pos[0] % BLOCK_SIZE == 0);
    assert(pos[1] % BLOCK_SIZE == 0);
    assert(pos[2] % BLOCK_SIZE == 0);
    assert(!mesh_get_block_at(mesh, pos, NULL));
    mesh_prepare_write(mesh);
    block = block_new(pos);
    HASH_ADD(hh, mesh->blocks, pos, sizeof(block->pos), block);
    return block;
}

void mesh_get_at(const mesh_t *mesh, mesh_iterator_t *it,
                 const int pos[3], uint8_t out[4])
{
    block_t *block;
    int p[3];

    if (it && it->block_id && it->block_id == get_block_id(it->block)) {
        p[0] = pos[0] - it->block_pos[0];
        p[1] = pos[1] - it->block_pos[1];
        p[2] = pos[2] - it->block_pos[2];
        if (    p[0] >= 0 && p[0] < N &&
                p[1] >= 0 && p[1] < N &&
                p[2] >= 0 && p[2] < N) {
            if (!it->block)
                memset(out, 0, 4);
            else
                memcpy(out, BLOCK_AT(it->block, p[0], p[1], p[2]), 4);
            return;
        }
    }

    block = mesh_get_block_at(mesh, pos, it);
    return block_get_at(block, pos, out);
}

void mesh_set_at(mesh_t *mesh, mesh_iterator_t *iter,
                 const int pos[3], const uint8_t v[4])
{
    int p[3] = {pos[0] & ~(int)(N - 1),
                pos[1] & ~(int)(N - 1),
                pos[2] & ~(int)(N - 1)};
    mesh_prepare_write(mesh);

    block_t *block = mesh_get_block_at(mesh, p, iter);

    if (!block) {
        block = mesh_add_block(mesh, p);
        if (iter) {
            iter->block = block;
            iter->block_id = get_block_id(block);
            vec3_copy(p, iter->block_pos);
        }
    }

    block_prepare_write(block);
    p[0] = pos[0] - block->pos[0];
    p[1] = pos[1] - block->pos[1];
    p[2] = pos[2] - block->pos[2];
    assert(p[0] >= 0 && p[0] < N);
    assert(p[1] >= 0 && p[1] < N);
    assert(p[2] >= 0 && p[2] < N);
    memcpy(BLOCK_AT(block, p[0], p[1], p[2]), v, 4);
}

void mesh_clear_block(mesh_t *mesh, mesh_iterator_t *it, const int pos[3])
{
    block_t *block;
    mesh_prepare_write(mesh);
    block = mesh_get_block_at(mesh, pos, it);
    if (!block) return;
    HASH_DEL(mesh->blocks, block);
    assert(mesh->blocks != block);
    block_delete(block);
    if (it) it->block = NULL;
}


static bool mesh_iter_next_block_box(mesh_iterator_t *it)
{
    int i;
    const mesh_t *mesh = it->mesh;
    if (!it->block_id) {
        it->block_pos[0] = it->bbox[0][0] & ~(int)(N - 1);
        it->block_pos[1] = it->bbox[0][1] & ~(int)(N - 1);
        it->block_pos[2] = it->bbox[0][2] & ~(int)(N - 1);
        goto end;
    }

    for (i = 0; i < 3; i++) {
        it->block_pos[i] += N;
        if (it->block_pos[i] <= it->bbox[1][i]) break;
        it->block_pos[i] = it->bbox[0][i] & ~(int)(N - 1);
    }
    if (i == 3) return false;

end:
    HASH_FIND(hh, mesh->blocks, it->block_pos, 3 * sizeof(int), it->block);
    it->block_id = get_block_id(it->block);
    vec3_copy(it->block_pos, it->pos);
    return true;
}

static bool mesh_iter_next_block_union(mesh_iterator_t *it)
{
    it->block = it->block ? it->block->hh.next : it->mesh->blocks;
    if (!it->block && !(it->flags & MESH_ITER_MESH2)) {
        it->block = it->mesh2->blocks;
        it->flags |= MESH_ITER_MESH2;
    }
    if (!it->block) return false;
    it->block_id = it->block->id;
    vec3_copy(it->block->pos, it->block_pos);
    vec3_copy(it->block->pos, it->pos);

    // Discard blocks that we already did from the first mesh.
    if (it->flags & MESH_ITER_MESH2) {
        if (mesh_get_block_at(it->mesh, it->block_pos, NULL))
            return mesh_iter_next_block_union(it);
    }
    return true;
}

static bool mesh_iter_next_block(mesh_iterator_t *it)
{
    if (it->block_id && it->block_id != get_block_id(it->block)) {
        it->block = mesh_get_block_at(
            (it->flags & MESH_ITER_MESH2) ? it->mesh2 : it->mesh,
            it->block_pos,
            it);
    }

    if (it->flags & MESH_ITER_BOX) return mesh_iter_next_block_box(it);
    if (it->mesh2) return mesh_iter_next_block_union(it);

    it->block = it->block ? it->block->hh.next : it->mesh->blocks;
    if (!it->block) return false;
    it->block_id = it->block->id;
    vec3_copy(it->block->pos, it->block_pos);
    vec3_copy(it->block->pos, it->pos);
    return true;
}

int mesh_iter(mesh_iterator_t *it, int pos[3])
{
    int i;
    if (!it->block_id) { // First call.
        // XXX: this is not good: mesh_iter shouldn't make change to the
        // mesh.
        if (it->flags & MESH_ITER_INCLUDES_NEIGHBORS)
            mesh_add_neighbors_blocks((mesh_t*)it->mesh);
        if (!mesh_iter_next_block(it)) return 0;
        goto end;
    }
    if (it->flags & MESH_ITER_BLOCKS) goto next_block;

    for (i = 0; i < 3; i++) {
        if (++it->pos[i] < it->block_pos[i] + N) break;
        it->pos[i] = it->block_pos[i];
    }
    if (i < 3) goto end;

next_block:
    if (!mesh_iter_next_block(it)) {
        // XXX: this is not good: mesh_iter shouldn't make changes to the
        // mesh.
        if (it->flags & MESH_ITER_INCLUDES_NEIGHBORS)
            mesh_remove_empty_blocks((mesh_t*)it->mesh, true);
        return 0;
    }

end:
    if (pos) vec3_copy(it->pos, pos);
    return 1;
}

uint64_t mesh_get_key(const mesh_t *mesh)
{
    return mesh ? mesh->key : 0;
}

void *mesh_get_block_data(const mesh_t *mesh, mesh_accessor_t *iter,
                          const int bpos[3], uint64_t *id)
{
    block_t *block = NULL;
    if (    iter &&
            iter->block_id &&
            iter->block_id == get_block_id(iter->block) &&
            memcmp(&iter->pos, bpos, sizeof(iter->pos)) == 0) {
        block = iter->block;
    } else {
        HASH_FIND(hh, mesh->blocks, bpos, sizeof(iter->pos), block);
    }
    if (id) *id = block ? block->data->id : 0;
    return block ? block->data->voxels : NULL;
}

uint8_t mesh_get_alpha_at(const mesh_t *mesh, mesh_iterator_t *iter,
                          const int pos[3])
{
    uint8_t v[4];
    mesh_get_at(mesh, iter, pos, v);
    return v[3];
}

void mesh_copy_block(const mesh_t *src, const int src_pos[3],
                     mesh_t *dst, const int dst_pos[3])
{
    block_t *b1, *b2;
    mesh_prepare_write(dst);
    b1 = mesh_get_block_at(src, src_pos, NULL);
    b2 = mesh_get_block_at(dst, dst_pos, NULL);
    if (!b2) b2 = mesh_add_block(dst, dst_pos);
    block_set_data(b2, b1->data);
}

void mesh_read(const mesh_t *mesh,
               const int pos[3], const int size[3],
               uint8_t *data)
{
    // For the moment we only support the case where we get the rectangle
    // of a block plus a one voxel border around it!
    assert(pos[0] - (pos[0] & ~(int)(N - 1)) == N - 1);
    assert(pos[1] - (pos[1] & ~(int)(N - 1)) == N - 1);
    assert(pos[2] - (pos[2] & ~(int)(N - 1)) == N - 1);
    assert(size[0] == N + 2);
    assert(size[1] == N + 2);
    assert(size[2] == N + 2);

    block_t *block;
    int block_pos[3] = {pos[0] + 1, pos[1] + 1, pos[2] + 1};
    int i, z, y, x, dx, dy, dz, p[3];
    uint8_t v[4];
    mesh_accessor_t accessor;

    memset(data, 0, size[0] * size[1] * size[2] * 4);
    block = mesh_get_block_at(mesh, block_pos, NULL);
    if (!block) goto rest;

    for (z = 0; z < N; z++)
    for (y = 0; y < N; y++)
    for (x = 0; x < N; x++) {
        dx = x + 1;
        dy = y + 1;
        dz = z + 1;
        memcpy(&data[(dz * size[1] * size[0] + dy * size[0] + dx) * 4],
               &block->data->voxels[z * N * N + y * N + x],
               4);
    }

rest:
    // Fill the rest.
    // In order to optimize access, we iter in such a way to avoid changing
    // the current block: start with the planes, then the edges, and finally
    // the 8 corners.
    accessor = mesh_get_accessor(mesh);
    const int ranges[26][3][2] = {
        // 6 planes.
        {{0,         1}, {1,     N + 1}, {1,     N + 1}},
        {{N + 1, N + 2}, {1,     N + 1}, {1,     N + 1}},
        {{1,     N + 1}, {0,         1}, {1,     N + 1}},
        {{1,     N + 1}, {N + 1, N + 2}, {1,     N + 1}},
        {{1,     N + 1}, {1,     N + 1}, {0,         1}},
        {{1,     N + 1}, {1,     N + 1}, {N + 1, N + 2}},
        // 4 edges moving along X.
        {{1,     N + 1}, {0,         1}, {0,         1}},
        {{1,     N + 1}, {N + 1, N + 2}, {0,         1}},
        {{1,     N + 1}, {0,         1}, {N + 1, N + 2}},
        {{1,     N + 1}, {N + 1, N + 2}, {N + 1, N + 2}},
        // 4 edges moving along Y.
        {{0,         1}, {1,     N + 1}, {0,         1}},
        {{N + 1, N + 2}, {1,     N + 1}, {0,         1}},
        {{0,         1}, {1,     N + 1}, {1,     N + 2}},
        {{N + 1, N + 2}, {1,     N + 1}, {1,     N + 2}},
        // 4 edges moving along Z.
        {{0,         1}, {0,         1}, {1,     N + 1}},
        {{N + 1, N + 2}, {0,         1}, {1,     N + 1}},
        {{0,         1}, {N + 1, N + 2}, {1,     N + 1}},
        {{N + 1, N + 2}, {N + 1, N + 2}, {1,     N + 1}},
        // 8 Corners.
        {{0,     1    }, {0,     1    }, {0,     1    }},
        {{N + 1, N + 2}, {0,     1    }, {0,     1    }},
        {{0,     1    }, {N + 1, N + 2}, {0,     1    }},
        {{N + 1, N + 2}, {N + 1, N + 2}, {0,     1    }},
        {{0,     1    }, {0,     1    }, {N + 1, N + 2}},
        {{N + 1, N + 2}, {0,     1    }, {N + 1, N + 2}},
        {{0,     1    }, {N + 1, N + 2}, {N + 1, N + 2}},
        {{N + 1, N + 2}, {N + 1, N + 2}, {N + 1, N + 2}},
    };

    for (i = 0; i < 26; i++)
    for (z = ranges[i][2][0]; z < ranges[i][2][1]; z++)
    for (y = ranges[i][1][0]; y < ranges[i][1][1]; y++)
    for (x = ranges[i][0][0]; x < ranges[i][0][1]; x++) {
        p[0] = pos[0] + x;
        p[1] = pos[1] + y;
        p[2] = pos[2] + z;
        mesh_get_at(mesh, &accessor, p, v);
        memcpy(&data[(z * size[1] * size[0] + y * size[0] + x) * 4], v, 4);
    }
}


void mesh_get_global_stats(mesh_global_stats_t *stats)
{
    *stats = g_global_stats;
}
