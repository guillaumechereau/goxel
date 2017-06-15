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

    STATE_PAINT,

    STATE_ENTER     = 0x0100,
};

static int iter(const inputs_t *inputs, int state, void **data,
                const vec4_t *view, bool inside)
{
    box_t box;
    painter_t painter = goxel->painter;
    mesh_t *mesh = goxel->image->active_layer->mesh;
    cursor_t *curs = &goxel->cursor;
    curs->snap_mask = SNAP_CAMERA;
    curs->snap_offset = 0;

    painter.mode = MODE_SUB_CLAMP;
    painter.shape = &shape_cylinder;
    painter.color = uvec4b(255, 255, 255, 255);

    // Create the tool box from the camera along the visible ray.
    box.mat = mat4_identity;
    box.w = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                     vec4(1, 0, 0, 0)).xyz;
    box.h = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                     vec4(0, 1, 0, 0)).xyz;
    box.d = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                     vec4(0, 0, 1, 0)).xyz;
    box.d = vec3_neg(curs->normal);
    box.p = curs->pos;
    // Just a large value for the size of the laser box.
    mat4_itranslate(&box.mat, 0, 0, -1024);
    mat4_iscale(&box.mat, goxel->tool_radius, goxel->tool_radius, 1024);
    render_box(&goxel->rend, &box, NULL, EFFECT_WIREFRAME);

    switch (state) {
    case STATE_IDLE:
        if (curs->flags & CURSOR_PRESSED) return STATE_PAINT;
        break;
    case STATE_PAINT | STATE_ENTER:
        image_history_push(goxel->image);
        break;
    case STATE_PAINT:
        if (!(curs->flags & CURSOR_PRESSED)) return STATE_IDLE;
        mesh_op(mesh, &painter, &box);
        goxel_update_meshes(goxel, -1);
        break;
    }
    return state;
}

static int gui(void)
{
    tool_gui_radius();
    tool_gui_smoothness();
    return 0;
}

TOOL_REGISTER(TOOL_LASER, laser,
              .iter_fn = iter,
              .gui_fn = gui,
              .shortcut = "L",
)
