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

    STATE_ENTER = 0x0100,
};

typedef struct {
    vec3_t start_pos;
    mesh_t *mesh_orig;
} data_t;

static data_t *get_data(void **data_)
{
    data_t **data = (data_t**)data_;
    if (!*data) {
        *data = calloc(1, sizeof(**data));
        (*data)->mesh_orig = mesh_copy(goxel->image->active_layer->mesh);
    }
    return *data;
}

static void set_snap_hint(goxel_t *goxel, int snap)
{
    if (snap == SNAP_MESH)
        goxel_set_hint_text(goxel, "[Snapped to mesh]");
    if (snap == SNAP_PLANE)
        goxel_set_hint_text(goxel, "[Snapped to plane]");
    if (snap == SNAP_SELECTION_IN || snap == SNAP_SELECTION_OUT)
        goxel_set_hint_text(goxel, "[Snapped to selection]");
}

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

static int iter(const inputs_t *inputs, int state, void **data_,
                const vec2_t *view_size, bool inside)
{
    data_t *data = get_data(data_);
    const bool down = inputs->mouse_down[0];
    const bool up = !down;
    int snaped = 0;
    vec3_t pos = vec3_zero, normal;
    box_t box;
    uvec4b_t box_color = HEXCOLOR(0xffff00ff);
    mesh_t *mesh = goxel->image->active_layer->mesh;

    if (inside)
        snaped = goxel_unproject(
                goxel, view_size, &inputs->mouse_pos,
                goxel->painter.mode == MODE_ADD && !goxel->snap_offset,
                &pos, &normal);
    set_snap_hint(goxel, snaped);
    if (snaped) {
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
    }
    switch (state) {

    case STATE_IDLE:
        if (snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED | STATE_ENTER:
        mesh_set(data->mesh_orig, mesh);
        break;

    case STATE_SNAPED:
        if (!snaped) return STATE_CANCEL;
        goxel_set_help_text(goxel, "Click and drag to draw.");
        data->start_pos = pos;
        box = get_box(&data->start_pos, &pos, &normal, 0,
                      &goxel->plane);
        render_box(&goxel->rend, &box, &box_color, EFFECT_WIREFRAME);
        if (down) {
            state = STATE_PAINT;
            image_history_push(goxel->image);
        }
        break;

    case STATE_PAINT:
        goxel_set_help_text(goxel, "Drag.");
        if (!snaped || !inside) return state;
        box = get_box(&data->start_pos, &pos, &normal, 0, &goxel->plane);
        render_box(&goxel->rend, &box, &box_color, EFFECT_WIREFRAME);
        mesh_set(mesh, data->mesh_orig);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, MESH_LAYERS);
        if (up) {
            if (!goxel->tool_shape_two_steps) {
                goxel_update_meshes(goxel, -1);
                state = STATE_END;
            } else {
                goxel->tool_plane = plane_from_normal(pos, goxel->plane.u);
                state = STATE_PAINT2;
            }
        }
        break;

    case STATE_PAINT2:
        goxel_set_help_text(goxel, "Adjust height.");
        if (!snaped || !inside) return state;
        pos = vec3_add(goxel->tool_plane.p,
                    vec3_project(vec3_sub(pos, goxel->tool_plane.p),
                                 goxel->plane.n));
        box = get_box(&data->start_pos, &pos, &normal, 0,
                      &goxel->plane);
        render_box(&goxel->rend, &box, &box_color, EFFECT_WIREFRAME);
        mesh_set(mesh, data->mesh_orig);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, MESH_LAYERS);
        if (down) {
            goxel_update_meshes(goxel, -1);
            return STATE_WAIT_UP;
        }
        break;

    case STATE_WAIT_UP:
        goxel->tool_plane = plane_null;
        if (up) return STATE_END;
        break;

    }
    return state;
}

static int cancel(int state, void **data_)
{
    if (!(*data_)) return 0;
    data_t *data = get_data(data_);
    mesh_set(goxel->image->active_layer->mesh, data->mesh_orig);
    mesh_delete(data->mesh_orig);
    free(data);
    *data_ = NULL;
    return 0;
}

static int gui(void)
{
    tool_gui_smoothness();
    gui_checkbox("Two steps", &goxel->tool_shape_two_steps,
                 "Second click set the height");
    tool_gui_snap();
    tool_gui_mode();
    tool_gui_shape();
    tool_gui_color();
    if (!box_is_null(goxel->selection))
        gui_action_button("fill_selection", "Fill selection", 1.0, "");

    return 0;
}

TOOL_REGISTER(TOOL_SHAPE, shape,
              .iter_fn = iter,
              .cancel_fn = cancel,
              .gui_fn = gui,
              .shortcut = "S",
)
