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
} tool_move_t;


static int iter(tool_t *tool, const vec4_t *view)
{
    return 0;
}

static int gui(tool_t *tool)
{
    layer_t *layer;
    mat4_t mat = mat4_identity;
    int i;
    double v;

    layer = goxel->image->active_layer;
    gui_group_begin(NULL);
    i = 0;
    if (gui_input_int("Move X", &i, 0, 0))
        mat4_itranslate(&mat, i, 0, 0);
    i = 0;
    if (gui_input_int("Move Y", &i, 0, 0))
        mat4_itranslate(&mat, 0, i, 0);
    i = 0;
    if (gui_input_int("Move Z", &i, 0, 0))
        mat4_itranslate(&mat, 0, 0, i);
    gui_group_end();
    gui_group_begin(NULL);
    i = 0;
    if (gui_input_int("Rot X", &i, 0, 0))
        mat4_irotate(&mat, i * M_PI / 2, 1, 0, 0);
    i = 0;
    if (gui_input_int("Rot Y", &i, 0, 0))
        mat4_irotate(&mat, i * M_PI / 2, 0, 1, 0);
    i = 0;
    if (gui_input_int("Rot Z", &i, 0, 0))
        mat4_irotate(&mat, i * M_PI / 2, 0, 0, 1);
    gui_group_end();
    if (layer->image && gui_input_int("Scale", &i, 0, 0)) {
        v = pow(2, i);
        mat4_iscale(&mat, v, v, v);
    }

    gui_group_begin(NULL);
    if (gui_button("flip X", -1)) mat4_iscale(&mat, -1,  1,  1);
    if (gui_button("flip Y", -1)) mat4_iscale(&mat,  1, -1,  1);
    if (gui_button("flip Z", -1)) mat4_iscale(&mat,  1,  1, -1);
    gui_group_end();

    if (memcmp(&mat, &mat4_identity, sizeof(mat))) {
        image_history_push(goxel->image);
        mesh_move(layer->mesh, &mat);
        layer->mat = mat4_mul(mat, layer->mat);
        goxel_update_meshes(goxel, -1);
    }
    return 0;
}

TOOL_REGISTER(TOOL_MOVE, move, tool_move_t,
              .iter_fn = iter,
              .gui_fn = gui,
              .shortcut = "M",
)
