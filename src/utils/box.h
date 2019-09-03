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

#ifndef BOX_H
#define BOX_H

#include "utils/vec.h"

#include <assert.h>

static float min(float x, float y)
{
    return x < y ? x : y;
}

static float max(float x, float y)
{
    return x > y ? x : y;
}

// A Box is represented as the 4x4 matrix that transforms the unit cube into
// the box.

static inline bool box_is_bbox(const float b[4][4])
{
    int i, j;
    for (i = 0; i < 3; i++)
    for (j = 0; j < 4; j++) {
        if (mat4_identity[i][j] == 0 && b[i][j] != 0)
            return false;
    }
    return true;
}

static inline void bbox_from_extents(float box[4][4], const float pos[3],
                                     float hw, float hh, float hd)
{
    mat4_set_identity(box);
    vec3_copy(pos, box[3]);
    box[0][0] = hw;
    box[1][1] = hh;
    box[2][2] = hd;
}

static inline bool box_is_null(const float b[4][4])
{
    return b[3][3] == 0;
}

static inline void bbox_from_aabb(float box[4][4], const int aabb[2][3])
{
    const float pos[3] = {(aabb[1][0] + aabb[0][0]) / 2.f,
                          (aabb[1][1] + aabb[0][1]) / 2.f,
                          (aabb[1][2] + aabb[0][2]) / 2.f};
    const float size[3] = {(float)(aabb[1][0] - aabb[0][0]),
                           (float)(aabb[1][1] - aabb[0][1]),
                           (float)(aabb[1][2] - aabb[0][2])};
    bbox_from_extents(box, pos, size[0] / 2, size[1] / 2, size[2] / 2);
}

static inline void bbox_to_aabb(const float b[4][4], int aabb[2][3])
{
    aabb[0][0] = round(b[3][0] - b[0][0]);
    aabb[0][1] = round(b[3][1] - b[1][1]);
    aabb[0][2] = round(b[3][2] - b[2][2]);
    aabb[1][0] = round(b[3][0] + b[0][0]);
    aabb[1][1] = round(b[3][1] + b[1][1]);
    aabb[1][2] = round(b[3][2] + b[2][2]);
}


// XXX: remove?
static inline void bbox_from_points(
        float box[4][4], const float a[3], const float b[3])
{
    float v0[3], v1[3], mid[3];
    v0[0] = min(a[0], b[0]);
    v0[1] = min(a[1], b[1]);
    v0[2] = min(a[2], b[2]);
    v1[0] = max(a[0], b[0]);
    v1[1] = max(a[1], b[1]);
    v1[2] = max(a[2], b[2]);
    vec3_mix(v0, v1, 0.5, mid);
    bbox_from_extents(box, mid, (v1[0] - v0[0]) / 2,
                                (v1[1] - v0[1]) / 2,
                                (v1[2] - v0[2]) / 2);
}

static inline void bbox_from_npoints(
        float box[4][4], int n, const float (*points)[3])
{
    assert(n >= 1);
    int i;
    float v0[3], v1[3], mid[3];
    vec3_copy(points[0], v0);
    vec3_copy(points[0], v1);
    for (i = 1; i < n; i++) {
        v0[0] = min(v0[0], points[i][0]);
        v0[1] = min(v0[1], points[i][1]);
        v0[2] = min(v0[2], points[i][2]);
        v1[0] = max(v1[0], points[i][0]);
        v1[1] = max(v1[1], points[i][1]);
        v1[2] = max(v1[2], points[i][2]);
    }
    vec3_mix(v0, v1, 0.5, mid);
    bbox_from_extents(box, mid, (v1[0] - v0[0]) / 2,
                                (v1[1] - v0[1]) / 2,
                                (v1[2] - v0[2]) / 2);
}

static inline bool bbox_contains(const float a[4][4], const float b[4][4]) {
    assert(box_is_bbox(a));
    assert(box_is_bbox(b));
    float a0[3], a1[3], b0[3], b1[3];
    vec3_set(a0, a[3][0] - a[0][0], a[3][1] - a[1][1], a[3][2] - a[2][2]);
    vec3_set(a1, a[3][0] + a[0][0], a[3][1] + a[1][1], a[3][2] + a[2][2]);
    vec3_set(b0, b[3][0] - b[0][0], b[3][1] - b[1][1], b[3][2] - b[2][2]);
    vec3_set(b1, b[3][0] + b[0][0], b[3][1] + b[1][1], b[3][2] + b[2][2]);
    return (a0[0] <= b0[0] && a1[0] >= b1[0] &&
            a0[1] <= b0[1] && a1[1] >= b1[1] &&
            a0[2] <= b0[2] && a1[2] >= b1[2]);
}

static inline bool box_contains(const float a[4][4], const float b[4][4])
{
    const float PS[8][3] = {
        {-1, -1, +1},
        {+1, -1, +1},
        {+1, +1, +1},
        {-1, +1, +1},
        {-1, -1, -1},
        {+1, -1, -1},
        {+1, +1, -1},
        {-1, +1, -1},
    };
    float p[3];
    int i;
    float imat[4][4];

    mat4_invert(a, imat);
    for (i = 0; i < 8; i++) {
        mat4_mul_vec3(b, PS[i], p);
        mat4_mul_vec3(imat, p, p);
        if (    p[0] < -1 || p[0] > 1 ||
                p[1] < -1 || p[1] > 1 ||
                p[2] < -1 || p[2] > 1)
            return false;
    }
    return true;
}

static inline bool bbox_contains_vec(const float b[4][4], const float v[3])
{
    assert(box_is_bbox(b));
    float b0[3], b1[3];
    vec3_set(b0, b[3][0] - b[0][0], b[3][1] - b[1][1], b[3][2] - b[2][2]);
    vec3_set(b1, b[3][0] + b[0][0], b[3][1] + b[1][1], b[3][2] + b[2][2]);

    return (b0[0] <= v[0] && b1[0] > v[0] &&
            b0[1] <= v[1] && b1[1] > v[1] &&
            b0[2] <= v[2] && b1[2] > v[2]);
}

static inline void box_get_bbox(const float b[4][4], float out[4][4])
{
    float p[8][3] = {
        {-1, -1, +1},
        {+1, -1, +1},
        {+1, +1, +1},
        {-1, +1, +1},
        {-1, -1, -1},
        {+1, -1, -1},
        {+1, +1, -1},
        {-1, +1, -1},
    };
    int i;
    for (i = 0; i < 8; i++) {
        mat4_mul_vec3(b, p[i], p[i]);
    }
    bbox_from_npoints(out, 8, p);
}

static inline void bbox_grow(const float b[4][4], float x, float y, float z,
                             float out[4][4])
{
    mat4_copy(b, out);
    out[0][0] += x;
    out[1][1] += y;
    out[2][2] += z;
}

static inline void box_get_size(const float b[4][4], float out[3])
{
    float v[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    int i;
    for (i = 0; i < 3; i++) {
        mat4_mul_vec4(b, v[i], v[i]);
        out[i] = vec3_norm(v[i]);
    }
}

static inline void box_swap_axis(const float b[4][4], int x, int y, int z,
                                 float out[4][4])
{
    float m[4][4];
    assert(x >= 0 && x <= 2);
    assert(y >= 0 && y <= 2);
    assert(z >= 0 && z <= 2);
    mat4_copy(b, m);
    mat4_copy(m, out);
    vec4_copy(m[x], out[0]);
    vec4_copy(m[y], out[1]);
    vec4_copy(m[z], out[2]);
}

// Create a new box with the 4 points opposit to the face f and the
// new point.
static inline void box_move_face(
        const float b[4][4], int f, const float p[3], float out[4][4])
{
    const float PS[8][3] = {
        {-1, -1, -1},
        {+1, -1, -1},
        {+1, -1, +1},
        {-1, -1, +1},
        {-1, +1, -1},
        {+1, +1, -1},
        {+1, +1, +1},
        {-1, +1, +1},
    };
    const int FS[6][4] = {
        {0, 1, 2, 3},
        {5, 4, 7, 6},
        {0, 4, 5, 1},
        {2, 6, 7, 3},
        {1, 5, 6, 2},
        {0, 3, 7, 4}
    };
    const int FO[6] = {1, 0, 3, 2, 5, 4};
    float ps[5][3];
    int i;

    // XXX: for the moment we only support bbox, but we could make the
    // function generic.
    assert(box_is_bbox(b));
    f = FO[f];
    for (i = 0; i < 4; i++)
        mat4_mul_vec3(b, PS[FS[f][i]], ps[i]);
    vec3_copy(p, ps[4]);
    bbox_from_npoints(out, 5, ps);
}

static inline float box_get_volume(const float box[4][4])
{
    // The volume is the determinant of the 3x3 matrix of the box
    // time 8 (because the unit cube has a volume of 8).
    const float *v = &box[0][0];
    float a, b, c, d, e, f, g, h, i;
    a = v[0]; b = v[1]; c = v[2];
    d = v[4]; e = v[5]; f = v[6];
    g = v[8]; h = v[9]; i = v[10];
    return 8 * fabs(a*e*i + b*f*g + c*d*h - c*e*g - b*d*i - a*f*h);
}

static inline void box_get_vertices(const float box[4][4],
                                    float vertices[8][3])
{
    int i;
    const float P[8][3] = {
        {-1, -1, -1},
        {+1, -1, -1},
        {+1, -1, +1},
        {-1, -1, +1},
        {-1, +1, -1},
        {+1, +1, -1},
        {+1, +1, +1},
        {-1, +1, +1},
    };
    for (i = 0; i < 8; i++) {
        mat4_mul_vec3(box, P[i], vertices[i]);
    }
}

bool box_intersect_box(const float b1[4][4], const float b2[4][4]);

static inline bool box_intersect_aabb(const float box[4][4],
                                      const int aabb[2][3])
{
    float b2[4][4];
    bbox_from_aabb(b2, aabb);
    return box_intersect_box(box, b2);
}

/*
 * Function: box_union
 * Return a box that constains two other ones.
 */
void box_union(const float a[4][4], const float b[4][4], float out[4][4]);

#endif // BOX_H
