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
} tool_pick_color_t;


int tool_color_picker_iter(tool_t *tool, const vec4_t *view)
{
    uvec4b_t color;
    mesh_t *mesh = goxel->layers_mesh;
    cursor_t *curs = &goxel->cursor;
    curs->snap_mask = SNAP_MESH;
    curs->snap_offset = -0.5;

    goxel_set_help_text(goxel, "Click on a voxel to pick the color");
    if (!curs->snaped) return 0;
    color = mesh_get_at(mesh, &curs->pos);
    color.a = 255;
    goxel_set_help_text(goxel, "%d %d %d", color.r, color.g, color.b);
    if (curs->flags & CURSOR_PRESSED) goxel->painter.color = color;
    return 0;
}

static int gui(tool_t *tool)
{
    tool_gui_color();
    return 0;
}

TOOL_REGISTER(TOOL_PICK_COLOR, pick_color, tool_pick_color_t,
             .iter_fn = tool_color_picker_iter,
             .gui_fn = gui,
             .shortcut = "C",
)
