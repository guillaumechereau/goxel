/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

typedef struct {
    tool_t tool;

    mesh_t *mesh_orig; // Original mesh.
    mesh_t *mesh;      // Mesh containing only the tool path.
    float start_pos[3];

    struct {
        gesture3d_t drag;
        gesture3d_t hover;
    } gestures;

} tool_line_t;

// XXX: same as in brush.c.
static void get_box(const float p0[3], const float p1[3], const float n[3],
                    float r, const float plane[4][4], float out[4][4])
{
    float rot[4][4], box[4][4];
    float v[3];

    if (p1 && vec3_dist(p0, p1) < 0.5) p1 = NULL;

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

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_line_t *tool = USER_GET(user, 0);
    painter_t painter;
    const float radius = goxel.tool_radius;
    cursor_t *curs = gest->cursor;
    float box[4][4];

    if (gest->state == GESTURE_BEGIN) {
        vec3_copy(curs->pos, tool->start_pos);
        assert(tool->mesh_orig);
        mesh_set(tool->mesh_orig, goxel.image->active_layer->mesh);
        image_history_push(goxel.image);
    }

    painter = *(painter_t*)USER_GET(user, 1);
    painter.mode = MODE_MAX;
    vec4_set(painter.color, 255, 255, 255, 255);
    get_box(tool->start_pos, curs->pos, curs->normal, radius, NULL, box);
    mesh_clear(tool->mesh);
    mesh_op(tool->mesh, &painter, box);

    painter = *(painter_t*)USER_GET(user, 1);
    if (!goxel.tool_mesh) goxel.tool_mesh = mesh_new();
    mesh_set(goxel.tool_mesh, tool->mesh_orig);
    mesh_merge(goxel.tool_mesh, tool->mesh, painter.mode, painter.color);

    if (gest->state == GESTURE_END) {
        mesh_set(goxel.image->active_layer->mesh, goxel.tool_mesh);
        mesh_set(tool->mesh_orig, goxel.tool_mesh);
        mesh_delete(goxel.tool_mesh);
        goxel.tool_mesh = NULL;
    }

    return 0;
}

static int on_hover(gesture3d_t *gest, void *user)
{
    mesh_t *mesh = goxel.image->active_layer->mesh;

    const painter_t *painter = USER_GET(user, 1);
    cursor_t *curs = gest->cursor;
    float box[4][4];

    if (gest->state == GESTURE_END) {
        mesh_delete(goxel.tool_mesh);
        goxel.tool_mesh = NULL;
        return 0;
    }
    get_box(curs->pos, NULL, curs->normal, goxel.tool_radius, NULL, box);

    if (!goxel.tool_mesh) goxel.tool_mesh = mesh_new();
    mesh_set(goxel.tool_mesh, mesh);
    mesh_op(goxel.tool_mesh, painter, box);
    return 0;
}

static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    tool_line_t *tool = (void*)tool_;
    cursor_t *curs = &goxel.cursor;
    // XXX: for the moment we force rounded positions for the brush tool
    // to make things easier.
    curs->snap_mask |= SNAP_ROUNDED;

    if (!tool->mesh_orig)
        tool->mesh_orig = mesh_copy(goxel.image->active_layer->mesh);
    if (!tool->mesh)
        tool->mesh = mesh_new();

    if (!tool->gestures.drag.type) {
        tool->gestures.drag = (gesture3d_t) {
            .type = GESTURE_DRAG,
            .callback = on_drag,
        };
        tool->gestures.hover = (gesture3d_t) {
            .type = GESTURE_HOVER,
            .callback = on_hover,
        };
    }

    curs->snap_offset = goxel.snap_offset * goxel.tool_radius +
        ((painter->mode == MODE_OVER) ? 0.5 : -0.5);

    gesture3d(&tool->gestures.hover, curs, USER_PASS(tool, painter));
    gesture3d(&tool->gestures.drag, curs, USER_PASS(tool, painter));
    return 0;
}

static int gui(tool_t *tool)
{
    tool_gui_color();
    tool_gui_radius();
    tool_gui_smoothness();
    tool_gui_snap();
    tool_gui_shape(NULL);
    tool_gui_symmetry();
    return 0;
}

TOOL_REGISTER(TOOL_LINE, line, tool_line_t,
              .name = "Line",
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_ALLOW_PICK_COLOR,
)
