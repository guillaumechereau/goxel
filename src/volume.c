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

#include "volume.h"
#include "uthash.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

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
    VOLUME_ITER_FINISHED                  = 1 << 9,
    VOLUME_ITER_BOX                       = 1 << 10,
    VOLUME_ITER_VOLUME2                     = 1 << 11,
};

typedef struct tile_data tile_data_t;
struct tile_data
{
    int         ref;
    uint64_t    id;
    uint8_t     voxels[TILE_SIZE * TILE_SIZE * TILE_SIZE][4]; // RGBA voxels.
};

struct tile
{
    UT_hash_handle  hh;     // The hash table of pos -> tiles in a volume.
    tile_data_t     *data;
    int             pos[3];
    uint64_t        id;
};

struct volume
{
    int ref;
    tile_t *tiles;
    int *tiles_ref;   // Used to implement copy on write of the tiles.
    uint64_t key; // Two volumes with the same key have the same value.
};

static uint64_t g_uid = 2; // Global id counter.

static volume_global_stats_t g_global_stats = {};

#define N TILE_SIZE

#define vec3_copy(a, b) do {b[0] = a[0]; b[1] = a[1]; b[2] = a[2];} while (0)
#define vec3_equal(a, b) (b[0] == a[0] && b[1] == a[1] && b[2] == a[2])

#define TILE_ITER(x, y, z) \
    for (z = 0; z < N; z++) \
        for (y = 0; y < N; y++) \
            for (x = 0; x < N; x++)

#define DATA_AT(d, x, y, z) (d->voxels[x + y * N + z * N * N])
#define TILE_AT(c, x, y, z) (DATA_AT(c->data, x, y, z))

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

static tile_data_t *get_empty_data(void)
{
    static tile_data_t *data = NULL;
    if (!data) {
        data = calloc(1, sizeof(*data));
        data->ref = 1;
        data->id = 0;
    }
    return data;
}

static bool tile_is_empty(const tile_t *tile, bool fast)
{
    int x, y, z;
    if (!tile) return true;
    if (tile->data->id == 0) return true;
    if (fast) return false;

    TILE_ITER(x, y, z) {
        if (TILE_AT(tile, x, y, z)[3]) return false;
    }
    return true;
}

static tile_t *tile_new(const int pos[3])
{
    tile_t *tile = calloc(1, sizeof(*tile));
    memcpy(tile->pos, pos, sizeof(tile->pos));
    tile->data = get_empty_data();
    tile->data->ref++;
    tile->id = g_uid++;
    return tile;
}

static void tile_delete(tile_t *tile)
{
    tile->data->ref--;
    if (tile->data->ref == 0) {
        free(tile->data);
        g_global_stats.nb_tiles--;
        g_global_stats.mem -= sizeof(*tile->data);
    }
    free(tile);
}

static tile_t *tile_copy(const tile_t *other)
{
    tile_t *tile = malloc(sizeof(*tile));
    *tile = *other;
    memset(&tile->hh, 0, sizeof(tile->hh));
    tile->data->ref++;
    tile->id = g_uid++;
    return tile;
}

static void tile_set_data(tile_t *tile, tile_data_t *data)
{
    tile->data->ref--;
    if (tile->data->ref == 0) {
        free(tile->data);
        g_global_stats.nb_tiles--;
        g_global_stats.mem -= sizeof(*tile->data);
    }
    tile->data = data;
    data->ref++;
}

// Copy the data if there are any other tiles having reference to it.
static void tile_prepare_write(tile_t *tile)
{
    if (tile->data->ref == 1) {
        tile->data->id = ++g_uid;
        return;
    }
    tile->data->ref--;
    tile_data_t *data;
    data = calloc(1, sizeof(*tile->data));
    memcpy(data->voxels, tile->data->voxels, N * N * N * 4);
    data->ref = 1;
    tile->data = data;
    tile->data->id = ++g_uid;

    g_global_stats.nb_tiles++;
    g_global_stats.mem += sizeof(*tile->data);
}

static void tile_get_at(const tile_t *tile, const int pos[3],
                         uint8_t out[4])
{
    int x, y, z;
    if (!tile) {
        memset(out, 0, 4);
        return;
    }
    x = pos[0] - tile->pos[0];
    y = pos[1] - tile->pos[1];
    z = pos[2] - tile->pos[2];
    assert(x >= 0 && x < N);
    assert(y >= 0 && y < N);
    assert(z >= 0 && z < N);
    memcpy(out, TILE_AT(tile, x, y, z), 4);
}

/*
 * Function: volume_get_bbox
 *
 * Get the bounding box of a volume.
 *
 * Inputs:
 *   volume   - The volume
 *   exact  - If true, compute the exact bounding box.  If false, returns
 *            an approximation that might be slightly bigger than the
 *            actual box, but faster to compute.
 *
 * Outputs:
 *   bbox  - The bounding box as the bottom left and top right corner of
 *           the volume.  If the volume is empty, this will contain all zero.
 *
 * Returns:
 *   true if the volume is not empty.
 */
bool volume_get_bbox(const volume_t *volume, int bbox[2][3], bool exact)
{
    tile_t *tile;
    int ret[2][3] = {{INT_MAX, INT_MAX, INT_MAX},
                     {INT_MIN, INT_MIN, INT_MIN}};
    int pos[3];
    volume_iterator_t iter;
    bool empty = false;

    if (!exact) {
        for (tile = volume->tiles; tile; tile = tile->hh.next) {
            if (tile_is_empty(tile, true)) continue;
            ret[0][0] = min(ret[0][0], tile->pos[0]);
            ret[0][1] = min(ret[0][1], tile->pos[1]);
            ret[0][2] = min(ret[0][2], tile->pos[2]);
            ret[1][0] = max(ret[1][0], tile->pos[0] + N);
            ret[1][1] = max(ret[1][1], tile->pos[1] + N);
            ret[1][2] = max(ret[1][2], tile->pos[1] + N);
        }
    } else {
        iter = volume_get_iterator(volume, VOLUME_ITER_SKIP_EMPTY);
        while (volume_iter(&iter, pos)) {
            if (!volume_get_alpha_at(volume, &iter, pos)) continue;
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

static void volume_prepare_write(volume_t *volume)
{
    tile_t *tiles, *tile, *new_tile;
    assert(*volume->tiles_ref > 0);
    volume->key = g_uid++;
    if (*volume->tiles_ref == 1)
        return;
    (*volume->tiles_ref)--;
    volume->tiles_ref = calloc(1, sizeof(*volume->tiles_ref));
    *volume->tiles_ref = 1;
    tiles = volume->tiles;
    volume->tiles = NULL;
    for (tile = tiles; tile; tile = tile->hh.next) {
        tile->id = g_uid++; // Invalidate all accessors.
        new_tile = tile_copy(tile);
        HASH_ADD(hh, volume->tiles, pos, sizeof(new_tile->pos), new_tile);
    }
    g_global_stats.nb_volumes++;
}

static tile_t *volume_add_tile(volume_t *volume, const int pos[3]);

static void volume_add_neighbors_tiles(volume_t *volume)
{
    const int POS[6][3] = {
        {0, 0, -1}, {0, 0, +1},
        {0, -1, 0}, {0, +1, 0},
        {-1, 0, 0}, {+1, 0, 0},
    };
    int i, p[3] = {};
    uint64_t key = volume->key;
    tile_t *tile, *tmp, *other;

    volume_prepare_write(volume);
    HASH_ITER(hh, volume->tiles, tile, tmp) {
        if (tile_is_empty(tile, true)) continue;
        for (i = 0; i < 6; i++) {
            p[0] = tile->pos[0] + POS[i][0] * N;
            p[1] = tile->pos[1] + POS[i][1] * N;
            p[2] = tile->pos[2] + POS[i][2] * N;
            HASH_FIND(hh, volume->tiles, p, 3 * sizeof(int), other);
            if (!other) volume_add_tile(volume, p);
        }
    }
    // Adding empty tiles shouldn't change the key of the volume.
    volume->key = key;
}

void volume_remove_empty_tiles(volume_t *volume, bool fast)
{
    tile_t *tile, *tmp;
    uint64_t key = volume->key;
    volume_prepare_write(volume);
    HASH_ITER(hh, volume->tiles, tile, tmp) {
        if (tile_is_empty(tile, false)) {
            HASH_DEL(volume->tiles, tile);
            assert(volume->tiles != tile);
            tile_delete(tile);
        }
    }
    // Empty tiles shouldn't change the key of the volume.
    volume->key = key;
}

bool volume_is_empty(const volume_t *volume)
{
    return volume == NULL || volume->tiles == NULL;
}

volume_t *volume_new(void)
{
    volume_t *volume;
    volume = calloc(1, sizeof(*volume));
    volume->ref = 1;
    volume->tiles_ref = calloc(1, sizeof(*volume->tiles_ref));
    volume->key = 1; // Empty volume key.
    *volume->tiles_ref = 1;
    g_global_stats.nb_volumes++;
    return volume;
}

volume_t *volume_dup(const volume_t *volume)
{
    volume_t *ret = (volume_t*)volume;
    ret->ref++;
    return ret;
}

volume_iterator_t volume_get_iterator(const volume_t *volume, int flags)
{
    return (volume_iterator_t){
        .volume = volume,
        .flags = flags,
    };
}

volume_iterator_t volume_get_union_iterator(
        const volume_t *m1, const volume_t *m2, int flags)
{
    return (volume_iterator_t){
        .volume = m1,
        .volume2 = m2,
        .flags = flags,
    };
}

volume_iterator_t volume_get_box_iterator(const volume_t *volume,
                                      const float box[4][4], int flags)
{
    int volume_bbox[2][3];
    volume_iterator_t iter = {
        .volume = volume,
        .flags = VOLUME_ITER_BOX | VOLUME_ITER_VOXELS | flags,
    };
    memcpy(iter.box, box, sizeof(iter.box));
    box_get_bbox(iter.box, iter.bbox);

    if (flags & VOLUME_ITER_SKIP_EMPTY) {
        volume_get_bbox(volume, volume_bbox, false);
        bbox_intersection(volume_bbox, iter.bbox, iter.bbox);
    }

    return iter;
}

volume_accessor_t volume_get_accessor(const volume_t *volume)
{
    return (volume_accessor_t){0};
}


void volume_clear(volume_t *volume)
{
    assert(volume);
    tile_t *tile, *tmp;
    volume_prepare_write(volume);
    HASH_ITER(hh, volume->tiles, tile, tmp) {
        HASH_DEL(volume->tiles, tile);
        assert(volume->tiles != tile);
        tile_delete(tile);
    }
    volume->key = 1; // Empty volume key.
    volume->tiles = NULL;
}

void volume_delete(volume_t *volume)
{
    tile_t *tile, *tmp;
    if (!volume) return;
    if (--volume->ref > 0) return;
    (*volume->tiles_ref)--;
    if (*volume->tiles_ref == 0) {
        HASH_ITER(hh, volume->tiles, tile, tmp) {
            HASH_DEL(volume->tiles, tile);
            assert(volume->tiles != tile);
            tile_delete(tile);
        }
        free(volume->tiles_ref);
        g_global_stats.nb_volumes--;
    }
    free(volume);
}

volume_t *volume_copy(const volume_t *volume)
{
    volume_t *ret;

    if (!volume) return volume_new();
    ret = calloc(1, sizeof(*volume));
    ret->ref = 1;
    ret->tiles = volume->tiles;
    ret->tiles_ref = volume->tiles_ref;
    ret->key = volume->key;
    (*ret->tiles_ref)++;
    return ret;
}

void volume_set(volume_t *volume, const volume_t *other)
{
    tile_t *tile, *tmp;
    assert(volume && other);
    if (volume->tiles == other->tiles) return; // Already the same.
    (*volume->tiles_ref)--;
    if (*volume->tiles_ref == 0) {
        HASH_ITER(hh, volume->tiles, tile, tmp) {
            HASH_DEL(volume->tiles, tile);
            assert(volume->tiles != tile);
            tile_delete(tile);
        }
        free(volume->tiles_ref);
        g_global_stats.nb_volumes--;
    }
    volume->tiles = other->tiles;
    volume->tiles_ref = other->tiles_ref;
    volume->key = other->key;
    (*volume->tiles_ref)++;
}

static uint64_t get_tile_id(const tile_t *tile)
{
    return tile ? tile->id : 1;
}

static tile_t *volume_get_tile_at(const volume_t *volume, const int pos[3],
                                  volume_accessor_t *it)
{
    tile_t *tile;
    int p[3] = {};
    p[0] = pos[0] & ~(int)(N - 1);
    p[1] = pos[1] & ~(int)(N - 1);
    p[2] = pos[2] & ~(int)(N - 1);
    if (!it) {
        HASH_FIND(hh, volume->tiles, p, sizeof(p), tile);
        return tile;
    }

    if (    it->tile_id && it->tile_id == get_tile_id(it->tile) &&
            vec3_equal(it->tile_pos, p)) {
        return it->tile;
    }
    HASH_FIND(hh, volume->tiles, p, sizeof(p), tile);
    it->tile = tile;
    it->tile_id = get_tile_id(tile);
    vec3_copy(p, it->tile_pos);
    return tile;
}

static tile_t *volume_add_tile(volume_t *volume, const int pos[3])
{
    tile_t *tile;
    assert(pos[0] % TILE_SIZE == 0);
    assert(pos[1] % TILE_SIZE == 0);
    assert(pos[2] % TILE_SIZE == 0);
    assert(!volume_get_tile_at(volume, pos, NULL));
    volume_prepare_write(volume);
    tile = tile_new(pos);
    HASH_ADD(hh, volume->tiles, pos, sizeof(tile->pos), tile);
    return tile;
}

void volume_get_at(const volume_t *volume, volume_iterator_t *it,
                 const int pos[3], uint8_t out[4])
{
    tile_t *tile;
    int p[3];

    if (it && it->tile_id && it->tile_id == get_tile_id(it->tile)) {
        p[0] = pos[0] - it->tile_pos[0];
        p[1] = pos[1] - it->tile_pos[1];
        p[2] = pos[2] - it->tile_pos[2];
        if (    p[0] >= 0 && p[0] < N &&
                p[1] >= 0 && p[1] < N &&
                p[2] >= 0 && p[2] < N) {
            if (!it->tile)
                memset(out, 0, 4);
            else
                memcpy(out, TILE_AT(it->tile, p[0], p[1], p[2]), 4);
            return;
        }
    }

    tile = volume_get_tile_at(volume, pos, it);
    return tile_get_at(tile, pos, out);
}

void volume_set_at(volume_t *volume, volume_iterator_t *iter,
                 const int pos[3], const uint8_t v[4])
{
    int p[3] = {pos[0] & ~(int)(N - 1),
                pos[1] & ~(int)(N - 1),
                pos[2] & ~(int)(N - 1)};
    volume_prepare_write(volume);

    tile_t *tile = volume_get_tile_at(volume, p, iter);

    if (!tile) {
        tile = volume_add_tile(volume, p);
        if (iter) {
            iter->tile = tile;
            iter->tile_id = get_tile_id(tile);
            vec3_copy(p, iter->tile_pos);
        }
    }

    tile_prepare_write(tile);
    p[0] = pos[0] - tile->pos[0];
    p[1] = pos[1] - tile->pos[1];
    p[2] = pos[2] - tile->pos[2];
    assert(p[0] >= 0 && p[0] < N);
    assert(p[1] >= 0 && p[1] < N);
    assert(p[2] >= 0 && p[2] < N);
    memcpy(TILE_AT(tile, p[0], p[1], p[2]), v, 4);
}

void volume_clear_tile(volume_t *volume, volume_iterator_t *it, const int pos[3])
{
    tile_t *tile;
    volume_prepare_write(volume);
    tile = volume_get_tile_at(volume, pos, it);
    if (!tile) return;
    HASH_DEL(volume->tiles, tile);
    assert(volume->tiles != tile);
    tile_delete(tile);
    if (it) it->tile = NULL;
}


static bool volume_iter_next_tile_box(volume_iterator_t *it)
{
    int i;
    const volume_t *volume = it->volume;
    if (!it->tile_id) {
        it->tile_pos[0] = it->bbox[0][0] & ~(int)(N - 1);
        it->tile_pos[1] = it->bbox[0][1] & ~(int)(N - 1);
        it->tile_pos[2] = it->bbox[0][2] & ~(int)(N - 1);
        goto end;
    }

    for (i = 0; i < 3; i++) {
        it->tile_pos[i] += N;
        if (it->tile_pos[i] <= it->bbox[1][i]) break;
        it->tile_pos[i] = it->bbox[0][i] & ~(int)(N - 1);
    }
    if (i == 3) return false;

end:
    HASH_FIND(hh, volume->tiles, it->tile_pos, 3 * sizeof(int), it->tile);
    it->tile_id = get_tile_id(it->tile);
    vec3_copy(it->tile_pos, it->pos);
    return true;
}

static bool volume_iter_next_tile_union(volume_iterator_t *it)
{
    it->tile = it->tile ? it->tile->hh.next : it->volume->tiles;
    if (!it->tile && !(it->flags & VOLUME_ITER_VOLUME2)) {
        it->tile = it->volume2->tiles;
        it->flags |= VOLUME_ITER_VOLUME2;
    }
    if (!it->tile) return false;
    it->tile_id = it->tile->id;
    vec3_copy(it->tile->pos, it->tile_pos);
    vec3_copy(it->tile->pos, it->pos);

    // Discard tiles that we already did from the first volume.
    if (it->flags & VOLUME_ITER_VOLUME2) {
        if (volume_get_tile_at(it->volume, it->tile_pos, NULL))
            return volume_iter_next_tile_union(it);
    }
    return true;
}

static bool volume_iter_next_tile(volume_iterator_t *it)
{
    if (it->tile_id && it->tile_id != get_tile_id(it->tile)) {
        it->tile = volume_get_tile_at(
            (it->flags & VOLUME_ITER_VOLUME2) ? it->volume2 : it->volume,
            it->tile_pos,
            it);
    }

    if (it->flags & VOLUME_ITER_BOX) return volume_iter_next_tile_box(it);
    if (it->volume2) return volume_iter_next_tile_union(it);

    it->tile = it->tile ? it->tile->hh.next : it->volume->tiles;
    if (!it->tile) return false;
    it->tile_id = it->tile->id;
    vec3_copy(it->tile->pos, it->tile_pos);
    vec3_copy(it->tile->pos, it->pos);
    return true;
}

int volume_iter(volume_iterator_t *it, int pos[3])
{
    int i;
    if (!it->tile_id) { // First call.
        // XXX: this is not good: volume_iter shouldn't make change to the
        // volume.
        if (it->flags & VOLUME_ITER_INCLUDES_NEIGHBORS)
            volume_add_neighbors_tiles((volume_t*)it->volume);
        if (!volume_iter_next_tile(it)) return 0;
        goto end;
    }
    if (it->flags & VOLUME_ITER_TILES) goto next_tile;

    for (i = 0; i < 3; i++) {
        if (++it->pos[i] < it->tile_pos[i] + N) break;
        it->pos[i] = it->tile_pos[i];
    }
    if (i < 3) goto end;

next_tile:
    if (!volume_iter_next_tile(it)) {
        // XXX: this is not good: volume_iter shouldn't make changes to the
        // volume.
        if (it->flags & VOLUME_ITER_INCLUDES_NEIGHBORS)
            volume_remove_empty_tiles((volume_t*)it->volume, true);
        return 0;
    }

end:
    if (pos) vec3_copy(it->pos, pos);
    return 1;
}

uint64_t volume_get_key(const volume_t *volume)
{
    return volume ? volume->key : 0;
}

void *volume_get_tile_data(const volume_t *volume, volume_accessor_t *iter,
                          const int bpos[3], uint64_t *id)
{
    tile_t *tile = NULL;
    if (    iter &&
            iter->tile_id &&
            iter->tile_id == get_tile_id(iter->tile) &&
            memcmp(&iter->pos, bpos, sizeof(iter->pos)) == 0) {
        tile = iter->tile;
    } else {
        HASH_FIND(hh, volume->tiles, bpos, sizeof(iter->pos), tile);
    }
    if (id) *id = tile ? tile->data->id : 0;
    return tile ? tile->data->voxels : NULL;
}

uint8_t volume_get_alpha_at(const volume_t *volume, volume_iterator_t *iter,
                          const int pos[3])
{
    uint8_t v[4];
    volume_get_at(volume, iter, pos, v);
    return v[3];
}

void volume_copy_tile(const volume_t *src, const int src_pos[3],
                     volume_t *dst, const int dst_pos[3])
{
    tile_t *b1, *b2;
    volume_prepare_write(dst);
    b1 = volume_get_tile_at(src, src_pos, NULL);
    b2 = volume_get_tile_at(dst, dst_pos, NULL);
    if (!b2) b2 = volume_add_tile(dst, dst_pos);
    tile_set_data(b2, b1->data);
}

void volume_read(const volume_t *volume,
               const int pos[3], const int size[3],
               uint8_t *data)
{
    // For the moment we only support the case where we get the rectangle
    // of a tile plus a one voxel border around it!
    assert(pos[0] - (pos[0] & ~(int)(N - 1)) == N - 1);
    assert(pos[1] - (pos[1] & ~(int)(N - 1)) == N - 1);
    assert(pos[2] - (pos[2] & ~(int)(N - 1)) == N - 1);
    assert(size[0] == N + 2);
    assert(size[1] == N + 2);
    assert(size[2] == N + 2);

    tile_t *tile;
    int tile_pos[3] = {pos[0] + 1, pos[1] + 1, pos[2] + 1};
    int i, z, y, x, dx, dy, dz, p[3];
    uint8_t v[4];
    volume_accessor_t accessor;

    memset(data, 0, size[0] * size[1] * size[2] * 4);
    tile = volume_get_tile_at(volume, tile_pos, NULL);
    if (!tile) goto rest;

    for (z = 0; z < N; z++)
    for (y = 0; y < N; y++)
    for (x = 0; x < N; x++) {
        dx = x + 1;
        dy = y + 1;
        dz = z + 1;
        memcpy(&data[(dz * size[1] * size[0] + dy * size[0] + dx) * 4],
               &tile->data->voxels[z * N * N + y * N + x],
               4);
    }

rest:
    // Fill the rest.
    // In order to optimize access, we iter in such a way to avoid changing
    // the current tile: start with the planes, then the edges, and finally
    // the 8 corners.
    accessor = volume_get_accessor(volume);
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
        volume_get_at(volume, &accessor, p, v);
        memcpy(&data[(z * size[1] * size[0] + y * size[0] + x) * 4], v, 4);
    }
}

int volume_get_tiles_count(const volume_t *volume)
{
    return HASH_COUNT(volume->tiles);
}

void volume_get_global_stats(volume_global_stats_t *stats)
{
    *stats = g_global_stats;
}

char* volume_copy_to_string(const volume_t *volume, const int aabb[2][3])
{
    int pos[3];
    int volume_pos[3];
    int size[3];
    char str_header[256];
    size_t total_size;
    char *encoded_str;
    char *writer;
    int i;
    uint8_t voxel[4];

    size[0] = aabb[1][0] - aabb[0][0];
    size[1] = aabb[1][1] - aabb[0][1];
    size[2] = aabb[1][2] - aabb[0][2];

    sprintf(str_header, "goxel::voxels::%dx%dx%d::", size[0], size[1], size[2]);
    total_size = strlen(str_header) + size[0] * size[1] * size[2] * 8 + 1;
    encoded_str = malloc(total_size);
    writer = encoded_str + strlen(str_header);

    strcpy(encoded_str, str_header);

    for (pos[2] = 0; pos[2] < size[2]; pos[2]++) {
        for (pos[1] = 0; pos[1] < size[1]; pos[1]++) {
            for (pos[0] = 0; pos[0] < size[0]; pos[0]++) {
                memcpy(volume_pos, pos, sizeof(pos));

                for (i = 0; i < 3; i++) {
                    volume_pos[i] += aabb[0][i];
                }

                volume_get_at(volume, NULL, volume_pos, voxel);
                for (i = 0; i < 4; i++) {
                    writer += sprintf(writer, "%02hhx", voxel[i]);
                }
            }
        }
    }

    return encoded_str;
}