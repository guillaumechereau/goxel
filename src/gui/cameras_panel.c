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

void gui_cameras_panel(void)
{
    camera_t *cam;
    int i = 0;
    bool current;
    float rot[3][3], e1[3], e2[3], eul[3], pitch, yaw, v;
    char buf[256];

    gui_group_begin(NULL);
    DL_FOREACH(goxel.image->cameras, cam) {
        current = goxel.image->active_camera == cam;
        if (gui_layer_item(i, -1, NULL, &current, cam->name, sizeof(cam->name))) {
            goxel.image->active_camera = cam;
        }
        i++;
    }
    gui_group_end();

    gui_row_begin(0);
    gui_action_button(ACTION_img_new_camera, NULL, 0);
    gui_action_button(ACTION_img_del_camera, NULL, 0);
    gui_action_button(ACTION_img_move_camera_up, NULL, 0);
    gui_action_button(ACTION_img_move_camera_down, NULL, 0);
    gui_row_end();

    if (!goxel.image->cameras) image_add_camera(goxel.image, NULL);

    cam = goxel.image->active_camera;

    if (gui_section_begin(cam->name, GUI_SECTION_COLLAPSABLE)) {
        gui_input_float(_(DISTANCE), &cam->dist, 10.0, 0, 0, NULL);

        gui_checkbox(_(ORTHOGRAPHIC), &cam->ortho, NULL);

        gui_group_begin(NULL);
        gui_row_begin(2);
        gui_action_button(ACTION_view_left, _(LEFT), 1.0);
        gui_action_button(ACTION_view_right, _(RIGHT), 1.0);
        gui_row_end();
        gui_row_begin(2);
        gui_action_button(ACTION_view_front, _(FRONT), 1.0);
        gui_action_button(ACTION_view_top, _(TOP), 1.0);
        gui_row_end();
        gui_action_button(ACTION_view_default, _(RESET), 1.0);
        gui_group_end();

        // Allow to edit euler angles (Should this be a generic widget?)
        gui_group_begin(NULL);
        mat4_to_mat3(cam->mat, rot);
        mat3_to_eul2(rot, EULER_ORDER_XYZ, e1, e2);
        if (fabs(e1[1]) < fabs(e2[1]))
            vec3_copy(e1, eul);
        else
            vec3_copy(e2, eul);

        pitch = round(eul[0] * DR2D);
        if (pitch < 0) pitch += 360;
        v = pitch;
        snprintf(buf, sizeof(buf), "%s: X", _(ANGLE));
        if (gui_input_float(buf, &v, 1, -90, 90, "%.0f")) {
            v = (v - pitch) * DD2R;
            camera_turntable(cam, 0, v);
        }

        yaw = round(eul[2] * DR2D);
        v = yaw;
        if (gui_input_float("Z", &v, 1, -180, 180, "%.0f")) {
            v = (v - yaw) * DD2R;
            camera_turntable(cam, v, 0);
        }
        gui_group_end();
    } gui_section_end();
}

