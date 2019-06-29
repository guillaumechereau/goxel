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

static const float plane_null[4][4] = {};

static inline void plane_from_vectors(float plane[4][4],
        const float pos[3], const float u[3], const float v[3])
{
    mat4_set_identity(plane);
    vec3_copy(u, plane[0]);
    vec3_copy(v, plane[1]);
    vec3_cross(u, v, plane[2]);
    vec3_copy(pos, plane[3]);
}

static inline bool plane_is_null(const float p[4][4]) {
    return p[3][3] == 0;
}

// Check if a plane intersect a line.
// if out is set, it receive the position of the intersection in the
// plane local coordinates.  Apply the plane matrix on it to get the
// object coordinate position.
static inline bool plane_line_intersection(const float plane[4][4],
        const float p[3], const float n[3], float out[3])
{
    float v[3], m[4][4];
    mat4_set_identity(m);
    vec3_copy(plane[0], m[0]);
    vec3_copy(plane[1], m[1]);
    vec3_copy(n, m[2]);
    if (!mat4_invert(m, m)) return false;
    if (out) {
        vec3_sub(p, plane[3], v);
        mat4_mul_vec3(m, v, out);
        out[2] = 0;
    }
    return true;
}

static inline void plane_from_normal(float plane[4][4],
                                     const float pos[3], const float n[3])
{
    int i;
    const float AXES[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    mat4_set_identity(plane);
    vec3_copy(pos, plane[3]);
    vec3_normalize(n, plane[2]);
    for (i = 0; i < 3; i++) {
        vec3_cross(plane[2], AXES[i], plane[0]);
        if (vec3_norm2(plane[0]) > 0) break;
    }
    vec3_cross(plane[2], plane[0], plane[1]);
}


static inline bool plane_triangle_intersection(const float plane[4][4],
                    const float triangle[3][3], float segs[2][3])
{
    int i;
    float pinv[4][4], tri[3][3], r0, r1;

    // Turn triangle into plane local coordinates.
    if (!mat4_invert(plane, pinv)) return false;
    for (i = 0; i < 3; i++)
        mat4_mul_vec3(pinv, triangle[i], tri[i]);

    for (i = 0; i < 3; i++) {
        if (tri[i][2] != tri[(i + 1) % 3][2] &&
            tri[i][2] != tri[(i + 2) % 3][2]) break;
    }
    if (i == 3) return false;

    r0 = -tri[i][2] / (tri[(i + 1) % 3][2] - tri[i][2]);
    r1 = -tri[i][2] / (tri[(i + 2) % 3][2] - tri[i][2]);

    vec3_mix(triangle[i], triangle[(i + 1) % 3], r0, segs[0]);
    vec3_mix(triangle[i], triangle[(i + 2) % 3], r1, segs[1]);
    return true;
}
