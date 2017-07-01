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

int gesture3d(gesture3d_t *gest, const cursor_t *curs, void *user)
{
    if (gest->type == GESTURE_DRAG) {
        gest->cursor = *curs;
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (curs->snaped && (curs->flags & CURSOR_PRESSED))
                gest->state = GESTURE_BEGIN;
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            gest->state = GESTURE_UPDATE;
            if (!(curs->flags & CURSOR_PRESSED))
                gest->state = GESTURE_END;
            break;
        }
    }

    if (gest->type == GESTURE_HOVER) {
        gest->cursor = *curs;
        switch (gest->state) {
        case GESTURE_POSSIBLE:
            if (curs->snaped && !(curs->flags & CURSOR_PRESSED))
                gest->state = GESTURE_BEGIN;
            break;
        case GESTURE_BEGIN:
        case GESTURE_UPDATE:
            gest->state = GESTURE_UPDATE;
            if (curs->flags & CURSOR_PRESSED)
                gest->state = GESTURE_END;
            break;
        }
    }

    if (IS_IN(gest->state, GESTURE_BEGIN, GESTURE_UPDATE, GESTURE_END))
        gest->callback(gest, user);
    if (gest->state == GESTURE_END) gest->state = GESTURE_POSSIBLE;
    return 0;
}
