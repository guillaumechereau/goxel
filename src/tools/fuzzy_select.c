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
    int mode; // MODE_REPLACE, MODE_OVER, MODE_SUB
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

static int on_click(gesture3d_t *gest)
{
    tool_fuzzy_select_t *tool = gest->user;
    image_t *img = goxel.image;
    volume_t *volume = img->active_layer->volume;
    volume_t *sel;
    int pi[3];
    int mode = tool->mode;

    if (gest->flags & GESTURE3D_FLAG_SHIFT)
        mode = MODE_OVER;
    else if (gest->flags & GESTURE3D_FLAG_CTRL)
        mode = MODE_SUB;

    pi[0] = floor(gest->pos[0]);
    pi[1] = floor(gest->pos[1]);
    pi[2] = floor(gest->pos[2]);
    sel = volume_new();
    volume_select(volume, pi, select_cond, tool, sel);
    if (img->selection_mask == NULL) img->selection_mask = volume_new();
    volume_merge(img->selection_mask, sel, mode, NULL);
    volume_delete(sel);
    image_history_push(goxel.image);

    return 0;
}

static void init(tool_t *tool_)
{
    tool_fuzzy_select_t *tool = (void*)tool_;
    tool->mode = MODE_REPLACE;
}

static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    tool_fuzzy_select_t *tool = (void*)tool_;
    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_CLICK,
        .snap_mask = SNAP_VOLUME | SNAP_IMAGE_BOX,
        .snap_offset = -0.5,
        .callback = on_click,
        .user = tool,
    });
    return 0;
}

static int gui(tool_t *tool_)
{
    tool_fuzzy_select_t *tool = (void*)tool_;
    bool use_color = tool->threshold < 255;
    image_t *img = goxel.image;

    tool_gui_mask_mode(&tool->mode);

    if (gui_checkbox(_(COLORS), &use_color, _(SELECT_BY_COLOR))) {
        tool->threshold = use_color ? 0 : 255;
    }
    if (use_color) {
        gui_input_int(_(THRESHOLD), &tool->threshold, 1, 254);
    }

    if (volume_is_empty(img->selection_mask))
        return 0;

    return 0;
}

TOOL_REGISTER(TOOL_FUZZY_SELECT, fuzzy_select, tool_fuzzy_select_t,
              .name = STR_FUZZY_SELECT,
              .init_fn = init,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_SHOW_MASK,
)
