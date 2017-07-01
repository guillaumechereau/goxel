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
    STATE_IDLE      = 0,
    STATE_CANCEL    = 1,
    STATE_END       = 2,

    STATE_SNAPED,
    STATE_PAINT,
    STATE_PAINT2,
    STATE_WAIT_UP,
    STATE_WAIT_KEY_UP,
    STATE_SNAPED_FACE,
    STATE_MOVE_FACE,

    STATE_ENTER     = 0x0100,
};

enum {
    DRAG_RESIZE,
    DRAG_MOVE,
};

static int g_drag_mode = 0;

typedef struct {
    tool_t  tool;

    int     snap_face;
    vec3_t  start_pos;
    bool    adjust;

    struct {
        gesture3d_t hover;
        gesture3d_t drag;
        gesture3d_t resize;
    } gestures;

} tool_selection_t;

static box_t get_box(const vec3_t *p0, const vec3_t *p1, const vec3_t *n,
                     float r, const plane_t *plane)
{
    mat4_t rot;
    box_t box;
    if (p1 == NULL) {
        box = bbox_from_extents(*p0, r, r, r);
        box = box_swap_axis(box, 2, 0, 1);
        return box;
    }
    if (r == 0) {
        box = bbox_grow(bbox_from_points(*p0, *p1), 0.5, 0.5, 0.5);
        // Apply the plane rotation.
        rot = plane->mat;
        rot.vecs[3] = vec4(0, 0, 0, 1);
        mat4_imul(&box.mat, rot);
        return box;
    }

    // Create a box for a line:
    int i;
    const vec3_t AXES[] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};

    box.mat = mat4_identity;
    box.p = vec3_mix(*p0, *p1, 0.5);
    box.d = vec3_sub(*p1, box.p);
    for (i = 0; i < 3; i++) {
        box.w = vec3_cross(box.d, AXES[i]);
        if (vec3_norm2(box.w) > 0) break;
    }
    if (i == 3) return box;
    box.w = vec3_mul(vec3_normalized(box.w), r);
    box.h = vec3_mul(vec3_normalized(vec3_cross(box.d, box.w)), r);
    return box;
}


// Get the face index from the normal.
static int get_face(vec3_t n)
{
    int f;
    for (f = 0; f < 6; f++) {
        if (vec3_dot(n, vec3(VEC3_SPLIT(FACES_NORMALS[f]))) > 0.5)
            return f;
    }
    return -1;
}

static int on_hover(gesture3d_t *gest, void *user)
{
    box_t box;
    cursor_t *curs = gest->cursor;
    uvec4b_t box_color = HEXCOLOR(0xffff00ff);

    goxel_set_help_text(goxel, "Click and drag to set selection.");
    box = get_box(&curs->pos, &curs->pos, &curs->normal, 0, &goxel->plane);
    render_box(&goxel->rend, &box, &box_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_resize(gesture3d_t *gest, void *user)
{
    plane_t face_plane;
    cursor_t *curs = gest->cursor;
    tool_selection_t *tool = user;
    vec3_t n, pos;

    if (box_is_null(goxel->selection)) return GESTURE_FAILED;

    if (gest->type == GESTURE_HOVER) {
        goxel_set_help_text(goxel, "Drag to move face");
        if (curs->snaped != SNAP_SELECTION_OUT) return GESTURE_FAILED;
        tool->snap_face = get_face(curs->normal);
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        face_plane.mat = mat4_mul(goxel->selection.mat,
                                  FACES_MATS[tool->snap_face]);
        render_img(&goxel->rend, NULL, &face_plane.mat, EFFECT_NO_SHADING);
        if (curs->flags & CURSOR_PRESSED) {
            gest->type = GESTURE_DRAG;
            goxel->tool_plane = plane(curs->pos, curs->normal,
                                      vec3_normalized(face_plane.u));
        }
        return 0;
    }
    if (gest->type == GESTURE_DRAG) {
        goxel_set_help_text(goxel, "Drag to move face");
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        face_plane.mat = mat4_mul(goxel->selection.mat,
                                  FACES_MATS[tool->snap_face]);

        n = vec3_normalized(face_plane.n);
        pos = vec3_add(goxel->tool_plane.p,
                vec3_project(vec3_sub(curs->pos, goxel->tool_plane.p), n));
        pos.x = round(pos.x);
        pos.y = round(pos.y);
        pos.z = round(pos.z);
        if (g_drag_mode == DRAG_RESIZE) {
            goxel->selection = box_move_face(goxel->selection,
                                             tool->snap_face, pos);
        } else {
            vec3_t d = vec3_add(goxel->selection.p, face_plane.n);
            vec3_t ofs = vec3_project(vec3_sub(pos, d), n);
            vec3_iadd(&goxel->selection.p, ofs);
        }

        if (gest->state == GESTURE_END) {
            gest->type = GESTURE_HOVER;
            goxel->tool_plane = plane_null;
        }
        return 0;
    }
    return 0;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_selection_t *tool = user;
    cursor_t *curs = gest->cursor;
    vec3_t pos;

    if (!tool->adjust) {
        if (gest->state == GESTURE_BEGIN)
            tool->start_pos = curs->pos;
        curs->snap_mask &= ~(SNAP_SELECTION_IN | SNAP_SELECTION_OUT);

        goxel_set_help_text(goxel, "Drag.");
        goxel->selection = get_box(&tool->start_pos,
                                   &curs->pos, &curs->normal,
                                   0, &goxel->plane);
        if (gest->state == GESTURE_END) tool->adjust = true;
    } else {
        goxel_set_help_text(goxel, "Adjust height.");
        if (gest->state == GESTURE_BEGIN)
            goxel->tool_plane = plane_from_normal(curs->pos, goxel->plane.u);
        pos = vec3_add(goxel->tool_plane.p,
                       vec3_project(vec3_sub(curs->pos, goxel->tool_plane.p),
                           goxel->plane.n));
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
        goxel->selection = get_box(&tool->start_pos, &pos, &curs->normal, 0,
                                   &goxel->plane);
        if (gest->state == GESTURE_END) {
            tool->adjust = false;
            goxel->tool_plane = plane_null;
            return GESTURE_FAILED;
        }
    }
    return 0;
}

// XXX: this is very close to tool_shape_iter.
static int iter(tool_t *tool, const vec4_t *view)
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

static int cancel(tool_t *tool)
{
    if (!tool) return 0;
    goxel->tool_plane = plane_null;
    return 0;
}

static int gui(tool_t *tool)
{
    int x, y, z, w, h, d;
    box_t *box = &goxel->selection;
    if (box_is_null(*box)) return 0;

    gui_text("Drag mode");
    gui_combo("##drag_mode", &g_drag_mode,
              (const char*[]) {"Resize", "Move"}, 2);
    gui_action_button("clear_selection", "Clear selection", 1.0, "");
    gui_action_button("cut_as_new_layer", "Cut as new layer", 1.0, "");
    w = round(box->w.x * 2);
    h = round(box->h.y * 2);
    d = round(box->d.z * 2);
    x = round(box->p.x - box->w.x);
    y = round(box->p.y - box->h.y);
    z = round(box->p.z - box->d.z);

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
    *box = bbox_from_extents(
            vec3(x + w / 2., y + h / 2., z + d / 2.),
            w / 2., h / 2., d / 2.);
    return 0;
}

TOOL_REGISTER(TOOL_SELECTION, selection, tool_selection_t,
              .iter_fn = iter,
              .cancel_fn = cancel,
              .gui_fn = gui,
              .shortcut = "R",
)
