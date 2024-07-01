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


static int update_state(gesture3d_t *gest)
{
    bool pressed = gest->flags & GESTURE3D_FLAG_PRESSED;
    int r, ret = 0;
    const int btns_mask = GESTURE3D_FLAG_CTRL;

    if (gest->state == GESTURE3D_STATE_FAILED && !pressed)
        gest->state = GESTURE3D_STATE_POSSIBLE;

    if (gest->type == GESTURE3D_TYPE_DRAG) {
        switch (gest->state) {
        case GESTURE3D_STATE_POSSIBLE:
            if ((gest->buttons & btns_mask) != (gest->flags & btns_mask))
                break;
            if (gest->snaped && pressed) {
                gest->state = GESTURE3D_STATE_BEGIN;
            }
            break;
        case GESTURE3D_STATE_BEGIN:
        case GESTURE3D_STATE_UPDATE:
            gest->state = GESTURE3D_STATE_UPDATE;
            if (!pressed) {
                gest->state = GESTURE3D_STATE_END;
            }
            break;
        default:
            assert(false);
            break;
        }
    }

    if (gest->type == GESTURE3D_TYPE_CLICK) {
        switch (gest->state) {
        case GESTURE3D_STATE_POSSIBLE:
            if (gest->snaped && !pressed) {
                gest->state = GESTURE3D_STATE_RECOGNISED;
            }
            break;
        case GESTURE3D_STATE_RECOGNISED:
            if ((gest->buttons & btns_mask) != (gest->flags & btns_mask))
                break;
            if (gest->snaped && pressed) {
                gest->state = GESTURE3D_STATE_TRIGGERED;
            }
            break;
        default:
            assert(false);
            break;
        }
    }

    if (!DEFINED(GOXEL_MOBILE) && gest->type == GESTURE3D_TYPE_HOVER) {
        switch (gest->state) {
        case GESTURE3D_STATE_POSSIBLE:
            if ((gest->buttons & btns_mask) != (gest->flags & btns_mask))
                break;
            if (gest->snaped && !pressed &&
                    !(gest->flags & GESTURE3D_FLAG_OUT)) {
                gest->state = GESTURE3D_STATE_BEGIN;
            }
            break;
        case GESTURE3D_STATE_BEGIN:
        case GESTURE3D_STATE_UPDATE:
            gest->state = GESTURE3D_STATE_UPDATE;
            if ((gest->buttons & btns_mask) != (gest->flags & btns_mask)) {
                gest->state = GESTURE3D_STATE_END;
            }
            if (pressed) {
                gest->state = GESTURE3D_STATE_END;
            }
            if (!gest->snaped) {
                gest->state = GESTURE3D_STATE_END;
            }
            if (gest->flags & GESTURE3D_FLAG_OUT) {
                gest->state = GESTURE3D_STATE_END;
            }
            break;
        default:
            assert(false);
            break;
        }
    }

    if (    gest->state == GESTURE3D_STATE_BEGIN ||
            gest->state == GESTURE3D_STATE_UPDATE ||
            gest->state == GESTURE3D_STATE_END ||
            gest->state == GESTURE3D_STATE_TRIGGERED)
    {
        r = gest->callback(gest);
        if (r == GESTURE3D_STATE_FAILED) {
            gest->state = GESTURE3D_STATE_FAILED;
            ret = 0;
        } else {
            ret = gest->state;
        }
    }
    if (    gest->state == GESTURE3D_STATE_END ||
            gest->state == GESTURE3D_STATE_TRIGGERED) {
        gest->state = GESTURE3D_STATE_POSSIBLE;
    }

    return ret;
}

int gesture3d(const gesture3d_t *gest, int *nb, gesture3d_t gestures[])
{
    int i;
    gesture3d_t *other, *match = NULL;

    // Search if we already have this gesture in the list.
    for (i = 0; i < *nb; i++) {
        match = &gestures[i];
        if (    match->callback == gest->callback &&
                match->type == gest->type) {
            break;
        }
    }
    // If no match add the gesture in the list.
    if (i == *nb) {
        match = &gestures[(*nb)++];
        memset(match, 0, sizeof(*match));
    }
    assert(match);

    // Search if we already have a different active gesture.
    // XXX: should depend on the type.
    for (i = 0; i < *nb; i++) {
        other = &gestures[i];
        if (    other->callback == gest->callback &&
                other->type == gest->type) {
            continue;
        }
        if (other->type != GESTURE3D_TYPE_HOVER && other->state != 0) {
            return false;
        }
    }

    // Update the gesture data until it has started.  That way we can
    // modify the gesture inside its callback function.
    if (match->state == 0) {
        match->type = gest->type;
        match->buttons = gest->buttons;
        match->snap_mask = gest->snap_mask;
        match->snap_offset = gest->snap_offset;
        mat4_copy(gest->snap_shape, match->snap_shape);
        match->callback = gest->callback;
    }
    match->user = gest->user;
    match->alive = true;
    return update_state(match);
}

void gesture3d_remove_dead(int *nb, gesture3d_t gestures[])
{
    int i, count;
    gesture3d_t *gest;

    for (i = *nb - 1; i >= 0; i--) {
        gest = &gestures[i];
        if (gest->alive) {
            gest->alive = false;
            continue;
        }
        count = *nb - i - 1;
        if (count > 0) {
            memmove(&gestures[i], &gestures[i + 1], count * sizeof(*gest));
        }
        (*nb)--;
    }
}
