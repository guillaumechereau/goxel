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
    uvec4b_t    voxels[BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE]; // RGBA voxels.
};

struct block
{
    UT_hash_handle  hh;     // The hash table of pos -> blocks in a mesh.
    block_data_t    *data;
    vec3i_t         pos;
    int             id;     // id of the block in the mesh it belongs.
};

static const int N = BLOCK_SIZE;

#define BLOCK_ITER(x, y, z) \
    for (z = 0; z < N; z++) \
        for (y = 0; y < N; y++) \
            for (x = 0; x < N; x++)

#define BLOCK_ITER_INSIDE(x, y, z) \
    for (z = 1; z < N - 1; z++) \
        for (y = 1; y < N - 1; y++) \
            for (x = 1; x < N - 1; x++)

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
        if (BLOCK_AT(block, x, y, z).a) return false;
    }
    return true;
}

block_t *block_new(const vec3i_t *pos)
{
    block_t *block = calloc(1, sizeof(*block));
    block->pos = *pos;
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
    vec3_t pos = vec3(block->pos.x, block->pos.y, block->pos.z);
    if (!exact)
        return bbox_from_extents(pos, N / 2, N / 2, N / 2);
    BLOCK_ITER(x, y, z) {
        if (BLOCK_AT(block, x, y, z).a) {
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

// XXX: only used once.
static vec3_t block_get_voxel_pos(const block_t *block, int x, int y, int z)
{
    return vec3(block->pos.x + x - BLOCK_SIZE / 2,
                block->pos.y + y - BLOCK_SIZE / 2,
                block->pos.z + z - BLOCK_SIZE / 2);
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
                uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
                void *user_data)
{
    int x, y, z;
    vec3_t p;
    uvec4b_t c;
    block_prepare_write(block);
    BLOCK_ITER(x, y, z) {
        p = block_get_voxel_pos(block, x, y, z);
        c = get_color(&p, user_data);
        BLOCK_AT(block, x, y, z) = c;
    }
}

static bool can_skip(uvec4b_t v, int mode, uvec4b_t c)
{
    return (v.a && (mode == MODE_OVER) && uvec4b_equal(c, v)) ||
            (!v.a && (mode == MODE_SUB ||
                      mode == MODE_PAINT ||
                      mode == MODE_SUB_CLAMP));
}

static uvec4b_t combine(uvec4b_t a, uvec4b_t b, int mode)
{
    uvec4b_t ret = a;
    int i, aa = a.a, ba = b.a;
    if (mode == MODE_PAINT) {
        ret.rgb = uvec3b_mix(a.rgb, b.rgb, ba / 255.);
    }
    else if (mode == MODE_OVER) {
        if (255 * ba + aa * (255 - ba)) {
            for (i = 0; i < 3; i++) {
                ret.v[i] = (255 * b.v[i] * ba + a.v[i] * aa * (255 - ba)) /
                           (255 * ba + aa * (255 - ba));
            }
        }
        ret.a = ba + aa * (255 - ba) / 255;
    }
    else if (mode == MODE_SUB) {
        ret.a = max(0, aa - ba);
    }
    else if (mode == MODE_MAX) {
        ret.a = max(a.a, b.a);
        ret.rgb = b.rgb;
    } else if (mode == MODE_SUB_CLAMP) {
        ret.a = min(aa, 255 - ba);
        ret.rgb = a.rgb;
    } else if (mode == MODE_MULT_ALPHA) {
        ret.r = ret.r * ba / 255;
        ret.g = ret.g * ba / 255;
        ret.b = ret.b * ba / 255;
        ret.a = ret.a * ba / 255;
    } else {
        assert(false);
    }
    return ret;
}

// XXX: cleanup this function.
void block_op(block_t *block, painter_t *painter, const box_t *box)
{
    int x, y, z;
    mat4_t mat = mat4_identity;
    vec3_t p, size;
    float k, v;
    int mode = painter->mode;
    uvec4b_t c;
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

    mat4_itranslate(&mat, block->pos.x, block->pos.y, block->pos.z);
    mat4_itranslate(&mat, -N / 2 + 0.5, -N / 2 + 0.5, -N / 2 + 0.5);

    if (painter->box) {
        clip = *painter->box;
        min_x = max(min_x, clip.p.x - clip.w.x - block->pos.x + N / 2);
        max_x = min(max_x, clip.p.x + clip.w.x - block->pos.x + N / 2);
        min_y = max(min_y, clip.p.y - clip.h.y - block->pos.y + N / 2);
        max_y = min(max_y, clip.p.y + clip.h.y - block->pos.y + N / 2);
        min_z = max(min_z, clip.p.z - clip.d.z - block->pos.z + N / 2);
        max_z = min(max_z, clip.p.z + clip.d.z - block->pos.z + N / 2);
    }

    for (z = min_z; z < max_z; z++)
    for (y = min_y; y < max_y; y++)
    for (x = min_x; x < max_x; x++) {
        c = painter->color;
        if (can_skip(BLOCK_AT(block, x, y, z), mode, c)) continue;
        p = mat4_mul_vec3(mat, vec3(x, y, z));
        k = shape_func(&p, &size, painter->smoothness);
        k = clamp(k / painter->smoothness, -1.0f, 1.0f);
        v = k / 2.0f + 0.5f;
        if (invert) v = 1.0f - v;
        if (v) {
            block_prepare_write(block);
            c.a *= v;
            BLOCK_AT(block, x, y, z) = combine(
                BLOCK_AT(block, x, y, z), c, mode);
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
        BLOCK_AT(block, x, y, z) = combine(DATA_AT(block->data, x, y, z),
                                           DATA_AT(other->data, x, y, z),
                                           mode);
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
    x = pos[0] - block->pos.x + N / 2;
    y = pos[1] - block->pos.y + N / 2;
    z = pos[2] - block->pos.z + N / 2;
    assert(x >= 0 && x < N);
    assert(y >= 0 && y < N);
    assert(z >= 0 && z < N);
    memcpy(out, BLOCK_AT(block, x, y, z).v, 4);
}

void block_set_at(block_t *block, const int pos[3], const uint8_t v[4])
{
    int x, y, z;
    block_prepare_write(block);
    x = pos[0] - block->pos.x + N / 2;
    y = pos[1] - block->pos.y + N / 2;
    z = pos[2] - block->pos.z + N / 2;
    assert(x >= 0 && x < N);
    assert(y >= 0 && y < N);
    assert(z >= 0 && z < N);
    memcpy(BLOCK_AT(block, x, y, z).v, v, 4);
}

void block_blit(block_t *block, uvec4b_t *data,
                int x, int y, int z, int w, int h, int d)
{
    int bx, by, bz;
    int dx, dy, dz;
    uvec4b_t v;
    BLOCK_ITER(bx, by, bz) {
        dx = bx - x + block->pos.x - N / 2;
        dy = by - y + block->pos.y - N / 2;
        dz = bz - z + block->pos.z - N / 2;
        if (dx < 0 || dx >= w || dy < 0 || dy >= h || dz < 0 || dz >= d)
            continue;
        v = data[dz * w * h + dy * w + dx];
        if (v.a) {
            block_prepare_write(block);
            BLOCK_AT(block, bx, by, bz) = v;
        }
    }
}

void block_shift_alpha(block_t *block, int v)
{
    block_prepare_write(block);
    int i;
    for (i = 0; i < N * N * N; i++) {
        block->data->voxels[i].a = clamp(block->data->voxels[i].a + v, 0, 255);
    }
}

