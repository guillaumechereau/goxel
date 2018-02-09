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
    float v[3];
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
        mat4_imul(box.mat.v2, rot.v2);
        return box;
    }

    // Create a box for a line:
    int i;
    const vec3_t AXES[] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};

    box.mat = mat4_identity;
    vec3_mix(p0->v, p1->v, 0.5, box.p.v);
    vec3_sub(p1->v, box.p.v, box.d.v);
    for (i = 0; i < 3; i++) {
        vec3_cross(box.d.v, AXES[i].v, box.w.v);
        if (vec3_norm2(box.w.v) > 0) break;
    }
    if (i == 3) return box;
    vec3_normalize(box.w.v, v);
    vec3_mul(v, r, box.w.v);
    vec3_cross(box.d.v, box.w.v, v);
    vec3_normalize(v, v);
    vec3_mul(v, r, box.h.v);
    return box;
}


// Get the face index from the normal.
// XXX: used in a few other places!
static int get_face(vec3_t n)
{
    int f;
    const int *n2;
    for (f = 0; f < 6; f++) {
        n2 = FACES_NORMALS[f];
        if (vec3_dot(n.v, vec3(n2[0], n2[1], n2[2]).v) > 0.5)
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
    box = get_box(&curs->pos, &curs->pos, &curs->normal, 0, &goxel->plane);
    render_box(&goxel->rend, &box, box_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_resize(gesture3d_t *gest, void *user)
{
    plane_t face_plane;
    cursor_t *curs = gest->cursor;
    tool_selection_t *tool = user;
    vec3_t n, pos, v;

    if (box_is_null(goxel->selection)) return GESTURE_FAILED;

    if (gest->type == GESTURE_HOVER) {
        goxel_set_help_text(goxel, "Drag to move face");
        if (curs->snaped != SNAP_SELECTION_OUT) return GESTURE_FAILED;
        tool->snap_face = get_face(curs->normal);
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        mat4_mul(goxel->selection.mat.v2, FACES_MATS[tool->snap_face].v2,
                 face_plane.mat.v2);
        render_img(&goxel->rend, NULL, face_plane.mat.v2, EFFECT_NO_SHADING);
        if (curs->flags & CURSOR_PRESSED) {
            gest->type = GESTURE_DRAG;
            vec3_normalize(face_plane.u.v, v.v);
            goxel->tool_plane = plane(curs->pos, curs->normal, v);
        }
        return 0;
    }
    if (gest->type == GESTURE_DRAG) {
        goxel_set_help_text(goxel, "Drag to move face");
        curs->snap_offset = 0;
        curs->snap_mask &= ~SNAP_ROUNDED;
        mat4_mul(goxel->selection.mat.v2, FACES_MATS[tool->snap_face].v2,
                 face_plane.mat.v2);

        vec3_normalize(face_plane.n.v, n.v);
        vec3_sub(curs->pos.v, goxel->tool_plane.p.v, v.v);
        vec3_project(v.v, n.v, v.v);
        vec3_add(goxel->tool_plane.p.v, v.v, pos.v);
        pos.x = round(pos.x);
        pos.y = round(pos.y);
        pos.z = round(pos.z);
        if (g_drag_mode == DRAG_RESIZE) {
            goxel->selection = box_move_face(goxel->selection,
                                             tool->snap_face, pos);
        } else {
            vec3_t d;
            vec3_add(goxel->selection.p.v, face_plane.n.v, d.v);
            vec3_sub(pos.v, d.v, d.v);
            float ofs[3];
            vec3_project(d.v, n.v, ofs);
            vec3_iadd(goxel->selection.p.v, ofs);
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
    vec3_t pos, v;

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
        vec3_sub(curs->pos.v, goxel->tool_plane.p.v, v.v);
        vec3_project(v.v, goxel->plane.n.v, v.v);
        vec3_add(goxel->tool_plane.p.v, v.v, pos.v);
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
    if (box_is_null(*box)) return 0;

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
              .gui_fn = gui,
              .shortcut = "R",
)
