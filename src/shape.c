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

#include "goxel.h"

shape_t shape_sphere;
shape_t shape_cube;
shape_t shape_cylinder;

static float sphere_func(const vec3_t *p, const vec3_t *s, float smoothness)
{
    float d = vec3_norm(*p);
    vec3_t a;
    if (p->x == 0 && p->y == 0 && p->z == 0) return max3(s->x, s->y, s->z);
    a = vec3(s->x * p->x / d, s->y * p->y / d, s->z * p->z / d);
    return vec3_norm(a) - d;
}

static float cube_func(const vec3_t *p, const vec3_t *s, float sm)
{
    int i;
    float min_v = INFINITY;
    float ret = INFINITY, v;

    // Check if we are outside the max cube:
    if  (p->x < -s->x - sm || p->x >= +s->x + sm ||
         p->y < -s->y - sm || p->y >= +s->y + sm ||
         p->z < -s->z - sm || p->z >= +s->z + sm) return -INFINITY;

    // Or inside the min cube:
    if  (p->x >= -s->x + sm && p->x < +s->x - sm &&
         p->y >= -s->y + sm && p->y < +s->y - sm &&
         p->z >= -s->z + sm && p->z < +s->z - sm) return +INFINITY;

    for (i = 0; i < 3; i++) {
        if (p->v[i]) {
            v = s->v[i] / fabs(p->v[i]);
            if (v < min_v) {
                min_v = v;
                ret = s->v[i] - fabs(p->v[i]);
            }
        }
    }
    return ret;
}

static float cylinder_func(const vec3_t *p, const vec3_t *s, float smoothness)
{
    float d = vec2_norm(p->xy);
    float rz, r;
    rz = s->z - fabs(p->z);
    if (p->x == 0 && p->y == 0) return min(rz, max3(s->x, s->y, s->z));
    // Ellipse polar form relative to center:
    // r(θ) = a b / √((b cosΘ)² + (a sinΘ)²)
    r = s->x * s->y / vec2_norm(vec2(s->y * p->x / d, s->x * p->y / d));
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
