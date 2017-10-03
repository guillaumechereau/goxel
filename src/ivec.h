/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef IVEC_H
#define IVEC_H

#include <stdint.h>
#include <stdbool.h>

#define IVEC(...) {{__VA_ARGS__}}

typedef union {
    struct  { uint8_t x, y, z; };
    struct  { uint8_t r, g, b; };
    uint8_t v[3];
} uvec3b_t;

static inline uvec3b_t uvec3b(uint8_t x, uint8_t y, uint8_t z) {
    return (uvec3b_t)IVEC(x, y, z);
}

static inline uvec3b_t uvec3b_mix(uvec3b_t a, uvec3b_t b, float t)
{
    return uvec3b(a.x * (1 - t) + b.x * t,
                  a.y * (1 - t) + b.y * t,
                  a.z * (1 - t) + b.z * t);
}

static inline void vec3i_set(int v[3], int x, int y, int z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

#endif // IVEC_H
