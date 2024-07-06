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

static bool snap_button(const char *label, int s)
{
    bool v = goxel.snap_mask & s;
    if (gui_selectable(label, &v, NULL, -1)) {
        set_flag(&goxel.snap_mask, s, v);
        return true;
    }
    return false;
}

void gui_snap_panel(void)
{
    float v;
    gui_group_begin(NULL);
    gui_row_begin(2);
    snap_button(_(VOLUME), SNAP_VOLUME);
    snap_button(_(PLANE), SNAP_PLANE);
    gui_row_end();
    if (!box_is_null(goxel.image->selection_box)) {
        snap_button(_(SELECTION_IN), SNAP_SELECTION_IN);
        snap_button(_(SELECTION_OUT), SNAP_SELECTION_OUT);
    }
    if (!box_is_null(goxel.image->box)) {
        snap_button(_(BOX), SNAP_IMAGE_BOX);
    }
    gui_group_end();

    v = goxel.snap_offset;
    if (gui_input_float(_(OFFSET), &v, 0.1, -1, +1, "%.1f"))
        goxel.snap_offset = clamp(v, -1, +1);
}
