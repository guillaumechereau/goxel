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
    MESH_FOUND                          = 1 << 8,
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
};

struct mesh
{
    block_t *blocks;
    int *ref;   // Used to implement copy on write of the blocks.
    uint64_t id;     // global uniq id, change each time a mesh changes.
};

#define N BLOCK_SIZE

#define vec3_copy(a, b) do {b[0] = a[0]; b[1] = a[1]; b[2] = a[2];} while (0)

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
static int mesh_del(void *data_)
{
    mesh_t *mesh = data_;
    mesh_delete(mesh);
    return 0;
}

static block_t *mesh_get_block_at(const mesh_t *mesh, const int pos[3],
                                  mesh_accessor_t *it);
static block_t *mesh_add_block(mesh_t *mesh, const int pos[3]);

void mesh_copy_block(const mesh_t *src, const int src_pos[3],
                     mesh_t *dst, const int dst_pos[3])
{
    block_t *b1, *b2;
    b1 = mesh_get_block_at(src, src_pos, NULL);
    b2 = mesh_get_block_at(dst, dst_pos, NULL);
    if (!b2) b2 = mesh_add_block(dst, dst_pos);
    block_set_data(b2, b1->data);
}

static void block_merge(mesh_t *mesh, const mesh_t *other, const int pos[3],
                        int mode)
{
    int p[3];
    int x, y, z;
    uint64_t id1, id2;
    mesh_t *block;
    uint8_t v1[4], v2[4];
    static cache_t *cache = NULL;
    mesh_accessor_t a1, a2, a3;

    mesh_get_block_data(mesh,  NULL, pos, &id1);
    mesh_get_block_data(other, NULL, pos, &id2);

    if (id2 == 0) return;
    if (IS_IN(mode, MODE_OVER, MODE_MAX) && id1 == 0) {
        mesh_copy_block(other, pos, mesh, pos);
        return;
    }

    // Check if the merge op has been cached.
    if (!cache) cache = cache_create(512);
    struct {
        uint64_t id1;
        uint64_t id2;
        int      mode;
        int      _pad;
    } key = { id1, id2, mode };
    _Static_assert(sizeof(key) == 24, "");
    block = cache_get(cache, &key, sizeof(key));
    if (block) goto end;

    block = mesh_new();
    a1 = mesh_get_accessor(mesh);
    a2 = mesh_get_accessor(other);
    a3 = mesh_get_accessor(block);

    for (z = 0; z < N; z++)
    for (y = 0; y < N; y++)
    for (x = 0; x < N; x++) {
        p[0] = pos[0] + x;
        p[1] = pos[1] + y;
        p[2] = pos[2] + z;
        mesh_get_at(mesh,  p, &a1, v1);
        mesh_get_at(other, p, &a2, v2);
        combine(v1, v2, mode, v1);
        mesh_set_at(block, (int[]){x, y, z}, v1, &a3);
    }
    cache_add(cache, &key, sizeof(key), block, 1, mesh_del);

end:
    mesh_copy_block(block, (int[]){0, 0, 0}, mesh, pos);
    return;
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
    mesh->ref = calloc(1, sizeof(*mesh->ref));
    mesh->id = goxel->next_uid++;
    *mesh->ref = 1;
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

mesh_iterator_t mesh_get_box_iterator(const mesh_t *mesh, const box_t box)
{
    int i, j;
    vec3_t vertices[8];
    mesh_iterator_t iter = {
        .mesh = mesh,
        .box = box,
        .flags = MESH_ITER_BOX | MESH_ITER_VOXELS,
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
    (*mesh->ref)++;
}

static block_t *mesh_get_block_at(const mesh_t *mesh, const int pos[3],
                                  mesh_accessor_t *it)
{
    block_t *block;
    int p[3] = {pos[0] - mod(pos[0], N),
                pos[1] - mod(pos[1], N),
                pos[2] - mod(pos[2], N)};

    if (!it) {
        HASH_FIND(hh, mesh->blocks, p, 3 * sizeof(int), block);
        return block;
    }

    if ((it->flags & MESH_FOUND) &&
            memcmp(&it->block_pos, p, sizeof(p)) == 0) {
        return it->block;
    }
    HASH_FIND(hh, mesh->blocks, p, 3 * sizeof(int), block);
    it->block = block;
    vec3_copy(p, it->block_pos);
    it->flags |= MESH_FOUND;
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

void mesh_merge(mesh_t *mesh, const mesh_t *other, int mode)
{
    assert(mesh && other);
    mesh_iterator_t iter;
    int bpos[3];
    iter = mesh_get_union_iterator(mesh, other, MESH_ITER_BLOCKS);
    while (mesh_iter(&iter, bpos)) {
        block_merge(mesh, other, bpos, mode);
    }
}

void mesh_get_at(const mesh_t *mesh, const int pos[3],
                 mesh_iterator_t *iter, uint8_t out[4])
{
    block_t *block = mesh_get_block_at(mesh, pos, iter);
    return block_get_at(block, pos, out);
}

void mesh_set_at(mesh_t *mesh, const int pos[3], const uint8_t v[4],
                 mesh_iterator_t *iter)
{
    int p[3] = {pos[0] - mod(pos[0], N),
                pos[1] - mod(pos[1], N),
                pos[2] - mod(pos[2], N)};
    mesh_prepare_write(mesh);
    // XXX: it would be better to avoid this.  Instead all the blocks could
    // be stored in an array each with a uniq id (!= hash or mesh id), and
    // we could check that a block hasn't changed somehow.
    if (iter && iter->block && iter->block->data->ref > 1)
        iter->flags &= ~MESH_FOUND;
    block_t *block = mesh_get_block_at(mesh, p, iter);
    if (!block) {
        block = mesh_add_block(mesh, p);
        if (iter) iter->flags &= ~MESH_FOUND;
    }
    return block_set_at(block, pos, v);
}


static bool mesh_iter_next_block_box(mesh_iterator_t *it)
{
    int i;
    const mesh_t *mesh = it->mesh;
    if (!it->block_found) {
        it->block_pos[0] = it->bbox[0][0] - mod(it->bbox[0][0], N);
        it->block_pos[1] = it->bbox[0][1] - mod(it->bbox[0][1], N);
        it->block_pos[2] = it->bbox[0][2] - mod(it->bbox[0][2], N);
        goto end;
    }

    for (i = 0; i < 3; i++) {
        it->block_pos[i] += N;
        if (it->block_pos[i] < it->bbox[1][i]) break;
        it->block_pos[i] = it->bbox[0][i] - mod(it->bbox[0][i], N);
    }
    if (i == 3) return false;

end:
    it->block_found = true;
    HASH_FIND(hh, mesh->blocks, it->block_pos, 3 * sizeof(int), it->block);
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
    it->block_found = true;
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
    if (it->flags & MESH_ITER_BOX) return mesh_iter_next_block_box(it);
    if (it->mesh2) return mesh_iter_next_block_union(it);

    it->block = it->block ? it->block->hh.next : it->mesh->blocks;
    if (!it->block) return false;
    it->block_found = true;
    vec3_copy(it->block->pos, it->block_pos);
    vec3_copy(it->block->pos, it->pos);
    return true;
}

int mesh_iter(mesh_iterator_t *it, int pos[3])
{
    int i;
    if (!it->block_found) { // First call.
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
    if (!mesh_iter_next_block(it)) return 0;

end:
    vec3_copy(it->pos, pos);
    return 1;
}

uint64_t mesh_get_id(const mesh_t *mesh)
{
    return mesh->id;
}

void *mesh_get_block_data(const mesh_t *mesh, mesh_accessor_t *iter,
                          const int bpos[3], uint64_t *id)
{
    block_t *block = NULL;
    if (iter && (iter->flags & MESH_FOUND) &&
            memcmp(&iter->pos, bpos, sizeof(iter->pos)) == 0) {
        block = iter->block;
    } else {
        HASH_FIND(hh, mesh->blocks, bpos, sizeof(iter->pos), block);
    }
    if (id) *id = block ? block->data->id : 0;
    return block ? block->data->voxels : NULL;
}

uint8_t mesh_get_alpha_at(const mesh_t *mesh, const int pos[3],
                          mesh_iterator_t *iter)
{
    uint8_t v[4];
    mesh_get_at(mesh, pos, iter, v);
    return v[3];
}
