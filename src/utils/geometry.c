
/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "geometry.h"

#include <stdio.h>
#include <math.h>

static float dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/*
 * Function to compute the distance between two rays and find the closest
 * points
 */
float rays_distance(const float o1[3], const float d1[3],
                    const float o2[3], const float d2[3],
                    float *t1, float *t2)
{
    float u[3], v[3], w[3];
    float a, b, c, d, e, det, sc, tc;
    float dp[3];
    float p1[3], p2[3];
    
    u[0] = d1[0];
    u[1] = d1[1];
    u[2] = d1[2];
    
    v[0] = d2[0];
    v[1] = d2[1];
    v[2] = d2[2];
    
    w[0] = o1[0] - o2[0];
    w[1] = o1[1] - o2[1];
    w[2] = o1[2] - o2[2];

    a = dot(u, u);
    b = dot(u, v);
    c = dot(v, v);
    d = dot(u, w);
    e = dot(v, w);

    det = a * c - b * b;

    if (det < 1e-8) {
        sc = 0.0;
        tc = (b > c ? d / b : e / c);
    } else {
        sc = (b * e - c * d) / det;
        tc = (a * e - b * d) / det;
    }

    if (t1) *t1 = sc;
    if (t2) *t2 = tc;

    p1[0] = o1[0] + sc * u[0];
    p1[1] = o1[1] + sc * u[1];
    p1[2] = o1[2] + sc * u[2];

    p2[0] = o2[0] + tc * v[0];
    p2[1] = o2[1] + tc * v[1];
    p2[2] = o2[2] + tc * v[2];

    dp[0] = p1[0] - p2[0];
    dp[1] = p1[1] - p2[1];
    dp[2] = p1[2] - p2[2];
    return sqrt(dot(dp, dp));
}
