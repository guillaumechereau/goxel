/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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
#include "xxhash.h"

#include <limits.h>

#define N BLOCK_SIZE

// Used for the cache.
static int mesh_del(void *data_)
{
    mesh_t *mesh = data_;
    mesh_delete(mesh);
    return 0;
}

int mesh_select(const mesh_t *mesh,
                const int start_pos[3],
                int (*cond)(void *user, const mesh_t *mesh,
                            const int base_pos[3],
                            const int new_pos[3],
                            mesh_accessor_t *mesh_accessor),
                void *user, mesh_t *selection)
{
    int i, a;
    int pos[3], p[3];
    bool keep = true;
    mesh_iterator_t iter;
    mesh_accessor_t mesh_accessor, selection_accessor;
    mesh_clear(selection);

    mesh_accessor = mesh_get_accessor(mesh);
    selection_accessor = mesh_get_accessor(selection);

    if (!mesh_get_alpha_at(mesh, &mesh_accessor, start_pos))
        return 0;
    mesh_set_at(selection, &selection_accessor, start_pos,
                (uint8_t[]){255, 255, 255, 255});

    // XXX: Very inefficient algorithm!
    // Iter and test all the neighbors of the selection until there is
    // no more possible changes.
    while (keep) {
        keep = false;
        iter = mesh_get_iterator(selection, MESH_ITER_VOXELS);
        while (mesh_iter(&iter, pos)) {
            // Shouldn't be needed if the iter function did filter the voxels.
            if (!mesh_get_alpha_at(selection, &selection_accessor, pos))
                continue;

            for (i = 0; i < 6; i++) {
                p[0] = pos[0] + FACES_NORMALS[i][0];
                p[1] = pos[1] + FACES_NORMALS[i][1];
                p[2] = pos[2] + FACES_NORMALS[i][2];
                if (mesh_get_alpha_at(selection, &selection_accessor, p))
                    continue; // Already done.
                if (!mesh_get_alpha_at(mesh, &mesh_accessor, p))
                    continue; // No voxel here.
                a = cond(user, mesh, pos, p, &mesh_accessor);
                if (a) {
                    mesh_set_at(selection, &selection_accessor, p,
                                (uint8_t[]){255, 255, 255, a});
                    keep = true;
                }
            }
        }
    }
    return 0;
}


// XXX: need to redo this function from scratch.  Even the API is a bit
// stupid.
void mesh_extrude(mesh_t *mesh,
                  const float plane[4][4],
                  const float box[4][4])
{
    float proj[4][4];
    float n[3], pos[3], p[3];
    mesh_iterator_t iter;
    int vpos[3];
    uint8_t value[4];

    vec3_normalize(plane[2], n);
    vec3_copy(plane[3], pos);

    // Generate the projection into the plane.
    // XXX: *very* ugly code, fix this!
    mat4_set_identity(proj);

    if (fabs(plane[2][0]) > 0.1) {
        proj[0][0] = 0;
        proj[3][0] = pos[0];
    }
    if (fabs(plane[2][1]) > 0.1) {
        proj[1][1] = 0;
        proj[3][1] = pos[1];
    }
    if (fabs(plane[2][2]) > 0.1) {
        proj[2][2] = 0;
        proj[3][2] = pos[2];
    }

    // XXX: use an accessor to speed up access.
    iter = mesh_get_box_iterator(mesh, box, 0);
    while (mesh_iter(&iter, vpos)) {
        vec3_set(p, vpos[0], vpos[1], vpos[2]);
        if (!bbox_contains_vec(box, p)) {
            memset(value, 0, 4);
        } else {
            mat4_mul_vec3(proj, p, p);
            int pi[3] = {floor(p[0]), floor(p[1]), floor(p[2])};
            mesh_get_at(mesh, NULL, pi, value);
        }
        mesh_set_at(mesh, NULL, vpos, value);
    }

}

static void mesh_fill(
        mesh_t *mesh,
        const float box[4][4],
        void (*get_color)(const int pos[3], uint8_t out[4], void *user_data),
        void *user_data)
{
    int pos[3];
    uint8_t color[4];
    mesh_iterator_t iter;
    mesh_accessor_t accessor;

    mesh_clear(mesh);
    accessor = mesh_get_accessor(mesh);
    iter = mesh_get_box_iterator(mesh, box, 0);
    while (mesh_iter(&iter, pos)) {
        get_color(pos, color, user_data);
        mesh_set_at(mesh, &accessor, pos, color);
    }
}

static void mesh_move_get_color(const int pos[3], uint8_t c[4], void *user)
{
    float p[3] = {pos[0], pos[1], pos[2]};
    mesh_t *mesh = USER_GET(user, 0);
    float (*mat)[4][4] = USER_GET(user, 1);
    mat4_mul_vec3(*mat, p, p);
    int pi[3] = {round(p[0]), round(p[1]), round(p[2])};
    mesh_get_at(mesh, NULL, pi, c);
}

void mesh_move(mesh_t *mesh, const float mat[4][4])
{
    float box[4][4];
    mesh_t *src_mesh = mesh_copy(mesh);
    float imat[4][4];

    mat4_invert(mat, imat);
    mesh_get_box(mesh, true, box);
    if (box_is_null(box)) return;
    mat4_mul(mat, box, box);
    mesh_fill(mesh, box, mesh_move_get_color, USER_PASS(src_mesh, &imat));
    mesh_delete(src_mesh);
    mesh_remove_empty_blocks(mesh, false);
}

void mesh_blit(mesh_t *mesh, const uint8_t *data,
               int x, int y, int z, int w, int h, int d,
               mesh_iterator_t *iter)
{
    mesh_iterator_t default_iter = {0};
    int pos[3];
    if (!iter) iter = &default_iter;
    for (pos[2] = z; pos[2] < z + d; pos[2]++)
    for (pos[1] = y; pos[1] < y + h; pos[1]++)
    for (pos[0] = x; pos[0] < x + w; pos[0]++) {
        mesh_set_at(mesh, iter, pos, data);
        data += 4;
    }
    mesh_remove_empty_blocks(mesh, false);
}

void mesh_shift_alpha(mesh_t *mesh, int v)
{
    mesh_iterator_t iter;
    int pos[3];
    uint8_t value[4];

    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, pos)) {
        mesh_get_at(mesh, &iter, pos, value);
        value[3] = clamp(value[3] + v, 0, 255);
        mesh_set_at(mesh, NULL, pos, value);
    }
}

// Multiply two colors together.
static void color_mul(const uint8_t a[4], const uint8_t b[4],
                      uint8_t out[4])
{
    out[0] = (int)a[0] * b[0] / 255;
    out[1] = (int)a[1] * b[1] / 255;
    out[2] = (int)a[2] * b[2] / 255;
    out[3] = (int)a[3] * b[3] / 255;
}

// XXX: cleanup this: in fact we might not need that many modes!
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
    } else if (mode == MODE_INTERSECT) {
        ret[3] = min(aa, ba);
    } else {
        assert(false);
    }
    memcpy(out, ret, 4);
}


void mesh_op(mesh_t *mesh, const painter_t *painter, const float box[4][4])
{
    int i, vp[3];
    uint8_t value[4], new_value[4], c[4];
    mesh_iterator_t iter;
    mesh_accessor_t accessor;
    float size[3], p[3];
    float mat[4][4];
    float (*shape_func)(const float[3], const float[3], float smoothness);
    float k, v;
    int mode = painter->mode;
    bool use_box, skip_src_empty, skip_dst_empty;
    painter_t painter2;
    float box2[4][4];
    int aabb[2][3];
    mesh_t *cached;
    static cache_t *cache = NULL;
    const float *sym_o = painter->symmetry_origin;

    // Check if the operation has been cached.
    if (!cache) cache = cache_create(32);
    struct {
        uint64_t  id;
        float     box[4][4];
        painter_t painter;
    } key;
    memset(&key, 0, sizeof(key));
    key.id = mesh_get_key(mesh);
    mat4_copy(box, key.box);
    key.painter = *painter;
    cached = cache_get(cache, &key, sizeof(key));
    if (cached) {
        mesh_set(mesh, cached);
        return;
    }

    if (painter->symmetry) {
        painter2 = *painter;
        for (i = 0; i < 3; i++) {
            if (!(painter->symmetry & (1 << i))) continue;
            painter2.symmetry &= ~(1 << i);
            mat4_set_identity(box2);
            mat4_itranslate(box2, +sym_o[0], +sym_o[1], +sym_o[2]);
            if (i == 0) mat4_iscale(box2, -1,  1,  1);
            if (i == 1) mat4_iscale(box2,  1, -1,  1);
            if (i == 2) mat4_iscale(box2,  1,  1, -1);
            mat4_itranslate(box2, -sym_o[0], -sym_o[1], -sym_o[2]);
            mat4_imul(box2, box);
            mesh_op(mesh, &painter2, box2);
        }
    }

    shape_func = painter->shape->func;
    box_get_size(box, size);
    mat4_copy(box, mat);
    mat4_iscale(mat, 1 / size[0], 1 / size[1], 1 / size[2]);
    mat4_invert(mat, mat);
    use_box = painter->box && !box_is_null(*painter->box);
    skip_src_empty = mode == MODE_SUB ||
                     mode == MODE_SUB_CLAMP ||
                     mode == MODE_MULT_ALPHA;
    skip_dst_empty = mode == MODE_SUB ||
                     mode == MODE_SUB_CLAMP ||
                     mode == MODE_MULT_ALPHA ||
                     mode == MODE_INTERSECT;

    // for intersection start by deleting all the blocks that are not in
    // the box.
    if (mode == MODE_INTERSECT) {
        iter = mesh_get_iterator(mesh, MESH_ITER_BLOCKS);
        while (mesh_iter(&iter, vp)) {
            mesh_get_block_aabb(vp, aabb);
            if (box_intersect_aabb(box, aabb)) continue;
            mesh_clear_block(mesh, &iter, vp);
        }
    }

    iter = mesh_get_box_iterator(mesh, box,
                                 skip_dst_empty ? MESH_ITER_SKIP_EMPTY : 0);

    // XXX: for the moment we cannot use the same accessor for both
    // setting and getting!  Need to fix that!!
    accessor = mesh_get_accessor(mesh);
    while (mesh_iter(&iter, vp)) {
        vec3_set(p, vp[0] + 0.5, vp[1] + 0.5, vp[2] + 0.5);
        if (use_box && !bbox_contains_vec(*painter->box, p)) continue;
        mat4_mul_vec3(mat, p, p);
        k = shape_func(p, size, painter->smoothness);
        if (painter->smoothness) {
            v = clamp(k / painter->smoothness, -1.0f, 1.0f) / 2.0f + 0.5f;
        } else {
            v = (k >= 0.f) ? 1.f : 0.f;
        }
        if (!v && skip_src_empty) continue;
        memcpy(c, painter->color, 4);
        c[3] *= v;
        if (!c[3] && skip_src_empty) continue;
        mesh_get_at(mesh, &accessor, vp, value);
        if (!value[3] && skip_dst_empty) continue;
        combine(value, c, mode, new_value);
        if (!vec4_equal(value, new_value))
            mesh_set_at(mesh, &accessor, vp, new_value);
    }

    cache_add(cache, &key, sizeof(key), mesh_copy(mesh), 1, mesh_del);
}

// XXX: remove this function!
void mesh_get_box(const mesh_t *mesh, bool exact, float box[4][4])
{
    int bbox[2][3];
    mesh_get_bbox(mesh, bbox, exact);
    bbox_from_aabb(box, bbox);
}

static void block_merge(mesh_t *mesh, const mesh_t *other, const int pos[3],
                        int mode, const uint8_t color[4])
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

    // XXX: cleanup this code!

    if (    (mode == MODE_OVER ||
             mode == MODE_MAX ||
             mode == MODE_SUB ||
             mode == MODE_SUB_CLAMP) && id2 == 0)
    {
        return;
    }

    if ((mode == MODE_OVER || mode == MODE_MAX) && id1 == 0 && !color) {
        mesh_copy_block(other, pos, mesh, pos);
        return;
    }

    if ((mode == MODE_MULT_ALPHA) && id1 == 0) return;
    if ((mode == MODE_MULT_ALPHA) && id2 == 0) {
        // XXX: could just delete the block.
    }

    // Check if the merge op has been cached.
    if (!cache) cache = cache_create(512);
    struct {
        uint64_t id1;
        uint64_t id2;
        int      mode;
        uint8_t  color[4];
    } key = { id1, id2, mode };
    if (color) memcpy(key.color, color, 4);
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
        mesh_get_at(mesh, &a1, p, v1);
        mesh_get_at(other, &a2, p, v2);
        if (color) color_mul(v2, color, v2);
        combine(v1, v2, mode, v1);
        mesh_set_at(block, &a3, (int[]){x, y, z}, v1);
    }
    cache_add(cache, &key, sizeof(key), block, 1, mesh_del);

end:
    mesh_copy_block(block, (int[]){0, 0, 0}, mesh, pos);
    return;
}

void mesh_merge(mesh_t *mesh, const mesh_t *other, int mode,
                const uint8_t color[4])
{
    mesh_t *cached;
    assert(mesh && other);
    static cache_t *cache = NULL;
    mesh_iterator_t iter;
    int bpos[3];
    uint64_t id1, id2;

    // Check if the merge op has been cached.
    if (!cache) cache = cache_create(512);
    id1 = mesh_get_key(mesh);
    id2 = mesh_get_key(other);
    struct {
        uint64_t id1;
        uint64_t id2;
        int      mode;
        uint8_t  color[4];
    } key = { id1, id2, mode };
    if (color) memcpy(key.color, color, 4);
    _Static_assert(sizeof(key) == 24, "");
    cached = cache_get(cache, &key, sizeof(key));
    if (cached) {
        mesh_set(mesh, cached);
        return;
    }

    iter = mesh_get_union_iterator(mesh, other, MESH_ITER_BLOCKS);
    while (mesh_iter(&iter, bpos)) {
        block_merge(mesh, other, bpos, mode, color);
    }

    cache_add(cache, &key, sizeof(key), mesh_copy(mesh), 1, mesh_del);
}

void mesh_crop(mesh_t *mesh, const float box[4][4])
{
    painter_t painter = {
        .mode = MODE_INTERSECT,
        .color = {255, 255, 255, 255},
        .shape = &shape_cube,
    };
    mesh_op(mesh, &painter, box);
}

/* Function: mesh_crc32
 * Compute the crc32 of the mesh data as an array of xyz rgba values.
 *
 * This is only used in the tests, to make sure that we can still open
 * old file formats.
 */
uint32_t mesh_crc32(const mesh_t *mesh)
{
    mesh_iterator_t iter;
    int pos[3];
    uint8_t v[4];
    uint32_t ret = 0;
    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, pos)) {
        mesh_get_at(mesh, &iter, pos, v);
        if (!v[3]) continue;
        ret = XXH32(pos, sizeof(pos), ret);
        ret = XXH32(v, sizeof(v), ret);
    }
    return ret;
}
