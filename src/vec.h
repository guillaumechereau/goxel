/* Copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <math.h>
#include <stdbool.h>

typedef float real_t;

typedef union {
    struct {
        real_t x;
        real_t y;
    };
    real_t v[2];
} vec2_t;

typedef union {
    struct {
        real_t x;
        real_t y;
        real_t z;
    };
    struct {
        real_t r;
        real_t g;
        real_t b;
    };
    vec2_t xy;
    real_t v[3];
} vec3_t;

typedef union {
    struct {
        real_t x;
        real_t y;
        real_t z;
        real_t w;
    };
    struct {
        real_t r;
        real_t g;
        real_t b;
        real_t a;
    };
    vec3_t xyz;
    vec3_t rgb;
    real_t v[4];
} vec4_t;

typedef union {
    real_t v[16];
    vec4_t vecs[4];
} mat4_t;


typedef union {
    struct {
        real_t x, y, z, w;
    };
    struct {
        vec3_t v;
        real_t a;
    };
} quat_t;

#define VEC2(x, y) {{x, y}}
#define VEC3(x, y, z) {{x, y, z}}
#define VEC4(x, y, z, d) {{x, y, z, d}}

#define VEC3_SPLIT(v) (v).x, (v).y, (v).z
#define VEC4_SPLIT(v) (v).x, (v).y, (v).z, (v).w

#define MAT(...) {{__VA_ARGS__}}

#define DECL static inline

static const vec3_t vec3_zero = VEC3(0, 0, 0);
static const vec4_t vec4_zero = VEC4(0, 0, 0, 0);

static const mat4_t mat4_identity = MAT(1, 0, 0, 0,
                                        0, 1, 0, 0,
                                        0, 0, 1, 0,
                                        0, 0, 0, 1);

static const mat4_t mat4_zero = MAT(0, 0, 0, 0,
                                    0, 0, 0, 0,
                                    0, 0, 0, 0,
                                    0, 0, 0, 0);

static const quat_t quat_identity = {{0, 0, 0, 1}};

DECL vec2_t vec2(real_t x, real_t y)
{
    return (vec2_t)VEC2(x, y);
}

DECL vec3_t vec3(real_t x, real_t y, real_t z)
{
    return (vec3_t)VEC3(x, y, z);
}

DECL vec4_t vec4(real_t x, real_t y, real_t z, real_t w)
{
    return (vec4_t)VEC4(x, y, z, w);
}

DECL bool vec2_equal(vec2_t a, vec2_t b)
{
    return a.x == b.x && a.y == b.y;
}

DECL bool vec3_equal(vec3_t a, vec3_t b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

DECL vec3_t vec3_add(vec3_t a, vec3_t b)
{
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

DECL vec3_t vec3_addk(vec3_t a, vec3_t b, real_t k)
{
    return vec3(a.x + b.x * k, a.y + b.y * k, a.z + b.z * k);
}

DECL void vec3_iaddk(vec3_t *a, vec3_t b, real_t k)
{
    *a = vec3_addk(*a, b, k);
}

DECL void vec3_iadd(vec3_t *a, vec3_t b)
{
    *a = vec3_add(*a, b);
}

DECL vec2_t vec2_sub(vec2_t a, vec2_t b)
{
    return vec2(a.x - b.x, a.y - b.y);
}

DECL vec3_t vec3_sub(vec3_t a, vec3_t b)
{
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

DECL void vec3_isub(vec3_t *a, vec3_t b)
{
    *a = vec3_sub(*a, b);
}

DECL vec3_t vec3_mul(vec3_t a, real_t k)
{
    return vec3(a.x * k, a.y * k, a.z * k);
}

DECL void vec3_imul(vec3_t *a, real_t k)
{
    *a = vec3_mul(*a, k);
}

DECL vec3_t vec3_neg(vec3_t a)
{
    return vec3_mul(a, -1);
}

DECL real_t vec2_dot(vec2_t a, vec2_t b) {
    return a.x * b.x + a.y * b.y;
}
DECL real_t vec3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

DECL real_t vec2_norm2(vec2_t a) {
    return vec2_dot(a, a);
}
DECL real_t vec3_norm2(vec3_t a) {
    return vec3_dot(a, a);
}

DECL real_t vec2_norm(vec2_t a)
{
    return sqrt(vec2_norm2(a));
}

DECL real_t vec3_norm(vec3_t a)
{
    return sqrt(vec3_norm2(a));
}

DECL vec3_t vec3_normalized(vec3_t a)
{
    return vec3_mul(a, 1 / vec3_norm(a));
}

DECL void vec3_normalize(vec3_t *a)
{
    *a = vec3_normalized(*a);
}

DECL real_t vec2_dist2(vec2_t a, vec2_t b)
{
    return vec2_norm2(vec2_sub(a, b));
}

DECL real_t vec2_dist(vec2_t a, vec2_t b)
{
    return sqrt(vec2_dist2(a, b));
}

DECL real_t vec3_dist2(vec3_t a, vec3_t b)
{
    return vec3_norm2(vec3_sub(a, b));
}

DECL vec3_t vec3_mix(vec3_t a, vec3_t b, real_t t)
{
    return vec3(a.x * (1 - t) + b.x * t,
                a.y * (1 - t) + b.y * t,
                a.z * (1 - t) + b.z * t);
}
DECL vec3_t vec3_lerp(vec3_t a, vec3_t b, real_t t) {
    return vec3_mix(a, b, t);
}

DECL vec3_t vec3_lerp_const(vec3_t a, vec3_t b, real_t d)
{
    real_t d2 = vec3_dist2(a, b);
    if (d2 < d * d) return b;
    return vec3_addk(a, vec3_normalized(vec3_sub(b, a)), d);
}
DECL bool vec3_ilerp_const(vec3_t *a, vec3_t b, real_t d)
{
    *a = vec3_lerp_const(*a, b, d);
    return vec3_equal(*a, b);
}

DECL vec3_t vec3_project(vec3_t a, vec3_t b)
{
    return vec3_mul(b, vec3_dot(a, b) / vec3_dot(b, b));
}

DECL vec3_t vec3_cross(vec3_t a, vec3_t b)
{
    return vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

DECL mat4_t mat4(real_t x1, real_t x2, real_t x3, real_t x4,
                 real_t y1, real_t y2, real_t y3, real_t y4,
                 real_t z1, real_t z2, real_t z3, real_t z4,
                 real_t w1, real_t w2, real_t w3, real_t w4)
{
    return (mat4_t)MAT(x1, x2, x3, x4,
                       y1, y2, y3, y4,
                       z1, z2, z3, z4,
                       w1, w2, w3, w4);
}

DECL vec4_t mat4_mul_vec(mat4_t m, vec4_t v)
{
    vec4_t ret = vec4_zero;
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ret.v[i] += m.v[j * 4 + i] * v.v[j];
        }
    }
    return ret;
}

DECL vec3_t mat4_mul_vec3(mat4_t m, vec3_t v)
{
    vec4_t v4 = vec4(v.x, v.y, v.z, 1);
    return mat4_mul_vec(m, v4).xyz;
}

DECL mat4_t mat4_translate(mat4_t m, real_t x, real_t y, real_t z)
{
#define M(row,col)  m.v[(row) * 4 + (col)]
    int i;
    for (i = 0; i < 4; i++) {
       M(4 - 1, i) += M(0, i) * x + M(1, i) * y + M(2, i) * z;
    }
#undef M
    return m;
}

DECL void mat4_itranslate(mat4_t *m, real_t x, real_t y, real_t z)
{
    *m = mat4_translate(*m, x, y, z);
}

DECL mat4_t mat4_scale(mat4_t m, real_t x, real_t y, real_t z)
{
    int i;
    for (i = 0; i < 4; i++) {
        m.v[i] *= x;
        m.v[i + 4] *= y;
        m.v[i + 2 * 4] *= z;
    }
    return m;
}

DECL void mat4_iscale(mat4_t *m, real_t x, real_t y, real_t z)
{
    *m = mat4_scale(*m, x, y, z);
}

DECL bool mat4_invert(mat4_t *mat)
{
    real_t det;
    int i;
    const real_t *m = mat->v;
    real_t inv[16];

#define M(i, j, k) m[i] * m[j] * m[k]
    inv[0] =   M(5, 10, 15) - M(5, 11, 14)  - M(9, 6, 15) +
               M(9, 7, 14)  + M(13, 6, 11)  - M(13, 7, 10);
    inv[4] =  -M(4, 10, 15) + M( 4, 11, 14) + M( 8, 6, 15) -
               M(8, 7, 14)  - M(12,  6, 11) + M(12, 7, 10);
    inv[8] =   M(4,  9, 15) - M( 4, 11, 13) - M(8,  5, 15) +
               M(8,  7, 13) + M(12,  5, 11) - M(12, 7, 9);
    inv[12] = -M(4, 9, 14)  + M(4, 10, 13)  + M(8, 5, 14) -
               M(8, 6, 13)  - M(12, 5, 10)  + M(12, 6, 9);
    inv[1] =  -M(1, 10, 15) + M(1, 11, 14)  + M(9, 2, 15) -
               M(9, 3, 14)  - M(13, 2, 11)  + M(13, 3, 10);
    inv[5] =   M(0, 10, 15) - M(0, 11, 14)  - M(8, 2, 15) +
               M(8, 3, 14)  + M(12, 2, 11)  - M(12, 3, 10);
    inv[9] =  -M(0, 9, 15)  + M(0, 11, 13)  + M(8, 1, 15) -
               M(8, 3, 13)  - M(12, 1, 11)  + M(12, 3, 9);
    inv[13] =  M(0, 9, 14)  - M(0, 10, 13)  - M(8, 1, 14) +
               M(8, 2, 13)  + M(12, 1, 10)  - M(12, 2, 9);
    inv[2] =   M(1, 6, 15)  - M(1, 7, 14)   - M(5, 2, 15) +
               M(5, 3, 14)  + M(13, 2, 7)   - M(13, 3, 6);
    inv[6] =  -M(0, 6, 15)  + M(0, 7, 14)   + M(4, 2, 15) -
               M(4, 3, 14)  - M(12, 2, 7)   + M(12, 3, 6);
    inv[10] =  M(0, 5, 15)  - M(0, 7, 13)   - M(4, 1, 15) +
               M(4, 3, 13)  + M(12, 1, 7)   - M(12, 3, 5);
    inv[14] = -M(0, 5, 14)  + M(0, 6, 13)   + M(4, 1, 14) -
               M(4, 2, 13)  - M(12, 1, 6)   + M(12, 2, 5);
    inv[3] =  -M(1, 6, 11)  + M(1, 7, 10)   + M(5, 2, 11) -
               M(5, 3, 10)  - M(9, 2, 7)    + M(9, 3, 6);
    inv[7] =   M(0, 6, 11)  - M(0, 7, 10)   - M(4, 2, 11) +
               M(4, 3, 10)  + M(8, 2, 7)    - M(8, 3, 6);
    inv[11] = -M(0, 5, 11)  + M(0, 7, 9)    + M(4, 1, 11) -
               M(4, 3, 9)   - M(8, 1, 7)    + M(8, 3, 5);
    inv[15] =  M(0, 5, 10)  - M(0, 6, 9)    - M(4, 1, 10) +
               M(4, 2, 9)   + M(8, 1, 6)    - M(8, 2, 5);
#undef M

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) {
        return false;
    }

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        mat->v[i] = inv[i] * det;
    return true;
}

DECL mat4_t mat4_inverted(mat4_t mat)
{
    mat4_t ret = mat;
    if (mat4_invert(&ret))
        return ret;
    else
        return mat4_zero;
}

DECL void mat4_igrow(mat4_t *m, real_t x, real_t y, real_t z)
{
    // XXX: need to optimize this.
    real_t sx, sy, sz;
    sx = vec3_norm(mat4_mul_vec(*m, vec4(1, 0, 0, 0)).xyz);
    sy = vec3_norm(mat4_mul_vec(*m, vec4(0, 1, 0, 0)).xyz);
    sz = vec3_norm(mat4_mul_vec(*m, vec4(0, 0, 1, 0)).xyz);
    sx = (2 * x + sx) / sx;
    sy = (2 * y + sy) / sy;
    sz = (2 * z + sz) / sz;
    mat4_iscale(m, sx, sy, sz);
}

DECL mat4_t mat4_mul(mat4_t a, mat4_t b)
{
    int i, j, k;
    mat4_t ret;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ret.v[j * 4 + i] = 0.0;
            for (k = 0; k < 4; ++k) {
                ret.v[j * 4 + i] += a.v[k * 4 + i] * b.v[j * 4 + k];
            }
        }
    }
    return ret;
}
DECL void mat4_imul(mat4_t *a, mat4_t b)
{
    *a = mat4_mul(*a, b);
}

DECL mat4_t mat4_ortho(real_t left, real_t right, real_t bottom,
                       real_t top, real_t nearval, real_t farval)
{
    real_t tx = -(right + left) / (right - left);
    real_t ty = -(top + bottom) / (top - bottom);
    real_t tz = -(farval + nearval) / (farval - nearval);
    const mat4_t ret = mat4(
        2 / (right - left), 0, 0, 0,
        0, 2 / (top - bottom), 0, 0,
        0, 0, -2 / (farval - nearval), 0,
        tx, ty, tz, 1
    );
    return ret;
}

DECL quat_t quat_from_axis(real_t a, real_t x, real_t y, real_t z);
DECL mat4_t quat_to_mat4(quat_t q);
DECL mat4_t mat4_rotate(mat4_t m, real_t a, real_t x, real_t y, real_t z)
{
    if (a == 0.0)
        return m;
    mat4_t tmp = mat4_identity;
    real_t s = sin(a);
    real_t c = cos(a);
#define M(row,col)  tmp.v[col * 4 + row]
    if (x == 1.0 && y == 0.0 && z == 0.0) {
        M(1,1) = c;
        M(2,2) = c;
        M(1,2) = -s;
        M(2,1) = s;
    } else if (x == 0.0 && y == 1.0 && z == 0.0) {
        M(0, 0) = c;
        M(2, 2) = c;
        M(0, 2) = s;
        M(2, 0) = -s;
    } else if (x == 0.0 && y == 0.0 && z == 1.0) {
        M(0, 0) = c;
        M(1, 1) = c;
        M(0, 1) = -s;
        M(1, 0) = s;
    } else {
        quat_t quat = quat_from_axis(a, x, y, z);
        tmp = quat_to_mat4(quat);
    }
#undef M
    return mat4_mul(m, tmp);
}
DECL void mat4_irotate(mat4_t *m, real_t a, real_t x, real_t y, real_t z)
{
    *m = mat4_rotate(*m, a, x, y, z);
}

DECL quat_t quat(real_t x, real_t y, real_t z, real_t w) {
   return (quat_t){{x, y, z, w}};
}

DECL quat_t quat_from_axis(real_t a, real_t x, real_t y, real_t z)
{
    real_t sin_angle;
    vec3_t vn;
    a *= 0.5f;

    vn = vec3(x, y, z);
    vec3_normalize(&vn);

    sin_angle = sin(a);

    return quat(vn.x * sin_angle,
                vn.y * sin_angle,
                vn.z * sin_angle,
                cos(a));
}

DECL mat4_t quat_to_mat4(quat_t q)
{
    real_t x, y, z, w;
    x = q.x;
    y = q.y;
    z = q.z;
    w = q.w;
    return mat4(
            1-2*y*y-2*z*z,     2*x*y+2*z*w,     2*x*z-2*y*w,     0,
              2*x*y-2*z*w,   1-2*x*x-2*z*z,     2*y*z+2*x*w,     0,
              2*x*z+2*y*w,     2*y*z-2*x*w,   1-2*x*x-2*y*y,     0,
              0,               0,               0,               1);
}

DECL quat_t quat_conjugate(quat_t q) {
    return quat(-q.x, -q.y, -q.z, q.w);
}

DECL quat_t quat_mul(quat_t a, quat_t b)
{
    return quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
                a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

DECL void quat_imul(quat_t *a, quat_t b)
{
    *a = quat_mul(*a, b);
}

DECL quat_t quat_rotate(quat_t q, real_t a, real_t x, real_t y, real_t z)
{
    quat_t other = quat_from_axis(a, x, y, z);
    return quat_mul(q, other);
}

DECL void quat_irotate(quat_t *q, real_t a, real_t x, real_t y, real_t z)
{
    *q = quat_rotate(*q, a, x, y, z);
}

DECL vec4_t quat_mul_vec4(quat_t q, vec4_t v)
{
    mat4_t m = quat_to_mat4(q);
    return mat4_mul_vec(m, v);
}

DECL mat4_t mat4_mul_quat(mat4_t mat, quat_t q)
{
    mat4_t qm = quat_to_mat4(q);
    return mat4_mul(mat, qm);
}

DECL void mat4_imul_quat(mat4_t *mat, quat_t q)
{
    *mat = mat4_mul_quat(*mat, q);
}
