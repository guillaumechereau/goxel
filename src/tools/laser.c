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


typedef struct {
    tool_t tool;
    float  box[4][4];
} tool_laser_t;


static int on_hover(gesture3d_t *gest, void *user)
{
    tool_laser_t *laser = user;
    float v[4];
    float view_mat_inv[4][4] = {};
    camera_t *camera = goxel.image->active_camera;

    // Create the tool box from the camera along the visible ray.
    mat4_set_identity(laser->box);
    mat4_invert(camera->view_mat, view_mat_inv);
    mat4_mul_vec4(view_mat_inv, VEC(1, 0, 0, 0), v);
    vec3_copy(v, laser->box[0]);
    mat4_mul_vec4(view_mat_inv, VEC(0, 1, 0, 0), v);
    vec3_copy(v, laser->box[1]);
    mat4_mul_vec4(view_mat_inv, VEC(0, 0, 1, 0), v);
    vec3_copy(v, laser->box[2]);
    vec3_neg(gest->normal, laser->box[2]);
    vec3_copy(gest->pos, laser->box[3]);
    // Just a large value for the size of the laser box.
    mat4_itranslate(laser->box, 0, 0, -1024);
    mat4_iscale(laser->box, goxel.tool_radius, goxel.tool_radius, 1024);
    render_box(&goxel.rend, laser->box, NULL, EFFECT_WIREFRAME);
    return 0;
}


static int on_drag(gesture3d_t *gest, void *user)
{
    tool_laser_t *laser = (tool_laser_t*)USER_GET(user, 0);
    painter_t painter = *(painter_t*)USER_GET(user, 1);
    volume_t *volume = goxel.image->active_layer->volume;

    on_hover(gest, laser);
    painter.mode = MODE_SUB_CLAMP;
    painter.shape = &shape_cylinder;
    vec4_set(painter.color, 255, 255, 255, 255);

    if (gest->state == GESTURE3D_STATE_BEGIN)
        image_history_push(goxel.image);

    volume_op(volume, &painter, laser->box);
    return 0;
}

static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    tool_laser_t *laser = (tool_laser_t*)tool;

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_HOVER,
        .callback = on_hover,
        .snap_mask = SNAP_CAMERA,
        .user = laser,
    });

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_DRAG,
        .callback = on_drag,
        .snap_mask = SNAP_CAMERA,
        .user = USER_PASS(laser, painter),
    });

    return tool->state;
}

static int gui(tool_t *tool)
{
    tool_gui_radius();
    tool_gui_smoothness();
    return 0;
}

TOOL_REGISTER(TOOL_LASER, laser, tool_laser_t,
              .name = STR_LASER,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT,
              .default_shortcut = "L",
)
