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

static void recenter(int mode)
{
    int bbox[2][3];
    int i;
    float v;

    if (mode == 2 && box_is_null(goxel.image->box)) mode = 1;
    if (mode == 1 && box_is_null(goxel.image->active_layer->box)) mode = 0;

    switch (mode) {
    case 0: // Volume.
        volume_get_bbox(goxel.image->active_layer->volume, bbox, true);
        break;
    case 1: // Layer.
        bbox_to_aabb(goxel.image->active_layer->box, bbox);
        break;
    case 2: // Image.
        bbox_to_aabb(goxel.image->box, bbox);
        break;
    }

    for (i = 0; i < 3; i++) {
        if (!(goxel.painter.symmetry & (1 << i))) continue;
        v = round(bbox[0][i] + bbox[1][i] - 1) / 2.0;
        goxel.painter.symmetry_origin[i] = v;
    }
}

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
        gui_input_float(labels_l[i], &goxel.painter.symmetry_origin[i], 0.5, 0,
                        0, "%.1f");
    }

    if (gui_section_begin(_("Recenter"), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        if (gui_button(_("Volume"), -1, 0)) recenter(0);
        if (gui_button(_("Layer"), -1, 0)) recenter(1);
        if (gui_button(_("Image"), -1, 0)) recenter(2);
    } gui_section_end();
}
