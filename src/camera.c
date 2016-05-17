/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

void camera_update(camera_t *camera)
{
    float zoom;
    const float size = 16;

    zoom = pow(1.25f, camera->zoom);

    if (camera->move_to_target) {
        camera->move_to_target = !vec3_ilerp_const(
                &camera->ofs, vec3_neg(camera->target), 1.0 / zoom);
    }

    // Update the camera mats
    camera->view_mat = mat4_identity;
    mat4_itranslate(&camera->view_mat, 0, 0, -camera->dist);
    mat4_imul_quat(&camera->view_mat, camera->rot);
    mat4_itranslate(&camera->view_mat,
           camera->ofs.x, camera->ofs.y, camera->ofs.z);

    camera->proj_mat = mat4_ortho(
            -size, +size,
            -size / camera->aspect, +size / camera->aspect,
            0, 1000);
    mat4_iscale(&camera->proj_mat, zoom, zoom, zoom);
}
