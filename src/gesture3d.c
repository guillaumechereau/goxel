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

int gesture3d(gesture3d_t *gest)
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
