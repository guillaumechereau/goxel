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


int tool_color_picker_iter(tool_t *tool, const painter_t *painter,
                           const float viewport[4])
{
    return 0;
}

static int gui(tool_t *tool)
{
    tool_gui_color();
    return 0;
}

TOOL_REGISTER(TOOL_PICK_COLOR, pick_color, tool_pick_color_t,
             .name = N_("Color Picker"),
             .iter_fn = tool_color_picker_iter,
             .gui_fn = gui,
             .default_shortcut = "C",
             .flags = TOOL_ALLOW_PICK_COLOR,
)
