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

#ifndef GUI_CUSTOM_TOPBAR

static int gui_mode_select(void)
{

    gui_choice_begin("Mode", &goxel.painter.mode, false);
    gui_choice("Add", MODE_OVER, ICON_MODE_ADD);
    gui_choice("Sub", MODE_SUB, ICON_MODE_SUB);
    gui_choice("Paint", MODE_PAINT, ICON_MODE_PAINT);
    gui_choice_end();
    return 0;
}

void gui_top_bar(void)
{
    gui_action_button(ACTION_undo, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_redo, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_layer_clear, NULL, 0);
    gui_same_line();
    gui_mode_select();
    gui_same_line();
    gui_color("##color", goxel.painter.color);
}

#endif // GUI_CUSTOM_TOPBAR
