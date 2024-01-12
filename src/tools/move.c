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

static void do_move(layer_t *layer, const float mat[4][4],
                    const float origin[3])
{
    float m[4][4] = MAT4_IDENTITY;
    const float default_origin[3] = {0.5, 0.5, 0.5};

    if (origin == NULL) origin = default_origin;

    if (mat4_equal(mat, mat4_identity)) return;
    // Change referential to the volume origin.
    // XXX: maybe this should be done in volume_move directy??
    mat4_itranslate(m, +origin[0], +origin[1], +origin[2]);
    mat4_imul(m, mat);
    mat4_itranslate(m, -origin[0], -origin[1], -origin[2]);

    mat4_mul(mat, layer->mat, layer->mat);
    if (layer->base_id || layer->image || layer->shape) {
        layer->base_volume_key = 0; // Mark it as dirty.
    } else {
        volume_move(layer->volume, m);
        if (!box_is_null(layer->box)) {
            mat4_mul(m, layer->box, layer->box);
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
        do_move(layer, transf, NULL);
    }
    return 0;
}

static void compute_origin(const layer_t *layer, float origin[3])
{
    int bbox[2][3];

    volume_get_bbox(layer->volume, bbox, true);
    origin[0] = (bbox[0][0] + bbox[1][0] - 1) / 2.0;
    origin[1] = (bbox[0][1] + bbox[1][1] - 1) / 2.0;
    origin[2] = (bbox[0][2] + bbox[1][2] - 1) / 2.0;

    // Make sure we always center on a grid point.
    origin[0] = floor(origin[0]) + 0.5;
    origin[1] = floor(origin[1]) + 0.5;
    origin[2] = floor(origin[2]) + 0.5;
}

static int gui(tool_t *tool)
{
    layer_t *layer;
    float mat[4][4] = MAT4_IDENTITY, v;
    float origin[3];
    int i;
    int x, y, z;

    layer = goxel.image->active_layer;
    if (layer->shape) {
        tool_gui_drag_mode(&goxel.tool_drag_mode);
    } else {
        goxel.tool_drag_mode = DRAG_MOVE;
    }

    x = (int)round(layer->mat[3][0]);
    y = (int)round(layer->mat[3][1]);
    z = (int)round(layer->mat[3][2]);

    gui_group_begin("Position");
    if (gui_input_int("X", &x, 0, 0))
        mat[3][0] = x - (int)round(layer->mat[3][0]);
    if (gui_input_int("Y", &y, 0, 0))
        mat[3][1] = y - (int)round(layer->mat[3][1]);
    if (gui_input_int("Z", &z, 0, 0))
        mat[3][2] = z - (int)round(layer->mat[3][2]);
    gui_group_end();

    gui_group_begin("Rotation");

    gui_row_begin(2);
    if (gui_button("-X", 0, 0))
        mat4_irotate(mat, -M_PI / 2, 1, 0, 0);
    if (gui_button("+X", 0, 0))
        mat4_irotate(mat, +M_PI / 2, 1, 0, 0);
    gui_row_end();

    gui_row_begin(2);
    if (gui_button("-Y", 0, 0))
        mat4_irotate(mat, -M_PI / 2, 0, 1, 0);
    if (gui_button("+Y", 0, 0))
        mat4_irotate(mat, +M_PI / 2, 0, 1, 0);
    gui_row_end();

    gui_row_begin(2);
    if (gui_button("-Z", 0, 0))
        mat4_irotate(mat, -M_PI / 2, 0, 0, 1);
    if (gui_button("+Z", 0, 0))
        mat4_irotate(mat, +M_PI / 2, 0, 0, 1);
    gui_row_end();

    gui_group_end();

    if (layer->image && gui_input_int("Scale", &i, 0, 0)) {
        v = pow(2, i);
        mat4_iscale(mat, v, v, v);
    }

    gui_group_begin("Flip");
    gui_row_begin(3);
    if (gui_button("X", -1, 0)) mat4_iscale(mat, -1,  1,  1);
    if (gui_button("Y", -1, 0)) mat4_iscale(mat,  1, -1,  1);
    if (gui_button("Z", -1, 0)) mat4_iscale(mat,  1,  1, -1);
    gui_row_end();
    gui_group_end();

    gui_group_begin("Scale");
    gui_row_begin(3);
    if (gui_button("x2", -1, 0)) mat4_iscale(mat, 2, 2, 2);
    if (gui_button("x0.5", -1, 0)) mat4_iscale(mat, 0.5, 0.5, 0.5);
    gui_row_end();
    gui_group_end();

    if (memcmp(&mat, &mat4_identity, sizeof(mat))) {
        image_history_push(goxel.image);
        compute_origin(layer, origin);
        do_move(layer, mat, origin);
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
