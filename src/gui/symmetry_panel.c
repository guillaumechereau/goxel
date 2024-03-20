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

void gui_symmetry_panel(void)
{
    int i;
    bool v;
    const char *labels_u[] = {"X", "Y", "Z"};
    const char *labels_l[] = {"x", "y", "z"};
    gui_group_begin("##Axis");
    gui_row_begin(3);
    for (i = 0; i < 3; i++) {
        v = (goxel.painter.symmetry >> i) & 0x1;
        if (gui_selectable(labels_u[i], &v, NULL, 0))
            set_flag(&goxel.painter.symmetry, 1 << i, v);
    }
    gui_row_end();
    gui_group_end();
    for (i = 0; i < 3; i++) {
        gui_input_float(labels_l[i], &goxel.painter.symmetry_origin[i],
                         0.5, -FLT_MAX, +FLT_MAX, "%.1f");
    }
}
