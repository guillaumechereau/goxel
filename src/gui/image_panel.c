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

void gui_image_panel(void)
{
    bool bounded;
    image_t *image = goxel.image;
    int bbox[2][3];
    float (*box)[4][4] = &image->box;

    bounded = !box_is_null(*box);
    if (gui_checkbox("Bounded", &bounded, NULL)) {
        if (bounded) {
            mesh_get_bbox(goxel_get_layers_mesh(), bbox, true);
            if (bbox[0][0] > bbox[1][0]) memset(bbox, 0, sizeof(bbox));
            bbox_from_aabb(*box, bbox);
        } else {
            mat4_copy(mat4_zero, *box);
            goxel.snap_mask |= SNAP_PLANE;
        }
    }
    if (bounded) gui_bbox(*box);
}

