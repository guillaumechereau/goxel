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

    struct {
        gesture3d_t hover;
        gesture3d_t drag;
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

static int on_hover(gesture3d_t *gest, void *user)
{
    float box[4][4];
    cursor_t *curs = gest->cursor;
    uint8_t box_color[4] = {255, 255, 0, 255};

    goxel_set_help_text("Click and drag to set selection.");
    get_box(curs->pos, curs->pos, curs->normal, 0, goxel.plane, box);
    render_box(&goxel.rend, box, box_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_selection_t *tool = user;
    cursor_t *curs = gest->cursor;

    if (gest->state == GESTURE_BEGIN)
        vec3_copy(curs->pos, tool->start_pos);
    curs->snap_mask &= ~(SNAP_SELECTION_IN | SNAP_SELECTION_OUT);
    goxel_set_help_text("Drag.");
    get_box(tool->start_pos, curs->pos, curs->normal,
            0, goxel.plane, goxel.selection);
    return 0;
}

// XXX: this is very close to tool_shape_iter.
static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    float transf[4][4];

    tool_selection_t *selection = (tool_selection_t*)tool;
    cursor_t *curs = &goxel.cursor;
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
    }
    if (box_edit(SNAP_SELECTION_OUT, g_drag_mode == DRAG_RESIZE ? 1 : 0,
                 transf, NULL)) {
        mat4_mul(transf, goxel.selection, goxel.selection);
        return 0;
    }

    if (gesture3d(&selection->gestures.drag, curs, selection)) goto end;
    if (gesture3d(&selection->gestures.hover, curs, selection)) goto end;

end:
    return tool->state;
}

static float get_magnitude(float box[4][4], int axis_index)
{
    return box[0][axis_index] + box[1][axis_index] + box[2][axis_index];
}

static int gui(tool_t *tool)
{
    float x_mag, y_mag, z_mag;
    int x, y, z, w, h, d;
    float (*box)[4][4] = &goxel.selection;
    if (box_is_null(*box)) return 0;

    gui_text("Drag mode");
    gui_combo("##drag_mode", &g_drag_mode,
              (const char*[]) {"Resize", "Move"}, 2);

    gui_group_begin(NULL);
    if (gui_action_button(ACTION_reset_selection, "Reset", 1.0)) {
        gui_group_end();
        return 0;
    }
    gui_action_button(ACTION_fill_selection, "Fill", 1.0);
    gui_action_button(ACTION_layer_clear, "Clear", 1.0);
    gui_action_button(ACTION_cut_as_new_layer, "Cut as new layer", 1.0);
    gui_group_end();

    x_mag = fabs(get_magnitude(*box, 0));
    y_mag = fabs(get_magnitude(*box, 1));
    z_mag = fabs(get_magnitude(*box, 2));
    w = round(x_mag * 2);
    h = round(y_mag * 2);
    d = round(z_mag * 2);
    x = round((*box)[3][0] - x_mag);
    y = round((*box)[3][1] - y_mag);
    z = round((*box)[3][2] - z_mag);

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
    bbox_from_extents(*box,
            VEC(x + w / 2., y + h / 2., z + d / 2.),
            w / 2., h / 2., d / 2.);
    return 0;
}

TOOL_REGISTER(TOOL_SELECTION, selection, tool_selection_t,
              .name = "Selection",
              .iter_fn = iter,
              .gui_fn = gui,
              .default_shortcut = "R",
)
