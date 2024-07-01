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

#ifndef GESTURE3D_H
#define GESTURE3D_H

typedef struct gesture3d gesture3d_t;

enum {
    GESTURE3D_TYPE_DRAG         = 1 << 0,
    GESTURE3D_TYPE_CLICK        = 1 << 1,
    GESTURE3D_TYPE_HOVER        = 1 << 3,
};

enum {
    GESTURE3D_STATE_POSSIBLE = 0,
    GESTURE3D_STATE_RECOGNISED,
    GESTURE3D_STATE_BEGIN,
    GESTURE3D_STATE_UPDATE,
    GESTURE3D_STATE_END,
    GESTURE3D_STATE_TRIGGERED,
    GESTURE3D_STATE_FAILED,
};

enum {
    GESTURE3D_FLAG_PRESSED      = 1 << 0,
    GESTURE3D_FLAG_SHIFT        = 1 << 1,
    GESTURE3D_FLAG_CTRL         = 1 << 2,
    GESTURE3D_FLAG_OUT          = 1 << 3, // Outside of sensing area.
};

// #### 3d gestures
struct gesture3d
{
    int         type;
    int         buttons; // GESTURE3D_FLAG_SHIFT | GESTURE3D_FLAG_CTRL
    int         snap_mask;
    float       snap_offset;
    float       snap_shape[4][4]; // Used for plane snapping.
    int         (*callback)(gesture3d_t *gest, void *user);
    void        *user;

    // Automatically updated attributes.
    int         state;
    float       pos[3];
    float       normal[3];
    int         snaped;
    int         flags;
};

int gesture3d(gesture3d_t *gest, void *user);

#endif // GESTURE3D_H
