/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

void gui_edit_panel(void)
{
    image_t *img = goxel.image;

    gui_group_begin(NULL);
    gui_enabled_begin(!box_is_null(img->selection_box));
    gui_action_button(ACTION_fill_selection_box, _(FILL), 1.0);
    gui_group_end();
    gui_enabled_end();

    gui_group_begin(NULL);
    gui_enabled_begin(!volume_is_empty(goxel.image->selection_mask));
    gui_action_button(ACTION_layer_clear, _(CLEAR), 1.0);
    gui_action_button(ACTION_paint_selection, _(PAINT), 1.0);
    gui_action_button(ACTION_cut_as_new_layer, "Cut as new layer", 1.0);
    gui_enabled_end();
    gui_group_end();
}
