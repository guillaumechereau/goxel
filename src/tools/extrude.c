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
    struct {
        gesture3d_t drag;
    } gestures;
} tool_extrude_t;

static int select_cond(uvec4b_t value,
                       const uvec4b_t neighboors[6],
                       const uint8_t mask[6],
                       void *user)
{
    int i;
    if (value.a == 0) return 0;
    if (neighboors[3].a) return 0;
    for (i = 0; i < 6; i++) {
        if (i == 2 || i == 3) continue;
        if (mask[i]) return 255;
    }
    return 0;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    mesh_t *mesh = goxel->image->active_layer->mesh;
    cursor_t *curs = gest->cursor;

    if (gest->state == GESTURE_BEGIN) {
        mesh_t *test = mesh_new();
        mesh_select(mesh, &curs->pos, select_cond, NULL, test);
        mesh_merge(mesh, test, MODE_SUB);
        mesh_delete(test);
        goxel_update_meshes(goxel, -1);
    }
    return 0;
}

static int iter(tool_t *tool_, const vec4_t *view)
{
    tool_extrude_t *tool = (tool_extrude_t*)tool_;
    cursor_t *curs = &goxel->cursor;
    curs->snap_offset = -0.5;

    if (!tool->gestures.drag.type) {
        tool->gestures.drag = (gesture3d_t) {
            .type = GESTURE_DRAG,
            .callback = on_drag,
        };
    }
    gesture3d(&tool->gestures.drag, curs, tool);
    return 0;
}

static int gui(tool_t *tool)
{
    return 0;
}

TOOL_REGISTER(TOOL_EXTRUDE, extrude, tool_extrude_t,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT,
)
