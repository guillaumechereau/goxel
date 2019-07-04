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


typedef struct {
    tool_t tool;
    float  start_pos[3];
    mesh_t *mesh_orig;
    bool   planar; // Stay on the original plane.

    struct {
        gesture3d_t drag;
        gesture3d_t hover;
    } gestures;

} tool_shape_t;


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

    goxel_set_help_text("Click and drag to draw.");
    get_box(curs->pos, curs->pos, curs->normal, 0, goxel.plane, box);
    render_box(&goxel.rend, box, box_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_shape_t *shape = USER_GET(user, 0);
    const painter_t *painter = USER_GET(user, 1);
    mesh_t *layer_mesh = goxel.image->active_layer->mesh;
    float box[4][4], pos[3];
    cursor_t *curs = gest->cursor;

    if (gest->state == GESTURE_BEGIN) {
        mesh_set(shape->mesh_orig, layer_mesh);
        vec3_copy(curs->pos, shape->start_pos);
        image_history_push(goxel.image);
        if (shape->planar) {
            vec3_addk(curs->pos, curs->normal, -curs->snap_offset, pos);
            plane_from_normal(goxel.tool_plane, pos, curs->normal);
        }
    }

    goxel_set_help_text("Drag.");
    get_box(shape->start_pos, curs->pos, curs->normal, 0, goxel.plane, box);
    if (!goxel.tool_mesh) goxel.tool_mesh = mesh_new();
    mesh_set(goxel.tool_mesh, shape->mesh_orig);
    mesh_op(goxel.tool_mesh, painter, box);

    if (gest->state == GESTURE_END) {
        mesh_set(layer_mesh, goxel.tool_mesh);
        mesh_delete(goxel.tool_mesh);
        goxel.tool_mesh = NULL;
        mat4_copy(plane_null, goxel.tool_plane);
    }
    return 0;
}


static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    tool_shape_t *shape = (tool_shape_t*)tool;
    cursor_t *curs = &goxel.cursor;
    curs->snap_mask |= SNAP_ROUNDED;
    curs->snap_offset = (painter->mode == MODE_OVER) ? 0.5 : -0.5;

    if (!shape->mesh_orig)
        shape->mesh_orig = mesh_copy(goxel.image->active_layer->mesh);

    if (!shape->gestures.drag.type) {
        shape->gestures.drag = (gesture3d_t) {
            .type = GESTURE_DRAG,
            .callback = on_drag,
        };
        shape->gestures.hover = (gesture3d_t) {
            .type = GESTURE_HOVER,
            .callback = on_hover,
        };
    }

    gesture3d(&shape->gestures.drag, curs, USER_PASS(shape, painter));
    gesture3d(&shape->gestures.hover, curs, USER_PASS(shape, painter));

    return tool->state;
}


static int gui(tool_t *tool)
{
    tool_shape_t *tool_shape = (void*)tool;
    tool_gui_color();
    tool_gui_smoothness();
    gui_checkbox("Planar", &tool_shape->planar, "Stay on original plane");
    tool_gui_snap();
    tool_gui_shape(NULL);
    tool_gui_symmetry();
    return 0;
}

TOOL_REGISTER(TOOL_SHAPE, shape, tool_shape_t,
              .name = "Shape",
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_ALLOW_PICK_COLOR,
              .default_shortcut = "S",
)
