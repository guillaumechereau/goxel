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
    int x;
} filter_wrap_t;

static bool wrap_box(int *out_axis, int *sign)
{
    char buf[8];
    bool ret = false;
    static const char* AXIS_NAMES[] = {"X", "Y", "Z"};

    gui_group_begin(_(WRAP));

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

    gui_group_end();
    return ret;
}

static int gui(filter_t *filter)
{
    //filter_wrap_t *wrap = (void*)filter;
    int axis, sign;

    if (wrap_box(&axis, &sign)) {
#if 0
        wrap_aabb[0][0] = x;
        wrap_aabb[0][1] = y;
        wrap_aabb[0][2] = z;
        wrap_aabb[1][0] = x + w;
        wrap_aabb[1][1] = y + h;
        wrap_aabb[1][2] = z + d;

        image_history_push(goxel.image);
        for (layer_t *layer = goxel.image->layers; layer; layer=layer->next) {
            if (layer->visible) {
                volume_wrap(layer->volume, axis, sign, wrap_aabb);
            }
        }
#endif
    }
    return 0;
}

FILTER_REGISTER(wrap, filter_wrap_t,
    .name = "Wrap voxels",
    .gui_fn = gui,
)
