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

static const uint8_t FACES_COLOR[6][3] = {
    {0, 255, 0},
    {0, 255, 0},
    {0, 0, 255},
    {0, 0, 255},
    {255, 0, 0},
    {255, 0, 0},
};

typedef struct data
{
    int snap;
    int mode; // 0: move, 1: resize.
    float box[4][4];
    float start_box[4][4];
    float transf[4][4];
    int snap_face;
    int state; // 0: init, 1: snaped, 2: first change, 3: following changes.
} data_t;

static data_t g_data = {};

// Get the face index from the normal.
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

static void get_transf(const float src[4][4], const float dst[4][4],
                       float out[4][4])
{
    mat4_invert(src, out);
    mat4_mul(dst, out, out);
}

static void render_gizmo(const float plane[4][4], int face)
{
    uint8_t color[4] = {0, 0, 0, 16};
    float a[3], b[3], dir[3];

    memcpy(color, FACES_COLOR[face], 3);
    render_rect_fill(&goxel.rend, plane, color);
    vec3_normalize(plane[2], dir);
    vec3_copy(plane[3], a);
    vec3_addk(a, dir, 3, b);
    color[3] = 100;
    render_line(&goxel.rend, a, b, color, EFFECT_ARROW | EFFECT_NO_DEPTH_TEST);
}

static int on_hover(gesture3d_t *gest, void *user)
{
    data_t *data = (void*)user;
    cursor_t *curs = gest->cursor;
    float face_plane[4][4];

    goxel_set_help_text("Drag to move face");
    if (curs->snaped != data->snap) {
        data->state = 0;
        return GESTURE_FAILED;
    }
    data->state = 1; // Snaped.
    data->snap_face = get_face(curs->normal);
    curs->snap_offset = 0;
    curs->snap_mask &= ~SNAP_ROUNDED;
    // Render a white box on the side.
    // XXX: replace that with something better like an arrow.
    mat4_mul(data->box, FACES_MATS[data->snap_face], face_plane);
    mat4_iscale(face_plane, 2, 2, 1);
    mat4_itranslate(face_plane, 0, 0, 0.001);
    render_gizmo(face_plane, data->snap_face);
    return 0;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    data_t *data = (void*)user;
    cursor_t *curs = gest->cursor;
    float face_plane[4][4], v[3], pos[3], n[3], d[3], ofs[3], box[4][4];

    goxel_set_help_text("Drag to move face");

    if (gest->state == GESTURE_BEGIN) {
        if (curs->snaped != data->snap) {
            data->state = 0;
            return GESTURE_FAILED;
        }
        mat4_copy(data->box, data->start_box);
        data->state = 2;
        data->snap_face = get_face(curs->normal);
        mat4_mul(data->box, FACES_MATS[data->snap_face], face_plane);
        vec3_normalize(face_plane[0], v);
        plane_from_vectors(goxel.tool_plane, curs->pos, curs->normal, v);
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        return 0;
    }
    data->state = 3;
    curs->snap_offset = 0;
    curs->snap_mask &= ~SNAP_ROUNDED;

    mat4_mul(data->start_box, FACES_MATS[data->snap_face], face_plane);
    vec3_normalize(face_plane[2], n);
    vec3_sub(curs->pos, goxel.tool_plane[3], v);
    vec3_project(v, n, v);
    vec3_add(goxel.tool_plane[3], v, pos);
    pos[0] = round(pos[0]);
    pos[1] = round(pos[1]);
    pos[2] = round(pos[2]);

    if (data->mode == 1) { // Resize
        box_move_face(data->start_box, data->snap_face, pos, box);
        if (box_get_volume(box) == 0) return 0;
        get_transf(data->box, box, data->transf);
    } else { // Move
        vec3_add(data->box[3], face_plane[2], d);
        vec3_sub(pos, d, ofs);
        vec3_project(ofs, n, ofs);
        mat4_set_identity(data->transf);
        mat4_itranslate(data->transf, ofs[0], ofs[1], ofs[2]);
    }

    if (gest->state == GESTURE_END) {
        mat4_copy(plane_null, goxel.tool_plane);
        data->state = 0;
    }
    return 0;
}

static void normalize_box(const float box[4][4], float out[4][4])
{
    mat4_copy(box, out);
    float vertices[8][3];
    box_get_vertices(box, vertices);
    bbox_from_npoints(out, 8, vertices);
}

int box_edit(int snap, int mode, float transf[4][4], bool *first)
{
    cursor_t *curs = &goxel.cursor;
    float box[4][4] = {};
    int ret;

    if (snap == SNAP_LAYER_OUT) {
        curs->snap_mask = SNAP_LAYER_OUT;
        volume_get_box(goxel.image->active_layer->volume, true, box);
        // Fix problem with shape layer box.
        if (goxel.image->active_layer->shape) {
            normalize_box(goxel.image->active_layer->mat, box);
        }
    }
    if (snap == SNAP_SELECTION_OUT) {
        curs->snap_mask |= SNAP_SELECTION_OUT;
        mat4_copy(goxel.selection, box);
    }
    if (box_is_null(box)) return 0;

    g_data.snap = snap;
    g_data.mode = mode;
    mat4_copy(box, g_data.box);
    mat4_set_identity(g_data.transf);

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_HOVER,
        .callback = on_hover,
    }, curs, &g_data);
    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_DRAG,
        .callback = on_drag,
    }, curs, &g_data);

    ret = g_data.state;

    render_box(&goxel.rend, box, NULL, EFFECT_STRIP | EFFECT_WIREFRAME);
    if (transf) mat4_copy(g_data.transf, transf);
    if (first) *first = g_data.state == 2;
    if (g_data.state == 2) g_data.state = 3;
    return ret;
}
