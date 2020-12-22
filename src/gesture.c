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

// XXX: this value should be set depending on the screen resolution.
static float g_start_dist = 8;

static bool test_button(const inputs_t *inputs, const touch_t *touch, int mask)
{
    if (mask & GESTURE_SHIFT && !inputs->keys[KEY_LEFT_SHIFT]) return false;
    if (mask & GESTURE_CTRL && !inputs->keys[KEY_CONTROL]) return false;
    if ((mask & GESTURE_LMB) && !touch->down[0]) return false;
    if ((mask & GESTURE_MMB) && !touch->down[1]) return false;
    if ((mask & GESTURE_RMB) && !touch->down[2]) return false;
    return true;
}

static bool rect_contains(const float rect[4], const float pos[2])
{
    return pos[0] >= rect[0] && pos[0] < rect[0] + rect[2] &&
           pos[1] >= rect[1] && pos[1] < rect[1] + rect[3];
}

static float get_angle(const float a0[2], const float a1[2],
                       const float b0[2], const float b1[2])
{
    float u[2], v[2], dot, det;
    vec2_sub(a1, a0, u);
    vec2_normalize(u, u);
    vec2_sub(b1, b0, v);
    vec2_normalize(v, v);
    dot = vec2_dot(u, v);
    det = vec2_cross(u, v);
    return atan2(det, dot);
}

static int update(gesture_t *gest, const inputs_t *inputs, int mask)
{
    const touch_t *ts = inputs->touches;
    int nb_ts = 0;
    int i, j;

    for (i = 0; i < ARRAY_SIZE(inputs->touches); i++) {
        for (j = 0; j < ARRAY_SIZE(inputs->touches[i].down); j++) {
            if (ts[i].down[j]) {
                nb_ts++;
                break;
            }
        }
    }

    if (gest->type == GESTURE_DRAG) {
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (nb_ts == 1 && test_button(inputs, &ts[0], gest->button)) {
                vec2_copy(ts[0].pos, gest->start_pos[0]);
                vec2_copy(gest->start_pos[0], gest->pos);
                vec2_copy(gest->start_pos[0], gest->last_pos);
                if (!rect_contains(gest->viewport, gest->pos)) {
                    gest->state = GESTURE_FAILED;
                    break;
                }
                gest->state = (mask & (GESTURE_CLICK | GESTURE_PINCH)) ?
                            GESTURE_RECOGNISED : GESTURE_BEGIN;
            }
            break;
        case GESTURE_RECOGNISED:
            if (vec2_dist(gest->start_pos[0], ts[0].pos) >= g_start_dist)
                gest->state = GESTURE_BEGIN;
            if (nb_ts == 0) {
                gest->state = (!(mask & GESTURE_CLICK)) ?
                    GESTURE_BEGIN : GESTURE_FAILED;
            }
            if (nb_ts > 1) gest->state = GESTURE_FAILED;
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            vec2_copy(ts[0].pos, gest->pos);
            gest->state = GESTURE_UPDATE;
            if (!test_button(inputs, &ts[0], gest->button))
                gest->state = GESTURE_END;
            break;
        }
    }

    if (gest->type == GESTURE_CLICK) {
        vec2_copy(ts[0].pos, gest->pos);
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (test_button(inputs, &ts[0], gest->button)) {
                vec2_copy(ts[0].pos, gest->start_pos[0]);
                gest->state = GESTURE_RECOGNISED;
            }
            break;
        case GESTURE_RECOGNISED:
            if (!test_button(inputs, &ts[0], gest->button))
                gest->state = GESTURE_TRIGGERED;
            break;
        }
    }

    if (gest->type == GESTURE_PINCH) {
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (ts[0].down[0] && ts[1].down[0]) {
                gest->state = GESTURE_BEGIN;
                vec2_copy(ts[0].pos, gest->start_pos[0]);
                vec2_copy(ts[1].pos, gest->start_pos[1]);
                gest->pinch = 1;
                gest->rotation = 0;
                vec2_mix(ts[0].pos, ts[1].pos, 0.5, gest->pos);
            }
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            gest->state = GESTURE_UPDATE;
            gest->pinch = vec2_dist(ts[0].pos, ts[1].pos) /
                          vec2_dist(gest->start_pos[0], gest->start_pos[1]);
            gest->rotation = get_angle(gest->start_pos[0], gest->start_pos[1],
                                       ts[0].pos, ts[1].pos);
            vec2_mix(ts[0].pos, ts[1].pos, 0.5, gest->pos);
            if (!ts[0].down[0] || !ts[1].down[0])
                gest->state = GESTURE_END;
            break;
        }
    }

    if (gest->type == GESTURE_HOVER) {
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (DEFINED(GOXEL_MOBILE)) break; //Workaround.
            if (nb_ts == 0) {
                vec2_copy(ts[0].pos, gest->pos);
                if (rect_contains(gest->viewport, gest->pos)) {
                    gest->state = GESTURE_BEGIN;
                }
            }
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            vec2_copy(ts[0].pos, gest->pos);
            gest->state = rect_contains(gest->viewport, gest->pos) ?
                GESTURE_UPDATE : GESTURE_END;
            break;
        }
    }

    return 0;
}


int gesture_update(int nb, gesture_t *gestures[],
                   const inputs_t *inputs, const float viewport[4],
                   void *user)
{
    int i, j, mask = 0;
    bool allup = true; // Set if all the mouse buttons are up.
    gesture_t *gest, *triggered = NULL;

    for (i = 0; allup && i < ARRAY_SIZE(inputs->touches); i++) {
        for (j = 0; allup && j < ARRAY_SIZE(inputs->touches[i].down); j++) {
            if (inputs->touches[i].down[j]) {
                allup = false;
                break;
            }
        }
    }

    for (i = 0; i < nb; i++) {
        gest = gestures[i];
        if (gest->state == GESTURE_POSSIBLE) mask |= gest->type;
    }

    for (i = 0; i < nb; i++) {
        gest = gestures[i];
        vec4_copy(viewport, gest->viewport);
        if ((gest->state == GESTURE_FAILED) && allup) {
            gest->state = GESTURE_POSSIBLE;
        }
        if (gest->state == GESTURE_END || gest->state == GESTURE_TRIGGERED)
            gest->state = GESTURE_POSSIBLE;

        update(gest, inputs, mask);
        if (    gest->state == GESTURE_BEGIN ||
                gest->state == GESTURE_UPDATE ||
                gest->state == GESTURE_END ||
                gest->state == GESTURE_TRIGGERED)
        {
            gest->callback(gest, user);
            vec2_copy(gest->pos, gest->last_pos);
            triggered = gest;
            break;
        }
    }

    // If one gesture started, fail all the other gestures.
    if (triggered && triggered->state == GESTURE_BEGIN) {
        for (i = 0; i < nb; i++) {
            gest = gestures[i];
            if (gest != triggered)
                gest->state = GESTURE_FAILED;
        }
    }
    return triggered ? 1 : 0;
}
