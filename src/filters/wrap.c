/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * A filter for moving voxels along an axis and wrapping at the boundary
 * of an AABB.
 */

typedef struct {
    filter_t filter;
    bool current_only;
} filter_wrap_t;

static bool wrap_box(int *out_axis, int *sign)
{
    char buf[8];
    bool ret = false;
    static const char* AXIS_NAMES[] = {"X", "Y", "Z"};

    for (int axis = 0; axis < 3; axis++) {
        gui_row_begin(2);

        snprintf(buf, sizeof(buf), "-%s", AXIS_NAMES[axis]);
        if (gui_button(buf, 1.0, 0)) {
            *out_axis = axis;
            *sign = -1;
            ret = true;
        }

        snprintf(buf, sizeof(buf), "+%s", AXIS_NAMES[axis]);
        if (gui_button(buf, 1.0, 0)) {
            *out_axis = axis;
            *sign = 1;
            ret = true;
        }

        gui_row_end();
    }

    return ret;
}

static int gui(filter_t *filter)
{
    filter_wrap_t *wrap = (void*)filter;
    int axis, sign;
    float box[4][4] = {};
    int aabb[2][3];
    bool should_wrap;
    layer_t *layer;

    memcpy(box, goxel.selection, sizeof(box));
    
    if (box_is_null(box))
        memcpy(box, goxel.image->active_layer->box, sizeof(box));

    if (box_is_null(box))
        memcpy(box, goxel.image->box, sizeof(box));

    gui_group_begin(NULL);

    should_wrap = wrap_box(&axis, &sign);

    gui_checkbox(
        "Current layer only",
        &wrap->current_only,
        "If checked, only voxels on the current layer will be wrapped.\n"
        "If unchecked, voxels on all layers will be wrapped."
    );

    gui_group_end();

    if (should_wrap) {
        if (box_is_null(box))
            return 0;

        if (wrap->current_only && !goxel.image->active_layer->visible)
            return 0;

        bbox_to_aabb(box, aabb);
        image_history_push(goxel.image);

        DL_FOREACH(goxel.image->layers, layer) {
            if (wrap->current_only && layer != goxel.image->active_layer)
                continue;

            volume_wrap(layer->volume, axis, sign, aabb);
        }
    }

    return 0;
}

FILTER_REGISTER(wrap, filter_wrap_t,
    .name = "Wrap voxels",
    .gui_fn = gui,
)
