/* Goxel 3D voxels editor
 *
 * copyright (c) 2017-2022 Guillaume Chereau <guillaume@noctua-software.com>
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
    bool custom_rotation;
} tool_plane_t;


static int on_click(gesture3d_t *gest)
{
    float pos[3];
    pos[0] = round(gest->pos[0]);
    pos[1] = round(gest->pos[1]);
    pos[2] = round(gest->pos[2]);
    plane_from_normal(goxel.plane, pos, gest->normal);
    mat4_itranslate(goxel.plane, 0, 0, -1);
    return 0;
}

static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    goxel_add_hint(HINT_LARGE, GLYPH_MOUSE_LMB, "Set the Plane");
    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_CLICK,
        .snap_mask = SNAP_VOLUME,
        .callback = on_click,
        .user = tool,
    });

    return 0;
}

/*
 * Algo provided by sariug <ugurcansari93@gmail.com>
 * Note: we could probably make it much faster by checking the volume blocks
 * first instead of the voxels.
 */
static void cut(bool above)
{
    const uint8_t color[4] = {0, 0, 0, 0};
    int vp[3];
    float p[3], p_vec[3], d;
    volume_t *volume = goxel.image->active_layer->volume;
    volume_iterator_t iter;
    volume_accessor_t accessor;

    iter = volume_get_iterator(volume,
            VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    accessor = volume_get_accessor(volume);
    while (volume_iter(&iter, vp)) {
        vec3_set(p, vp[0] + 0.5, vp[1] + 0.5, vp[2] + 0.5);
        vec3_sub(p, goxel.plane[3], p_vec);
        vec3_normalize(p_vec, p_vec);
        d = vec3_dot(p_vec, goxel.plane[2]) * (above ? +1 : -1);
        if (d > 0)
            volume_set_at(volume, &accessor, vp, color);
    }
    image_history_push(goxel.image);
}

static int gui(tool_t *tool_)
{
    bool v;
    int x, y, z;

    tool_plane_t *tool = (tool_plane_t*)tool_;
    v = goxel.snap_mask & SNAP_PLANE;
    if (gui_checkbox(_("Visible"), &v, NULL)) {
        set_flag(&goxel.snap_mask, SNAP_PLANE, v);
    }

    x = (int)round(goxel.plane[3][0]);
    y = (int)round(goxel.plane[3][1]);
    z = (int)round(goxel.plane[3][2]);
    gui_group_begin(_("Origin"));
    if (gui_input_int("X", &x, 0, 0)) goxel.plane[3][0] = x;
    if (gui_input_int("Y", &y, 0, 0)) goxel.plane[3][1] = y;
    if (gui_input_int("Z", &z, 0, 0)) goxel.plane[3][2] = z;
    gui_group_end();
    gui_group_begin(_("Rotation"));
    gui_checkbox("Custom", &tool->custom_rotation, NULL);
    if (tool->custom_rotation)
        gui_rotation_mat4(goxel.plane);
    else
        gui_rotation_mat4_axis(goxel.plane);
    gui_group_end();

    if (gui_button("Cut Above", 1, 0)) {
        cut(true);
    }
    if (gui_button("Cut Below", 1, 0)) {
        cut(false);
    }

    return 0;
}

TOOL_REGISTER(TOOL_SET_PLANE, plane, tool_plane_t,
              .name = N_("Plane"),
              .iter_fn = iter,
              .gui_fn = gui,
              .default_shortcut = "P"
)
