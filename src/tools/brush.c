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

    STATE_ENTER = 0x0100,
};

typedef struct {
    tool_t tool;

    mesh_t *mesh_orig; // Original mesh.
    mesh_t *mesh;      // Mesh containing only the tool path.

    vec3_t start_pos;
    // Cache of the last operation.
    // XXX: could we remove this?
    struct     {
        vec3_t     pos;
        bool       pressed;
        int        mode;
    } last_op;
} tool_brush_t;

static bool check_can_skip(tool_brush_t *brush, const cursor_t *curs,
                           int mode)
{
    const bool pressed = curs->flags & CURSOR_PRESSED;
    if (    pressed == brush->last_op.pressed &&
            mode == brush->last_op.mode &&
            vec3_equal(curs->pos, brush->last_op.pos))
        return true;
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


static int iter(tool_t *tool, const vec4_t *view)
{
    tool_brush_t *brush = (tool_brush_t*)tool;
    box_t box;
    painter_t painter2;
    mesh_t *mesh = goxel->image->active_layer->mesh;
    cursor_t *curs = &goxel->cursor;
    bool shift = curs->flags & CURSOR_SHIFT;

    if (!brush->mesh_orig)
        brush->mesh_orig = mesh_copy(goxel->image->active_layer->mesh);
    if (!brush->mesh)
        brush->mesh = mesh_new();

    curs->snap_offset = goxel->snap_offset * goxel->tool_radius +
        ((goxel->painter.mode == MODE_OVER) ? 0.5 : -0.5);

    switch (tool->state) {
    case STATE_IDLE:
        if (curs->snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED | STATE_ENTER:
        mesh_set(brush->mesh_orig, mesh);
        brush->last_op.mode = 0; // Discard last op.
        break;

    case STATE_SNAPED:
        if (!curs->snaped) return STATE_CANCEL;
        if (shift)
            render_line(&goxel->rend, &brush->start_pos, &curs->pos, NULL);
        if (check_can_skip(brush, curs, goxel->painter.mode))
            return tool->state;
        box = get_box(&curs->pos, NULL, &curs->normal,
                      goxel->tool_radius, NULL);

        mesh_set(mesh, brush->mesh_orig);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, MESH_LAYERS);

        if (curs->flags & CURSOR_PRESSED) {
            tool->state = STATE_PAINT;
            brush->last_op.mode = 0;
            mesh_set(mesh, brush->mesh_orig);
            image_history_push(goxel->image);
            mesh_clear(brush->mesh);
        }
        if (shift) {
            render_line(&goxel->rend, &brush->start_pos, &curs->pos, NULL);
            if (curs->flags & CURSOR_PRESSED) {
                painter2 = goxel->painter;
                painter2.shape = &shape_cylinder;
                box = get_box(&brush->start_pos, &curs->pos, &curs->normal,
                              goxel->tool_radius, NULL);
                mesh_set(mesh, brush->mesh_orig);
                mesh_op(mesh, &painter2, &box);
                mesh_set(brush->mesh_orig, mesh);
                goxel_update_meshes(goxel, MESH_LAYERS);
                brush->start_pos = curs->pos;
            }
        }
        break;

    case STATE_PAINT:
        if (!curs->snaped) return tool->state;
        if (check_can_skip(brush, curs, goxel->painter.mode))
            return tool->state;
        if (!(curs->flags & CURSOR_PRESSED)) {
            goxel->camera.target = curs->pos;
            mesh_set(goxel->pick_mesh, goxel->layers_mesh);
            mesh_set(brush->mesh_orig, mesh);
            return STATE_IDLE;
        }
        box = get_box(&curs->pos, NULL, &curs->normal,
                      goxel->tool_radius, NULL);
        painter2 = goxel->painter;
        painter2.mode = MODE_MAX;
        mesh_op(brush->mesh, &painter2, &box);
        mesh_set(mesh, brush->mesh_orig);
        mesh_merge(mesh, brush->mesh, goxel->painter.mode);
        goxel_update_meshes(goxel, MESH_LAYERS);
        brush->start_pos = curs->pos;
        break;
    }
    return tool->state;
}

static int cancel(tool_t *tool)
{
    if (!tool) return 0;
    tool_brush_t *brush = (tool_brush_t*)tool;
    mesh_set(goxel->image->active_layer->mesh, brush->mesh_orig);
    mesh_delete(brush->mesh_orig);
    mesh_delete(brush->mesh);
    brush->mesh_orig = NULL;
    brush->mesh = NULL;
    return 0;
}

static int gui(tool_t *tool)
{
    tool_gui_radius();
    tool_gui_smoothness();
    tool_gui_snap();
    tool_gui_mode();
    tool_gui_shape();
    tool_gui_color();
    return 0;
}

TOOL_REGISTER(TOOL_BRUSH, brush, tool_brush_t,
              .iter_fn = iter,
              .cancel_fn = cancel,
              .gui_fn = gui,
              .shortcut = "B"
)
