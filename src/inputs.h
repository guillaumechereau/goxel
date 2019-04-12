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

#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>

// A finger touch or mouse click state.
// `down` represent each button in the mouse.  For touch events only the
// first element is set.
typedef struct {
    float   pos[2];
    bool    down[3];
} touch_t;

typedef struct inputs
{
    int         window_size[2];
    float       scale;
    bool        keys[512]; // Table of all the pressed keys.
    uint32_t    chars[16];
    touch_t     touches[4];
    float       mouse_wheel;
    int         framebuffer; // Screen framebuffer
} inputs_t;

// Conveniance function to add a char in the inputs.
void inputs_insert_char(inputs_t *inputs, uint32_t c);

#endif // INPUTS_H
