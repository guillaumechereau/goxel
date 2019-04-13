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

// XXX: better replace this by something more automatic.
static void auto_grid(int nb, int i, int col)
{
    if ((i + 1) % col != 0) gui_same_line();
}

static int gui_mode_select(void)
{
    int i;
    bool v;
    struct {
        int        mode;
        const char *name;
        int        icon;
    } values[] = {
        {MODE_OVER,   "Add",  ICON_MODE_ADD},
        {MODE_SUB,    "Sub",  ICON_MODE_SUB},
        {MODE_PAINT,  "Paint", ICON_MODE_PAINT},
    };
    gui_group_begin(NULL);
    for (i = 0; i < (int)ARRAY_SIZE(values); i++) {
        v = goxel.painter.mode == values[i].mode;
        if (gui_selectable_icon(values[i].name, &v, values[i].icon)) {
            goxel.painter.mode = values[i].mode;
        }
        auto_grid(ARRAY_SIZE(values), i, 4);
    }
    gui_group_end();
    return 0;
}

void gui_top_bar(void)
{
    gui_action_button("undo", NULL, 0, "");
    gui_same_line();
    gui_action_button("redo", NULL, 0, "");
    gui_same_line();
    gui_action_button("layer_clear", NULL, 0, "");
    gui_same_line();
    gui_mode_select();
    gui_same_line();
    gui_color("##color", goxel.painter.color);
}

