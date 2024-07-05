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

enum {
    FLAG_SNAP_FACE      = 1 << 0,
    FLAG_SNAP_GIZMO     = 1 << 1,
};

typedef struct data
{
    int mode; // 0: move, 1: resize.
    float box[4][4];
    float start_box[4][4];
    float transf[4][4];
    int snap_face;
    int flags;
    int gesture_state;
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

static void render_gizmo(const float pose[4][4], int face, float alpha)
{
    uint8_t color[4] = {0, 0, 0, 255 * alpha};
    float a[3], b[3], cone_mat[4][4];

    memcpy(color, FACES_COLOR[face], 3);
    vec3_copy(pose[3], a);
    vec3_add(a, pose[2], b);
    render_line(&goxel.rend, a, b, color, EFFECT_NO_DEPTH_TEST);
    mat4_translate(pose, 0, 0, 1, cone_mat);
    mat4_iscale(cone_mat, 0.15, 0.15, 0.3);
    mat4_itranslate(cone_mat, 0, 0, -1);
    render_cone(&goxel.rend, cone_mat, color, EFFECT_NO_DEPTH_TEST);
}

static void highlight_face(const float box[4][4], int face)
{
    float plane[4][4];
    uint8_t color[4] = {0, 0, 0, 16};

    mat4_mul(box, FACES_MATS[face], plane);
    mat4_iscale(plane, 2, 2, 1);
    mat4_itranslate(plane, 0, 0, 0.001);
    memcpy(color, FACES_COLOR[face], 3);
    render_rect_fill(&goxel.rend, plane, color);
}

static int on_hover(gesture3d_t *gest)
{
    data_t *data = gest->user;;

    if (g_data.flags & FLAG_SNAP_GIZMO) return -1;
    goxel_set_help_text("Drag to move face");
    data->flags |= FLAG_SNAP_FACE;
    data->snap_face = get_face(gest->normal);
    highlight_face(data->box, data->snap_face);
    return 0;
}

static int on_drag(gesture3d_t *gest)
{
    data_t *data = gest->user;
    float face_plane[4][4], v[3], pos[3], n[3], d[3], ofs[3], box[4][4];

    goxel_set_help_text("Drag to move face");
    data->gesture_state = gest->state;

    if (gest->state == GESTURE3D_STATE_BEGIN) {
        mat4_copy(data->box, data->start_box);
        data->snap_face = get_face(gest->normal);
        mat4_mul(data->box, FACES_MATS[data->snap_face], face_plane);
        vec3_normalize(face_plane[0], v);
        gest->snap_mask = SNAP_SHAPE_PLANE;
        plane_from_vectors(gest->snap_shape, gest->pos, gest->normal, v);
        return 0;
    }

    mat4_mul(data->start_box, FACES_MATS[data->snap_face], face_plane);
    vec3_normalize(face_plane[2], n);
    vec3_sub(gest->pos, gest->snap_shape[3], v);
    vec3_project(v, n, v);
    vec3_add(gest->snap_shape[3], v, pos);
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
    return 0;
}

static int on_gizmo_hover(gesture3d_t *gest)
{
    goxel_set_help_text("Drag to move");
    g_data.snap_face = gest->user_key;
    g_data.flags |= FLAG_SNAP_GIZMO;
    return 0;
}

static int on_gizmo_drag(gesture3d_t *gest)
{
    float v[3];
    int face = gest->user_key;
    float box[4][4];

    goxel_set_help_text("Drag to move");
    g_data.gesture_state = gest->state;

    if (gest->state == GESTURE3D_STATE_BEGIN) {
        mat4_copy(g_data.box, g_data.start_box);
        g_data.snap_face = face;
        gest->snap_mask = SNAP_SHAPE_LINE;
        return 0;
    }

    vec3_sub(gest->pos, gest->start_pos, v);
    v[0] = round(v[0]);
    v[1] = round(v[1]);
    v[2] = round(v[2]);
    mat4_copy(g_data.start_box, box);
    vec3_add(box[3], v, box[3]);
    get_transf(g_data.box, box, g_data.transf);

    return 0;
}

static float compute_gizmo_scale(const float pos[3])
{
    const float base_scale = 40;
    float p[4] = {pos[0], pos[1], pos[2], 1};
    float viewproj[4][4];
    const camera_t *cam = goxel.image->active_camera;
    float vx[3], vy[3];
    float len_px, len_sc, pixsize;

    mat4_mul(cam->proj_mat, cam->view_mat, viewproj);

    // Compute pixel size of the projection.
    mat4_mul_dir3(cam->proj_mat, VEC(1, 0, 0), vx);
    mat4_mul_dir3(cam->proj_mat, VEC(0, 1, 0), vy);
    len_px = 2.0f / sqrtf(fmin(vec3_norm2(vx), vec3_norm2(vy)));
    len_sc = fmax(goxel.gui.viewport[2], goxel.gui.viewport[3]);
    pixsize = len_px / len_sc;

    // Multiply by the W factor.
    mat4_mul_vec4(viewproj, p, p);
    return base_scale * p[3] * pixsize;
}

static void gizmo(const float box[4][4], int face)
{
    float shape[4][4];
    float alpha = 0.15;
    float scale;

    // Compute the gizmo arrow shape.
    mat4_mul(box, FACES_MATS[face], shape);
    vec3_normalize(shape[0], shape[0]);
    vec3_normalize(shape[1], shape[1]);
    vec3_normalize(shape[2], shape[2]);
    mat4_iscale(shape, 0.5, 0.5, 1.0);

    scale = compute_gizmo_scale(shape[3]);
    mat4_iscale(shape, scale, scale, scale);

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_HOVER,
        .snap_mask = SNAP_SHAPE_SEGMENT,
        .snap_shape = MAT4_COPY(shape),
        .callback = on_gizmo_hover,
        .user_key = face,
    });

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_DRAG,
        .snap_mask = SNAP_SHAPE_SEGMENT,
        .snap_shape = MAT4_COPY(shape),
        .callback = on_gizmo_drag,
        .user_key = face,
        .name = "Gizmo Drag",
    });

    if ((g_data.flags & FLAG_SNAP_GIZMO) && g_data.snap_face == face)
        alpha = 1.0;
    render_gizmo(shape, face, alpha);
}

int box_edit(const float box[4][4], int mode, float transf[4][4])
{
    int i;

    if (box_is_null(box)) return 0;
    g_data.mode = mode;
    mat4_copy(box, g_data.box);
    mat4_set_identity(g_data.transf);
    g_data.flags = 0;
    g_data.gesture_state = 0;

    for (i = 0; i < 6; i++) {
        gizmo(box, i);
    }

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_HOVER,
        .snap_mask = SNAP_SHAPE_BOX,
        .snap_shape = MAT4_COPY(box),
        .callback = on_hover,
        .user = &g_data,
    });
    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_DRAG,
        .snap_mask = SNAP_SHAPE_BOX,
        .snap_shape = MAT4_COPY(box),
        .callback = on_drag,
        .user = &g_data,
    });


    render_box(&goxel.rend, box, NULL, EFFECT_STRIP | EFFECT_WIREFRAME);
    if (transf) mat4_copy(g_data.transf, transf);

    // Avoid sending an update event if we didn't actually move.
    if (    (g_data.gesture_state == GESTURE3D_STATE_UPDATE) &&
            mat4_is_identity(g_data.transf)) {
        return 0;
    }

    return g_data.gesture_state;
}

bool box_edit_is_active(void)
{
    return g_data.gesture_state ||
           (g_data.flags & (FLAG_SNAP_GIZMO | FLAG_SNAP_FACE));
}
