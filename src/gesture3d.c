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

enum {
    GESTURE3D_STATE_FAILED      = -1,
    GESTURE3D_STATE_POSSIBLE    = 0,
    GESTURE3D_STATE_SNAPED      = 1,
    GESTURE3D_STATE_DOWN,
    GESTURE3D_STATE_TRIGGERED,
};

static void update_drag(gesture3d_t *gest, bool pressed, bool btns_match,
                        int others_mask)
{
    switch (gest->state) {
    case GESTURE3D_STATE_POSSIBLE:
        if (!btns_match) break;
        if (others_mask & GESTURE3D_TYPE_DRAG) break;

        if (gest->snaped && pressed) {
            vec3_copy(gest->pos, gest->start_pos);
            vec3_copy(gest->normal, gest->start_normal);
            if (gest->flags & GESTURE3D_FLAG_DRAG_DELAY)
                gest->state = GESTURE3D_STATE_DOWN;
            else
                gest->state = GESTURE3D_STATE_BEGIN;
        }
        break;

    case GESTURE3D_STATE_DOWN:
        // Require a bit of movement before starting.
        if (!pressed) {
            gest->state = GESTURE3D_STATE_POSSIBLE;
        }
        // XXX: should depend on 2d distance.
        if (vec3_dist2(gest->pos, gest->start_pos) > 1) {
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

static void update_click(gesture3d_t *gest, bool pressed, bool btns_match)
{
    switch (gest->state) {
    case GESTURE3D_STATE_POSSIBLE:
        if (gest->snaped && !pressed && btns_match) {
            gest->state = GESTURE3D_STATE_SNAPED;
        }
        break;
    case GESTURE3D_STATE_SNAPED:
        if (!gest->snaped || !btns_match) {
            gest->state = GESTURE3D_STATE_POSSIBLE;
        }
        if (pressed) {
            gest->state = GESTURE3D_STATE_DOWN;
        }
        break;
    case GESTURE3D_STATE_DOWN:
        if (!pressed) {
            gest->state = GESTURE3D_STATE_TRIGGERED;
        }
        break;
    default:
        assert(false);
        break;
    }
}

static void update_hover(gesture3d_t *gest, bool pressed, bool btns_match)
{
    switch (gest->state) {
    case GESTURE3D_STATE_POSSIBLE:
        if (!btns_match) break;
        if (gest->snaped && !pressed &&
                !(gest->flags & GESTURE3D_FLAG_OUT)) {
            gest->state = GESTURE3D_STATE_BEGIN;
        }
        break;
    case GESTURE3D_STATE_BEGIN:
    case GESTURE3D_STATE_UPDATE:
        gest->state = GESTURE3D_STATE_UPDATE;
        if (!btns_match) {
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
        break;
    }
}

static int update_state(gesture3d_t *gest, int others_mask)
{
    bool pressed = gest->flags & GESTURE3D_FLAG_PRESSED;
    int r, ret = 0;
    const int btns_mask = GESTURE3D_FLAG_CTRL | GESTURE3D_FLAG_SHIFT;
    bool btns_match = true;

    if (gest->buttons) {
        btns_match = (gest->buttons & btns_mask) == (gest->flags & btns_mask);
    }

    if (gest->state == GESTURE3D_STATE_FAILED && !pressed)
        gest->state = GESTURE3D_STATE_POSSIBLE;

    if (gest->type == GESTURE3D_TYPE_DRAG) {
        update_drag(gest, pressed, btns_match, others_mask);
    }

    if (gest->type == GESTURE3D_TYPE_CLICK) {
        update_click(gest, pressed, btns_match);
    }

    if (!DEFINED(GOXEL_MOBILE) && gest->type == GESTURE3D_TYPE_HOVER) {
        update_hover(gest, pressed, btns_match);
    }

    if (    gest->state == GESTURE3D_STATE_BEGIN ||
            gest->state == GESTURE3D_STATE_UPDATE ||
            gest->state == GESTURE3D_STATE_END ||
            gest->state == GESTURE3D_STATE_TRIGGERED)
    {
        r = gest->callback(gest);
        if (r != 0) {
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
    int i, others_mask = 0;
    gesture3d_t *other, *match = NULL;

    // Search if we already have this gesture in the list.
    for (i = 0; i < *nb; i++) {
        match = &gestures[i];
        if (    match->callback == gest->callback &&
                match->user_key == gest->user_key &&
                match->type == gest->type) {
            break;
        }
    }
    // If no match add the gesture in the list.
    if (i == *nb) {
        match = &gestures[(*nb)++];
        *match = *gest;
    }
    assert(match);

    // Search if we already have a different active gesture.
    // XXX: should depend on the type.
    for (i = 0; i < *nb; i++) {
        other = &gestures[i];
        if (    other->callback == gest->callback &&
                other->user_key == gest->user_key &&
                other->type == gest->type) {
            continue;
        }
        if (    other->type != GESTURE3D_TYPE_HOVER &&
                other->state >= GESTURE3D_STATE_BEGIN) {
            match->state = GESTURE3D_STATE_FAILED;
            return 0;
        }

        if (other->state > GESTURE3D_STATE_POSSIBLE) {
            others_mask |= other->type;
        }
    }

    // Update the gesture data until it has started.  That way we can
    // modify the gesture inside its callback function.
    if (match->state < GESTURE3D_STATE_BEGIN) {
        match->type = gest->type;
        match->buttons = gest->buttons;
        match->snap_mask = gest->snap_mask;
        match->snap_offset = gest->snap_offset;
        mat4_copy(gest->snap_shape, match->snap_shape);
        match->callback = gest->callback;
    }
    match->user = gest->user;
    match->alive = true;
    return update_state(match, others_mask);
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
