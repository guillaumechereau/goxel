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
#include <limits.h>

#define N BLOCK_SIZE

int mesh_select(const mesh_t *mesh,
                const int start_pos[3],
                int (*cond)(const uint8_t value[4],
                            const uint8_t neighboors[6][4],
                            const uint8_t mask[6],
                            void *user),
                void *user, mesh_t *selection)
{
    int i, j, a;
    uint8_t v2[4];
    int pos[3], p[3], p2[3];
    bool keep = true;
    uint8_t neighboors[6][4];
    uint8_t mask[6];
    mesh_iterator_t iter;
    mesh_accessor_t mesh_accessor, selection_accessor;
    mesh_clear(selection);

    mesh_accessor = mesh_get_accessor(mesh);
    selection_accessor = mesh_get_accessor(selection);

    mesh_set_at(selection, start_pos, (uint8_t[]){255, 255, 255, 255},
                &selection_accessor);

    // XXX: Very inefficient algorithm!
    // Iter and test all the neighbors of the selection until there is
    // no more possible changes.
    while (keep) {
        keep = false;
        iter = mesh_get_iterator(selection);
        while (mesh_iter(&iter, pos)) {
            for (i = 0; i < 6; i++) {
                p[0] = pos[0] + FACES_NORMALS[i][0];
                p[1] = pos[1] + FACES_NORMALS[i][1];
                p[2] = pos[2] + FACES_NORMALS[i][2];
                mesh_get_at(selection, p, &selection_accessor, v2);
                if (v2[3]) continue; // Already done.
                mesh_get_at(mesh, p, &mesh_accessor, v2);
                // Compute neighboors and mask.
                for (j = 0; j < 6; j++) {
                    p2[0] = p[0] + FACES_NORMALS[j][0];
                    p2[1] = p[1] + FACES_NORMALS[j][1];
                    p2[2] = p[2] + FACES_NORMALS[j][2];
                    mesh_get_at(mesh, p2, &mesh_accessor, neighboors[j]);
                    mask[j] = mesh_get_alpha_at(selection, p2,
                                                &selection_accessor);
                }
                a = cond(v2, neighboors, mask, user);
                if (a) {
                    mesh_set_at(selection, p, (uint8_t[]){255, 255, 255, a},
                                &selection_accessor);
                    keep = true;
                }
            }
        }
    }
    return 0;
}


// XXX: need to redo this function from scratch.  Even the API is a bit
// stupid.
void mesh_extrude(mesh_t *mesh, const plane_t *plane, const box_t *box)
{
    mat4_t proj;
    vec3_t n = plane->n, pos;
    mesh_iterator_t iter;
    int vpos[3];
    uint8_t value[4];

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

    // XXX: use an accessor to speed up access.
    iter = mesh_get_box_iterator(mesh, *box);
    while (mesh_iter(&iter, vpos)) {
        vec3_t p = vec3(vpos[0], vpos[1], vpos[2]);
        if (!bbox_contains_vec(*box, p)) {
            memset(value, 0, 4);
        } else {
            p = mat4_mul_vec3(proj, p);
            int pi[3] = {floor(p.x), floor(p.y), floor(p.z)};
            mesh_get_at(mesh, pi, NULL, value);
        }
        mesh_set_at(mesh, vpos, value, NULL);
    }

}

static void mesh_fill(
        mesh_t *mesh,
        const box_t *box,
        void (*get_color)(const int pos[3], uint8_t out[4], void *user_data),
        void *user_data)
{
    int pos[3];
    uint8_t color[4];
    mesh_iterator_t iter;
    mesh_accessor_t accessor;

    mesh_clear(mesh);
    accessor = mesh_get_accessor(mesh);
    iter = mesh_get_box_iterator(mesh, *box);
    while (mesh_iter(&iter, pos)) {
        get_color(pos, color, user_data);
        mesh_set_at(mesh, pos, color, &accessor);
    }
}

static void mesh_move_get_color(const int pos[3], uint8_t c[4], void *user)
{
    vec3_t p = vec3(pos[0], pos[1], pos[2]);
    mesh_t *mesh = USER_GET(user, 0);
    mat4_t *mat = USER_GET(user, 1);
    p = mat4_mul_vec3(*mat, p);
    int pi[3] = {round(p.x), round(p.y), round(p.z)};
    mesh_get_at(mesh, pi, NULL, c);
}

void mesh_move(mesh_t *mesh, const mat4_t *mat)
{
    box_t box;
    mesh_t *src_mesh = mesh_copy(mesh);
    mat4_t imat = mat4_inverted(*mat);
    box = mesh_get_box(mesh, true);
    if (box_is_null(box)) return;
    box.mat = mat4_mul(*mat, box.mat);
    mesh_fill(mesh, &box, mesh_move_get_color, USER_PASS(src_mesh, &imat));
    mesh_delete(src_mesh);
    mesh_remove_empty_blocks(mesh);
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
        mesh_set_at(mesh, pos, data, iter);
        data += 4;
    }
    mesh_remove_empty_blocks(mesh);
}

void mesh_shift_alpha(mesh_t *mesh, int v)
{
    mesh_iterator_t iter;
    int pos[3];
    uint8_t value[4];

    iter = mesh_get_iterator(mesh);
    while (mesh_iter(&iter, pos)) {
        mesh_get_at(mesh, pos, &iter, value);
        value[3] = clamp(value[3] + v, 0, 255);
        mesh_set_at(mesh, pos, value, NULL);
    }
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
    } else {
        assert(false);
    }
    memcpy(out, ret, 4);
}


void mesh_op(mesh_t *mesh, painter_t *painter, const box_t *box)
{
    int vp[3];
    uint8_t value[4];
    mesh_iterator_t iter;
    mesh_accessor_t accessor;
    vec3_t size, p;
    mat4_t mat = mat4_identity;
    float (*shape_func)(const vec3_t*, const vec3_t*, float smoothness);
    float k, v;
    int mode = painter->mode;
    bool invert = false;

    // XXX: don't do that anymore.
    if (mode == MODE_INTERSECT) {
        mode = MODE_SUB;
        invert = true;
    }

    shape_func = painter->shape->func;
    size = box_get_size(*box);
    mat4_imul(&mat, box->mat);
    mat4_iscale(&mat, 1 / size.x, 1 / size.y, 1 / size.z);
    mat4_invert(&mat);

    // XXX: for the moment we cannot use the same accessor for both
    // setting and getting!  Need to fix that!!
    accessor = mesh_get_accessor(mesh);
    iter = mesh_get_box_iterator(mesh, *box);
    while (mesh_iter(&iter, vp)) {
        p = mat4_mul_vec3(mat, vec3(vp[0], vp[1], vp[2]));
        k = shape_func(&p, &size, painter->smoothness);
        k = clamp(k / painter->smoothness, -1.0f, 1.0f);
        v = k / 2.0f + 0.5f;
        if (invert) v = 1.0f - v;
        if (v) {
            mesh_get_at(mesh, vp, &accessor, value);
            combine(value, painter->color, mode, value);
            mesh_set_at(mesh, vp, value, &accessor);
        }
    }
}

box_t mesh_get_box(const mesh_t *mesh, bool exact)
{
    box_t ret = box_null, box;
    mesh_iterator_t iter;
    vec3_t pos;
    int vpos[3];
    uint8_t value[4];
    int xmin = INT_MAX, xmax = INT_MIN;
    int ymin = INT_MAX, ymax = INT_MIN;
    int zmin = INT_MAX, zmax = INT_MIN;

    if (!exact) {
        iter = mesh_get_blocks_iterator(mesh);
        while (mesh_iter(&iter, vpos)) {
            pos = vec3(vpos[0] + N / 2, vpos[1] + N / 2, vpos[2] + N / 2);
            box = bbox_from_extents(pos, N / 2, N / 2, N / 2);
            ret = bbox_merge(ret, box);
        }
    } else {
        iter = mesh_get_iterator(mesh);
        while (mesh_iter(&iter, vpos)) {
            mesh_get_at(mesh, vpos, &iter, value);
            if (!value[3]) continue;
            xmin = min(xmin, vpos[0]);
            ymin = min(ymin, vpos[1]);
            zmin = min(zmin, vpos[2]);
            xmax = max(xmax, vpos[0]);
            ymax = max(ymax, vpos[1]);
            zmax = max(zmax, vpos[2]);
        }
        if (xmin > xmax) return box_null;
        ret = bbox_from_points(vec3(xmin, ymin, zmin),
                               vec3(xmax + 1, ymax + 1, zmax + 1));
    }
    return ret;
}

