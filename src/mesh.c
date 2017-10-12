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

#include "goxel.h"
#include <limits.h>

// Flags for the iterator/accessor status.
enum {
    MESH_FOUND                          = 1 << 0,
    MESH_ITER_FINISHED                  = 1 << 1,
    MESH_ITER_BOX                       = 1 << 2,
    MESH_ITER_SKIP_EMPTY                = 1 << 3,
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
    int             id;     // id of the block in the mesh it belongs.
};

struct mesh
{
    block_t *blocks;
    int next_block_id;
    int *ref;   // Used to implement copy on write of the blocks.
    uint64_t id;     // global uniq id, change each time a mesh changes.
};

#define N BLOCK_SIZE

// XXX: move this in goxel.h?
static int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}

#define BLOCK_ITER(x, y, z) \
    for (z = 0; z < N; z++) \
        for (y = 0; y < N; y++) \
            for (x = 0; x < N; x++)

#define DATA_AT(d, x, y, z) (d->voxels[x + y * N + z * N * N])
#define BLOCK_AT(c, x, y, z) (DATA_AT(c->data, x, y, z))

static block_data_t *get_empty_data(void)
{
    static block_data_t *data = NULL;
    if (!data) {
        data = calloc(1, sizeof(*data));
        data->ref = 1;
        data->id = 0;
        goxel->block_count++;
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
    return block;
}

static void block_delete(block_t *block)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        goxel->block_count--;
    }
    free(block);
}

static block_t *block_copy(const block_t *other)
{
    block_t *block = malloc(sizeof(*block));
    *block = *other;
    memset(&block->hh, 0, sizeof(block->hh));
    block->data->ref++;
    return block;
}

static void block_set_data(block_t *block, block_data_t *data)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        goxel->block_count--;
    }
    block->data = data;
    data->ref++;
}

// Copy the data if there are any other blocks having reference to it.
static void block_prepare_write(block_t *block)
{
    if (block->data->ref == 1) {
        block->data->id = ++goxel->next_uid;
        return;
    }
    block->data->ref--;
    block_data_t *data;
    data = calloc(1, sizeof(*block->data));
    memcpy(data->voxels, block->data->voxels, N * N * N * 4);
    data->ref = 1;
    block->data = data;
    block->data->id = ++goxel->next_uid;
    goxel->block_count++;
}

// XXX: cleanup, or even better: remove totally and do that at the mesh
// level.
static void combine(const uint8_t a[4], const uint8_t b[4], int mode,
                    uint8_t out[4])
{
    int i, aa = a[3], ba = b[3];
    uint8_t ret[4];
    memcpy(ret, a, 4);
    if (mode == MODE_PAINT) {
        ret[0] = mix(a[0], b[0], ba / 255.);
        ret[1] = mix(a[1], b[1], ba / 255.);
        ret[2] = mix(a[2], b[2], ba / 255.);
    }
    else if (mode == MODE_OVER) {
        if (255 * ba + aa * (255 - ba)) {
            for (i = 0; i < 3; i++) {
                ret[i] = (255 * b[i] * ba + a[i] * aa * (255 - ba)) /
                         (255 * ba + aa * (255 - ba));
            }
        }
        ret[3] = ba + aa * (255 - ba) / 255;
    }
    else if (mode == MODE_SUB) {
        ret[3] = max(0, aa - ba);
    }
    else if (mode == MODE_MAX) {
        ret[0] = b[0];
        ret[1] = b[1];
        ret[2] = b[2];
        ret[3] = max(a[3], b[3]);
    } else if (mode == MODE_SUB_CLAMP) {
        ret[0] = a[0];
        ret[1] = a[1];
        ret[2] = a[2];
        ret[3] = min(aa, 255 - ba);
    } else if (mode == MODE_MULT_ALPHA) {
        ret[0] = ret[0] * ba / 255;
        ret[1] = ret[1] * ba / 255;
        ret[2] = ret[2] * ba / 255;
        ret[3] = ret[3] * ba / 255;
    } else {
        assert(false);
    }
    memcpy(out, ret, 4);
}


// Used for the cache.
static int block_del(void *data_)
{
    block_data_t *data = data_;
    data->ref--;
    if (data->ref == 0) {
        free(data);
        goxel->block_count--;
    }
    return 0;
}

// XXX: can we remove that?
static void block_merge(block_t *block, const block_t *other, int mode)
{
    int x, y, z;
    block_data_t *data;
    static cache_t *cache = NULL;

    if (!other || other->data == get_empty_data()) return;
    if (IS_IN(mode, MODE_OVER, MODE_MAX) && block->data == get_empty_data()) {
        block_set_data(block, other->data);
        return;
    }

    // Check if the merge op has been cached.
    if (!cache) cache = cache_create(512);
    struct {
        uint64_t id1;
        uint64_t id2;
        int      mode;
        int      _pad;
    } key = {
        block->data->id, other->data->id, mode
    };
    _Static_assert(sizeof(key) == 24, "");
    data = cache_get(cache, &key, sizeof(key));
    if (data) {
        block_set_data(block, data);
        return;
    }

    block_prepare_write(block);
    BLOCK_ITER(x, y, z) {
        combine(DATA_AT(block->data, x, y, z),
                DATA_AT(other->data, x, y, z),
                mode,
                BLOCK_AT(block, x, y, z));
    }
    block->data->ref++;
    cache_add(cache, &key, sizeof(key), block->data, 1, block_del);
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

static void block_set_at(block_t *block, const int pos[3], const uint8_t v[4])
{
    int x, y, z;
    block_prepare_write(block);
    x = pos[0] - block->pos[0];
    y = pos[1] - block->pos[1];
    z = pos[2] - block->pos[2];
    assert(x >= 0 && x < N);
    assert(y >= 0 && y < N);
    assert(z >= 0 && z < N);
    memcpy(BLOCK_AT(block, x, y, z), v, 4);
}

static void mesh_prepare_write(mesh_t *mesh)
{
    block_t *blocks, *block, *new_block;
    assert(*mesh->ref > 0);
    mesh->id = goxel->next_uid++;
    if (*mesh->ref == 1)
        return;
    (*mesh->ref)--;
    mesh->ref = calloc(1, sizeof(*mesh->ref));
    *mesh->ref = 1;
    blocks = mesh->blocks;
    mesh->blocks = NULL;
    for (block = blocks; block; block = block->hh.next) {
        new_block = block_copy(block);
        new_block->id = block->id;
        HASH_ADD(hh, mesh->blocks, pos, sizeof(new_block->pos), new_block);
    }
}

void mesh_remove_empty_blocks(mesh_t *mesh)
{
    block_t *block, *tmp;
    mesh_prepare_write(mesh);
    HASH_ITER(hh, mesh->blocks, block, tmp) {
        if (block_is_empty(block, false)) {
            HASH_DEL(mesh->blocks, block);
            block_delete(block);
        }
    }
}

bool mesh_is_empty(const mesh_t *mesh)
{
    return mesh->blocks == NULL;
}

mesh_t *mesh_new(void)
{
    mesh_t *mesh;
    mesh = calloc(1, sizeof(*mesh));
    mesh->next_block_id = 1;
    mesh->ref = calloc(1, sizeof(*mesh->ref));
    mesh->id = goxel->next_uid++;
    *mesh->ref = 1;
    return mesh;
}

mesh_iterator_t mesh_get_iterator(const mesh_t *mesh)
{
    return (mesh_iterator_t){0};
}

mesh_iterator_t mesh_get_box_iterator(const mesh_t *mesh,
            const box_t box, bool skip_empty)
{
    int i, j;
    vec3_t vertices[8];
    mesh_iterator_t iter = {
        .box = box,
        .flags = MESH_ITER_BOX | (skip_empty ? MESH_ITER_SKIP_EMPTY : 0),
        .bbox = {{INT_MAX, INT_MAX, INT_MAX}, {INT_MIN, INT_MIN, INT_MIN}},
    };
    // Compute the bbox from the box.
    box_get_vertices(box, vertices);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 3; j++) {
            iter.bbox[0][j] = min(iter.bbox[0][j], vertices[i].v[j]);
            iter.bbox[1][j] = max(iter.bbox[1][j], vertices[i].v[j]);
        }
    }
    // Initial block position:
    iter.bpos[0] = iter.bbox[0][0] - mod(iter.bbox[0][0], N);
    iter.bpos[1] = iter.bbox[0][1] - mod(iter.bbox[0][1], N);
    iter.bpos[2] = iter.bbox[0][2] - mod(iter.bbox[0][2], N);
    // Initial block.
    HASH_FIND(hh, mesh->blocks, iter.bpos, 3 * sizeof(int), iter.block);
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
        block_delete(block);
    }
    mesh->blocks = NULL;
    mesh->next_block_id = 1;
}

void mesh_delete(mesh_t *mesh)
{
    block_t *block, *tmp;
    if (!mesh) return;
    (*mesh->ref)--;
    if (*mesh->ref == 0) {
        HASH_ITER(hh, mesh->blocks, block, tmp) {
            HASH_DEL(mesh->blocks, block);
            block_delete(block);
        }
        free(mesh->ref);
    }
    free(mesh);
}

mesh_t *mesh_copy(const mesh_t *other)
{
    mesh_t *mesh = calloc(1, sizeof(*mesh));
    mesh->blocks = other->blocks;
    mesh->ref = other->ref;
    mesh->id = other->id;
    mesh->next_block_id = other->next_block_id;
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
            block_delete(block);
        }
        free(mesh->ref);
    }
    mesh->blocks = other->blocks;
    mesh->ref = other->ref;
    mesh->next_block_id = other->next_block_id;
    (*mesh->ref)++;
}

static block_t *mesh_get_block_at(const mesh_t *mesh, const int pos[3])
{
    block_t *block;
    HASH_FIND(hh, mesh->blocks, pos, 3 * sizeof(int), block);
    return block;
}

static block_t *mesh_add_block(mesh_t *mesh, const int pos[3])
{
    block_t *block;
    assert(pos[0] % BLOCK_SIZE == 0);
    assert(pos[1] % BLOCK_SIZE == 0);
    assert(pos[2] % BLOCK_SIZE == 0);
    assert(!mesh_get_block_at(mesh, pos));
    mesh_prepare_write(mesh);
    block = block_new(pos);
    block->id = mesh->next_block_id++;
    HASH_ADD(hh, mesh->blocks, pos, sizeof(block->pos), block);
    return block;
}

void mesh_merge(mesh_t *mesh, const mesh_t *other, int mode)
{
    assert(mesh && other);
    block_t *block, *other_block, *tmp;
    mesh_prepare_write(mesh);
    bool is_empty;
    mesh_iterator_t iter;
    int bpos[3];

    // Add empty blocks if needed.
    if (IS_IN(mode, MODE_OVER, MODE_MAX)) {
        iter = mesh_get_iterator(other);
        while (mesh_iter_blocks(other, &iter, bpos, NULL, NULL)) {
            if (!mesh_get_block_at(mesh, bpos)) {
                mesh_add_block(mesh, bpos);
            }
        }
    }

    HASH_ITER(hh, mesh->blocks, block, tmp) {
        other_block = mesh_get_block_at(other, block->pos);

        // XXX: instead of testing here, we should do it in block_merge
        // directly.
        is_empty = false;
        if (    block_is_empty(block, true) &&
                block_is_empty(other_block, true))
            is_empty = true;
        if (mode == MODE_MULT_ALPHA && block_is_empty(other_block, true))
            is_empty = true;

        if (is_empty) {
            HASH_DEL(mesh->blocks, block);
            block_delete(block);
            continue;
        }

        block_merge(block, other_block, mode);
    }
}

void mesh_get_at(const mesh_t *mesh, const int pos[3],
                 mesh_iterator_t *iter, uint8_t out[4])
{
    block_t *block;
    int p[3] = {pos[0] - mod(pos[0], N),
                pos[1] - mod(pos[1], N),
                pos[2] - mod(pos[2], N)};
    if (iter && (iter->flags & MESH_FOUND) &&
            memcmp(&iter->pos, p, sizeof(p)) == 0) {
        block_get_at(iter->block, pos, out);
        return;
    }
    HASH_FIND(hh, mesh->blocks, p, sizeof(p), block);
    if (iter) {
        iter->flags |= MESH_FOUND;
        iter->block = block;
        memcpy(iter->pos, p, sizeof(iter->pos));
    }
    return block_get_at(block, pos, out);
}

void mesh_set_at(mesh_t *mesh, const int pos[3], const uint8_t v[4],
                 mesh_iterator_t *iter)
{
    block_t *block;
    int p[3] = {pos[0] - mod(pos[0], N),
                pos[1] - mod(pos[1], N),
                pos[2] - mod(pos[2], N)};
    mesh_prepare_write(mesh);
    if (iter && iter->block && iter->block->data->ref > 1)
        iter->flags &= ~MESH_FOUND;
    if (iter && (iter->flags & MESH_FOUND) &&
            memcmp(&iter->pos, p, sizeof(p)) == 0) {
        if (!iter->block) iter->block = mesh_add_block(mesh, p);
        return block_set_at(iter->block, pos, v);
    }
    HASH_FIND(hh, mesh->blocks, p, sizeof(p), block);
    if (!block) block = mesh_add_block(mesh, p);
    if (iter) {
        iter->flags |= MESH_FOUND;
        iter->block = block;
        memcpy(iter->pos, p, sizeof(iter->pos));
    }
    return block_set_at(block, pos, v);
}

bool mesh_iter_voxels(const mesh_t *mesh, mesh_iterator_t *it,
                      int pos[3], uint8_t value[4])
{
    int i;
    if (it->flags & MESH_ITER_FINISHED) return false;
    if (!(it->flags & MESH_ITER_BOX)) {
        if (!mesh->blocks) return false;
        if (!it->block) it->block = mesh->blocks;
        pos[0] = it->pos[0] + it->block->pos[0];
        pos[1] = it->pos[1] + it->block->pos[1];
        pos[2] = it->pos[2] + it->block->pos[2];
        memcpy(value, it->block->data->voxels[
                it->pos[0] + it->pos[1] * N + it->pos[2] * N * N], 4);
        for (i = 0; i < 3; i++) {
            if (++it->pos[i] < N) break;
            it->pos[i] = 0;
        }
        if (i == 3) {
            it->block = it->block->hh.next;
            if (!it->block) it->flags |= MESH_ITER_FINISHED;
        }
    } else {
        assert(!(it->flags & MESH_ITER_SKIP_EMPTY)); // TODO later?
        pos[0] = it->bpos[0] + it->pos[0];
        pos[1] = it->bpos[1] + it->pos[1];
        pos[2] = it->bpos[2] + it->pos[2];
        block_get_at(it->block, pos, value);
        // Increase position in the block.
        for (i = 0; i < 3; i++) {
            if (++it->pos[i] < N) break;
            it->pos[i] = 0;
        }
        if (i < 3) return true; // Still in the same block.

        // If not then we look for the next block pos in the box.
        for (i = 0; i < 3; i++) {
            it->bpos[i] += N;
            if (it->bpos[i] < it->bbox[1][i]) break;
            it->bpos[i] = it->bbox[0][i] - mod(it->bbox[0][i], N);
        }
        if (i == 3) {
            it->flags |= MESH_ITER_FINISHED;
            return true;
        }
        HASH_FIND(hh, mesh->blocks, it->bpos, 3 * sizeof(int), it->block);
        // Test if the new block intersect the box. and skip it if not!
        // Need to implement a good box/box collision algo.
    }
    return true;
}

bool mesh_iter_blocks(const mesh_t *mesh, mesh_iterator_t *it,
                      int pos[3], uint64_t *data_id, int *id)
{
    if ((it->flags & MESH_ITER_FINISHED) || !mesh->blocks) return false;
    if (!it->block) it->block = mesh->blocks;
    if (pos) memcpy(pos, it->block->pos, sizeof(it->block->pos));
    if (data_id) *data_id = it->block->data->id;
    if (id) *id = it->block->id;
    it->block = it->block->hh.next;
    if (!it->block) it->flags |= MESH_ITER_FINISHED;
    return true;
}

uint64_t mesh_get_id(const mesh_t *mesh)
{
    return mesh->id;
}

void *mesh_get_block_data(const mesh_t *mesh, const int bpos[3],
                          mesh_accessor_t *iter)
{
    block_t *block = NULL;
    if (iter && (iter->flags & MESH_FOUND) &&
            memcmp(&iter->pos, bpos, sizeof(iter->pos)) == 0) {
        block = iter->block;
    } else {
        HASH_FIND(hh, mesh->blocks, bpos, sizeof(iter->pos), block);
    }
    return block ? block->data->voxels : NULL;
}

uint8_t mesh_get_alpha_at(const mesh_t *mesh, const int pos[3],
                          mesh_iterator_t *iter)
{
    uint8_t v[4];
    mesh_get_at(mesh, pos, iter, v);
    return v[3];
}
