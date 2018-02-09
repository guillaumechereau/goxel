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
    vec3_t start_pos;
    vec3_t last_pos;
    // Cache of the last operation.
    // XXX: could we remove this?
    struct     {
        vec3_t     pos;
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
    mesh_t *mesh = goxel->image->active_layer->mesh;
    const bool pressed = curs->flags & CURSOR_PRESSED;
    if (    pressed == brush->last_op.pressed &&
            mode == brush->last_op.mode &&
            brush->last_op.mesh_key == mesh_get_key(mesh) &&
            vec3_equal(curs->pos.v, brush->last_op.pos.v)) {
        return true;
    }
    brush->last_op.pressed = pressed;
    brush->last_op.mode = mode;
    brush->last_op.pos = curs->pos;
    return false;
}

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

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_brush_t *brush = (tool_brush_t*)user;
    painter_t painter2;
    box_t box;
    cursor_t *curs = gest->cursor;
    bool shift = curs->flags & CURSOR_SHIFT;
    float r = goxel->tool_radius;
    int nb, i;
    vec3_t pos;

    if (gest->state == GESTURE_BEGIN) {
        mesh_set(brush->mesh_orig, goxel->image->active_layer->mesh);
        brush->last_op.mode = 0; // Discard last op.
        brush->last_pos = curs->pos;
        image_history_push(goxel->image);
        mesh_clear(brush->mesh);

        if (shift) {
            painter2 = goxel->painter;
            painter2.shape = &shape_cylinder;
            painter2.mode = MODE_MAX;
            box = get_box(&brush->start_pos, &curs->pos, &curs->normal,
                          r, NULL);
            mesh_op(brush->mesh, &painter2, &box);
        }
    }

    if (    (gest->state == GESTURE_UPDATE) &&
            (check_can_skip(brush, curs, goxel->painter.mode)))
        return 0;

    painter2 = goxel->painter;
    painter2.mode = MODE_MAX;
    vec4_set(painter2.color, 255, 255, 255, 255);

    // Render several times if the space between the current pos
    // and the last pos is larger than the size of the tool shape.
    nb = ceil(vec3_dist(curs->pos.v, brush->last_pos.v) / (2 * r));
    nb = max(nb, 1);
    for (i = 0; i < nb; i++) {
        vec3_mix(brush->last_pos.v, curs->pos.v, (i + 1.0) / nb, pos.v);
        box = get_box(&pos, NULL, &curs->normal, r, NULL);
        mesh_op(brush->mesh, &painter2, &box);
    }

    if (!goxel->tool_mesh) goxel->tool_mesh = mesh_new();
    mesh_set(goxel->tool_mesh, brush->mesh_orig);
    mesh_merge(goxel->tool_mesh, brush->mesh, goxel->painter.mode,
               goxel->painter.color);
    goxel_update_meshes(goxel, MESH_RENDER);
    brush->start_pos = curs->pos;
    brush->last_op.mesh_key = mesh_get_key(goxel->tool_mesh);

    if (gest->state == GESTURE_END) {
        mesh_set(goxel->image->active_layer->mesh, goxel->tool_mesh);
        mesh_set(brush->mesh_orig, goxel->tool_mesh);
        mesh_delete(goxel->tool_mesh);
        goxel->tool_mesh = NULL;
        goxel_update_meshes(goxel, -1);
    }
    brush->last_pos = curs->pos;
    return 0;
}

static int on_hover(gesture3d_t *gest, void *user)
{
    mesh_t *mesh = goxel->image->active_layer->mesh;
    tool_brush_t *brush = (tool_brush_t*)user;
    cursor_t *curs = gest->cursor;
    box_t box;
    bool shift = curs->flags & CURSOR_SHIFT;

    if (gest->state == GESTURE_END) {
        mesh_delete(goxel->tool_mesh);
        goxel->tool_mesh = NULL;
        goxel_update_meshes(goxel, MESH_RENDER);
        return 0;
    }

    if (shift)
        render_line(&goxel->rend, brush->start_pos.v, curs->pos.v, NULL);

    if (goxel->tool_mesh && check_can_skip(brush, curs, goxel->painter.mode))
        return 0;

    box = get_box(&curs->pos, NULL, &curs->normal,
                  goxel->tool_radius, NULL);

    if (!goxel->tool_mesh) goxel->tool_mesh = mesh_new();
    mesh_set(goxel->tool_mesh, mesh);
    mesh_op(goxel->tool_mesh, &goxel->painter, &box);
    goxel_update_meshes(goxel, MESH_RENDER);

    brush->last_op.mesh_key = mesh_get_key(mesh);

    return 0;
}


static int iter(tool_t *tool, const vec4_t *view)
{
    tool_brush_t *brush = (tool_brush_t*)tool;
    cursor_t *curs = &goxel->cursor;

    if (!brush->mesh_orig)
        brush->mesh_orig = mesh_copy(goxel->image->active_layer->mesh);
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

    curs->snap_offset = goxel->snap_offset * goxel->tool_radius +
        ((goxel->painter.mode == MODE_OVER) ? 0.5 : -0.5);

    gesture3d(&brush->gestures.hover, curs, brush);
    gesture3d(&brush->gestures.drag, curs, brush);

    return tool->state;
}


static int gui(tool_t *tool)
{
    tool_gui_radius();
    tool_gui_smoothness();
    tool_gui_snap();
    tool_gui_mode();
    tool_gui_shape();
    tool_gui_color();
    tool_gui_symmetry();
    return 0;
}

TOOL_REGISTER(TOOL_BRUSH, brush, tool_brush_t,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_ALLOW_PICK_COLOR,
              .shortcut = "B"
)
