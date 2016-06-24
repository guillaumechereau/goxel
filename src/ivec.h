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

#include <stdint.h>
#include <stdbool.h>

#define IVEC(...) {{__VA_ARGS__}}

typedef union {
    struct  { int x, y; };
    int     v[2];
} vec2i_t;

static inline vec2i_t vec2i(int x, int y) {
    return (vec2i_t)IVEC(x, y);
}

typedef union {
    struct  { uint8_t x, y; };
    uint8_t     v[2];
} uvec2b_t;

static inline uvec2b_t uvec2b(uint8_t x, uint8_t y) {
    return (uvec2b_t)IVEC(x, y);
}

typedef union {
    struct { int8_t x, y, z; };
    struct { int8_t r, g, b; };
    int8_t v[3];
} vec3b_t;

static inline vec3b_t vec3b(int8_t x, int8_t y, int8_t z) {
    return (vec3b_t)IVEC(x, y, z);
}


typedef union {
    struct { int8_t x, y, z, w; };
    struct { int8_t r, g, b, a; };
    int8_t v[4];
} vec4b_t;

static inline vec4b_t vec4b(int8_t x, int8_t y, int8_t z, int8_t w) {
    return (vec4b_t)IVEC(x, y, z, w);
}

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

typedef union {
    struct  { uint8_t x, y, z, w; };
    struct  { uint8_t r, g, b, a; };
    uvec3b_t xyz;
    uvec3b_t rgb;
    uint8_t v[4];
    uint32_t uint32;
} uvec4b_t;

static inline uvec4b_t uvec4b(uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
    return (uvec4b_t)IVEC(x, y, z, w);
}

static inline bool uvec4b_equal(uvec4b_t a, uvec4b_t b)
{
    int i;
    for (i = 0; i < 4; i++)
        if (a.v[i] != b.v[i]) return false;
    return true;
}

static inline vec3b_t vec3b_add(const vec3b_t a, const vec3b_t b)
{
    vec3b_t ret;
    int i;
    for (i = 0; i < 3; i++)
        ret.v[i] = a.v[i] + b.v[i];
    return ret;
}

static inline void vec3b_iadd(vec3b_t *a, const vec3b_t b)
{
    *a = vec3b_add(*a, b);
}

static inline vec3b_t vec3b_addk(const vec3b_t a, const vec3b_t b, float k)
{
    vec3b_t ret;
    int i;
    for (i = 0; i < 3; i++)
        ret.v[i] = a.v[i] + b.v[i] * k;
    return ret;
}

static inline void vec3b_iaddk(vec3b_t *a, const vec3b_t b, float k)
{
    *a = vec3b_addk(*a, b, k);
}

static inline vec3b_t vec3b_mul(vec3b_t a, float k)
{
    return vec3b(a.x * k, a.y * k, a.z * k);
}

static inline void vec3b_imul(vec3b_t *a, float k)
{
    *a = vec3b_mul(*a, k);
}

static inline vec3b_t vec3b_mix(vec3b_t a, vec3b_t b, float t)
{
    return vec3b(a.x * (1 - t) + b.x * t,
                 a.y * (1 - t) + b.y * t,
                 a.z * (1 - t) + b.z * t);
}

static const vec3b_t vec3b_zero = IVEC(0, 0, 0);
static const uvec4b_t uvec4b_zero = IVEC(0, 0, 0, 0);
