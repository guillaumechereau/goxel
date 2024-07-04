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

static bool layer_is_volume(const layer_t *layer)
{
    return !layer->base_id && !layer->image && !layer->shape;
}

static void move(layer_t *layer, const float mat[4][4])
{
    /*
     * Note: for voxel volume layers, rotation and scale are only
     * applied to the voxels, without modifying the layer transformation
     * matrix.  For translation we modify the matrix (so that the origin
     * is moved) but we also modify the voxels because we want all the layer
     * volume to stay aligned.
     */

    float m[4][4] = MAT4_IDENTITY;
    float origin[3];

    if (mat4_equal(mat, mat4_identity)) return;

    // Make sure we always center on a grid point.
    vec3_copy(layer->mat[3], origin);
    origin[0] = floor(layer->mat[3][0]) + 0.5;
    origin[1] = floor(layer->mat[3][1]) + 0.5;
    origin[2] = floor(layer->mat[3][2]) + 0.5;

    // Change referential to the volume origin.
    // XXX: maybe this should be done in volume_move directy??
    mat4_itranslate(m, +origin[0], +origin[1], +origin[2]);
    mat4_imul(m, mat);
    mat4_itranslate(m, -origin[0], -origin[1], -origin[2]);

    if (!layer_is_volume(layer)) {
        mat4_mul(m, layer->mat, layer->mat);
        layer->base_volume_key = 0; // Mark it as dirty.
    } else {
        // Only apply translation to the layer->mat.
        vec3_add(layer->mat[3], mat[3], layer->mat[3]);
        volume_move(layer->volume, m);
        if (!box_is_null(layer->box)) {
            mat4_mul(m, layer->box, layer->box);
            box_get_bbox(layer->box, layer->box);
        }
    }
}

static void normalize_box(const float box[4][4], float out[4][4])
{
    mat4_copy(box, out);
    float vertices[8][3];
    box_get_vertices(box, vertices);
    bbox_from_npoints(out, 8, vertices);
}

static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    float transf[4][4];
    float origin_box[4][4] = MAT4_IDENTITY;
    uint8_t color[4] = {255, 0, 0, 255};
    float box[4][4];
    int drag_mode = DRAG_MOVE;
    int box_edit_state;

    layer_t *layer = goxel.image->active_layer;

    if (layer->shape) drag_mode = DRAG_RESIZE;

    volume_get_box(goxel.image->active_layer->volume, true, box);
    // Fix problem with shape layer box.
    if (goxel.image->active_layer->shape) {
        normalize_box(goxel.image->active_layer->mat, box);
    }

    box_edit_state = box_edit(box, drag_mode, transf);
    if (box_edit_state) {
        if (box_edit_state == GESTURE3D_STATE_BEGIN) {
            image_history_push(goxel.image);
        }
        move(layer, transf);
    }

    // Render the origin point.
    if (layer_is_volume(layer)) {
        vec3_copy(layer->mat[3], origin_box[3]);
        mat4_iscale(origin_box, 0.1, 0.1, 0.1);
        render_box(&goxel.rend, origin_box, color,
                EFFECT_NO_DEPTH_TEST | EFFECT_NO_SHADING);
    }
    return 0;
}

static void center_origin(layer_t *layer)
{
    int bbox[2][3];
    float pos[3];

    volume_get_bbox(layer->volume, bbox, true);
    pos[0] = round((bbox[0][0] + bbox[1][0] - 1) / 2.0);
    pos[1] = round((bbox[0][1] + bbox[1][1] - 1) / 2.0);
    pos[2] = round((bbox[0][2] + bbox[1][2] - 1) / 2.0);
    vec3_copy(pos, layer->mat[3]);
}


static int gui(tool_t *tool)
{
    layer_t *layer;
    float mat[4][4] = MAT4_IDENTITY;
    int x, y, z;
    float origin[3];
    bool only_origin = false;

    layer = goxel.image->active_layer;
    x = (int)round(layer->mat[3][0]);
    y = (int)round(layer->mat[3][1]);
    z = (int)round(layer->mat[3][2]);

    gui_group_begin(_(POSITION));
    if (gui_input_int("X", &x, 0, 0))
        mat[3][0] = x - (int)round(layer->mat[3][0]);
    if (gui_input_int("Y", &y, 0, 0))
        mat[3][1] = y - (int)round(layer->mat[3][1]);
    if (gui_input_int("Z", &z, 0, 0))
        mat[3][2] = z - (int)round(layer->mat[3][2]);
    gui_group_end();

    gui_group_begin(_(ROTATION));

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

    gui_group_begin(_(FLIP));
    gui_row_begin(3);
    if (gui_button("X", -1, 0)) mat4_iscale(mat, -1,  1,  1);
    if (gui_button("Y", -1, 0)) mat4_iscale(mat,  1, -1,  1);
    if (gui_button("Z", -1, 0)) mat4_iscale(mat,  1,  1, -1);
    gui_row_end();
    gui_group_end();

    gui_group_begin(_(SCALE));
    gui_row_begin(3);
    if (gui_button("x2", -1, 0)) mat4_iscale(mat, 2, 2, 2);
    if (gui_button("x0.5", -1, 0)) mat4_iscale(mat, 0.5, 0.5, 0.5);
    gui_row_end();
    gui_group_end();

    if (layer_is_volume(layer)) {
        if (gui_section_begin(_(ORIGIN), GUI_SECTION_COLLAPSABLE_CLOSED)) {
            vec3_copy(layer->mat[3], origin);
            if (gui_input_float("X", &origin[0], 0.5, 0, 0, "%.1f")) {
                mat[3][0] = origin[0] - layer->mat[3][0];
                only_origin = true;
            }
            if (gui_input_float("Y", &origin[1], 0.5, 0, 0, "%.1f")) {
                mat[3][1] = origin[1] - layer->mat[3][1];
                only_origin = true;
            }
            if (gui_input_float("Z", &origin[2], 0.5, 0, 0, "%.1f")) {
                mat[3][2] = origin[2] - layer->mat[3][2];
                only_origin = true;
            }
            if (gui_button(_(RECENTER), -1, 0)) {
                center_origin(layer);
            }
        } gui_section_end();
    }

    if (memcmp(&mat, &mat4_identity, sizeof(mat))) {
        image_history_push(goxel.image);
        if (only_origin) {
            vec3_add(layer->mat[3], mat[3], layer->mat[3]);
        } else {
            move(layer, mat);
        }
    }

    return 0;
}

TOOL_REGISTER(TOOL_MOVE, move, tool_move_t,
              .name = STR_MOVE,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_MOVE,
              .default_shortcut = "M",
)
