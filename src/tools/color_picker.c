/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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


int tool_color_picker_iter(const inputs_t *inputs, int state, void **data,
                           const vec2_t *view_size, bool inside)
{
    bool snaped;
    vec3_t pos, normal;
    uvec4b_t color;
    mesh_t *mesh = goxel->layers_mesh;
    const bool pressed = inputs->mouse_down[0];
    goxel_set_help_text(goxel, "Click on a voxel to pick the color");
    snaped = inside && goxel_unproject_on_mesh(goxel, view_size,
                            &inputs->mouse_pos, mesh, &pos, &normal);
    if (!snaped) return 0;
    color = mesh_get_at(mesh, &pos);
    color.a = 255;
    goxel_set_help_text(goxel, "%d %d %d", color.r, color.g, color.b);
    if (pressed) goxel->painter.color = color;
    return 0;
}

TOOL_REGISTER(TOOL_PICK_COLOR, pick_color,
             .iter_fn = tool_color_picker_iter,
             .shortcut = "C",
)
