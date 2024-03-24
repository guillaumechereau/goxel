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

static void volume_wrap(volume_t *volume, int axis, int sign,
                        const int aabb[2][3])
{
    int pos[3];
    int buffer_pos[3];
    int volume_pos[3];
    int size[3];
    uint8_t *buffer;
    int i;
    size_t buffer_offset;

    if (aabb[1][axis] - aabb[0][axis] == 1) {
        return;
    }

    size[0] = aabb[1][0] - aabb[0][0];
    size[1] = aabb[1][1] - aabb[0][1];
    size[2] = aabb[1][2] - aabb[0][2];

    buffer =  malloc(4 * size[0] * size[1] * size[2]);

    for (pos[0] = 0; pos[0] < size[0]; pos[0]++) {
        for (pos[1] = 0; pos[1] < size[1]; pos[1]++) {
            for (pos[2] = 0; pos[2] < size[2]; pos[2]++) {
                memcpy(buffer_pos, pos, sizeof(pos));
                memcpy(volume_pos, pos, sizeof(pos));

                for (i = 0; i < 3; i++) {
                    volume_pos[i] += aabb[0][i];
                }

                buffer_pos[axis] += sign;

                if (buffer_pos[axis] < 0) {
                    buffer_pos[axis] += size[axis];
                }

                buffer_pos[axis] %= size[axis];

                buffer_offset = 4 * (
                    buffer_pos[2] * size[0] * size[1] +
                    buffer_pos[1] * size[0] + buffer_pos[0]
                );

                volume_get_at(volume, NULL, volume_pos, &buffer[buffer_offset]);
            }
        }
    }

    for (pos[0] = 0; pos[0] < size[0]; pos[0]++) {
        for (pos[1] = 0; pos[1] < size[1]; pos[1]++) {
            for (pos[2] = 0; pos[2] < size[2]; pos[2]++) {
                memcpy(volume_pos, pos, sizeof(pos));

                for (i = 0; i < 3; i++) {
                    volume_pos[i] += aabb[0][i];
                }

                buffer_offset = 4 * (
                    pos[2] * size[0] * size[1] +
                    pos[1] * size[0] + pos[0]
                );

                volume_set_at(volume, NULL, volume_pos, &buffer[buffer_offset]);
            }
        }
    }

    free(buffer);
}

static bool wrap_box(int *out_axis, int *sign)
{
    char buf[8];
    bool ret = false;
    int axis;
    const char *AXIS_NAMES[] = {"X", "Y", "Z"};

    *out_axis = 0;
    *sign = 1;

    for (axis = 0; axis < 3; axis++) {
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
