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

static const uint8_t ORIGIN_COLOR[4] = { 255, 0, 0, 255 };

typedef struct {
    tool_t tool;

    uint64_t start_mask_key;
    volume_t *start_volume;
    volume_t *start_selection;
    float box[4][4];
    float start_box[4][4];
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

// Compute transformation betwen two matrices.
static void get_transf(const float src[4][4], const float dst[4][4],
                       float out[4][4])
{
    mat4_invert(src, out);
    mat4_mul(dst, out, out);
}

static void normalize_box(const float box[4][4], float out[4][4])
{
    mat4_copy(box, out);
    float vertices[8][3];
    box_get_vertices(box, vertices);
    bbox_from_npoints(out, 8, vertices);
}

static int iter_selection(tool_move_t *tool, const painter_t *painter,
                          const float viewport[4])
{
    float transf[4][4];
    float transf_tot[4][4];
    int box_edit_state;
    image_t *img = goxel.image;
    layer_t *layer = img->active_layer;
    volume_t *mask = img->selection_mask;
    volume_t *tmp;

    if (tool->start_mask_key != volume_get_key(mask)) {
        volume_get_box(mask, true, tool->box);
        tool->start_mask_key = volume_get_key(mask);
    }

    box_edit_state = box_edit(tool->box, GIZMO_TRANSLATION, transf);
    if (!box_edit_state) return 0;

    if (box_edit_state == GESTURE3D_STATE_BEGIN) {
        assert(!tool->start_volume && !tool->start_selection);
        mat4_copy(tool->box, tool->start_box);
        tool->start_volume = volume_copy(layer->volume);
        volume_merge(tool->start_volume, mask, MODE_SUB, NULL);
        tool->start_selection = volume_copy(layer->volume);
        volume_merge(tool->start_selection, mask, MODE_INTERSECT, NULL);
    }

    mat4_mul(transf, tool->box, tool->box);
    get_transf(tool->start_box, tool->box, transf_tot);
    tmp = volume_copy(tool->start_selection);
    volume_move(tmp, transf_tot);
    volume_set(layer->volume, tool->start_volume);
    volume_merge(layer->volume, tmp, MODE_OVER, NULL);
    volume_delete(tmp);

    if (box_edit_state == GESTURE3D_STATE_END) {
        volume_delete(tool->start_volume);
        volume_delete(tool->start_selection);
        tool->start_volume = NULL;
        tool->start_selection = NULL;
        tool->box[3][3] = 0;
        volume_move(mask, transf_tot);
        image_history_push(goxel.image);
    }

    return 0;
}

static int iter_shape_layer(
        tool_move_t *tool, const painter_t *painter, const float viewport[4])
{
    float transf[4][4];
    float transf_tot[4][4];
    int box_edit_state;
    image_t *img = goxel.image;
    layer_t *layer = img->active_layer;

    normalize_box(layer->mat, tool->box); // Needed?
    box_edit_state = box_edit(
            tool->box, GIZMO_TRANSLATION | GIZMO_GROW, transf);
    if (box_edit_state == 0) return 0;

    if (box_edit_state == GESTURE3D_STATE_BEGIN) {
        mat4_copy(tool->box, tool->start_box);
    }

    mat4_mul(transf, tool->box, tool->box);
    get_transf(tool->start_box, tool->box, transf_tot);

    mat4_mul(transf_tot, tool->start_box, layer->mat);
    layer->base_volume_key = 0; // Mark it as dirty.

    if (box_edit_state == GESTURE3D_STATE_END) {
        image_history_push(goxel.image);
        tool->box[3][3] = 0;
    }

    return 0;
}

static int iter_volume_layer(
        tool_move_t *tool, const painter_t *painter, const float viewport[4])
{
    float transf[4][4];
    float origin_box[4][4] = MAT4_IDENTITY;
    float box[4][4];
    int box_edit_state;
    image_t *img = goxel.image;
    layer_t *layer = img->active_layer;

    volume_get_box(goxel.image->active_layer->volume, true, box);

    box_edit_state = box_edit(box, GIZMO_TRANSLATION, transf);
    if (box_edit_state) {
        move(layer, transf);
        if (box_edit_state == GESTURE3D_STATE_END) {
            image_history_push(goxel.image);
        }
    }

    // Render the origin point.
    if (layer_is_volume(layer)) {
        vec3_copy(layer->mat[3], origin_box[3]);
        mat4_iscale(origin_box, 0.1, 0.1, 0.1);
        render_box(&goxel.rend, origin_box, ORIGIN_COLOR,
                   EFFECT_NO_DEPTH_TEST | EFFECT_NO_SHADING);
    }

    return 0;
}

static int iter(tool_t *tool_,
                const painter_t *painter,
                const float viewport[4])
{
    tool_move_t *tool = (void *)tool_;
    image_t *img      = goxel.image;
    layer_t *layer    = img->active_layer;

    if (layer->shape) {
        return iter_shape_layer(tool, painter, viewport);
    }

    if (img->selection_mask && !volume_is_empty(img->selection_mask)) {
        return iter_selection(tool, painter, viewport);
    }

    return iter_volume_layer(tool, painter, viewport);
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

// Could move into gui.c ?
static bool input_tranform(const float mat[4][4], float transf[4][4])
{
    int x, y, z;

    mat4_set_identity(transf);

    x = (int)round(mat[3][0]);
    y = (int)round(mat[3][1]);
    z = (int)round(mat[3][2]);

    gui_group_begin(NULL);

    gui_group_begin(_(POSITION));
    if (gui_input_int("X", &x, 0, 0))
        transf[3][0] = x - (int)round(mat[3][0]);
    if (gui_input_int("Y", &y, 0, 0))
        transf[3][1] = y - (int)round(mat[3][1]);
    if (gui_input_int("Z", &z, 0, 0))
        transf[3][2] = z - (int)round(mat[3][2]);
    gui_group_end();

    gui_group_begin(_(ROTATION));
    gui_row_begin(2);
    if (gui_button("-X", 0, 0))
        mat4_irotate(transf, -M_PI / 2, 1, 0, 0);
    if (gui_button("+X", 0, 0))
        mat4_irotate(transf, +M_PI / 2, 1, 0, 0);
    gui_row_end();
    gui_row_begin(2);
    if (gui_button("-Y", 0, 0))
        mat4_irotate(transf, -M_PI / 2, 0, 1, 0);
    if (gui_button("+Y", 0, 0))
        mat4_irotate(transf, +M_PI / 2, 0, 1, 0);
    gui_row_end();
    gui_row_begin(2);
    if (gui_button("-Z", 0, 0))
        mat4_irotate(transf, -M_PI / 2, 0, 0, 1);
    if (gui_button("+Z", 0, 0))
        mat4_irotate(transf, +M_PI / 2, 0, 0, 1);
    gui_row_end();
    gui_group_end();

    gui_group_begin(_(FLIP));
    gui_row_begin(3);
    if (gui_button("X", -1, 0)) mat4_iscale(transf, -1,  1,  1);
    if (gui_button("Y", -1, 0)) mat4_iscale(transf,  1, -1,  1);
    if (gui_button("Z", -1, 0)) mat4_iscale(transf,  1,  1, -1);
    gui_row_end();
    gui_group_end();

    gui_group_begin(_(SCALE));
    gui_row_begin(3);
    if (gui_button("x2", -1, 0)) mat4_iscale(transf, 2, 2, 2);
    if (gui_button("x0.5", -1, 0)) mat4_iscale(transf, 0.5, 0.5, 0.5);
    gui_row_end();
    gui_group_end();

    gui_group_end();
    return !mat4_is_identity(transf);
}

static void gui_origin(layer_t *layer)
{
    // XXX: to cleanup.
    float mat[4][4] = MAT4_IDENTITY;
    float origin[3];

    if (gui_section_begin(_(ORIGIN), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        vec3_copy(layer->mat[3], origin);
        gui_group_begin(NULL);
        if (gui_input_float("X", &origin[0], 0.5, 0, 0, "%.1f")) {
            mat[3][0] = origin[0] - layer->mat[3][0];
        }
        if (gui_input_float("Y", &origin[1], 0.5, 0, 0, "%.1f")) {
            mat[3][1] = origin[1] - layer->mat[3][1];
        }
        if (gui_input_float("Z", &origin[2], 0.5, 0, 0, "%.1f")) {
            mat[3][2] = origin[2] - layer->mat[3][2];
        }
        if (gui_button(_(RECENTER), -1, 0)) {
            center_origin(layer);
        }
        gui_group_end();
        if (gui_is_item_activated()) image_history_push(goxel.image);
        if (memcmp(&mat, &mat4_identity, sizeof(mat))) {
            vec3_add(layer->mat[3], mat[3], layer->mat[3]);
        }
    } gui_section_end();

}

static int gui(tool_t *tool)
{
    layer_t *layer;
    float mat[4][4] = MAT4_IDENTITY;

    layer = goxel.image->active_layer;
    if (input_tranform(layer->mat, mat)) {
        move(layer, mat);
    }
    if (gui_is_item_deactivated()) {
        image_history_push(goxel.image);
    }

    if (layer_is_volume(layer)) {
        gui_origin(layer);
    }

    return 0;
}

TOOL_REGISTER(TOOL_MOVE, move, tool_move_t,
              .name = STR_MOVE,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_MOVE | TOOL_SHOW_MASK,
              .default_shortcut = "M",
)
