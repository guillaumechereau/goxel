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
    float v;
    gui_group_begin(NULL);
    v = goxel.rend.settings.occlusion_strength;
    if (gui_input_float("occlusion", &v, 0.1, 0.0, 1.0, NULL)) {
        v = clamp(v, 0, 1); \
        goxel.rend.settings.occlusion_strength = v;
    }
#define MAT_FLOAT(name, min, max) \
    v = goxel.rend.settings.name;  \
    if (gui_input_float(#name, &v, 0.1, min, max, NULL)) { \
        v = clamp(v, min, max); \
        goxel.rend.settings.name = v; \
    }

    MAT_FLOAT(metallic, 0, 1);
    MAT_FLOAT(roughness, 0, 1);

#undef MAT_FLOAT
    gui_group_end();
    gui_color_small_f4("Color", goxel.rend.settings.base_color);
}
