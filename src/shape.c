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

#include "shape.h"

#include <math.h>

static float min(float x, float y)
{
    return x < y ? x : y;
}

static float max(float x, float y)
{
    return x > y ? x : y;
}

static float max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

static float vec2_norm(const float v[static 2])
{
    return sqrt(v[0] * v[0] + v[1] * v[1]);
}

static float vec3_norm(const float v[static 3])
{
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

#define VEC(...) ((float[]){__VA_ARGS__})

shape_t shape_sphere;
shape_t shape_cube;
shape_t shape_cylinder;

static float sphere_func(const float p[3], const float s[3], float smoothness)
{
    float d = vec3_norm(p);
    float r;
    if (p[0] == 0 && p[1] == 0 && p[2] == 0) return max3(s[0], s[1], s[2]);
    r = s[0] * s[1] * s[2] / vec3_norm(VEC(s[1] * s[2] * p[0] / d,
                                           s[0] * s[2] * p[1] / d,
                                           s[0] * s[1] * p[2] / d));
    return r - d;
}

static float cube_func(const float p[3], const float s[3], float sm)
{
    int i;
    float min_v = INFINITY;
    float ret = INFINITY, v;

    // Check if we are outside the max cube:
    if  (p[0] < -s[0] - sm || p[0] >= +s[0] + sm ||
         p[1] < -s[1] - sm || p[1] >= +s[1] + sm ||
         p[2] < -s[2] - sm || p[2] >= +s[2] + sm) return -INFINITY;

    // Or inside the min cube:
    if  (p[0] >= -s[0] + sm && p[0] < +s[0] - sm &&
         p[1] >= -s[1] + sm && p[1] < +s[1] - sm &&
         p[2] >= -s[2] + sm && p[2] < +s[2] - sm) return +INFINITY;

    for (i = 0; i < 3; i++) {
        if (p[i]) {
            v = s[i] / fabs(p[i]);
            if (v < min_v) {
                min_v = v;
                ret = s[i] - fabs(p[i]);
            }
        }
    }
    return ret;
}

static float cylinder_func(const float p[3], const float s[3],
                           float smoothness)
{
    float d = vec2_norm(p);
    float rz, r;
    rz = s[2] - fabs(p[2]);
    if (p[0] == 0 && p[1] == 0) return min(rz, max3(s[0], s[1], s[2]));
    // Ellipse polar form relative to center:
    // r(θ) = a b / √((b cosΘ)² + (a sinΘ)²)
    r = s[0] * s[1] / vec2_norm(VEC(s[1] * p[0] / d, s[0] * p[1] / d));
    return min(rz, r - d);
}

void shapes_init(void)
{
    shape_sphere = (shape_t){
        .id     = "sphere",
        .func   = sphere_func,
    };
    shape_cube = (shape_t){
        .id     = "cube",
        .func   = cube_func,
    };
    shape_cylinder = (shape_t){
        .id     = "cylinder",
        .func = cylinder_func,
    };
}
