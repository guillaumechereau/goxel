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
    int     snap_face;
    vec3_t  start_pos;
} data_t;

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

static data_t *get_data(void **data_)
{
    data_t **data = (data_t**)data_;
    if (!*data) {
        *data = calloc(1, sizeof(**data));
    }
    return *data;
}

// XXX: this is very close to tool_shape_iter.
static int iter(const inputs_t *inputs, int state, void **data_,
                const vec4_t *view, bool inside)
{
    data_t *data = get_data(data_);
    const bool down = inputs->mouse_down[0];
    const bool up = !down;
    int snaped = 0;
    int face = -1;
    vec3_t pos = vec3_zero, normal = vec3_zero, n;
    plane_t face_plane;
    box_t box;
    uvec4b_t box_color = HEXCOLOR(0xffff00ff);

    // See if we can snap on a selection face.
    if (inside && !box_is_null(goxel->selection) &&
            IS_IN(state, STATE_IDLE, STATE_SNAPED, STATE_SNAPED_FACE)) {
        data->snap_face = -1;
        if (goxel_unproject_on_box(goxel, view, &inputs->mouse_pos,
                               &goxel->selection, false,
                               &pos, &normal, &face)) {
            data->snap_face = face;
            state = STATE_SNAPED_FACE;
        }
    }
    if (!box_is_null(goxel->selection) && data->snap_face != -1)
        face_plane.mat = mat4_mul(goxel->selection.mat,
                                  FACES_MATS[data->snap_face]);

    if (inside && face == -1)
        snaped = goxel_unproject(goxel, view, &inputs->mouse_pos, false,
                                 &pos, &normal);
    if (snaped) {
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
    }

    switch (state) {
    case STATE_IDLE:
        data->snap_face = -1;
        if (snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED:
        if (!snaped) return STATE_CANCEL;
        goxel_set_help_text(goxel, "Click and drag to set selection.");
        data->start_pos = pos;
        box = get_box(&data->start_pos, &pos, &normal, 0,
                      &goxel->plane);
        render_box(&goxel->rend, &box, &box_color, EFFECT_WIREFRAME);
        if (down) {
            state = STATE_PAINT;
        }
        break;

    case STATE_PAINT:
        goxel_set_help_text(goxel, "Drag.");
        if (!snaped || !inside) return state;
        goxel->selection = get_box(&data->start_pos, &pos, &normal, 0,
                                   &goxel->plane);
        if (up) {
            goxel->tool_plane = plane_from_normal(pos, goxel->plane.u);
            return STATE_PAINT2;
        }
        break;

    case STATE_PAINT2:
        goxel_set_help_text(goxel, "Adjust height.");
        if (!snaped || !inside) return state;
        pos = vec3_add(goxel->tool_plane.p,
                    vec3_project(vec3_sub(pos, goxel->tool_plane.p),
                                 goxel->plane.n));
        goxel->selection = get_box(&data->start_pos, &pos, &normal, 0,
                                   &goxel->plane);
        if (down) {
            return STATE_WAIT_UP;
        }
        break;

    case STATE_WAIT_UP:
        goxel->tool_plane = plane_null;
        goxel->selection = box_get_bbox(goxel->selection);
        return up ? STATE_IDLE : STATE_WAIT_UP;
        break;

    case STATE_SNAPED_FACE:
        if (face == -1) return STATE_IDLE;
        goxel_set_help_text(goxel, "Drag to move face");
        render_img(&goxel->rend, NULL, &face_plane.mat, EFFECT_NO_SHADING);
        if (down) {
            state = STATE_MOVE_FACE;
            goxel->tool_plane = plane(pos, normal,
                                      vec3_normalized(face_plane.u));
        }
        break;

    case STATE_MOVE_FACE:
        if (up) return STATE_IDLE;
        goxel_set_help_text(goxel, "Drag to move face");
        n = vec3_normalized(face_plane.n);
        goxel_unproject_on_plane(goxel, view, &inputs->mouse_pos,
                                 &goxel->tool_plane, &pos, &normal);
        pos = vec3_add(goxel->tool_plane.p,
                    vec3_project(vec3_sub(pos, goxel->tool_plane.p), n));
        pos.x = round(pos.x);
        pos.y = round(pos.y);
        pos.z = round(pos.z);
        if (g_drag_mode == DRAG_RESIZE) {
            goxel->selection = box_move_face(goxel->selection,
                                             data->snap_face, pos);
        } else {
            vec3_t d = vec3_add(goxel->selection.p, face_plane.n);
            vec3_t ofs = vec3_project(vec3_sub(pos, d), n);
            vec3_iadd(&goxel->selection.p, ofs);
        }
        break;
    }
    return state;
}

static int cancel(int state, void **data_)
{
    if (!(*data_)) return 0;
    goxel->tool_plane = plane_null;
    data_t *data = get_data(data_);
    free(data);
    *data_ = NULL;
    return 0;
}

static int gui(void)
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

TOOL_REGISTER(TOOL_SELECTION, selection,
              .iter_fn = iter,
              .cancel_fn = cancel,
              .gui_fn = gui,
              .shortcut = "R",
)
