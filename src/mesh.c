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

bool block_is_empty(const block_t *block, bool fast);
void block_delete(block_t *block);
block_t *block_new(const vec3i_t *pos);
block_t *block_copy(const block_t *other);
box_t block_get_box(const block_t *block, bool exact);
void block_fill(block_t *block,
                uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
                void *user_data);
void block_op(block_t *block, painter_t *painter, const box_t *box);
void block_merge(block_t *block, const block_t *other, int op);
void block_set_at(block_t *block, const vec3i_t *pos, uvec4b_t v);
void block_blit(block_t *block, uvec4b_t *data,
                int x, int y, int z, int w, int h, int d);
void block_shift_alpha(block_t *block, int v);

#define N BLOCK_SIZE

// Keep track of the last operation, so that it is fast to do it again.
typedef struct {
    mesh_t      *origin;
    mesh_t      *result;
    painter_t   painter;
    box_t       box;
} operation_t;

static operation_t g_last_op= {};

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

static void add_blocks(mesh_t *mesh, box_t box);
static void mesh_fill(
        mesh_t *mesh,
        const box_t *box,
        uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
        void *user_data)
{
    box_t bbox = box_get_bbox(*box);
    block_t *block;
    mesh_clear(mesh);
    add_blocks(mesh, bbox);
    MESH_ITER_BLOCKS(mesh, block) {
        block_fill(block, get_color, user_data);
    }
}

box_t mesh_get_box(const mesh_t *mesh, bool exact)
{
    box_t ret;
    block_t *block;
    if (!mesh->blocks) return box_null;
    ret = block_get_box(mesh->blocks, exact);
    MESH_ITER_BLOCKS(mesh, block) {
        ret = bbox_merge(ret, block_get_box(block, exact));
    }
    return ret;
}

static block_t *mesh_get_block_at(const mesh_t *mesh, const vec3i_t *pos)
{
    block_t *block;
    HASH_FIND(hh, mesh->blocks, pos, sizeof(*pos), block);
    return block;
}

static block_t *mesh_add_block(mesh_t *mesh, const vec3i_t *pos)
{
    block_t *block;
    assert(pos->x % (BLOCK_SIZE - 2) == 0);
    assert(pos->y % (BLOCK_SIZE - 2) == 0);
    assert(pos->z % (BLOCK_SIZE - 2) == 0);
    assert(!mesh_get_block_at(mesh, pos));
    mesh_prepare_write(mesh);
    block = block_new(pos);
    block->id = mesh->next_block_id++;
    HASH_ADD(hh, mesh->blocks, pos, sizeof(block->pos), block);
    return block;
}

// Add blocks if needed to fill the box.
static void add_blocks(mesh_t *mesh, box_t box)
{
    vec3_t a, b;
    float x, y, z;
    int i;
    const int s = BLOCK_SIZE - 2;
    vec3i_t p;

    a = vec3(box.p.x - box.w.x, box.p.y - box.h.y, box.p.z - box.d.z);
    b = vec3(box.p.x + box.w.x, box.p.y + box.h.y, box.p.z + box.d.z);
    for (i = 0; i < 3; i++) {
        a.v[i] = round(a.v[i] / s) * s;
        b.v[i] = round(b.v[i] / s) * s;
    }
    for (z = a.z; z <= b.z; z += s)
    for (y = a.y; y <= b.y; y += s)
    for (x = a.x; x <= b.x; x += s)
    {
        p = vec3i(x, y, z);
        assert(p.x % s == 0);
        assert(p.y % s == 0);
        assert(p.z % s == 0);
        if (!mesh_get_block_at(mesh, &p))
            mesh_add_block(mesh, &p);
    }
}

void mesh_op(mesh_t *mesh, painter_t *painter, const box_t *box)
{
    int i;
    painter_t painter2;
    box_t box2;

    if (painter->symmetry) {
        painter2 = *painter;
        for (i = 0; i < 3; i++) {
            if (!(painter->symmetry & (1 << i))) continue;
            painter2.symmetry &= ~(1 << i);
            box2 = *box;
            box2.mat = mat4_identity;
            if (i == 0) mat4_iscale(&box2.mat, -1,  1,  1);
            if (i == 1) mat4_iscale(&box2.mat,  1, -1,  1);
            if (i == 2) mat4_iscale(&box2.mat,  1,  1, -1);
            mat4_imul(&box2.mat, box->mat);
            mesh_op(mesh, &painter2, &box2);
        }
    }

    // In case we are doing the same operation as last time, we can just use
    // the value we buffered.
    if (!g_last_op.origin) g_last_op.origin = mesh_new();
    if (!g_last_op.result) g_last_op.result = mesh_new();
    #define EQUAL(a, b) (memcmp(&(a), &(b), sizeof(a)) == 0)
    if (    mesh->blocks == g_last_op.origin->blocks &&
            EQUAL(*painter, g_last_op.painter) &&
            EQUAL(*box, g_last_op.box)) {
        mesh_set(mesh, g_last_op.result);
        return;
    }
    #undef EQUAL
    mesh_set(g_last_op.origin, mesh);
    g_last_op.painter   = *painter;
    g_last_op.box       = *box;

    block_t *block, *tmp;
    box_t full_box, bbox, block_box;
    bool empty;

    // Grow the box to take the smoothness into account.
    full_box = *box;
    mat4_igrow(&full_box.mat, painter->smoothness,
                              painter->smoothness,
                              painter->smoothness);
    bbox = bbox_grow(box_get_bbox(full_box), 1, 1, 1);

    if (painter->box) {
        bbox = bbox_intersection(bbox, *painter->box);
        if (box_is_null(bbox)) return;
        bbox = bbox_grow(bbox, 1, 1, 1);
    }

    // For constructive modes, we have to add blocks if they are not present.
    mesh_prepare_write(mesh);
    if (IS_IN(painter->mode, MODE_OVER, MODE_MAX)) {
        add_blocks(mesh, bbox);
    }
    HASH_ITER(hh, mesh->blocks, block, tmp) {
        block_box = block_get_box(block, false);
        if (!bbox_intersect(bbox, block_box)) {
            if (painter->mode == MODE_INTERSECT) empty = true;
            else continue;
        }
        empty = false;
        // Optimization for the case when we delete large blocks.
        // XXX: this is too specific.  we need a way to tell if a given
        // shape totally contains a box.
        if (    painter->shape == &shape_cube && painter->mode == MODE_SUB &&
                box_contains(full_box, block_box))
            empty = true;
        if (!empty) {
            block_op(block, painter, box);
            if (block_is_empty(block, true)) empty = true;
        }
        if (empty) {
            HASH_DEL(mesh->blocks, block);
            block_delete(block);
        }
    }

    mesh_set(g_last_op.result, mesh);
}

void mesh_merge(mesh_t *mesh, const mesh_t *other, int mode)
{
    assert(mesh && other);
    block_t *block, *other_block, *tmp;
    mesh_prepare_write(mesh);
    bool is_empty;

    // Add empty blocks if needed.
    if (IS_IN(mode, MODE_OVER, MODE_MAX)) {
        MESH_ITER_BLOCKS(other, block) {
            if (!mesh_get_block_at(mesh, &block->pos)) {
                mesh_add_block(mesh, &block->pos);
            }
        }
    }

    HASH_ITER(hh, mesh->blocks, block, tmp) {
        other_block = mesh_get_block_at(other, &block->pos);

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

// XXX: move this in goxel.h?
static int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}

uvec4b_t mesh_get_at(const mesh_t *mesh, const vec3i_t *pos,
                     mesh_iterator_t *iter)
{
    block_t *block;
    vec3i_t p;
    const int s = BLOCK_SIZE - 2;

    p = vec3i(pos->x + s / 2, pos->y + s / 2, pos->z + s / 2);
    p = vec3i(p.x - mod(p.x, s), p.y - mod(p.y, s), p.z - mod(p.z, s));

    if (iter && memcmp(&iter->pos, &p, sizeof(p)) == 0) {
        return iter->block ? block_get_at(iter->block, pos) : uvec4b_zero;
    }

    HASH_FIND(hh, mesh->blocks, &p, sizeof(p), block);
    if (iter) {
        iter->block = block;
        memcpy(iter->pos, p.v, sizeof(p));
    }
    return block ? block_get_at(block, pos) : uvec4b_zero;
}

void mesh_set_at(mesh_t *mesh, const vec3i_t *pos, uvec4b_t v,
                 mesh_iterator_t *iter)
{
    block_t *block;
    vec3_t p = vec3(pos->x, pos->y, pos->z);
    mesh_prepare_write(mesh);
    add_blocks(mesh, bbox_from_extents(p, 2, 2, 2));
    MESH_ITER_BLOCKS(mesh, block) {
        if (bbox_contains_vec(block_get_box(block, false), p))
            block_set_at(block, pos, v);
    }
}

static uvec4b_t mesh_move_get_color(const vec3_t *pos, void *user)
{
    mesh_t *mesh = USER_GET(user, 0);
    mat4_t *mat = USER_GET(user, 1);
    vec3_t p = mat4_mul_vec3(*mat, *pos);
    vec3i_t pi = vec3i(p.x, p.y, p.z);
    return mesh_get_at(mesh, &pi, NULL);
}

void mesh_move(mesh_t *mesh, const mat4_t *mat)
{
    box_t box;
    mesh_t *src_mesh = mesh_copy(mesh);
    mat4_t imat = mat4_inverted(*mat);
    mesh_prepare_write(mesh);
    box = mesh_get_box(mesh, true);
    if (box_is_null(box)) return;
    box.mat = mat4_mul(*mat, box.mat);
    mesh_fill(mesh, &box, mesh_move_get_color, USER_PASS(src_mesh, &imat));
    mesh_delete(src_mesh);
    mesh_remove_empty_blocks(mesh);
}

void mesh_blit(mesh_t *mesh, uvec4b_t *data,
               int x, int y, int z,
               int w, int h, int d)
{
    box_t box;
    block_t *block;
    box = bbox_from_points(vec3(x, y, z), vec3(x + w, y + h, z + d));
    add_blocks(mesh, box);
    MESH_ITER_BLOCKS(mesh, block) {
        block_blit(block, data, x, y, z, w, h, d);
    }
    mesh_remove_empty_blocks(mesh);
}

void mesh_shift_alpha(mesh_t *mesh, int v)
{
    block_t *block;
    mesh_prepare_write(mesh);
    MESH_ITER_BLOCKS(mesh, block) {
        block_shift_alpha(block, v);
    }
    mesh_remove_empty_blocks(mesh);
}

int mesh_select(const mesh_t *mesh,
                const vec3i_t *start_pos,
                int (*cond)(uvec4b_t value,
                            const uvec4b_t neighboors[6],
                            const uint8_t mask[6],
                            void *user),
                void *user, mesh_t *selection)
{
    int x, y, z, i, j, a;
    uvec4b_t v1, v2;
    vec3i_t pos, p, p2;
    bool keep = true;
    uvec4b_t neighboors[6];
    uint8_t mask[6];
    mesh_iterator_t iter = {0};

    mesh_clear(selection);
    mesh_set_at(selection, start_pos, uvec4b(255, 255, 255, 255), &iter);

    // XXX: Very inefficient algorithm!
    // Iter and test all the neighbors of the selection until there is
    // no more possible changes.
    while (keep) {
        keep = false;
        MESH_ITER_VOXELS(selection, x, y, z, v1) {
            (void)v1;
            pos = vec3i(x, y, z);
            for (i = 0; i < 6; i++) {
                p = vec3i(pos.x + FACES_NORMALS[i].x,
                          pos.y + FACES_NORMALS[i].y,
                          pos.z + FACES_NORMALS[i].z);
                v2 = mesh_get_at(selection, &p, &iter);
                if (v2.a) continue; // Already done.
                v2 = mesh_get_at(mesh, &p, &iter);
                // Compute neighboors and mask.
                for (j = 0; j < 6; j++) {
                    p2 = vec3i(p.x + FACES_NORMALS[j].x,
                               p.y + FACES_NORMALS[j].y,
                               p.z + FACES_NORMALS[j].z);
                    neighboors[j] = mesh_get_at(mesh, &p2, &iter);
                    mask[j] = mesh_get_at(selection, &p2, &iter).a;
                }
                a = cond(v2, neighboors, mask, user);
                if (a) {
                    mesh_set_at(selection, &p, uvec4b(255, 255, 255, a),
                                &iter);
                    keep = true;
                }
            }
        }
    }
    return 0;
}

static uvec4b_t mesh_extrude_callback(const vec3_t *pos, void *user)
{
    mesh_t *mesh = USER_GET(user, 0);
    mat4_t *proj = USER_GET(user, 1);
    box_t *box = USER_GET(user, 2);
    if (!bbox_contains_vec(*box, *pos)) return uvec4b(0, 0, 0, 0);
    vec3_t p = mat4_mul_vec3(*proj, *pos);
    vec3i_t pi = vec3i(p.x, p.y, p.z);
    return mesh_get_at(mesh, &pi, NULL);
}

// XXX: need to redo this function from scratch.  Even the API is a bit
// stupid.
void mesh_extrude(mesh_t *mesh, const plane_t *plane, const box_t *box)
{
    mesh_prepare_write(mesh);
    block_t *block;
    box_t bbox;
    mat4_t proj;
    vec3_t n = plane->n, pos;
    vec3_normalize(&n);
    pos = plane->p;

    // Generate the projection into the plane.
    // XXX: *very* ugly code, fix this!
    proj = mat4_identity;

    if (fabs(plane->n.x) > 0.1) {
        proj.v[0] = 0;
        proj.v[12] = pos.x;
    }
    if (fabs(plane->n.y) > 0.1) {
        proj.v[5] = 0;
        proj.v[13] = pos.y;
    }
    if (fabs(plane->n.z) > 0.1) {
        proj.v[10] = 0;
        proj.v[14] = pos.z;
    }

    bbox = bbox_grow(*box, 1, 1, 1);
    add_blocks(mesh, bbox);
    MESH_ITER_BLOCKS(mesh, block) {
        block_fill(block, mesh_extrude_callback, USER_PASS(mesh, &proj, box));
    }
}

bool mesh_iter_voxels(const mesh_t *mesh, mesh_iterator_t *it,
                      int pos[3], uint8_t value[4])
{
    int x, y, z;
    if (it->finished || !mesh->blocks) return false;
    if (!it->block) {
        it->block = mesh->blocks;
        it->pos[0] = 1;
        it->pos[1] = 1;
        it->pos[2] = 1;
    }
    x = it->pos[0];
    y = it->pos[1];
    z = it->pos[2];
    pos[0] = x + it->block->pos.x - N / 2;
    pos[1] = y + it->block->pos.y - N / 2;
    pos[2] = z + it->block->pos.z - N / 2;
    memcpy(value, it->block->data->voxels[x + y * N + z * N * N].v, 4);

    if (++it->pos[0] >= N - 1) {
        it->pos[0] = 1;
        if (++it->pos[1] >= N - 1) {
            it->pos[1] = 1;
            if (++it->pos[2] >= N -1) {
                it->pos[2] = 1;
                it->block = it->block->hh.next;
                if (!it->block) it->finished = true;
            }
        }
    }
    return true;
}

bool mesh_iter_blocks(const mesh_t *mesh, mesh_iterator_t *it,
                      block_t **block)
{
    if (it->finished || !mesh->blocks) return false;
    if (!it->block) it->block = mesh->blocks;
    *block = it->block;
    it->block = it->block->hh.next;
    if (!it->block) it->finished = true;
    return true;
}
