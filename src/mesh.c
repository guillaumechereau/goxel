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
    mesh->id = goxel()->next_uid++;
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

mesh_t *mesh_new(void)
{
    mesh_t *mesh;
    mesh = calloc(1, sizeof(*mesh));
    mesh->next_block_id = 1;
    mesh->ref = calloc(1, sizeof(*mesh->ref));
    mesh->id = goxel()->next_uid++;
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

void mesh_fill(mesh_t *mesh,
               uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
               void *user_data)
{
    mesh_prepare_write(mesh);
    block_t *block;
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
    // LOG_D("w:%f h:%f d:%f", ret.w.x, ret.h.y, ret.d.z);
    return ret;
}

static block_t *mesh_get_block_at(const mesh_t *mesh, const vec3_t *pos)
{
    block_t *block;
    vec3_t p = vec3((int)pos->x, (int)pos->y, (int)pos->z);
    HASH_FIND(hh, mesh->blocks, &p, sizeof(p), block);
    return block;
}

// Add blocks if needed to fill the box.
static void add_blocks(mesh_t *mesh, box_t box)
{
    vec3_t a, b;
    float x, y, z;
    int i;
    const int s = BLOCK_SIZE - 2;
    vec3_t p;

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
        p = vec3(x, y, z);
        if (!mesh_get_block_at(mesh, &p))
            mesh_add_block(mesh, NULL, &p);
    }
}

void mesh_op(mesh_t *mesh, painter_t *painter, const box_t *box)
{
    PROFILED

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

    // For constructive modes, we have to add blocks if they are not present.
    mesh_prepare_write(mesh);
    if (IS_IN(painter->mode, MODE_ADD, MODE_MAX)) {
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

    // Add empty blocks if needed.
    if (IS_IN(mode, MODE_ADD, MODE_MAX)) {
        MESH_ITER_BLOCKS(other, block) {
            if (!mesh_get_block_at(mesh, &block->pos)) {
                mesh_add_block(mesh, NULL, &block->pos);
            }
        }
    }

    HASH_ITER(hh, mesh->blocks, block, tmp) {
        other_block = mesh_get_block_at(other, &block->pos);
        if (    block_is_empty(block, true) &&
                block_is_empty(other_block, true)) {
            HASH_DEL(mesh->blocks, block);
            block_delete(block);
            continue;
        }
        block_merge(block, other_block, mode);
    }
}

block_t *mesh_add_block(mesh_t *mesh, block_data_t *data, const vec3_t *pos)
{
    block_t *block;
    vec3_t p = vec3((int)pos->x, (int)pos->y, (int)pos->z);
    assert(!mesh_get_block_at(mesh, &p));
    mesh_prepare_write(mesh);
    block = block_new(&p, data);
    block->id = mesh->next_block_id++;
    HASH_ADD(hh, mesh->blocks, pos, sizeof(block->pos), block);
    return block;
}

uvec4b_t mesh_get_at(const mesh_t *mesh, const vec3_t *pos)
{
    block_t *block;
    MESH_ITER_BLOCKS(mesh, block) {
        if (bbox_contains_vec(block_get_box(block, false), *pos))
            return block_get_at(block, pos);
    }
    return uvec4b(0, 0, 0, 0);
}

void mesh_set_at(mesh_t *mesh, const vec3_t *pos, uvec4b_t v)
{
    block_t *block;
    mesh_prepare_write(mesh);
    add_blocks(mesh, bbox_from_extents(*pos, 1, 1, 1));
    MESH_ITER_BLOCKS(mesh, block) {
        if (bbox_contains_vec(block_get_box(block, false), *pos))
            block_set_at(block, pos, v);
    }
}

typedef struct
{
    mesh_t *mesh;
    mat4_t mat;
} mesh_move_data_t;

static uvec4b_t mesh_move_get_color(const vec3_t *pos, void *user_data)
{
    mesh_move_data_t *data = user_data;
    vec3_t p = mat4_mul_vec3(data->mat, *pos);
    return mesh_get_at(data->mesh, &p);
}

void mesh_move(mesh_t *mesh, const mat4_t *mat)
{
    box_t box;
    mesh_move_data_t data = {mesh_copy(mesh), mat4_inverted(*mat)};
    mesh_prepare_write(mesh);
    box = mesh_get_box(mesh, true);
    if (box_is_null(box)) return;
    box.mat = mat4_mul(*mat, box.mat);
    box = box_get_bbox(box);
    add_blocks(mesh, box);
    mesh_fill(mesh, mesh_move_get_color, &data);
    mesh_delete(data.mesh);
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
