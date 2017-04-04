/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

static const int STATE_MASK = 0x00ff;

enum {
    STATE_IDLE      = 0,
    STATE_CANCEL    = 1,
    STATE_END       = 2,
    // Added to a state the first time we enter into it.
    STATE_ENTER     = 0x0100,
};

static const tool_t *g_tools[TOOL_COUNT] = {};

static int tool_set_action(const action_t *a, astack_t *s)
{
    tool_cancel(goxel, goxel->tool, goxel->tool_state);
    goxel->tool = ((tool_t*)a->data)->id;
    return 0;
}

void tool_register_(const tool_t *tool)
{
    action_t action;
    g_tools[tool->id] = tool;
    action = (action_t) {
        .id = tool->action_id,
        .shortcut = tool->shortcut,
        .help = "set tool",
        .func = tool_set_action,
        .data = (void*)tool,
    };
    action_register(&action);
}

int tool_iter(goxel_t *goxel, int tool, const inputs_t *inputs, int state,
              const vec2_t *view_size, bool inside)
{
    int ret;
    assert(tool >= 0 && tool < TOOL_COUNT);
    assert(g_tools[tool]->iter_fn);

    while (true) {
        ret = g_tools[tool]->iter_fn(goxel, inputs, state, view_size, inside);
        if (ret == STATE_CANCEL) {
            mesh_set(goxel->image->active_layer->mesh, goxel->tool_mesh_orig);
            goxel_update_meshes(goxel, MESH_LAYERS);
            ret = 0;
        }
        if (ret == STATE_END) ret = 0;
        if (ret == 0)
            goxel->tool_plane = plane_null;

        if ((ret & STATE_MASK) != (state & STATE_MASK))
            ret |= STATE_ENTER;
        else
            ret &= ~STATE_ENTER;

        if (ret == state) break;
        state = ret;
    }

    return ret;
}

void tool_cancel(goxel_t *goxel, int tool, int state)
{
    if (state == 0) return;
    mesh_set(goxel->image->active_layer->mesh, goxel->tool_mesh_orig);
    goxel_update_meshes(goxel, MESH_LAYERS);
    goxel->tool_plane = plane_null;
    goxel->tool_state = 0;
}
