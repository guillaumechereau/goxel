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
    int move_mode;
} tool_plane_t;

static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    cursor_t *curs = &goxel.cursor;
    curs->snap_mask = SNAP_MESH;
    curs->snap_offset = 0;

    goxel_set_help_text("Click on the mesh to set plane.");

    if (curs->snaped && (curs->flags & CURSOR_PRESSED)) {
        curs->pos[0] = round(curs->pos[0]);
        curs->pos[1] = round(curs->pos[1]);
        curs->pos[2] = round(curs->pos[2]);
        plane_from_normal(goxel.plane, curs->pos, curs->normal);
        mat4_itranslate(goxel.plane, 0, 0, -1);
        goxel.snap_mask |= SNAP_PLANE;
    }
    return 0;
}

static void mat4_apply_quat(float mat[4][4], const float quat[4])
{
    float rot[3][3];
    int i, j;
    quat_to_mat3(quat, rot);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            mat[i][j] = rot[i][j];

}

/*
 * Algo provided by sariug <ugurcansari93@gmail.com>
 * Note: we could probably make it much faster by checking the mesh blocks
 * first instead of the voxels.
 */
static void cut(bool above)
{
    const uint8_t color[4] = {0, 0, 0, 0};
    int vp[3];
    float p[3], p_vec[3], d;
    mesh_t *mesh = goxel.image->active_layer->mesh;
    mesh_iterator_t iter;
    mesh_accessor_t accessor;

    image_history_push(goxel.image);
    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS | MESH_ITER_SKIP_EMPTY);
    accessor = mesh_get_accessor(mesh);
    while (mesh_iter(&iter, vp)) {
        vec3_set(p, vp[0] + 0.5, vp[1] + 0.5, vp[2] + 0.5);
        vec3_sub(p, goxel.plane[3], p_vec);
        vec3_normalize(p_vec, p_vec);
        d = vec3_dot(p_vec, goxel.plane[2]) * (above ? +1 : -1);
        if (d > 0)
            mesh_set_at(mesh, &accessor, vp, color);
    }
}

static int gui(tool_t *tool_)
{
    int i;
    bool v;
    float rot[3][3];
    float quat[4];

    tool_plane_t *tool = (tool_plane_t*)tool_;
    v = goxel.snap_mask & SNAP_PLANE;
    if (gui_checkbox("Visible", &v, NULL)) {
        set_flag(&goxel.snap_mask, SNAP_PLANE, v);
    }

    gui_combo("Move", &tool->move_mode, (const char*[]) {
              "Relative", "Absolute"}, 2);

    switch (tool->move_mode) {
    case 0:
        gui_group_begin(NULL);
        i = 0;
        if (gui_input_int("Move", &i, 0, 0))
            mat4_itranslate(goxel.plane, 0, 0, -i);
        i = 0;
        if (gui_input_int("Rot X", &i, 0, 0))
            mat4_irotate(goxel.plane, i * M_PI / 2, 1, 0, 0);
        i = 0;
        if (gui_input_int("Rot Y", &i, 0, 0))
            mat4_irotate(goxel.plane, i * M_PI / 2, 0, 1, 0);
        gui_group_end();
        break;

    case 1:
        gui_group_begin("Origin");
        gui_input_float("X", &goxel.plane[3][0], 1.0, 0, 0, NULL);
        gui_input_float("Y", &goxel.plane[3][1], 1.0, 0, 0, NULL);
        gui_input_float("Z", &goxel.plane[3][2], 1.0, 0, 0, NULL);
        gui_group_end();

        mat4_to_mat3(goxel.plane, rot);
        mat3_to_quat(rot, quat);
        if (gui_quat("Rotation", quat)) {
            mat4_apply_quat(goxel.plane, quat);
        }
        break;

    }

    if (gui_button("Cut Above", 1, 0)) {
        cut(true);
    }
    if (gui_button("Cut Below", 1, 0)) {
        cut(false);
    }

    return 0;
}

TOOL_REGISTER(TOOL_SET_PLANE, plane, tool_plane_t,
              .name = "plane",
              .iter_fn = iter,
              .gui_fn = gui,
              .default_shortcut = "P"
)
