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
 * 3d gesture support.  Still experimental.
 *
 * The idea is to make it work in immediate mode, where we can call
 * the `gesture3d` function at anytime and it will directly call the
 * passed callback if the gesture is recognised.  A bit like imgui.
 */

#ifndef GESTURE3D_H
#define GESTURE3D_H

typedef struct gesture3d gesture3d_t;

enum {
    GESTURE3D_TYPE_DRAG         = 1 << 0,
    GESTURE3D_TYPE_CLICK        = 1 << 1,
    GESTURE3D_TYPE_HOVER        = 1 << 3,
};

enum {
    GESTURE3D_STATE_BEGIN   = 8,
    GESTURE3D_STATE_UPDATE,
    GESTURE3D_STATE_END,
};

enum {
    GESTURE3D_FLAG_PRESSED      = 1 << 0,
    GESTURE3D_FLAG_SHIFT        = 1 << 1,
    GESTURE3D_FLAG_CTRL         = 1 << 2,
    GESTURE3D_FLAG_OUT          = 1 << 3, // Outside of sensing area.
    GESTURE3D_FLAG_DRAG_DELAY   = 1 << 4, // Don't drag until we move.
    GESTURE3D_FLAG_ALWAYS_CALL  = 1 << 5, // Allways call the callback.
};

// #### 3d gestures
struct gesture3d
{
    int         type;
    int         buttons; // GESTURE3D_FLAG_SHIFT | GESTURE3D_FLAG_CTRL
    int         buttons_mask; // GESTURE3D_FLAG_SHIFT | GESTURE3D_FLAG_CTRL
    int         snap_mask;
    float       snap_offset;
    float       snap_shape[4][4]; // Used for plane snapping.
    int         (*callback)(gesture3d_t *gest);
    void        *user;
    int         user_key;
    const char  *name; // For debug only.

    // Need to be updated manually at each frame for each gestures.
    float       pos[3];
    float       normal[3];
    int         snaped;
    int         flags;

    // Updated by the 'gesture3d' function.
    float       start_pos[3];
    float       start_normal[3];
    bool        alive;
    int         state;
};

/*
 * Process a single 3d gesture state.
 *
 * Parameters:
 *   gest       - The gesture we want to process.
 *   nb         - The total number of gestures.
 *   gestures   - Array of registered gestures.
 */
int gesture3d(const gesture3d_t *gest, int *nb, gesture3d_t gestures[]);

/*
 * Remove the dead gestures from an array of 3d gestures.
 */
void gesture3d_remove_dead(int *nb, gesture3d_t gestures[]);

#endif // GESTURE3D_H
