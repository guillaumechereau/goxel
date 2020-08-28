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

#ifndef GUI_TOOLS_COLUMNS_NB
#   define GUI_TOOLS_COLUMNS_NB 4
#endif


// XXX: better replace this by something more automatic.
static void auto_grid(int nb, int i, int col)
{
    if ((i + 1) % col != 0) gui_same_line();
}

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
        {TOOL_EXTRUDE,      ACTION_tool_set_extrude,      ICON_TOOL_EXTRUDE},
        {TOOL_LINE,         ACTION_tool_set_line,         ICON_TOOL_LINE},
    };

    const int nb = ARRAY_SIZE(values);
    int i;
    bool v;
    char label[64];
    const action_t *action = NULL;
    const tool_t *tool;

    gui_group_begin(NULL);
    for (i = 0; i < nb; i++) {
        tool = tool_get(values[i].tool);
        v = goxel.tool->id == values[i].tool;
        sprintf(label, "%s", tool->name);
        action = action_get(values[i].action, true);
        assert(action);
        if (*action->shortcut)
            sprintf(label, "%s (%s)", tool->name, action->shortcut);
        if (gui_selectable_icon(label, &v, values[i].icon)) {
            action_exec(action);
        }
        auto_grid(nb, i, GUI_TOOLS_COLUMNS_NB);
    }
    gui_group_end();

    if (gui_collapsing_header(goxel.tool->name, true))
        tool_gui(goxel.tool);
}

