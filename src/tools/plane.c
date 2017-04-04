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

static int iter(const inputs_t *inputs, int state, void **data,
                const vec2_t *view_size, bool inside)
{
    bool snaped;
    vec3_t pos = vec3_zero, normal = vec3_zero;
    mesh_t *mesh = goxel->layers_mesh;
    const bool pressed = inputs->mouse_down[0];
    goxel_set_help_text(goxel, "Click on the mesh to set plane.");
    snaped = inside && goxel_unproject_on_mesh(goxel, view_size,
                            &inputs->mouse_pos, mesh, &pos, &normal);
    if (snaped && pressed) {
        vec3_iadd(&pos, normal);
        goxel->plane = plane_from_normal(pos, normal);
    }
    return 0;
}

TOOL_REGISTER(TOOL_SET_PLANE, plane,
              .iter_fn = iter,
              .shortcut = "P"
)
