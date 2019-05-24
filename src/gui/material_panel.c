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

void gui_material_panel(void)
{
    material_t *mat = NULL;
    int i = 0;
    bool is_current;

    gui_group_begin(NULL);
    DL_FOREACH(goxel.image->materials, mat) {
        is_current = goxel.image->active_material == mat;
        if (gui_layer_item(i, -1, NULL, &is_current, mat->name,
                           sizeof(mat->name))) {
            if (is_current) {
                goxel.image->active_material = mat;
            } else {
                goxel.image->active_material = NULL;
            }
        }
        i++;
    }
    gui_group_end();

    gui_action_button("img_new_material", NULL, 0, "");
    gui_same_line();
    gui_action_button("img_del_material", NULL, 0, "");

    mat = goxel.image->active_material;
    if (!mat) return;

    gui_group_begin(NULL);
    gui_input_float("Metallic", &mat->metallic, 0.1, 0, 1, NULL);
    gui_input_float("Roughness", &mat->roughness, 0.1, 0, 1, NULL);
    gui_group_end();
    gui_color_small_f3("Color", mat->base_color);
    gui_input_float("Opacity", &mat->base_color[3], 0.1, 0, 1, NULL);
}
