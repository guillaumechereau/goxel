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

#ifndef _BOX_H_
#define _BOX_H_

#include "goxel.h"

// A Box is represented as the 4x4 matrix that transforms the unit cube into
// the box.
typedef union {
    mat4_t mat;
    struct {
        vec3_t w; float w_;
        vec3_t h; float h_;
        vec3_t d; float d_;
        vec3_t p; float p_;
    };
} box_t;

static inline bool box_is_bbox(box_t b)
{
    int i;
    for (i = 0; i < 12; i++) {
        if (mat4_identity.v[i] == 0 && b.mat.v[i] != 0)
            return false;
    }
    return true;
}

static inline box_t bbox_from_extents(vec3_t pos,
                                      float hw, float hh, float hd)
{
    box_t ret;
    ret.mat = mat4_identity;
    ret.p = pos;
    ret.w.x = hw;
    ret.h.y = hh;
    ret.d.z = hd;
    return ret;
}

static inline box_t box_null() {
    return bbox_from_extents(vec3_zero, -FLT_MAX, -FLT_MAX, -FLT_MAX);
}


// XXX: remove?
static inline box_t bbox_from_points(vec3_t a, vec3_t b)
{
    vec3_t v0, v1;
    v0.x = min(a.x, b.x);
    v0.y = min(a.y, b.y);
    v0.z = min(a.z, b.z);
    v1.x = max(a.x, b.x);
    v1.y = max(a.y, b.y);
    v1.z = max(a.z, b.z);
    return bbox_from_extents(vec3_mix(v0, v1, 0.5),
                            (v1.x - v0.x) / 2,
                            (v1.y - v0.y) / 2,
                            (v1.z - v0.z) / 2);
}

static inline box_t bbox_from_npoints(int n, const vec3_t *points)
{
    assert(n >= 1);
    int i;
    vec3_t v0, v1;
    v0 = v1 = points[0];
    for (i = 1; i < n; i++) {
        v0.x = min(v0.x, points[i].x);
        v0.y = min(v0.y, points[i].y);
        v0.z = min(v0.z, points[i].z);
        v1.x = max(v1.x, points[i].x);
        v1.y = max(v1.y, points[i].y);
        v1.z = max(v1.z, points[i].z);
    }
    return bbox_from_extents(vec3_mix(v0, v1, 0.5),
                            (v1.x - v0.x) / 2,
                            (v1.y - v0.y) / 2,
                            (v1.z - v0.z) / 2);
}

static inline bool bbox_intersect(box_t a, box_t b) {
    assert(box_is_bbox(a));
    assert(box_is_bbox(b));
    vec3_t a0, a1, b0, b1;
    a0 = vec3(a.p.x - a.w.x, a.p.y - a.h.y, a.p.z - a.d.z);
    a1 = vec3(a.p.x + a.w.x, a.p.y + a.h.y, a.p.z + a.d.z);
    b0 = vec3(b.p.x - b.w.x, b.p.y - b.h.y, b.p.z - b.d.z);
    b1 = vec3(b.p.x + b.w.x, b.p.y + b.h.y, b.p.z + b.d.z);
    return a0.x <= b1.x && b0.x <= a1.x &&
           a0.y <= b1.y && b0.y <= a1.y &&
           a0.z <= b1.z && b0.z <= a1.z;
}

static inline box_t bbox_merge(box_t a, box_t b)
{
    assert(box_is_bbox(a));
    assert(box_is_bbox(b));

    vec3_t a0, a1, b0, b1, r0, r1;
    a0 = vec3(a.p.x - a.w.x, a.p.y - a.h.y, a.p.z - a.d.z);
    a1 = vec3(a.p.x + a.w.x, a.p.y + a.h.y, a.p.z + a.d.z);
    b0 = vec3(b.p.x - b.w.x, b.p.y - b.h.y, b.p.z - b.d.z);
    b1 = vec3(b.p.x + b.w.x, b.p.y + b.h.y, b.p.z + b.d.z);

    r0.x = min(a0.x, b0.x);
    r0.y = min(a0.y, b0.y);
    r0.z = min(a0.z, b0.z);
    r1.x = max(a1.x, b1.x);
    r1.y = max(a1.y, b1.y);
    r1.z = max(a1.z, b1.z);

    return bbox_from_extents(vec3_mix(r0, r1, 0.5),
                            (r1.x - r0.x) / 2,
                            (r1.y - r0.y) / 2,
                            (r1.z - r0.z) / 2);
}

static inline bool bbox_contains_vec(box_t b, vec3_t v)
{
    assert(box_is_bbox(b));
    vec3_t b0, b1;
    b0 = vec3(b.p.x - b.w.x, b.p.y - b.h.y, b.p.z - b.d.z);
    b1 = vec3(b.p.x + b.w.x, b.p.y + b.h.y, b.p.z + b.d.z);

    return (b0.x <= v.x && b1.x >= v.x &&
            b0.y <= v.y && b1.y >= v.y &&
            b0.z <= v.z && b1.z >= v.z);
}

static inline box_t box_get_bbox(box_t b)
{
    vec3_t p[8] = {
        vec3(-1, -1, +1),
        vec3(+1, -1, +1),
        vec3(+1, +1, +1),
        vec3(-1, +1, +1),
        vec3(-1, -1, -1),
        vec3(+1, -1, -1),
        vec3(+1, +1, -1),
        vec3(-1, +1, -1),
    };
    int i;
    for (i = 0; i < 8; i++) {
        p[i] = mat4_mul_vec3(b.mat, p[i]);
    }
    return bbox_from_npoints(8, p);
}

static inline box_t bbox_grow(box_t b, float x, float y, float z)
{
    b.w.x += x;
    b.h.y += y;
    b.d.z += z;
    return b;
}

static inline vec3_t box_get_size(box_t b)
{
    return vec3(
        vec3_norm(mat4_mul_vec(b.mat, vec4(1, 0, 0, 0)).xyz),
        vec3_norm(mat4_mul_vec(b.mat, vec4(0, 1, 0, 0)).xyz),
        vec3_norm(mat4_mul_vec(b.mat, vec4(0, 0, 1, 0)).xyz)
    );
}

static inline box_t box_swap_axis(box_t b, int x, int y, int z)
{
    assert(x >= 0 && x <= 2);
    assert(y >= 0 && y <= 2);
    assert(z >= 0 && z <= 2);
    mat4_t m = b.mat;
    b.mat.vecs[0] = m.vecs[x];
    b.mat.vecs[1] = m.vecs[y];
    b.mat.vecs[2] = m.vecs[z];
    return b;
}

#endif // _BOX_H_
