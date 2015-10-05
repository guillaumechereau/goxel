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

/*  A plane is defined as the 4x4 matrix to transform from the plane local
 *  coordinate into global coordinates:
 *
 *
 *  n
 *  ^    v
 *  |   ^ . . . . .
 *  |  /         .
 *  | /         .
 *  |/         .
 *  +---------> u
 *  P
 *
 *  Here the plane defined by the point P and vectors u and v with normal n,
 *  will have a matrix like this:
 *
 *     [ux  uy  yz  0]
 *     [vx  vy  vz  0]
 *     [nx  ny  nz  0]
 *     [px  py  pz  1]
 *
 *  This representation has several advantages: we can access the plane unitary
 *  vectors, normal, and origin without any computation.  I used an union so
 *  that those values can be access directly as u, v, n, and p.  For the
 *  vec4 version we can also use u4, v4, n4, and p4.
 *
 *  It is also trivial to transform a point in the plane into world
 *  coordinates, simply by using matrix computation.
 *
 */

typedef union {
    mat4_t mat;
    struct {
        vec3_t u; float u_;
        vec3_t v; float v_;
        vec3_t n; float n_;
        vec3_t p; float p_;
    };
    struct {
        vec4_t u4;
        vec4_t v4;
        vec4_t n4;
        vec4_t p4;
    };
} plane_t;

static inline plane_t plane(vec3_t pos, vec3_t u, vec3_t v)
{
    plane_t ret;
    ret.mat = mat4_identity;
    ret.u = u;
    ret.v = v;
    ret.n = vec3_cross(u, v);
    ret.p = pos;
    return ret;
}

// Check if a plane intersect a line.
// if out is set, it receive the position of the intersection in the
// plane local coordinates.  Apply the plane matrix on it to get the
// object coordinate position.
static inline bool plane_line_intersection(plane_t plane, vec3_t p, vec3_t n,
                                           vec3_t *out)
{
    mat4_t m = mat4_identity;
    m.vecs[0].xyz = plane.u;
    m.vecs[1].xyz = plane.v;
    m.vecs[2].xyz = n;
    if (!mat4_invert(&m)) return false;
    if (out) {
        *out = mat4_mul_vec3(m, vec3_sub(p, plane.p));
        out->z = 0;
    }
    return true;
}

static inline plane_t plane_from_normal(vec3_t pos, vec3_t n)
{
    plane_t ret;
    int i;
    const vec3_t AXES[] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};
    ret.mat = mat4_identity;
    ret.p = pos;
    ret.n = vec3_normalized(n);
    for (i = 0; i < 3; i++) {
        ret.u = vec3_cross(ret.n, AXES[i]);
        if (vec3_norm2(ret.u) > 0) break;
    }
    ret.v = vec3_cross(ret.n, ret.u);
    return ret;
}
