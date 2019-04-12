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

// Represent a 3d cursor.
// The program keeps track of two cursors, that are then used by the tools.
enum {
    // The state flags of the cursor.
    CURSOR_PRESSED      = 1 << 0,
    CURSOR_SHIFT        = 1 << 1,
    CURSOR_CTRL         = 1 << 2,

    CURSOR_OUT          = 1 << 3, // Outside of sensing area.
};

typedef struct cursor {
    float  pos[3];
    float  normal[3];
    int    snap_mask;
    int    snaped;
    int    flags; // Union of CURSOR_* values.
    float  snap_offset; // XXX: fix this.
} cursor_t;

// #### 3d gestures
struct gesture3d
{
    int         type;
    int         state;
    int         buttons; // CURSOR_SHIFT | CURSOR_CTRL
    cursor_t    *cursor;
    int         (*callback)(gesture3d_t *gest, void *user);
};

int gesture3d(gesture3d_t *gest, cursor_t *curs, void *user);

#endif // GESTURE3D_H
