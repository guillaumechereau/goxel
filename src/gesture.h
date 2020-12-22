/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * Section: Mouse gestures
 * Detect 2d mouse or touch gestures.
 *
 * The way it works is that we have to create one instance of
 * <gesture_t> per gesture we can recognise, and then use the
 * <gesture_update> function to update their states and call the callback
 * functions of active gestures.
 */

/* XXX: This part of the code is not very clear */

#include "inputs.h"

/*
 * Enum: GESTURE_TYPES
 * Define the different types of recognised gestures.
 *
 * GESTURE_DRAG     - Click and drag the mouse.
 * GESTURE_CLICK    - Single click.
 * GESTURE_PINCH    - Two fingers pinch.
 * GESTURE_HOVER    - Move the mouse without clicking.
 */
enum {
    GESTURE_DRAG        = 1 << 0,
    GESTURE_CLICK       = 1 << 1,
    GESTURE_PINCH       = 1 << 2,
    GESTURE_HOVER       = 1 << 3,
};

/*
 * Enum: GESTURE_STATES
 * Define the states a gesture can be in.
 *
 * GESTURE_POSSIBLE     - The gesture is not recognised yet (default state).
 * GESTURE_RECOGNISED   - The gesture has been recognised.
 * GESTURE_BEGIN        - The gesture has begun.
 * GESTURE_UPDATE       - The gesture is in progress.
 * GESTURE_END          - The testure has ended.
 * GESTURE_TRIGGERED    - For click gestures: the gesture has occured.
 * GESTURE_FAILED       - The gesture has failed.
 */
enum {
    GESTURE_POSSIBLE = 0,
    GESTURE_RECOGNISED,
    GESTURE_BEGIN,
    GESTURE_UPDATE,
    GESTURE_END,
    GESTURE_TRIGGERED,
    GESTURE_FAILED,
};

enum {
    GESTURE_LMB     = 1 << 0,
    GESTURE_MMB     = 1 << 1,
    GESTURE_RMB     = 1 << 2,
    GESTURE_SHIFT   = 1 << 3,
    GESTURE_CTRL    = 1 << 4,
};

/*
 * Type: gesture_t
 * Structure used to handle a given gesture.
 */
typedef struct gesture gesture_t;
struct gesture
{
    int     type;
    int     button;
    int     state;
    float   viewport[4];
    float   pos[2];
    float   start_pos[2][2];
    float   last_pos[2];
    float   pinch;
    float   rotation;
    int     (*callback)(const gesture_t *gest, void *user);
};

/*
 * Function: gesture_update
 * Update the state of a list of gestures, and call the gestures
 * callbacks as needed.
 *
 * Parameters:
 *   nb         - Number of gestures.
 *   gestures   - A pointer to an array of <gesture_t> instances.
 *   inputs     - The inputs structure.
 *   viewport   - Current viewport rect.
 *   user       - User data pointer, passed to the callbacks.
 */
int gesture_update(int nb, gesture_t *gestures[],
                   const inputs_t *inputs, const float viewport[4],
                   void *user);
