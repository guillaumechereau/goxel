/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

void gui_tools_panel(void)
{
    // XXX: cleanup this.
    const struct {
        int         tool;
        int         action;
        int         icon;
    } values[] = {
        {TOOL_BRUSH,        ACTION_tool_set_brush,        ICON_TOOL_BRUSH},
        {TOOL_SHAPE,        ACTION_tool_set_shape,        ICON_TOOL_SHAPE},
        {TOOL_LASER,        ACTION_tool_set_laser,        ICON_TOOL_LASER},
        {TOOL_SET_PLANE,    ACTION_tool_set_plane,        ICON_TOOL_PLANE},
        {TOOL_MOVE,         ACTION_tool_set_move,         ICON_TOOL_MOVE},
        {TOOL_PICK_COLOR,   ACTION_tool_set_pick_color,   ICON_TOOL_PICK},
        {TOOL_SELECTION,    ACTION_tool_set_selection,    ICON_TOOL_SELECTION},
        {TOOL_FUZZY_SELECT, ACTION_tool_set_fuzzy_select, ICON_TOOL_FUZZY_SELECT},
        {TOOL_RECT_SELECT,  ACTION_tool_set_rect_select,  ICON_TOOL_RECT_SELECTION},
        {TOOL_EXTRUDE,      ACTION_tool_set_extrude,      ICON_TOOL_EXTRUDE},
        {TOOL_LINE,         ACTION_tool_set_line,         ICON_TOOL_LINE},
    };

    const int nb = ARRAY_SIZE(values);
    int i;
    const action_t *action = NULL;
    const tool_t *tool;
    int current = 0;
    gui_icon_info_t grid[64] = {};

    for (i = 0; i < nb; i++) {
        tool = tool_get(values[i].tool);
        action = action_get(values[i].action, true);
        assert(action);
        if (goxel.tool->id == values[i].tool) current = i;
        grid[i] = (gui_icon_info_t) {
            .label = tool->name,
            .sublabel = action->shortcut,
            .icon = values[i].icon,
        };
    }

    gui_section_begin("##Tools", false);
    if (gui_icons_grid(nb, grid, &current)) {
        action = action_get(values[current].action, true);
        action_exec(action);
    }
    gui_section_end();

    if (gui_collapsing_header(goxel.tool->name, true))
        tool_gui(goxel.tool);
}

