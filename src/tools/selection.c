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


enum {
    DRAG_RESIZE,
    DRAG_MOVE,
};

static int g_drag_mode = 0;

typedef struct {
    tool_t  tool;

    int     snap_face;
    float   start_pos[3];
    bool    adjust;

    struct {
        gesture3d_t hover;
        gesture3d_t drag;
        gesture3d_t resize;
    } gestures;

} tool_selection_t;

static void get_box(const float p0[3], const float p1[3], const float n[3],
                     float r, const float plane[4][4], float out[4][4])
{
    float rot[4][4], box[4][4];
    float v[3];
    if (p1 == NULL) {
        bbox_from_extents(box, p0, r, r, r);
        box_swap_axis(box, 2, 0, 1, box);
        mat4_copy(box, out);
        return;
    }
    if (r == 0) {
        bbox_from_points(box, p0, p1);
        bbox_grow(box, 0.5, 0.5, 0.5, box);
        // Apply the plane rotation.
        mat4_copy(plane, rot);
        vec4_set(rot[3], 0, 0, 0, 1);
        mat4_imul(box, rot);
        mat4_copy(box, out);
        return;
    }

    // Create a box for a line:
    int i;
    const float AXES[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    mat4_set_identity(box);
    vec3_mix(p0, p1, 0.5, box[3]);
    vec3_sub(p1, box[3], box[2]);
    for (i = 0; i < 3; i++) {
        vec3_cross(box[2], AXES[i], box[0]);
        if (vec3_norm2(box[0]) > 0) break;
    }
    if (i == 3) {
        mat4_copy(box, out);
        return;
    }
    vec3_normalize(box[0], v);
    vec3_mul(v, r, box[0]);
    vec3_cross(box[2], box[0], v);
    vec3_normalize(v, v);
    vec3_mul(v, r, box[1]);
    mat4_copy(box, out);
}


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

static int on_hover(gesture3d_t *gest, void *user)
{
    box_t box;
    cursor_t *curs = gest->cursor;
    uint8_t box_color[4] = {255, 255, 0, 255};

    goxel_set_help_text(goxel, "Click and drag to set selection.");
    get_box(curs->pos, curs->pos, curs->normal, 0, goxel->plane, box.mat);
    render_box(&goxel->rend, box.mat, box_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_resize(gesture3d_t *gest, void *user)
{
    float face_plane[4][4];
    cursor_t *curs = gest->cursor;
    tool_selection_t *tool = user;
    float n[3], pos[3], v[3];

    if (box_is_null(goxel->selection.mat)) return GESTURE_FAILED;

    if (gest->type == GESTURE_HOVER) {
        goxel_set_help_text(goxel, "Drag to move face");
        if (curs->snaped != SNAP_SELECTION_OUT) return GESTURE_FAILED;
        tool->snap_face = get_face(curs->normal);
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        mat4_mul(goxel->selection.mat, FACES_MATS[tool->snap_face],
                 face_plane);
        render_img(&goxel->rend, NULL, face_plane, EFFECT_NO_SHADING);
        if (curs->flags & CURSOR_PRESSED) {
            gest->type = GESTURE_DRAG;
            vec3_normalize(face_plane[0], v);
            plane_from_vectors(goxel->tool_plane, curs->pos, curs->normal, v);
        }
        return 0;
    }
    if (gest->type == GESTURE_DRAG) {
        goxel_set_help_text(goxel, "Drag to move face");
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        mat4_mul(goxel->selection.mat, FACES_MATS[tool->snap_face],
                 face_plane);

        vec3_normalize(face_plane[2], n);
        vec3_sub(curs->pos, goxel->tool_plane[3], v);
        vec3_project(v, n, v);
        vec3_add(goxel->tool_plane[3], v, pos);
        pos[0] = round(pos[0]);
        pos[1] = round(pos[1]);
        pos[2] = round(pos[2]);
        if (g_drag_mode == DRAG_RESIZE) {
            box_move_face(goxel->selection.mat, tool->snap_face, pos,
                          goxel->selection.mat);
        } else {
            float d[3], ofs[3];
            vec3_add(goxel->selection.p, face_plane[2], d);
            vec3_sub(pos, d, d);
            vec3_project(d, n, ofs);
            vec3_iadd(goxel->selection.p, ofs);
        }

        if (gest->state == GESTURE_END) {
            gest->type = GESTURE_HOVER;
            mat4_copy(plane_null, goxel->tool_plane);
        }
        return 0;
    }
    return 0;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_selection_t *tool = user;
    cursor_t *curs = gest->cursor;
    float pos[3], v[3];

    if (!tool->adjust) {
        if (gest->state == GESTURE_BEGIN)
            vec3_copy(curs->pos, tool->start_pos);
        curs->snap_mask &= ~(SNAP_SELECTION_IN | SNAP_SELECTION_OUT);

        goxel_set_help_text(goxel, "Drag.");
        get_box(tool->start_pos, curs->pos, curs->normal,
                0, goxel->plane, goxel->selection.mat);
        if (gest->state == GESTURE_END) tool->adjust = true;
    } else {
        goxel_set_help_text(goxel, "Adjust height.");
        if (gest->state == GESTURE_BEGIN)
            plane_from_normal(goxel->tool_plane, curs->pos, goxel->plane[0]);
        vec3_sub(curs->pos, goxel->tool_plane[3], v);
        vec3_project(v, goxel->plane[2], v);
        vec3_add(goxel->tool_plane[3], v, pos);
        pos[0] = round(pos[0] - 0.5) + 0.5;
        pos[1] = round(pos[1] - 0.5) + 0.5;
        pos[2] = round(pos[2] - 0.5) + 0.5;
        get_box(tool->start_pos, pos, curs->normal, 0,
                goxel->plane, goxel->selection.mat);
        if (gest->state == GESTURE_END) {
            tool->adjust = false;
            mat4_copy(plane_null, goxel->tool_plane);
            return GESTURE_FAILED;
        }
    }
    return 0;
}

// XXX: this is very close to tool_shape_iter.
static int iter(tool_t *tool, const float viewport[4])
{
    tool_selection_t *selection = (tool_selection_t*)tool;
    cursor_t *curs = &goxel->cursor;
    curs->snap_mask |= SNAP_ROUNDED;
    curs->snap_mask &= ~(SNAP_SELECTION_IN | SNAP_SELECTION_OUT);
    curs->snap_offset = 0.5;
    curs->snap_mask |= SNAP_SELECTION_OUT;

    if (!selection->gestures.drag.type) {
        selection->gestures.hover = (gesture3d_t) {
            .type = GESTURE_HOVER,
            .callback = on_hover,
        };
        selection->gestures.drag = (gesture3d_t) {
            .type = GESTURE_DRAG,
            .callback = on_drag,
        };
        selection->gestures.resize = (gesture3d_t) {
            .type = GESTURE_HOVER,
            .callback = on_resize,
        };
    }
    selection->gestures.drag.type = (!selection->adjust) ?
        GESTURE_DRAG : GESTURE_HOVER;

    if (gesture3d(&selection->gestures.resize, curs, selection)) goto end;
    if (gesture3d(&selection->gestures.drag, curs, selection)) goto end;
    if (gesture3d(&selection->gestures.hover, curs, selection)) goto end;

end:
    return tool->state;
}


static int gui(tool_t *tool)
{
    int x, y, z, w, h, d;
    box_t *box = &goxel->selection;
    if (box_is_null(box->mat)) return 0;

    gui_text("Drag mode");
    gui_combo("##drag_mode", &g_drag_mode,
              (const char*[]) {"Resize", "Move"}, 2);

    gui_group_begin(NULL);
    if (gui_action_button("reset_selection", "Reset", 1.0, "")) {
        gui_group_end();
        return 0;
    }
    gui_action_button("fill_selection", "Fill", 1.0, "");
    gui_action_button("layer_clear", "Clear", 1.0, "pp",
                      goxel->image->active_layer, box);
    gui_action_button("cut_as_new_layer", "Cut as new layer", 1.0, "");
    gui_group_end();

    w = round(box->w[0] * 2);
    h = round(box->h[1] * 2);
    d = round(box->d[2] * 2);
    x = round(box->p[0] - box->w[0]);
    y = round(box->p[1] - box->h[1]);
    z = round(box->p[2] - box->d[2]);

    gui_group_begin("Origin");
    gui_input_int("x", &x, 0, 0);
    gui_input_int("y", &y, 0, 0);
    gui_input_int("z", &z, 0, 0);
    gui_group_end();

    gui_group_begin("Size");
    gui_input_int("w", &w, 1, 2048);
    gui_input_int("h", &h, 1, 2048);
    gui_input_int("d", &d, 1, 2048);
    gui_group_end();
    bbox_from_extents(box->mat,
            VEC(x + w / 2., y + h / 2., z + d / 2.),
            w / 2., h / 2., d / 2.);
    return 0;
}

TOOL_REGISTER(TOOL_SELECTION, selection, tool_selection_t,
              .iter_fn = iter,
              .gui_fn = gui,
              .shortcut = "R",
)
