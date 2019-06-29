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

int gesture3d(gesture3d_t *gest, cursor_t *curs, void *user)
{
    bool pressed = curs->flags & CURSOR_PRESSED;
    int r, ret = 0;
    const int btns_mask = CURSOR_CTRL;

    gest->cursor = curs;

    if (gest->state == GESTURE_FAILED && !pressed)
        gest->state = GESTURE_POSSIBLE;

    if (gest->type == GESTURE_DRAG) {
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if ((gest->buttons & btns_mask) != (curs->flags & btns_mask))
                break;
            if (curs->snaped && pressed) gest->state = GESTURE_BEGIN;
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            gest->state = GESTURE_UPDATE;
            if (!pressed) gest->state = GESTURE_END;
            break;
        }
    }

    if (gest->type == GESTURE_CLICK) {
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (curs->snaped && !pressed) gest->state = GESTURE_RECOGNISED;
            break;
        case GESTURE_RECOGNISED:
            if ((gest->buttons & btns_mask) != (curs->flags & btns_mask))
                break;
            if (curs->snaped && pressed) gest->state = GESTURE_TRIGGERED;
            break;
        }
    }

    if (!DEFINED(GOXEL_MOBILE) && gest->type == GESTURE_HOVER) {
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if ((gest->buttons & btns_mask) != (curs->flags & btns_mask))
                break;
            if (curs->snaped && !pressed && !(curs->flags & CURSOR_OUT))
                gest->state = GESTURE_BEGIN;
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            gest->state = GESTURE_UPDATE;
            if ((gest->buttons & btns_mask) != (curs->flags & btns_mask))
                gest->state = GESTURE_END;
            if (pressed) gest->state = GESTURE_END;
            if (curs->flags & CURSOR_OUT) gest->state = GESTURE_END;
            break;
        }
    }

    if (    gest->state == GESTURE_BEGIN ||
            gest->state == GESTURE_UPDATE ||
            gest->state == GESTURE_END ||
            gest->state == GESTURE_TRIGGERED)
    {
        r = gest->callback(gest, user);
        if (r == GESTURE_FAILED) {
            gest->state = GESTURE_FAILED;
            ret = 0;
        } else {
            ret = gest->state;
        }
    }
    if (gest->state == GESTURE_END || gest->state == GESTURE_TRIGGERED)
        gest->state = GESTURE_POSSIBLE;

    return ret;
}
