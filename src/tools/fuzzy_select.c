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

typedef struct {
    tool_t tool;
    int threshold;
} tool_fuzzy_select_t;

static int select_cond(void *user, const volume_t *volume,
                       const int base_pos[3],
                       const int new_pos[3],
                       volume_accessor_t *volume_accessor)
{
    tool_fuzzy_select_t *tool = (void*)user;
    uint8_t v0[4], v1[4];
    int d;

    volume_get_at(volume, volume_accessor, base_pos, v0);
    volume_get_at(volume, volume_accessor, new_pos, v1);
    if (!v0[3] || !v1[3]) return 0;

    d = max3(abs(v0[0] - v1[0]), abs(v0[1] - v1[1]), abs(v0[2] - v1[2]));
    return d <= tool->threshold ? 255 : 0;
}

static int on_click(gesture3d_t *gest, const cursor_t *curs, void *user)
{
    volume_t *volume = goxel.image->active_layer->volume;
    volume_t *sel;
    int pi[3];
    tool_fuzzy_select_t *tool = (void*)user;

    pi[0] = floor(curs->pos[0]);
    pi[1] = floor(curs->pos[1]);
    pi[2] = floor(curs->pos[2]);
    sel = volume_new();
    volume_select(volume, pi, select_cond, tool, sel);
    if (goxel.mask == NULL) goxel.mask = volume_new();
    volume_merge(goxel.mask, sel, goxel.mask_mode ?: MODE_REPLACE, NULL);
    volume_delete(sel);
    return 0;
}


static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    tool_fuzzy_select_t *tool = (void*)tool_;
    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE_CLICK,
        .snap_mask = SNAP_VOLUME | SNAP_IMAGE_BOX,
        .snap_offset = -0.5,
        .callback = on_click,
        .user = tool,
    });
    return 0;
}

static layer_t *cut_as_new_layer(image_t *img, layer_t *layer,
                                 const volume_t *mask)
{
    layer_t *new_layer;

    new_layer = image_duplicate_layer(img, layer);
    volume_merge(new_layer->volume, mask, MODE_INTERSECT, NULL);
    volume_merge(layer->volume, mask, MODE_SUB, NULL);
    return new_layer;
}

static int gui(tool_t *tool_)
{
    tool_fuzzy_select_t *tool = (void*)tool_;
    bool use_color = tool->threshold < 255;

    if (gui_checkbox(_(COLORS), &use_color, _(SELECT_BY_COLOR))) {
        tool->threshold = use_color ? 0 : 255;
    }
    if (use_color) {
        gui_input_int(_(THRESHOLD), &tool->threshold, 1, 254);
    }

    tool_gui_mask_mode();

    if (volume_is_empty(goxel.mask))
        return 0;

    volume_t *volume = goxel.image->active_layer->volume;

    gui_group_begin(NULL);
    if (gui_button(_(CLEAR), 1, 0)) {
        image_history_push(goxel.image);
        volume_merge(volume, goxel.mask, MODE_SUB, NULL);
    }
    if (gui_button(_(FILL), 1, 0)) {
        image_history_push(goxel.image);
        volume_merge(volume, goxel.mask, MODE_OVER, goxel.painter.color);
    }
    if (gui_button(_(CUT_TO_NEW_LAYER), 1, 0)) {
        image_history_push(goxel.image);
        cut_as_new_layer(goxel.image, goxel.image->active_layer,
                         goxel.mask);
    }
    gui_group_end();
    return 0;
}

TOOL_REGISTER(TOOL_FUZZY_SELECT, fuzzy_select, tool_fuzzy_select_t,
              .name = STR_FUZZY_SELECT,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_SHOW_MASK,
)
