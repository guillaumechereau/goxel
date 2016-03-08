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

static float sphere_func(const vec3_t *p, const vec3_t *s)
{
    vec3_t pp = vec3(p->x / s->x, p->y / s->y, p->z / s->z);
    return smoothstep(1 + 1 / s->x, 1 - 1 / s->x, vec3_norm2(pp));
}

static float cube_func(const vec3_t *p, const vec3_t *s)
{
    return (p->x >= -s->x && p->x < +s->x &&
            p->y >= -s->y && p->y < +s->y &&
            p->z >= -s->z && p->z < +s->z) ? 1 : 0;
}

static float cylinder_func(const vec3_t *p, const vec3_t *s)
{
    vec3_t pp = vec3(p->x / s->x, p->y / s->y, p->z / s->z);
    if (pp.z <= -1 || pp.z > 1) return 0.0;
    return smoothstep(1 + 1 / s->x, 1 - 1 / s->x, vec2_norm2(pp.xy));

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
