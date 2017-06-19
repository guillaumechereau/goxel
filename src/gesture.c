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

// Still experimental gestures manager.

// XXX: this value should be set depending.
static float g_start_dist = 8;

static int update(gesture_t *gest, const inputs_t *inputs)
{
    if (gest->type == GESTURE_DRAG) {
        gest->pos = inputs->mouse_pos;
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (inputs->mouse_down[gest->button]) {
                gest->start_pos = inputs->mouse_pos;
                gest->state = GESTURE_RECOGNISED;
            }
            break;
        case GESTURE_RECOGNISED:
            if (vec2_dist(gest->start_pos, gest->pos) > g_start_dist)
                gest->state = GESTURE_BEGIN;
            if (!inputs->mouse_down[gest->button])
                gest->state = GESTURE_POSSIBLE;
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            gest->state = GESTURE_UPDATE;
            if (!inputs->mouse_down[gest->button])
                gest->state = GESTURE_END;
            break;
        }
    }

    if (gest->type == GESTURE_CLICK) {
        gest->pos = inputs->mouse_pos;
        switch (gest->state) {
            case GESTURE_POSSIBLE:
            if (inputs->mouse_down[gest->button]) {
                gest->start_pos = inputs->mouse_pos;
                gest->state = GESTURE_RECOGNISED;
            }
            break;
            case GESTURE_RECOGNISED:
            if (!inputs->mouse_down[gest->button]) {
                gest->state = GESTURE_TRIGGERED;
            }
            break;
        }
    }

    return 0;
}


int gesture_update(int nb, gesture_t *gestures[],
                   const inputs_t *inputs, const vec4_t *view, void *user)
{
    int i, nb_recognised = 0, ret = 0;
    bool began = false;
    bool allup = true;
    gesture_t *gest;

    for (i = 0; i < ARRAY_SIZE(inputs->mouse_down); i++)
        if (inputs->mouse_down[i]) allup = false;

    for (i = 0; i < nb; i++) {
        gest = gestures[i];
        gest->view = *view;
        if ((gest->state == GESTURE_FAILED) && allup) {
            gest->state = GESTURE_POSSIBLE;
        }
        if (IS_IN(gest->state, GESTURE_END, GESTURE_TRIGGERED))
            gest->state = GESTURE_POSSIBLE;

        update(gest, inputs);
        if (IS_IN(gest->state, GESTURE_BEGIN, GESTURE_TRIGGERED)) {
            began = true;
            break;
        }
        if (gest->state == GESTURE_RECOGNISED)
            nb_recognised++;
    }
    for (i = 0; i < nb; i++) {
        gest = gestures[i];
        if (began && gest->state == GESTURE_RECOGNISED)
            gest->state = GESTURE_FAILED;
        if (IS_IN(gest->state, GESTURE_BEGIN, GESTURE_UPDATE, GESTURE_END,
                               GESTURE_TRIGGERED)) {
            gest->callback(gest, user);
            ret = 1;
        }
    }
    return ret;
}
