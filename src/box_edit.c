/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

typedef struct data
{
    int snap;
    float box[4][4];
    float transf[4][4];
    int snap_face;
    gesture3d_t gesture;
    int state; // 0 init, 1 first change, 2 following changes.
} data_t;

static data_t g_data = {};

// Get the face index from the normal.
// XXX: used in a few other places!
static int get_face(const float n[3])
{
    int f;
    const int *n2;
    for (f = 0; f < 6; f++) {
        n2 = FACES_NORMALS[f];
        if (vec3_dot(n, VEC(n2[0], n2[1], n2[2])) > 0.5)
            return f;
    }
    return -1;
}

static int on_move(gesture3d_t *gest, void *user)
{
    data_t *data = (void*)user;
    float face_plane[4][4];
    cursor_t *curs = gest->cursor;
    float n[3], pos[3], d[3], ofs[3], v[3];

    if (box_is_null(data->box)) return GESTURE_FAILED;

    if (gest->type == GESTURE_HOVER) {
        goxel_set_help_text("Drag to move face");
        if (curs->snaped != data->snap) return GESTURE_FAILED;
        data->snap_face = get_face(curs->normal);
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        mat4_mul(data->box, FACES_MATS[data->snap_face], face_plane);
        render_img(&goxel.rend, NULL, face_plane, EFFECT_NO_SHADING);
        if (curs->flags & CURSOR_PRESSED) {
            gest->type = GESTURE_DRAG;
            vec3_normalize(face_plane[0], v);
            plane_from_vectors(goxel.tool_plane, curs->pos, curs->normal, v);
            data->state = 0;
        }
        return 0;
    }
    if (gest->type == GESTURE_DRAG) {
        goxel_set_help_text("Drag to move face");

        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        mat4_mul(data->box, FACES_MATS[data->snap_face], face_plane);

        vec3_normalize(face_plane[2], n);
        vec3_sub(curs->pos, goxel.tool_plane[3], v);
        vec3_project(v, n, v);
        vec3_add(goxel.tool_plane[3], v, pos);
        pos[0] = round(pos[0]);
        pos[1] = round(pos[1]);
        pos[2] = round(pos[2]);
        vec3_add(data->box[3], face_plane[2], d);
        vec3_sub(pos, d, ofs);
        vec3_project(ofs, n, ofs);
        vec3_iadd(data->box[3], ofs);

        mat4_set_identity(data->transf);
        mat4_itranslate(data->transf, ofs[0], ofs[1], ofs[2]);

        if (gest->state == GESTURE_END) {
            gest->type = GESTURE_HOVER;
            mat4_copy(plane_null, goxel.tool_plane);
        }
        return 0;
    }
    return 0;
}

int box_edit(int snap, float transf[4][4], bool *first)
{
    cursor_t *curs = &goxel.cursor;
    float box[4][4];

    assert (snap == SNAP_LAYER_OUT); // Selection not implemented yet.
    if (snap == SNAP_LAYER_OUT) {
        curs->snap_mask = SNAP_LAYER_OUT;
        mesh_get_box(goxel.image->active_layer->mesh, true, box);
    }

    if (!g_data.gesture.type) {
        g_data.gesture = (gesture3d_t) {
            .type = GESTURE_HOVER,
            .callback = on_move,
        };
    }
    g_data.snap = snap;
    mat4_copy(box, g_data.box);
    gesture3d(&g_data.gesture, curs, &g_data);
    render_box(&goxel.rend, box, NULL, EFFECT_STRIP | EFFECT_WIREFRAME);
    if (mat4_equal(g_data.box, box)) return 0;
    if (transf) mat4_copy(g_data.transf, transf);
    g_data.state = g_data.state == 0 ? 1 : 2;
    if (first) *first = g_data.state == 1;
    return 1;
}
