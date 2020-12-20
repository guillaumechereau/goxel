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

    mesh_t *mesh_orig; // Original mesh.
    mesh_t *mesh;      // Mesh containing only the tool path.

    // Gesture start and last pos (should we put it in the 3d gesture?)
    float start_pos[3];
    float last_pos[3];
    // Cache of the last operation.
    // XXX: could we remove this?
    struct     {
        float      pos[3];
        bool       pressed;
        int        mode;
        uint64_t   mesh_key;
    } last_op;

    struct {
        gesture3d_t drag;
        gesture3d_t hover;
    } gestures;

} tool_brush_t;

static bool check_can_skip(tool_brush_t *brush, const cursor_t *curs,
                           int mode)
{
    mesh_t *mesh = goxel.tool_mesh;
    const bool pressed = curs->flags & CURSOR_PRESSED;
    if (    pressed == brush->last_op.pressed &&
            mode == brush->last_op.mode &&
            brush->last_op.mesh_key == mesh_get_key(mesh) &&
            vec3_equal(curs->pos, brush->last_op.pos)) {
        return true;
    }
    brush->last_op.pressed = pressed;
    brush->last_op.mode = mode;
    brush->last_op.mesh_key = mesh_get_key(mesh);
    vec3_copy(curs->pos, brush->last_op.pos);
    return false;
}

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

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_brush_t *brush = USER_GET(user, 0);
    painter_t painter = *(painter_t*)USER_GET(user, 1);
    float box[4][4];
    cursor_t *curs = gest->cursor;
    bool shift = curs->flags & CURSOR_SHIFT;
    float r = goxel.tool_radius;
    int nb, i;
    float pos[3];

    if (gest->state == GESTURE_BEGIN) {
        mesh_set(brush->mesh_orig, goxel.image->active_layer->mesh);
        brush->last_op.mode = 0; // Discard last op.
        vec3_copy(curs->pos, brush->last_pos);
        image_history_push(goxel.image);
        mesh_clear(brush->mesh);

        if (shift) {
            painter.shape = &shape_cylinder;
            painter.mode = MODE_MAX;
            vec4_set(painter.color, 255, 255, 255, 255);
            get_box(brush->start_pos, curs->pos, curs->normal, r, NULL, box);
            mesh_op(brush->mesh, &painter, box);
        }
    }

    painter = *(painter_t*)USER_GET(user, 1);
    if (    (gest->state == GESTURE_UPDATE) &&
            (check_can_skip(brush, curs, painter.mode))) {
        return 0;
    }

    painter.mode = MODE_MAX;
    vec4_set(painter.color, 255, 255, 255, 255);

    // Render several times if the space between the current pos
    // and the last pos is larger than the size of the tool shape.
    nb = ceil(vec3_dist(curs->pos, brush->last_pos) / (2 * r));
    nb = max(nb, 1);
    for (i = 0; i < nb; i++) {
        vec3_mix(brush->last_pos, curs->pos, (i + 1.0) / nb, pos);
        get_box(pos, NULL, curs->normal, r, NULL, box);
        mesh_op(brush->mesh, &painter, box);
    }

    painter = *(painter_t*)USER_GET(user, 1);
    if (!goxel.tool_mesh) goxel.tool_mesh = mesh_new();
    mesh_set(goxel.tool_mesh, brush->mesh_orig);
    mesh_merge(goxel.tool_mesh, brush->mesh, painter.mode, painter.color);
    vec3_copy(curs->pos, brush->start_pos);
    brush->last_op.mesh_key = mesh_get_key(goxel.tool_mesh);

    if (gest->state == GESTURE_END) {
        mesh_set(goxel.image->active_layer->mesh, goxel.tool_mesh);
        mesh_set(brush->mesh_orig, goxel.tool_mesh);
        mesh_delete(goxel.tool_mesh);
        goxel.tool_mesh = NULL;
    }
    vec3_copy(curs->pos, brush->last_pos);
    return 0;
}

static int on_hover(gesture3d_t *gest, void *user)
{
    mesh_t *mesh = goxel.image->active_layer->mesh;
    tool_brush_t *brush = USER_GET(user, 0);
    const painter_t *painter = USER_GET(user, 1);
    cursor_t *curs = gest->cursor;
    float box[4][4];
    bool shift = curs->flags & CURSOR_SHIFT;

    if (gest->state == GESTURE_END || !curs->snaped) {
        mesh_delete(goxel.tool_mesh);
        goxel.tool_mesh = NULL;
        return 0;
    }

    if (shift)
        render_line(&goxel.rend, brush->start_pos, curs->pos, NULL, 0);

    if (goxel.tool_mesh && check_can_skip(brush, curs, painter->mode))
        return 0;

    get_box(curs->pos, NULL, curs->normal, goxel.tool_radius, NULL, box);

    if (!goxel.tool_mesh) goxel.tool_mesh = mesh_new();
    mesh_set(goxel.tool_mesh, mesh);
    mesh_op(goxel.tool_mesh, painter, box);

    brush->last_op.mesh_key = mesh_get_key(mesh);

    return 0;
}


static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    tool_brush_t *brush = (tool_brush_t*)tool;
    cursor_t *curs = &goxel.cursor;
    // XXX: for the moment we force rounded positions for the brush tool
    // to make things easier.
    curs->snap_mask |= SNAP_ROUNDED;

    if (!brush->mesh_orig)
        brush->mesh_orig = mesh_copy(goxel.image->active_layer->mesh);
    if (!brush->mesh)
        brush->mesh = mesh_new();

    if (!brush->gestures.drag.type) {
        brush->gestures.drag = (gesture3d_t) {
            .type = GESTURE_DRAG,
            .callback = on_drag,
        };
        brush->gestures.hover = (gesture3d_t) {
            .type = GESTURE_HOVER,
            .callback = on_hover,
        };
    }

    curs->snap_offset = goxel.snap_offset * goxel.tool_radius +
        ((painter->mode == MODE_OVER) ? 0.5 : -0.5);

    gesture3d(&brush->gestures.hover, curs, USER_PASS(brush, painter));
    gesture3d(&brush->gestures.drag, curs, USER_PASS(brush, painter));

    return tool->state;
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

TOOL_REGISTER(TOOL_BRUSH, brush, tool_brush_t,
              .name = "Brush",
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_ALLOW_PICK_COLOR,
              .default_shortcut = "B"
)
