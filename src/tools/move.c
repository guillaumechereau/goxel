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

enum {
    DRAG_MOVE = 0,
    DRAG_RESIZE,
};

typedef struct {
    tool_t tool;
} tool_move_t;

static void do_move(layer_t *layer, const float mat[4][4])
{
    float m[4][4] = MAT4_IDENTITY;

    if (mat4_equal(mat, mat4_identity)) return;
    // Change referential to the mesh origin.
    // XXX: maybe this should be done in mesh_move directy??
    mat4_itranslate(m, -0.5, -0.5, -0.5);
    mat4_imul(m, mat);
    mat4_itranslate(m, +0.5, +0.5, +0.5);

    if (layer->base_id || layer->image || layer->shape) {
        mat4_mul(mat, layer->mat, layer->mat);
        layer->base_mesh_key = 0;
    } else {
        mesh_move(layer->mesh, m);
        if (!box_is_null(layer->box)) {
            mat4_mul(mat, layer->box, layer->box);
            box_get_bbox(layer->box, layer->box);
        }
    }
}

static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    float transf[4][4];
    bool first;
    layer_t *layer = goxel.image->active_layer;
    if (box_edit(SNAP_LAYER_OUT, goxel.tool_drag_mode, transf, &first)) {
        if (first) image_history_push(goxel.image);
        do_move(layer, transf);
    }
    return 0;
}

static int gui(tool_t *tool)
{
    layer_t *layer;
    float mat[4][4] = MAT4_IDENTITY, v;
    int i;

    layer = goxel.image->active_layer;
    if (layer->shape) {
        tool_gui_drag_mode(&goxel.tool_drag_mode);
    } else {
        goxel.tool_drag_mode = DRAG_MOVE;
    }

    gui_group_begin(NULL);
    i = 0;
    if (gui_input_int("Move X", &i, 0, 0))
        mat4_itranslate(mat, i, 0, 0);
    i = 0;
    if (gui_input_int("Move Y", &i, 0, 0))
        mat4_itranslate(mat, 0, i, 0);
    i = 0;
    if (gui_input_int("Move Z", &i, 0, 0))
        mat4_itranslate(mat, 0, 0, i);
    gui_group_end();
    gui_group_begin(NULL);
    i = 0;
    if (gui_input_int("Rot X", &i, 0, 0))
        mat4_irotate(mat, i * M_PI / 2, 1, 0, 0);
    i = 0;
    if (gui_input_int("Rot Y", &i, 0, 0))
        mat4_irotate(mat, i * M_PI / 2, 0, 1, 0);
    i = 0;
    if (gui_input_int("Rot Z", &i, 0, 0))
        mat4_irotate(mat, i * M_PI / 2, 0, 0, 1);
    gui_group_end();
    if (layer->image && gui_input_int("Scale", &i, 0, 0)) {
        v = pow(2, i);
        mat4_iscale(mat, v, v, v);
    }

    gui_group_begin(NULL);
    if (gui_button("flip X", -1, 0)) mat4_iscale(mat, -1,  1,  1);
    if (gui_button("flip Y", -1, 0)) mat4_iscale(mat,  1, -1,  1);
    if (gui_button("flip Z", -1, 0)) mat4_iscale(mat,  1,  1, -1);
    gui_group_end();

    gui_group_begin(NULL);
    if (gui_button("Scale up",   -1, 0)) mat4_iscale(mat, 2, 2, 2);
    if (gui_button("Scale down", -1, 0)) mat4_iscale(mat, 0.5, 0.5, 0.5);
    gui_group_end();

    if (memcmp(&mat, &mat4_identity, sizeof(mat))) {
        image_history_push(goxel.image);
        do_move(layer, mat);
    }
    return 0;
}

TOOL_REGISTER(TOOL_MOVE, move, tool_move_t,
              .name = "Move",
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_MOVE,
              .default_shortcut = "M",
)
