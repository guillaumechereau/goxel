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


int tool_brush_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                    const vec2_t *view_size, bool inside);
int tool_shape_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                    const vec2_t *view_size, bool inside);
int tool_selection_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                        const vec2_t *view_size, bool inside);
int tool_laser_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                    const vec2_t *view_size, bool inside);
int tool_procedural_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                         const vec2_t *view_size, bool inside);
int tool_set_plane_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                        const vec2_t *view_size, bool inside);
int tool_move_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                   const vec2_t *view_size, bool inside);
int tool_color_picker_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                           const vec2_t *view_size, bool inside);

int tool_iter(goxel_t *goxel, int tool, const inputs_t *inputs, int state,
              const vec2_t *view_size, bool inside)
{
    int ret;

    typedef int (*tool_func_t)(goxel_t *goxel, const inputs_t *inputs,
                               int state, const vec2_t *view_size,
                               bool inside);
    static const tool_func_t FUNCS[] = {
        [TOOL_SHAPE]        = tool_shape_iter,
        [TOOL_BRUSH]        = tool_brush_iter,
        [TOOL_LASER]        = tool_laser_iter,
        [TOOL_SET_PLANE]    = tool_set_plane_iter,
        [TOOL_MOVE]         = tool_move_iter,
        [TOOL_PICK_COLOR]   = tool_color_picker_iter,
        [TOOL_SELECTION]    = tool_selection_iter,
        [TOOL_PROCEDURAL]   = tool_procedural_iter,
    };

    assert(tool >= 0 && tool < ARRAY_SIZE(FUNCS));
    assert(FUNCS[tool]);

    while (true) {
        ret = FUNCS[tool](goxel, inputs, state, view_size, inside);
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

#define TOOL_ACTION(t, T, s) \
    static void tool_set_##t(void) { \
        tool_cancel(goxel, goxel->tool, goxel->tool_state); \
        goxel->tool = TOOL_##T; \
    } \
    \
    ACTION_REGISTER(tool_set_##t, \
        .help = "Activate " #t " tool", \
        .cfunc = tool_set_##t, \
        .csig = "v", \
        .shortcut = s, \
    )

TOOL_ACTION(brush, BRUSH, "B")
TOOL_ACTION(shape, SHAPE, "S")
TOOL_ACTION(laser, LASER, "L")
TOOL_ACTION(plane, SET_PLANE, "P")
TOOL_ACTION(move, MOVE, "M")
TOOL_ACTION(pick, PICK_COLOR, "C")
TOOL_ACTION(selection, SELECTION, "R")
TOOL_ACTION(procedural, PROCEDURAL, NULL)
