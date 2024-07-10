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

#include "utils/box.h"

#include <limits.h>
#include <string.h>

bool box_is_bbox(const float b[4][4])
{
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            if (mat4_identity[i][j] == 0 && b[i][j] != 0) {
                return false;
            }
        }
    }
    return true;
}

void bbox_from_extents(
        float box[4][4], const float pos[3], float hw, float hh, float hd)
{
    mat4_set_identity(box);
    vec3_copy(pos, box[3]);
    box[0][0] = hw;
    box[1][1] = hh;
    box[2][2] = hd;
}

void bbox_from_aabb(float box[4][4], const int aabb[2][3])
{
    const float pos[3] = {
        (aabb[1][0] + aabb[0][0]) / 2.f,
        (aabb[1][1] + aabb[0][1]) / 2.f,
        (aabb[1][2] + aabb[0][2]) / 2.f,
    };
    const float size[3] = {
        (float)(aabb[1][0] - aabb[0][0]),
        (float)(aabb[1][1] - aabb[0][1]),
        (float)(aabb[1][2] - aabb[0][2]),
    };
    bbox_from_extents(box, pos, size[0] / 2, size[1] / 2, size[2] / 2);
}

void bbox_to_aabb(const float b[4][4], int aabb[2][3])
{
    aabb[0][0] = round(b[3][0] - b[0][0]);
    aabb[0][1] = round(b[3][1] - b[1][1]);
    aabb[0][2] = round(b[3][2] - b[2][2]);
    aabb[1][0] = round(b[3][0] + b[0][0]);
    aabb[1][1] = round(b[3][1] + b[1][1]);
    aabb[1][2] = round(b[3][2] + b[2][2]);
}

void bbox_from_points(float box[4][4], const float a[3], const float b[3])
{
    float v0[3], v1[3], mid[3];
    v0[0] = fmin(a[0], b[0]);
    v0[1] = fmin(a[1], b[1]);
    v0[2] = fmin(a[2], b[2]);
    v1[0] = fmax(a[0], b[0]);
    v1[1] = fmax(a[1], b[1]);
    v1[2] = fmax(a[2], b[2]);
    vec3_mix(v0, v1, 0.5, mid);
    bbox_from_extents(box, mid, (v1[0] - v0[0]) / 2, (v1[1] - v0[1]) / 2,
                      (v1[2] - v0[2]) / 2);
}

void bbox_from_npoints(float box[4][4], int n, const float (*points)[3])
{
    assert(n >= 1);
    int i;
    float v0[3], v1[3], mid[3];
    vec3_copy(points[0], v0);
    vec3_copy(points[0], v1);
    for (i = 1; i < n; i++) {
        v0[0] = fmin(v0[0], points[i][0]);
        v0[1] = fmin(v0[1], points[i][1]);
        v0[2] = fmin(v0[2], points[i][2]);
        v1[0] = fmax(v1[0], points[i][0]);
        v1[1] = fmax(v1[1], points[i][1]);
        v1[2] = fmax(v1[2], points[i][2]);
    }
    vec3_mix(v0, v1, 0.5, mid);
    bbox_from_extents(box, mid, (v1[0] - v0[0]) / 2, (v1[1] - v0[1]) / 2,
                      (v1[2] - v0[2]) / 2);
}

bool bbox_contains(const float a[4][4], const float b[4][4])
{
    assert(box_is_bbox(a));
    assert(box_is_bbox(b));
    float a0[3], a1[3], b0[3], b1[3];
    vec3_set(a0, a[3][0] - a[0][0], a[3][1] - a[1][1], a[3][2] - a[2][2]);
    vec3_set(a1, a[3][0] + a[0][0], a[3][1] + a[1][1], a[3][2] + a[2][2]);
    vec3_set(b0, b[3][0] - b[0][0], b[3][1] - b[1][1], b[3][2] - b[2][2]);
    vec3_set(b1, b[3][0] + b[0][0], b[3][1] + b[1][1], b[3][2] + b[2][2]);
    return (a0[0] <= b0[0] && a1[0] >= b1[0] && a0[1] <= b0[1] &&
            a1[1] >= b1[1] && a0[2] <= b0[2] && a1[2] >= b1[2]);
}

bool box_contains(const float a[4][4], const float b[4][4])
{
    const float PS[8][3] = {
        { -1, -1, +1 }, { +1, -1, +1 }, { +1, +1, +1 }, { -1, +1, +1 },
        { -1, -1, -1 }, { +1, -1, -1 }, { +1, +1, -1 }, { -1, +1, -1 },
    };
    float p[3];
    int i;
    float imat[4][4];

    mat4_invert(a, imat);
    for (i = 0; i < 8; i++) {
        mat4_mul_vec3(b, PS[i], p);
        mat4_mul_vec3(imat, p, p);
        if (p[0] < -1 || p[0] > 1 || p[1] < -1 || p[1] > 1 || p[2] < -1 ||
            p[2] > 1)
            return false;
    }
    return true;
}

bool bbox_contains_vec(const float b[4][4], const float v[3])
{
    assert(box_is_bbox(b));
    float b0[3], b1[3];
    vec3_set(b0, b[3][0] - b[0][0], b[3][1] - b[1][1], b[3][2] - b[2][2]);
    vec3_set(b1, b[3][0] + b[0][0], b[3][1] + b[1][1], b[3][2] + b[2][2]);

    return (b0[0] <= v[0] && b1[0] > v[0] && b0[1] <= v[1] && b1[1] > v[1] &&
            b0[2] <= v[2] && b1[2] > v[2]);
}

void box_get_bbox(const float b[4][4], float out[4][4])
{
    float p[8][3] = {
        { -1, -1, +1 }, { +1, -1, +1 }, { +1, +1, +1 }, { -1, +1, +1 },
        { -1, -1, -1 }, { +1, -1, -1 }, { +1, +1, -1 }, { -1, +1, -1 },
    };
    int i;
    for (i = 0; i < 8; i++) {
        mat4_mul_vec3(b, p[i], p[i]);
    }
    bbox_from_npoints(out, 8, p);
}

void bbox_grow(const float b[4][4], float x, float y, float z, float out[4][4])
{
    mat4_copy(b, out);
    out[0][0] += x;
    out[1][1] += y;
    out[2][2] += z;
}

void box_get_size(const float b[4][4], float out[3])
{
    float v[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };
    int i;
    for (i = 0; i < 3; i++) {
        mat4_mul_vec4(b, v[i], v[i]);
        out[i] = vec3_norm(v[i]);
    }
}

void box_swap_axis(const float b[4][4], int x, int y, int z, float out[4][4])
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

void box_move_face(
        const float b[4][4], int f, const float p[3], float out[4][4])
{
    float inv[4][4];
    float p_inv[3];
    float aabb[2][3] = { { -1, -1, -1 }, { +1, +1, +1 } };
    float ret[4][4];
    // face -> direction, axis.
    const int F[6][2] = {
        { 0, 1 }, { 1, 1 }, { 0, 2 }, { 1, 2 }, { 1, 0 }, { 0, 0 },
    };
    mat4_invert(b, inv);
    mat4_mul_vec3(inv, p, p_inv);
    aabb[F[f][0]][F[f][1]] = p_inv[F[f][1]];
    bbox_from_npoints(ret, 2, aabb);
    mat4_mul(b, ret, out);
}

float box_get_volume(const float box[4][4])
{
    // The volume is the determinant of the 3x3 matrix of the box
    // time 8 (because the unit cube has a volume of 8).
    const float *v = &box[0][0];
    float a, b, c, d, e, f, g, h, i;
    a = v[0];
    b = v[1];
    c = v[2];
    d = v[4];
    e = v[5];
    f = v[6];
    g = v[8];
    h = v[9];
    i = v[10];
    return 8 * fabs(a * e * i + b * f * g + c * d * h - c * e * g - b * d * i -
                    a * f * h);
}

void box_get_vertices(const float box[4][4], float vertices[8][3])
{
    int i;
    const float P[8][3] = {
        { -1, -1, -1 }, { +1, -1, -1 }, { +1, -1, +1 }, { -1, -1, +1 },
        { -1, +1, -1 }, { +1, +1, -1 }, { +1, +1, +1 }, { -1, +1, +1 },
    };
    for (i = 0; i < 8; i++) {
        mat4_mul_vec3(box, P[i], vertices[i]);
    }
}

bool box_intersect_aabb(const float box[4][4], const int aabb[2][3])
{
    float b2[4][4];
    bbox_from_aabb(b2, aabb);
    return box_intersect_box(box, b2);
}

static bool box_intersect_box_(const float b1[4][4], const float b2[4][4])
{
    float inv[4][4], box[4][4], vertices[8][3];
    int i, p;
    // The six planes equations:
    const int P[6][4] = {
        {-1, 0, 0, -1}, {1, 0, 0, -1},
        {0, -1, 0, -1}, {0, 1, 0, -1},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    if (!mat4_invert(b1, inv)) return false;
    mat4_mul(inv, b2, box);
    box_get_vertices(box, vertices);
    for (p = 0; p < 6; p++) {
        for (i = 0; i < 8; i++) {
            if (    P[p][0] * vertices[i][0] +
                    P[p][1] * vertices[i][1] +
                    P[p][2] * vertices[i][2] +
                    P[p][3] * 1 < 0) {
                break;
            }
        }
        if (i == 8) // All the points are outside a clipping plane.
            return false;
    }
    return true;
}

bool box_intersect_box(const float b1[4][4], const float b2[4][4])
{
    return box_intersect_box_(b1, b2) || box_intersect_box_(b2, b1);
}

void box_union(const float a[4][4], const float b[4][4], float out[4][4])
{
    float verts[16][3];

    if (box_is_null(a)) {
        mat4_copy(b, out);
        return;
    }
    if (box_is_null(b)) {
        mat4_copy(a, out);
        return;
    }

    box_get_vertices(a, verts + 0);
    box_get_vertices(b, verts + 8);
    bbox_from_npoints(out, 16, verts);
}

void box_get_aabb(const float box[4][4], int aabb[2][3])
{
    const float vertices[8][4] = {
        {-1, -1, +1, 1},
        {+1, -1, +1, 1},
        {+1, +1, +1, 1},
        {-1, +1, +1, 1},
        {-1, -1, -1, 1},
        {+1, -1, -1, 1},
        {+1, +1, -1, 1},
        {-1, +1, -1, 1}};
    int i;
    int ret[2][3] = {{INT_MAX, INT_MAX, INT_MAX},
                     {INT_MIN, INT_MIN, INT_MIN}};
    float p[4];
    for (i = 0; i < 8; i++) {
        mat4_mul_vec4(box, vertices[i], p);
        ret[0][0] = fmin(ret[0][0], (int)floor(p[0]));
        ret[0][1] = fmin(ret[0][1], (int)floor(p[1]));
        ret[0][2] = fmin(ret[0][2], (int)floor(p[2]));
        ret[1][0] = fmax(ret[1][0], (int)ceil(p[0]));
        ret[1][1] = fmax(ret[1][1], (int)ceil(p[1]));
        ret[1][2] = fmax(ret[1][2], (int)ceil(p[2]));
    }
    memcpy(aabb, ret, sizeof(ret));
}

void bbox_extends_from_points(
        const float b[4][4], int n, const float (*points)[3], float out[4][4])
{
    float b2[4][4];
    bbox_from_npoints(b2, n, points);
    box_union(b, b2, out);
}
