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

#ifndef VEC_H_
#define VEC_H_

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
    struct {
        vec2_t xy;
        vec2_t zw;
    };
    vec3_t xyz;
    vec3_t rgb;
    real_t v[4];
} vec4_t;

typedef union {
    real_t v[9];
    real_t v2[3][3];
    vec3_t vecs[3];
} mat3_t;

typedef union {
    real_t v[16];
    real_t v2[4][4];
    vec4_t vecs[4];
} mat4_t;


typedef union {
    struct {
        real_t w, x, y, z;
    };
    struct {
        real_t a;
        vec3_t vec;
    };
    real_t v[4];
} quat_t;

enum  {
    EULER_ORDER_DEFAULT = 0, // XYZ.
    EULER_ORDER_XYZ = 0,
    EULER_ORDER_XZY,
    EULER_ORDER_YXZ,
    EULER_ORDER_YZX,
    EULER_ORDER_ZXY,
    EULER_ORDER_ZYX
};

#define VEC2(x, y) {{x, y}}
#define VEC3(x, y, z) {{x, y, z}}
#define VEC4(x, y, z, d) {{x, y, z, d}}

#define VEC3_SPLIT(v) (v).x, (v).y, (v).z
#define VEC4_SPLIT(v) (v).x, (v).y, (v).z, (v).w

#define MAT(...) {{__VA_ARGS__}}

#define QUAT(...) ((quat_t){{__VA_ARGS__}})

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

static const quat_t quat_identity = {{1, 0, 0, 0}};

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

DECL void vec2_set(float v[2], float x, float y)
{
    v[0] = x;
    v[1] = y;
}

DECL void vec3_set(float v[3], float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

DECL void vec4_set(float v[4], float x, float y, float z, float w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

DECL void vec2_copy(const float a[2], float out[2])
{
    out[0] = a[0];
    out[1] = a[1];
}

DECL void vec3_copy(const float a[3], float out[3])
{
    out[0] = a[0];
    out[1] = a[1];
    out[2] = a[2];
}

DECL void vec4_copy(const float a[4], float out[4])
{
    out[0] = a[0];
    out[1] = a[1];
    out[2] = a[2];
    out[3] = a[3];
}

DECL bool vec2_equal(const float a[2], const float b[2])
{
    return a[0] == b[0] && a[1] == b[1];
}

DECL bool vec3_equal(const float a[3], const float b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

DECL void vec3_add(const float a[3], const float b[3], float out[3])
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

DECL void vec3_iadd(float a[3], const float b[3])
{
    vec3_add(a, b, a);
}

DECL void vec3_addk(const float a[3], const float b[3], float k, float out[3])
{
    out[0] = a[0] + b[0] * k;
    out[1] = a[1] + b[1] * k;
    out[2] = a[2] + b[2] * k;
}

DECL void vec3_iaddk(float a[3], const float b[3], float k)
{
    vec3_addk(a, b, k, a);
}


DECL void vec2_sub(const float a[2], const float b[2], float out[2])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

DECL void vec3_sub(const float a[3], const float b[3], float out[3])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

DECL void vec3_isub(float a[3], const float b[3])
{
    vec3_sub(a, b, a);
}

DECL void vec2_mul(const float a[2], float k, float out[2])
{
    out[0] = a[0] * k;
    out[1] = a[1] * k;
}

DECL void vec3_mul(const float a[3], float k, float out[3])
{
    out[0] = a[0] * k;
    out[1] = a[1] * k;
    out[2] = a[2] * k;
}

DECL void vec3_imul(float a[3], float k)
{
    vec3_mul(a, k, a);
}

DECL void vec3_neg(const float a[3], float out[3])
{
    vec3_mul(a, -1, out);
}

DECL float vec2_dot(const float a[2], const float b[2])
{
    return a[0] * b[0] + a[1] * b[1];
}

DECL float vec3_dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

DECL float vec2_norm2(const float a[2])
{
    return vec2_dot(a, a);
}

DECL float vec3_norm2(const float a[3])
{
    return vec3_dot(a, a);
}

DECL float vec2_norm(const float a[2])
{
    return sqrt(vec2_norm2(a));
}

DECL void vec2_normalize(const float a[2], float out[2])
{
    vec2_mul(a, 1 / vec2_norm(a), out);
}

DECL float vec3_norm(const float a[3])
{
    return sqrt(vec3_norm2(a));
}

DECL void vec3_normalize(const float a[3], float out[3])
{
    vec3_mul(a, 1 / vec3_norm(a), out);
}

DECL float vec2_dist2(const float a[2], const float b[2])
{
    float u[2];
    vec2_sub(a, b, u);
    return vec2_norm2(u);
}

DECL float vec2_dist(const float a[2], const float b[2])
{
    return sqrt(vec2_dist2(a, b));
}

DECL float vec3_dist2(const float a[3], const float b[3])
{
    float d[3];
    vec3_sub(a, b, d);
    return vec3_norm2(d);
}

DECL float vec3_dist(const float a[3], const float b[3])
{
    return sqrt(vec3_dist2(a, b));
}

DECL void vec2_mix(const float a[2], const float b[2], float t, float out[2])
{
    out[0] = a[0] * (1 - t) + b[0] * t;
    out[1] = a[1] * (1 - t) + b[1] * t;
}

DECL void vec3_mix(const float a[3], const float b[3], float t, float out[3])
{
    out[0] = a[0] * (1 - t) + b[0] * t;
    out[1] = a[1] * (1 - t) + b[1] * t;
    out[2] = a[2] * (1 - t) + b[2] * t;
}

DECL void vec3_lerp(const float a[3], const float b[3], float t, float out[3])
{
    vec3_mix(a, b, t, out);
}

DECL bool vec3_lerp_const(const float a[3], const float b[3], float d,
                          float out[3])
{
    float u[3];
    float d2 = vec3_dist2(a, b);
    if (d2 < d * d) {
        vec3_copy(b, out);
        return true;
    }
    vec3_sub(b, a, u);
    vec3_normalize(u, u);
    vec3_addk(a, u, d, out);
    return false;
}

DECL bool vec3_ilerp_const(float a[3], const float b[3], float d)
{
    return vec3_lerp_const(a, b, d, a);
}

DECL void vec3_project(const float a[3], const float b[3], float out[3])
{
    vec3_mul(b, vec3_dot(a, b) / vec3_dot(b, b), out);
}

DECL float vec2_cross(const float a[2], const float b[2])
{
    return a[0] * b[1] - a[1] * b[0];
}

DECL void vec3_cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
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

DECL void mat4_copy(const float m[4][4], float out[4][4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            out[i][j] = m[i][j];
}

DECL void mat4_mul_vec4(const float m[4][4], const float v[4], float out[4])
{
    float ret[4] = {};
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ret[i] += m[j][i] * v[j];
        }
    }
    vec4_copy(ret, out);
}

DECL void mat4_mul_vec3(const float m[4][4], const float v[3], float out[3])
{
    float v4[4] = {v[0], v[1], v[2], 1.0f};
    mat4_mul_vec4(m, v4, v4);
    vec3_copy(v4, out);
}

DECL void mat4_translate(const float m[4][4], float x, float y, float z,
                         float out[4][4])
{
    float tmp[4][4];
    int i;
    mat4_copy(m, tmp);
#define M(row,col)  tmp[(row)][(col)]
    for (i = 0; i < 4; i++) {
       M(4 - 1, i) += M(0, i) * x + M(1, i) * y + M(2, i) * z;
    }
#undef M
    mat4_copy(tmp, out);
}

DECL void mat4_itranslate(float m[4][4], float x, float y, float z)
{
    mat4_translate(m, x, y, z, m);
}

DECL void mat4_scale(const float m[4][4], float x, float y, float z,
                     float out[4][4])
{
    int i;
    float tmp[4][4];
    mat4_copy(m, tmp);
    for (i = 0; i < 4; i++) {
        tmp[0][i] *= x;
        tmp[1][i] *= y;
        tmp[2][i] *= z;
    }
    mat4_copy(tmp, out);
}

DECL void mat4_iscale(float m[4][4], float x, float y, float z)
{
    mat4_scale(m, x, y, z, m);
}

DECL bool mat4_invert(const float mat[4][4], float out[4][4])
{
    real_t det;
    int i;
    const float *m = (const float*)mat;
    float inv[16];

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

    if (det == 0)
        return false;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        out[i / 4][i % 4] = inv[i] * det;
    return true;
}

DECL void mat4_igrow(float m[4][4], float x, float y, float z)
{
    // XXX: need to optimize this.
    float s[3];
    float v[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    int i;
    for (i = 0; i < 3; i++) {
        mat4_mul_vec4(m, v[i], v[i]);
        s[i] = vec3_norm(v[i]);
    }
    s[0] = (2 * x + s[0]) / s[0];
    s[1] = (2 * y + s[1]) / s[1];
    s[2] = (2 * z + s[2]) / s[2];
    mat4_iscale(m, s[0], s[1], s[2]);
}

DECL void mat4_mul(const float a[4][4], const float b[4][4], float out[4][4])
{
    int i, j, k;
    float ret[4][4] = {};
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ret[j][i] = 0.0;
            for (k = 0; k < 4; ++k) {
                ret[j][i] += a[k][i] * b[j][k];
            }
        }
    }
    mat4_copy(ret, out);
}

DECL void mat4_imul(float a[4][4], const float b[4][4])
{
    mat4_mul(a, b, a);
}

DECL void mat4_ortho(float m[4][4], float left, float right, float bottom,
                     float top, float nearval, float farval)
{
    float tx = -(right + left) / (right - left);
    float ty = -(top + bottom) / (top - bottom);
    float tz = -(farval + nearval) / (farval - nearval);
    const float ret[4][4] = {
        {2 / (right - left), 0, 0, 0},
        {0, 2 / (top - bottom), 0, 0},
        {0, 0, -2 / (farval - nearval), 0},
        {tx, ty, tz, 1},
    };
    mat4_copy(ret, m);
}

DECL void mat4_perspective(float m[4][4], float fovy, float aspect,
                           float nearval, float farval)
{
    float radian = fovy * M_PI / 180.f;
    float f = 1.f / tan(radian / 2.f);
    const float ret[4][4] = {
        {f / aspect, 0.f, 0.f, 0.f},
        {0.f, f, 0.f, 0.f},
        {0.f, 0.f, (farval + nearval) / (nearval - farval), -1.f},
        {0.f, 0.f, 2.f * farval * nearval / (nearval - farval), 0.f},
    };
    mat4_copy(ret, m);
}

DECL void mat4_transpose(const float m[4][4], float out[4][4])
{
    int i, j;
    float ret[4][4] = {};
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            ret[i][j] = m[j][i];
    mat4_copy(ret, out);
}

DECL void mat4_set_identity(float m[4][4])
{
    mat4_copy(mat4_identity.v2, m);
}

// Similar to gluLookAt
DECL void mat4_lookat(float m[4][4],
                      const float eye[3], const float center[3],
                      const float up[3])
{
    float f[3], s[3], u[3], ret[4][4];
    mat4_set_identity(ret);
    vec3_sub(center, eye, f);
    vec3_normalize(f, f);
    vec3_cross(f, up, s);
    vec3_normalize(s, s);
    vec3_cross(s, f, u);
    vec3_normalize(u, u);
    vec3_copy(s, ret[0]);
    vec3_copy(u, ret[1]);
    vec3_neg(f, ret[2]);
    mat4_transpose(ret, ret);
    mat4_translate(ret, -eye[0], -eye[1], -eye[2], ret);
    mat4_copy(ret, m);
}

DECL void quat_copy(const float q[4], float out[4])
{
    out[0] = q[0];
    out[1] = q[1];
    out[2] = q[2];
    out[3] = q[3];
}

DECL void quat_from_axis(float quat[4], float a, float x, float y, float z);
DECL void quat_to_mat4(const float q[4], float out[4][4]);

DECL void mat4_rotate(const float m[4][4], float a, float x, float y, float z,
                      float out[4][4])
{
    if (a == 0.0) {
        mat4_copy(m, out);
        return;
    }
    float tmp[4][4];
    float s = sin(a);
    float c = cos(a);
    mat4_set_identity(tmp);

#define M(row,col)  tmp[col][row]
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
        float quat[4];
        quat_from_axis(quat, a, x, y, z);
        quat_to_mat4(quat, tmp);
    }
#undef M
    mat4_mul(m, tmp, out);
}

DECL void mat4_irotate(float m[4][4], float a, float x, float y, float z)
{
    mat4_rotate(m, a, x, y, z, m);
}

DECL quat_t quat(real_t w, real_t x, real_t y, real_t z) {
   return (quat_t){{w, x, y, z}};
}

DECL void quat_from_axis(float quat[4], float a, float x, float y, float z)
{
    float sin_angle;
    float vn[3] = {x, y, z};
    a *= 0.5f;
    vec3_normalize(vn, vn);
    sin_angle = sin(a);
    quat[0] = cos(a);
    quat[1] = vn[0] * sin_angle;
    quat[2] = vn[1] * sin_angle;
    quat[3] = vn[2] * sin_angle;
}

DECL void quat_to_mat4(const float q[4], float out[4][4])
{
    float w, x, y, z;
    w = q[0];
    x = q[1];
    y = q[2];
    z = q[3];
    const float ret[4][4] = {
            {1-2*y*y-2*z*z,     2*x*y+2*z*w,     2*x*z-2*y*w,  0},
            { 2*x*y-2*z*w,   1-2*x*x-2*z*z,     2*y*z+2*x*w,   0},
            { 2*x*z+2*y*w,     2*y*z-2*x*w,   1-2*x*x-2*y*y,   0},
            { 0,               0,               0,             1}};
    mat4_copy(ret, out);
}

DECL void quat_conjugate(const float q[4], float out[4]) {
    out[0] =  q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] = -q[3];
}

DECL void quat_mul(const float a[4], const float b[4], float out[4])
{
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] + a[2] * b[0] + a[3] * b[1] - a[1] * b[3];
    out[3] = a[0] * b[3] + a[3] * b[0] + a[1] * b[2] - a[2] * b[1];
}

DECL void quat_imul(float a[4], const float b[4])
{
    quat_mul(a, b, a);
}

DECL void quat_rotate(const float q[4], float a, float x, float y, float z,
                      float out[4])
{
    float other[4];
    quat_from_axis(other, a, x, y, z);
    quat_mul(q, other, out);
}

DECL void quat_irotate(float q[4], float a, float x, float y, float z)
{
    quat_rotate(q, a, x, y, z, q);
}

DECL void quat_mul_vec4(const float q[4], const float v[4], float out[4])
{
    float m[4][4];
    quat_to_mat4(q, m);
    mat4_mul_vec4(m, v, out);
}

DECL void mat4_mul_quat(const float mat[4][4], const float q[4],
                        float out[4][4])
{
    float qm[4][4];
    quat_to_mat4(q, qm);
    mat4_mul(mat, qm, out);
}

DECL void mat4_imul_quat(float mat[4][4], const float q[4])
{
    mat4_mul_quat(mat, q, mat);
}

void mat3_to_eul(const float m[3][3], int order, float e[3]);
void quat_to_mat3(const float q[4], float out[3][3]);

DECL void quat_to_eul(const float q[4], int order, float e[3])
{
    float m[3][3];
    quat_to_mat3(q, m);
    mat3_to_eul(m, order, e);
}

void eul_to_quat(const float e[3], int order, float out[4]);

#endif // VEC_H_
