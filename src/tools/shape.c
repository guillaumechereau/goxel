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
    float  start_pos[3];
    volume_t *volume_orig;
    bool   planar; // Stay on the original plane.
} tool_shape_t;


static void get_box(const float p0[3], const float p1[3], const float n[3],
                    float out[4][4])
{
    float box[4][4];

    if (p1 == NULL) {
        bbox_from_extents(box, p0, 0, 0, 0);
        box_swap_axis(box, 2, 0, 1, box);
        mat4_copy(box, out);
        return;
    }
    bbox_from_points(box, p0, p1);
    bbox_grow(box, 0.5, 0.5, 0.5, box);
    mat4_copy(box, out);
}

static int on_hover(gesture3d_t *gest)
{
    float box[4][4];
    uint8_t box_color[4] = {255, 255, 0, 255};

    goxel_add_hint(HINT_LARGE, GLYPH_MOUSE_LMB, "Draw Shape");
    get_box(gest->pos, gest->pos, gest->normal, box);
    render_box(&goxel.rend, box, box_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_drag(gesture3d_t *gest)
{
    tool_shape_t *shape = USER_GET(gest->user, 0);
    const painter_t *painter = USER_GET(gest->user, 1);
    volume_t *layer_volume = goxel.image->active_layer->volume;
    float box[4][4], pos[3];

    if (gest->state == GESTURE3D_STATE_BEGIN) {
        volume_set(shape->volume_orig, layer_volume);
        vec3_copy(gest->pos, shape->start_pos);
        if (shape->planar) {
            vec3_addk(gest->pos, gest->normal, -gest->snap_offset, pos);
            gest->snap_mask = SNAP_SHAPE_PLANE;
            plane_from_normal(gest->snap_shape, pos, gest->normal);
        }
    }

    goxel_add_hint(HINT_LARGE, GLYPH_MOUSE_LMB, "Draw Shape");
    get_box(shape->start_pos, gest->pos, gest->normal, box);
    if (!goxel.tool_volume) goxel.tool_volume = volume_new();
    volume_set(goxel.tool_volume, shape->volume_orig);
    volume_op(goxel.tool_volume, painter, box);

    if (gest->state == GESTURE3D_STATE_END) {
        volume_set(layer_volume, goxel.tool_volume);
        volume_delete(goxel.tool_volume);
        goxel.tool_volume = NULL;
        image_history_push(goxel.image);
    }
    return 0;
}


static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    tool_shape_t *shape = (tool_shape_t*)tool;
    float snap_offset;

    if (goxel.snap_mask & (SNAP_SELECTION_IN | SNAP_SELECTION_OUT)) {
        render_box(&goxel.rend, goxel.image->selection_box, NULL,
                   EFFECT_STRIP | EFFECT_WIREFRAME);
    }

    snap_offset = goxel.snap_offsets[2] * goxel.tool_radius +
        ((painter->mode == MODE_OVER) ? 0.5 : -0.5);

    if (!shape->volume_orig)
        shape->volume_orig = volume_copy(goxel.image->active_layer->volume);

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_DRAG,
        .buttons_mask = GESTURE3D_FLAG_CTRL,
        .snap_mask = goxel.snap_mask | SNAP_ROUNDED,
        .snap_offset = snap_offset,
        .callback = on_drag,
        .user = USER_PASS(shape, painter),
    });
    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_HOVER,
        .buttons_mask = GESTURE3D_FLAG_CTRL,
        .snap_mask = goxel.snap_mask | SNAP_ROUNDED,
        .snap_offset = snap_offset,
        .callback = on_hover,
        .user = USER_PASS(shape, painter),
    });

    return tool->state;
}


static int gui(tool_t *tool)
{
    tool_shape_t *tool_shape = (void*)tool;
    tool_gui_color();
    tool_gui_smoothness();
    gui_checkbox("Planar", &tool_shape->planar, "Stay on original plane");
    tool_gui_shape(NULL);
    return 0;
}

TOOL_REGISTER(TOOL_SHAPE, shape, tool_shape_t,
              .name = N_("Shape"),
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_ALLOW_PICK_COLOR,
              .default_shortcut = "S",
)
