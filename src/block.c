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

/*
 * Here is the convention I used for the cube vertices, edges and faces:
 *
 *           v4 +----------e4---------+ v5
 *             /.                    /|
 *            / .                   / |
 *          e7  .                 e5  |                    +-----------+
 *          /   .                 /   |                   /           /|
 *         /    .                /    |                  /   f1      / |  <f2
 *     v7 +----------e6---------+ v6  |                 +-----------+  |
 *        |     .               |     e9            f5> |           |f4|
 *        |     e8              |     |                 |           |  |
 *        |     .               |     |                 |    f3     |  +
 *        |     .               |     |                 |           | /
 *        |  v0 . . . .e0 . . . | . . + v1              |           |/
 *       e11   .                |    /                  +-----------+
 *        |   .                e10  /                         ^
 *        |  e3                 |  e1                         f0
 *        | .                   | /
 *        |.                    |/
 *     v3 +---------e2----------+ v2
 *
 */


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

static const int N = BLOCK_SIZE;

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

bool block_is_empty(const block_t *block, bool fast)
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

block_t *block_new(const int pos[3])
{
    block_t *block = calloc(1, sizeof(*block));
    memcpy(block->pos, pos, sizeof(block->pos));
    block->data = get_empty_data();
    block->data->ref++;
    return block;
}

void block_delete(block_t *block)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        goxel->block_count--;
    }
    free(block);
}

block_t *block_copy(const block_t *other)
{
    block_t *block = malloc(sizeof(*block));
    *block = *other;
    memset(&block->hh, 0, sizeof(block->hh));
    block->data->ref++;
    return block;
}

void block_set_data(block_t *block, block_data_t *data)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        goxel->block_count--;
    }
    block->data = data;
    data->ref++;
}

box_t block_get_box(const block_t *block, bool exact)
{
    box_t ret;
    int x, y, z;
    int xmin = N, xmax = 0, ymin = N, ymax = 0, zmin = N, zmax = 0;
    vec3_t pos = vec3(block->pos[0] + N / 2,
                      block->pos[1] + N / 2,
                      block->pos[2] + N / 2);
    if (!exact)
        return bbox_from_extents(pos, N / 2, N / 2, N / 2);
    BLOCK_ITER(x, y, z) {
        if (BLOCK_AT(block, x, y, z)[3]) {
            xmin = min(xmin, x);
            ymin = min(ymin, y);
            zmin = min(zmin, z);
            xmax = max(xmax, x);
            ymax = max(ymax, y);
            zmax = max(zmax, z);
        }
    }
    if (xmin > xmax) return box_null;
    ret = bbox_from_points(vec3(xmin - 0.5, ymin - 0.5, zmin - 0.5),
                           vec3(xmax + 0.5, ymax + 0.5, zmax + 0.5));
    vec3_iadd(&ret.p, pos);
    vec3_isub(&ret.p, vec3(N / 2 - 0.5, N / 2 - 0.5, N / 2 - 0.5));
    return ret;
}

// Copy the data if there are any other blocks having reference to it.
static void block_prepare_write(block_t *block)
{
    if (block->data->ref == 1) {
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

void block_fill(block_t *block,
                void (*get_color)(const int pos[3], uint8_t out[4],
                                  void *user_data),
                void *user_data)
{
    int x, y, z;
    int p[3];
    uint8_t c[4];
    block_prepare_write(block);
    BLOCK_ITER(x, y, z) {
        p[0] = block->pos[0] + x;
        p[1] = block->pos[1] + y;
        p[2] = block->pos[2] + z;
        get_color(p, c, user_data);
        memcpy(BLOCK_AT(block, x, y, z), c, 4);
    }
}

static bool can_skip(const uint8_t v[4], int mode, const uint8_t c[4])
{
    return (v[3] && (mode == MODE_OVER) && vec4_equal(c, v)) ||
            (!v[3] && (mode == MODE_SUB ||
                       mode == MODE_PAINT ||
                       mode == MODE_SUB_CLAMP));
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

// XXX: cleanup this function.
void block_op(block_t *block, painter_t *painter, const box_t *box)
{
    int x, y, z;
    mat4_t mat = mat4_identity;
    vec3_t p, size;
    float k, v;
    int mode = painter->mode;
    uint8_t c[4];
    float (*shape_func)(const vec3_t*, const vec3_t*, float smoothness);
    shape_func = painter->shape->func;
    bool invert = false;
    int min_x = 0, min_y = 0, min_z = 0, max_x = N, max_y = N, max_z = N;
    box_t clip;

    if (mode == MODE_INTERSECT) {
        mode = MODE_SUB;
        invert = true;
    }

    size = box_get_size(*box);
    mat4_imul(&mat, box->mat);
    mat4_iscale(&mat, 1 / size.x, 1 / size.y, 1 / size.z);
    mat4_invert(&mat);

    mat4_itranslate(&mat, block->pos[0], block->pos[1], block->pos[2]);

    if (painter->box) {
        clip = *painter->box;
        min_x = max(min_x, clip.p.x - clip.w.x - block->pos[0]);
        max_x = min(max_x, clip.p.x + clip.w.x - block->pos[0]);
        min_y = max(min_y, clip.p.y - clip.h.y - block->pos[1]);
        max_y = min(max_y, clip.p.y + clip.h.y - block->pos[1]);
        min_z = max(min_z, clip.p.z - clip.d.z - block->pos[2]);
        max_z = min(max_z, clip.p.z + clip.d.z - block->pos[2]);
    }

    for (z = min_z; z < max_z; z++)
    for (y = min_y; y < max_y; y++)
    for (x = min_x; x < max_x; x++) {
        vec4_copy(painter->color, c);
        if (can_skip(BLOCK_AT(block, x, y, z), mode, c)) continue;
        p = mat4_mul_vec3(mat, vec3(x, y, z));
        k = shape_func(&p, &size, painter->smoothness);
        k = clamp(k / painter->smoothness, -1.0f, 1.0f);
        v = k / 2.0f + 0.5f;
        if (invert) v = 1.0f - v;
        if (v) {
            block_prepare_write(block);
            c[3] *= v;
            combine(BLOCK_AT(block, x, y, z), c, mode,
                    BLOCK_AT(block, x, y, z));
        }
    }
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

void block_merge(block_t *block, const block_t *other, int mode)
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

void block_get_at(const block_t *block, const int pos[3], uint8_t out[4])
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

void block_set_at(block_t *block, const int pos[3], const uint8_t v[4])
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

void block_shift_alpha(block_t *block, int v)
{
    block_prepare_write(block);
    int i;
    for (i = 0; i < N * N * N; i++) {
        block->data->voxels[i][3] = clamp(block->data->voxels[i][3] + v, 0, 255);
    }
}

