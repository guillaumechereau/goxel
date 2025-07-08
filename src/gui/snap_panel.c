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
    snap_button(_("Volume"), SNAP_VOLUME);
    snap_button(_("Plane"), SNAP_PLANE);
    gui_row_end();
    if (!box_is_null(goxel.image->selection_box)) {
        snap_button(_("Selection (In)"), SNAP_SELECTION_IN);
        snap_button(_("Selection (Out)"), SNAP_SELECTION_OUT);
    }
    if (!box_is_null(goxel.image->box)) {
        snap_button(_("Box"), SNAP_IMAGE_BOX);
    }
    gui_group_end();

    gui_group_begin(_("Snap Units"));
    if (gui_checkbox(_("Use Brush Size"), &goxel.use_brush_size, NULL)) {
        if (goxel.use_brush_size) {
            goxel.snap_units[0] = goxel.tool_radius * 2;
            goxel.snap_units[1] = goxel.tool_radius * 2;
            goxel.snap_units[2] = goxel.tool_radius * 2;
        }
    }
    
    // Update snap units to match brush size if enabled
    if (goxel.use_brush_size) {
        goxel.snap_units[0] = goxel.tool_radius * 2;
        goxel.snap_units[1] = goxel.tool_radius * 2;
        goxel.snap_units[2] = goxel.tool_radius * 2;
    }
    
    gui_row_begin(3);
    gui_enabled_begin(!goxel.use_brush_size);
    v = goxel.snap_units[0];
    if (gui_input_float("X", &v, 0.1, 0.1, 16, "%.1f") && !goxel.use_brush_size)
        goxel.snap_units[0] = clamp(v, 0.1, 16);
    
    v = goxel.snap_units[1];
    if (gui_input_float("Y", &v, 0.1, 0.1, 16, "%.1f") && !goxel.use_brush_size)
        goxel.snap_units[1] = clamp(v, 0.1, 16);
    
    v = goxel.snap_units[2];
    if (gui_input_float("Z", &v, 0.1, 0.1, 16, "%.1f") && !goxel.use_brush_size)
        goxel.snap_units[2] = clamp(v, 0.1, 16);
    
    gui_enabled_end();
    gui_row_end();
    gui_group_end();

    gui_group_begin(_("Snap Offsets"));
    gui_row_begin(3);
    v = goxel.snap_offsets[0];
    if (gui_input_float("X", &v, 0.1, -1, +1, "%.1f"))
        goxel.snap_offsets[0] = clamp(v, -1, +1);
    v = goxel.snap_offsets[1];
    if (gui_input_float("Y", &v, 0.1, -1, +1, "%.1f"))
        goxel.snap_offsets[1] = clamp(v, -1, +1);
    v = goxel.snap_offsets[2];
    if (gui_input_float("Z", &v, 0.1, -1, +1, "%.1f"))
        goxel.snap_offsets[2] = clamp(v, -1, +1);
    gui_row_end();
    gui_group_end();
}
