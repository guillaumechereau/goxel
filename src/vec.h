/* noc_vec
 *
 * Copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * NOC_VEC
 * =======
 *
 * Simple implementation of 2D, 3D and 4D vectors, 2x2, 3x3 and 4x4 matrix,
 * and quaternions.
 *
 * Usage
 * -----
 *
 * All the functions are defined as static inline, so you can just include
 * this file anywhere you want to use the library.
 *
 * By default the library uses float, if you want to use double, add the line:
 *
 * #define NOCVEC_REAL_TYPE double
 *
 * before including the file.
 *
 * Types
 * -----
 *
 * The following types are defined:
 *
 * vec2_t : 2D vector
 * vec3_t : 3D vector
 * vec4_t : 4D vector
 * mat2_t : 2x2 matrix
 * mat3_t : 3x3 matrix
 * mat4_t : 4x4 matrix
 * quat_t : quaternion
 *
 * Vectors members access
 * ----------------------
 *
 * The vectors are defined as union, so that we can access the attributes
 * in different ways:
 *
 * - as x,y,z,w coordinates.
 * - as r,g,b,a coordinates.
 * - as v, defined as an array of values.
 *
 *     a.x == a.r == a.v[0]
 *     a.y == a.g == a.v[1]
 *     a.z == a.b == a.v[2]
 *     a.w == a.a == a.v[3]
 *
 * For vec3_t and vec4_t, the attributes 'xy' and 'rg' map to the xy
 * component of the vector:
 *
 *     vec3_t a;
 *     vec2_t xy = a.xy;
 *
 * For vec4_t, the attributes 'xyz' and 'rgb' map to the xyz component of the
 * vector:
 *
 *     vec4_t a;
 *     vec3_t xyz = a.xyz;
 *
 * You can create a vector with the vec2, vec3 and vec4 functions.
 *
 * vec2_zero, vec3_zero and vec4_zero are static const 0 vectors.
 *
 *
 * Vectors functions
 * -----------------
 *
 * I use the notation vecN_xxx to refer to the functions vec2_xxx, vec3_xxx
 * and vec4_xxx.
 *
 * Some functions have an in place form, where the operation applies directly
 * to the first argument of the function (given as a pointer).  This can
 * sometime make the code easier to write:
 *
 *     a = vec2_add(a, b);
 *
 * Is equivalent to:
 *
 *     vec2_iadd(&a, b);
 *
 * vecN(...)            Create a vector from coordinates
 * vecN_[i]equal        Comparison
 * vecN_[i]add          Addition
 * vecN_[i]sub          Substraction
 * vecN_[i]mul          Multiplication by a scalar
 * vecN_[i]div          Division by a scalar
 * vecN_[i]addk         Multiplication by scalar and addition: c = a + b * k
 * vecN_[i]neg          Negation
 * vecN_dot             Scalar product
 * vecN_norm            Norm
 * vecN_norm2           Square of the norm (this is faster than vecN_norm)
 * vecN_normalized      Normalize the vector
 * vecN_normalize       Normalize in place
 * vecN_dist            Distance between two vectors
 * vecN_dist2           Square distance between two vectors
 * vecN_lerp            Linear interpolation
 * vecN_mix             Same as vecN_lerp
 * vec2_cross           2D cross product (return a scalar)
 * vec3_cross           3D cross product
 * vec2_rot             2D rotation by an angle in radian
 * vec2_perp            2D rotation by +90 deg
 * vec2_rperp           2D rotation by -90 deg
 * vec2_angle           Return the 2D angle of the vector
 * vec2_rotate          Rotate a vector by an other vector
 * vec2_unrotate        Inverse of vec2_rotate
 *
 * Matrix
 * ------
 *
 * The 3 types of matrices are mat2_t, mat3_t and mat4_t.  We can directly
 * access the values in two ways:
 *
 * - Using the attribute v, defined as an array of NxN scalar values.
 * - Usine the attribute vecs, defined as an array of N vecN_t:
 *
 *     mat4_t m;
 *     m.v[1] == m.vecs[0].y;
 *     m.v[5] == m.vecs[1].x;
 *
 * Predefined matrices:
 *
 *     matN_zero
 *     matN_identity
 *
 * Matrix functions
 * ----------------
 *
 * matN                 Matric creation
 * matN_[i]mul          Matrix multiplication (a * b)
 * matN_[i]premul       Matrix multiplication (b * a)
 * matN_[i]translate    Translation
 * matN_[i]scale        Scalar multiplication
 * matN_mul_vec         Apply matrix on a vector
 * mat4_[i]mul_quat     Multiply by a quaternion
 * mat4_[i]rotate       Rotation
 * mat4_mul_vec3        Apply 4x4 matrix on a 3D vector (assuming w = 1)
 * mat4_perspective     Create a perspective projection matrix
 * mat4_frustum         Create a frustum projection matrix
 * mat4_ortho           Create an orthographic projection matrix
 * mat4_invert          Invert matrix in place
 * mat4_inverted        Invert matrix (return mat4_zero if no result)
 *
 *
 * Quaternion
 * ----------
 *
 * A quaternion has the type quat_t.  We can access attributes as:
 *
 * direct values: x, y, z, w
 * a 3d vector and a scalar: v and a
 *
 * quat_identity is a const quaternion defined as (0, 0, 0, 1)
 *
 * Quaternion functions
 * --------------------
 *
 * quat                 Create quaternion from 4 scalar values
 * quat_from_axis       Create quaternion from an angle and a 3d vector
 * quat_[i]conjugate    Conjugate of a quaternion
 * quat_norm            Norm
 * quat_invert          Inverse the quaternion
 * quat_[i]mul          Multiply two quaternion
 * quat_[i]rotate       Rotation
 * quat_normalized      Normalize
 * quat_normalize       Normalize in place
 * quat_to_mat3         Convert to 3x3 matrix
 * quat_to_mat4         Convert to 4x4 matrix
 * quat_mul_vec3        Apply quaternion to a 3D vector
 * quat_mul_vec4        Apply quaternion to a 4D vector
 * quat_slerp           Spherical linear interpolation of two quaternions
 * quat_get_axis_angle  Get axis and angle of the quaternion
 */

#include <math.h>
#include <stdbool.h>

#ifndef N
    #define N 2
    #include "vec.h"
    #undef N

    #define N 3
    #include "vec.h"
    #undef N

    #define N 4
    #include "vec.h"
    #undef N
#else

#define NOC_DEF static inline

// XXX: I remove all the cpp operators because it makes some trouble with
// the gui code.
// #ifdef __cplusplus
#if 0
   #define NOC_CPP(x_) x_
#else
   #define NOC_CPP(x_)
#endif

#if N == 2
    #define TAKEC(x_, y_, z_, w_) x_, y_
    #define TAKE( x_, y_, z_, w_) x_  y_
    #define NE2(x_) x_
    #define NE3(x_)
    #define NGE2(x_) x_
    #define NGE3(x_)
    #define NGE4(x_)
    #define VEC2_SPLIT(v) (v).x, (v).y
#elif N == 3
    #define TAKEC(x_, y_, z_, w_) x_, y_, z_
    #define TAKE( x_, y_, z_, w_) x_  y_  z_
    #define NE2(x_)
    #define NE3(x_) x_
    #define NGE2(x_) x_
    #define NGE3(x_) x_
    #define NGE4(x_)
    #define VEC3_SPLIT(v) (v).x, (v).y, (v).z
#elif N == 4
    #define TAKEC(x_, y_, z_, w_) x_, y_, z_, w_
    #define TAKE( x_, y_, z_, w_) x_  y_  z_  w_
    #define NE2(x_)
    #define NE3(x_)
    #define NGE2(x_) x_
    #define NGE3(x_) x_
    #define NGE4(x_) x_
    #define VEC4_SPLIT(v) (v).x, (v).y, (v).z, (v).w
#endif

// Optional prefix and suffix
#ifndef NOC_VEC_PREF
    #define NOC_VEC_PREF
#endif
#ifndef NOC_VEC_SUF
    #define NOC_VEC_SUF
#endif

#define VNAME__(n_, pref_, suf_, x_) pref_##vec##n_##suf_##x_
#define VNAME_(n_, pref_, suf_, x_) VNAME__(n_, pref_, suf_, x_)
#define VNAME(x_) VNAME_(N, NOC_VEC_PREF, NOC_VEC_SUF, x_)
#define VNAMEN(n_, x_) VNAME_(n_, NOC_VEC_PREF, NOC_VEC_SUF, x_)
#define VT VNAME(_t)
#define V3T VNAMEN(3, _t)
#define V4T VNAMEN(4, _t)
#define VF(name, ...) VNAME(_##name)(__VA_ARGS__)
#ifdef NOCVEC_REAL_TYPE
    #define real_t NOCVEC_REAL_TYPE
#else
    #define real_t float
#endif

#define MNAME__(n_, x_) mat##n_##x_
#define MNAME_(n_, x_) MNAME__(n_, x_)
#define MNAME(x_) MNAME_(N, x_)
#define MT MNAME(_t)
#define MF(name, ...) MNAME(_##name)(__VA_ARGS__)

typedef union {
    struct { real_t TAKEC(x, y, z, w); };
    struct { real_t TAKEC(r, g, b, a); };
    NGE3(VNAMEN(2, _t) xy;  VNAMEN(2, _t) rg;)
    NGE4(VNAMEN(3, _t) xyz; VNAMEN(3, _t) rgb;)
    real_t v[N];
} VT;

static const VT VNAME(_zero) = {{TAKEC(0, 0, 0, 0)}};

NOC_DEF VT VNAME()(TAKEC(real_t x NOC_CPP(=0),
                         real_t y NOC_CPP(=0),
                         real_t z NOC_CPP(=0),
                         real_t w NOC_CPP(=0)))
{
    return (VT){{TAKEC(x, y, z, w)}};
}

NOC_DEF bool VNAME(_equal)(VT a, VT b)
{
    int i;
    for (i = 0; i < N; i++)
        if (a.v[i] != b.v[i]) return false;
    return true;
}
NOC_CPP(NOC_DEF bool operator==(VT a, VT b) {return VF(equal, a, b);})

// vecN_add
NOC_DEF VT VNAME(_add)(VT a, VT b)
{
    return VNAME()(TAKEC(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w));
}
NOC_DEF void VNAME(_iadd)(VT *a, VT b)
{
    *a = VF(add, *a, b);
}
NOC_CPP(NOC_DEF VT operator+(VT a, VT b) {return VF(add, a, b);})
NOC_CPP(NOC_DEF VT& operator+=(VT &a, VT b) {VF(iadd, &a, b); return a;})

// vecN_addk
NOC_DEF VT VNAME(_addk)(VT a, VT b, real_t k)
{
    return VNAME()(TAKEC(a.x + b.x * k,
                         a.y + b.y * k,
                         a.z + b.z * k,
                         a.w + b.w * k));
}
NOC_DEF void VNAME(_iaddk)(VT *a, VT b, real_t k)
{
    *a = VF(addk, *a, b, k);
}

// vecN_sub
NOC_DEF VT VNAME(_sub)(VT a, VT b)
{
    return VNAME()(TAKEC(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w));
}
NOC_DEF void VNAME(_isub)(VT *a, VT b)
{
    *a = VF(sub, *a, b);
}
NOC_CPP(NOC_DEF VT operator-(VT a, VT b) {return VF(sub, a, b);})
NOC_CPP(NOC_DEF VT& operator-=(VT &a, VT b) {VF(isub, &a, b); return a;})

// vecN_neg
NOC_DEF VT VNAME(_neg)(VT a)
{
    return VNAME()(TAKEC(-a.x, -a.y, -a.z, -a.w));
}
NOC_DEF void VNAME(_ineg)(VT *a)
{
    *a = VF(neg, *a);
}
NOC_CPP(NOC_DEF VT operator-(VT a) {return VF(neg, a);})

// vecN_mul
NOC_DEF VT VNAME(_mul)(VT a, real_t k)
{
    return VNAME()(TAKEC(a.x * k, a.y * k, a.z * k, a.w * k));
}
NOC_DEF void VNAME(_imul)(VT *a, real_t k)
{
    *a = VF(mul, *a, k);
}
NOC_CPP(NOC_DEF VT operator*(VT a, real_t k) {return VF(mul, a, k);})
NOC_CPP(NOC_DEF VT& operator*=(VT &a, real_t k) {return a = a * k;})

// vecN_div
NOC_DEF VT VNAME(_div)(VT a, real_t k)
{
    return VNAME()(TAKEC(a.x / k, a.y / k, a.z / k, a.w / k));
}
NOC_DEF void VNAME(_idiv)(VT *a, real_t k)
{
    *a = VF(div, *a, k);
}
NOC_CPP(NOC_DEF VT operator/(VT a, real_t k) {return VF(div, a, k);})
NOC_CPP(NOC_DEF VT& operator/=(VT &a, real_t k) {VF(idiv, &a, k); return a;})

// vecN_dot
NOC_DEF real_t VNAME(_dot)(VT a, VT b)
{
    real_t ret = 0;
    int i;
    for (i = 0; i < N; i++)
        ret += a.v[i] * b.v[i];
    return ret;
}
NOC_CPP(NOC_DEF real_t operator*(VT a, VT b) {return VF(dot, a, b);})

NOC_DEF real_t VNAME(_norm2)(VT a)
{
    return VF(dot, a, a);
}

NOC_DEF real_t VNAME(_norm)(VT a)
{
    return sqrt(VF(norm2, a));
}

NOC_DEF VT VNAME(_normalized)(VT a)
{
    return VF(mul, a, 1 / VF(norm, a));
}

NOC_DEF void VNAME(_normalize)(VT *a)
{
    *a = VF(normalized, *a);
}

NOC_DEF real_t VNAME(_dist2)(VT a, VT b)
{
    return VF(norm2, VF(sub, a, b));
}

NOC_DEF real_t VNAME(_dist)(VT a, VT b)
{
    return sqrt(VF(dist2, a, b));
}

// Cross product: vecN_cross
NE2(
    NOC_DEF real_t VNAME(_cross)(VT a, VT b) {
        return a.x * b.y - a.y * b.x;
    }
    NOC_CPP(NOC_DEF real_t operator^(VT a, VT b) {return VF(cross, a, b);})
)
NE3(
    NOC_DEF VT VNAME(_cross)(VT a, VT b) {
        return VNAME()(a.y * b.z - a.z * b.y,
                       a.z * b.x - a.x * b.z,
                       a.x * b.y - a.y * b.x);
    }
    NOC_CPP(NOC_DEF VT operator^(VT a, VT b) {return VF(cross, a, b);})
)

NOC_DEF VT VNAME(_mix)(VT a, VT b, real_t t)
{
    return VNAME()(TAKEC(a.x * (1 - t) + b.x * t,
                         a.y * (1 - t) + b.y * t,
                         a.z * (1 - t) + b.z * t,
                         a.w * (1 - t) + b.w * t));
}

NOC_DEF VT VNAME(_lerp)(VT a, VT b, real_t t)
{
    return VF(mix, a, b, t);
}

NOC_DEF VT VNAME(_project)(VT a, VT b)
{
    return VF(mul, b, VF(dot, a, b) / VF(dot, b, b));
}

NE2(
    NOC_DEF VT VNAME(_rot)(VT v, real_t a)
    {
        return VNAME()(v.x * cos(a) - v.y * sin(a),
                       v.x * sin(a) + v.y * cos(a));
    }

    NOC_DEF VT VNAME(_perp)(VT v)
    {
        return VNAME()(-v.y, v.x);
    }

    NOC_DEF VT VNAME(_rperp)(VT v)
    {
        return VNAME()(v.y, -v.x);
    }

    NOC_DEF real_t VNAME(_angle)(VT v)
    {
        return atan2(v.y, v.x);
    }

    NOC_DEF VT VNAME(_rotate)(VT a, VT b)
    {
        return VNAME()(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
    }

    NOC_DEF VT VNAME(_unrotate)(VT a, VT b)
    {
        return VNAME()(a.x * b.x + a.y * b.y, a.y * b.x + a.x * b.y);
    }
)


// Matrix

typedef union {
    VT     vecs[N];
    real_t v[N * N];
} MT;

static const MT MNAME(_zero) = {{
    TAKEC(
        {{TAKEC(0, 0, 0, 0)}},
        {{TAKEC(0, 0, 0, 0)}},
        {{TAKEC(0, 0, 0, 0)}},
        {{TAKEC(0, 0, 0, 0)}}
    )
}};

static const MT MNAME(_identity) = {{
    TAKEC(
        {{TAKEC(1, 0, 0, 0)}},
        {{TAKEC(0, 1, 0, 0)}},
        {{TAKEC(0, 0, 1, 0)}},
        {{TAKEC(0, 0, 0, 1)}}
    )
}};

NOC_DEF MT MNAME()(TAKEC(
            TAKEC(real_t x0, real_t y0, real_t z0, real_t w0),
            TAKEC(real_t x1, real_t y1, real_t z1, real_t w1),
            TAKEC(real_t x2, real_t y2, real_t z2, real_t w2),
            TAKEC(real_t x3, real_t y3, real_t z3, real_t w3)))
{
    return (MT) {{
        TAKEC(
            {{TAKEC(x0, y0, z0, w0)}},
            {{TAKEC(x1, y1, z1, w1)}},
            {{TAKEC(x2, y2, z2, w2)}},
            {{TAKEC(x3, y3, z3, w3)}}
        )
    }};
}

NOC_DEF MT MNAME(_mul)(MT a, MT b)
{
    int i, j, k;
    MT ret;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            ret.v[j*N + i] = 0.0;
            for (k = 0; k < N; ++k) {
                ret.v[j*N + i] += a.v[k*N + i] * b.v[j*N + k];
            }
        }
    }
    return ret;
}
NOC_DEF void MNAME(_imul)(MT *a, MT b)
{
    *a = MF(mul, *a, b);
}
NOC_CPP(MT operator*(MT a, MT b) {return MF(mul, a, b);})
NOC_CPP(MT operator*=(MT &a, MT b) {return a = a * b;})

NOC_DEF MT MNAME(_premul)(MT a, MT b)
{
    return MF(mul, b, a);
}
NOC_DEF void MNAME(_ipremul)(MT *a, MT b)
{
    *a = MF(premul, *a, b);
}

NOC_DEF VT MNAME(_mul_vec)(MT m, VT v)
{
    VT ret = {{0}};
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            ret.v[i] += m.v[j*N + i] * v.v[j];
        }
    }
    return ret;
}
NOC_CPP(NOC_DEF VT operator*(VT v, MT m) {return MF(mul_vec, m, v);})
NOC_CPP(NOC_DEF VT& operator*=(VT &v, MT m) {return v = v * m;})

NOC_DEF MT MNAME(_translate)(TAKEC(MT m, real_t x, real_t y, real_t z))
{
#define M(row,col)  m.v[(row) * N + (col)]
    int i;
    for (i = 0; i < N; i++) {
       M(N - 1, i) += TAKE( 0,
                            + M(0, i) * x,
                            + M(1, i) * y,
                            + M(2, i) * z);
    }
#undef M
    return m;
}
NOC_DEF void MNAME(_itranslate)(TAKEC(MT *m, real_t x, real_t y, real_t z))
{
    *m = MF(translate, TAKEC(*m, x, y, z));
}

NOC_DEF MT MNAME(_scale)(TAKEC(MT m, real_t x, real_t y, real_t z))
{
    int i;
    for (i = 0; i < N; i++) {
        TAKE(;                     ,
             m.v[i] *= x;          ,
             m.v[i + N] *= y;      ,
             m.v[i + 2 * N] *= z;)
    }
    return m;
}
NOC_DEF void MNAME(_iscale)(TAKEC(MT *m, real_t x, real_t y, real_t z))
{
    *m = MF(scale, TAKEC(*m, x, y, z));
}
NOC_DEF void MNAME(_to_float)(MT m, float *out)
{
    int i;
    for (i = 0; i < N * N; i++)
        out[i] = m.v[i];
}


#if N == 4

NOC_DEF V3T mat4_mul_vec3(mat4_t m, V3T v)
{
    V4T v4 = VNAMEN(4, )(v.x, v.y, v.z, 1);
    return mat4_mul_vec(m, v4).xyz;
}

NOC_DEF mat4_t mat4_ortho(real_t left, real_t right, real_t bottom,
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

NOC_DEF void mat4_iortho(mat4_t *m, real_t left, real_t right, real_t bottom,
                         real_t top, real_t nearval, real_t farval)
{
    const mat4_t tmp = mat4_ortho(left, right, bottom, top, nearval, farval);
    mat4_imul(m, tmp);
}

NOC_DEF mat4_t mat4_perspective(real_t fovy, real_t aspect,
                                real_t nearval, real_t farval)
{
    real_t radian = fovy * M_PI / 180;
    real_t f = 1. / tan(radian / 2.);
    const mat4_t tmp = mat4(
        f / aspect, 0., 0., 0.,
        0., f, 0., 0.,
        0., 0., (farval + nearval) / (nearval - farval), -1,
        0., 0., 2. * farval * nearval / (nearval - farval), 0.
    );
    return tmp;
}

NOC_DEF mat4_t mat4_frustum(real_t left, real_t right,
                            real_t bottom, real_t top,
                            real_t nearval, real_t farval)
{
   real_t x, y, a, b, c, d;
   mat4_t tmp;

   x = (2.0F*nearval) / (right-left);
   y = (2.0F*nearval) / (top-bottom);
   a = (right+left) / (right-left);
   b = (top+bottom) / (top-bottom);
   c = -(farval+nearval) / ( farval-nearval);
   d = -(2.0F*farval*nearval) / (farval-nearval);

#define M(row,col)  tmp.v[col*4+row]
   M(0,0) = x;     M(0,1) = 0.0F;  M(0,2) = a;      M(0,3) = 0.0F;
   M(1,0) = 0.0F;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0F;
   M(2,0) = 0.0F;  M(2,1) = 0.0F;  M(2,2) = c;      M(2,3) = d;
   M(3,0) = 0.0F;  M(3,1) = 0.0F;  M(3,2) = -1.0F;  M(3,3) = 0.0F;
#undef M

   return tmp;
}

NOC_DEF bool mat4_invert(mat4_t *mat)
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

NOC_DEF mat4_t mat4_inverted(mat4_t mat)
{
    mat4_t ret = mat;
    if (mat4_invert(&ret))
        return ret;
    else
        return mat4_zero;
}

// Quaternions
typedef union {
    struct {
        real_t x, y, z, w;
    };
    struct {
        V3T v;
        real_t a;
    };
} quat_t;

NOC_DEF quat_t quat(real_t x, real_t y, real_t z, real_t w) {
   return (quat_t){{x, y, z, w}};
}

static const quat_t quat_identity = {{0, 0, 0, 1}};
NOC_DEF quat_t quat_from_axis(real_t a, real_t x, real_t y, real_t z)
{
    real_t sin_angle;
    V3T vn;
    a *= 0.5f;

    vn = VNAMEN(3, )(x, y, z);
    VNAMEN(3, _normalize)(&vn);

    sin_angle = sin(a);

    return quat(vn.x * sin_angle,
                vn.y * sin_angle,
                vn.z * sin_angle,
                cos(a));
}

NOC_DEF quat_t quat_conjugate(quat_t q)
{
    return quat(-q.x, -q.y, -q.z, q.w);
}
NOC_DEF void quat_iconjugate(quat_t *q)
{
    *q = quat_conjugate(*q);
}

NOC_DEF real_t quat_norm(quat_t q)
{
    return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
}

NOC_DEF quat_t quat_invert(quat_t q)
{
    real_t norm = quat_norm(q);
    q = quat_conjugate(q);
    q.x /= norm;
    q.y /= norm;
    q.z /= norm;
    q.w /= norm;
    return q;
}

NOC_DEF quat_t quat_mul(quat_t a, quat_t b)
{
    return quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
                a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}
NOC_DEF void quat_imul(quat_t *a, quat_t b)
{
    *a = quat_mul(*a, b);
}

NOC_DEF quat_t quat_rotate(quat_t q, real_t a, real_t x, real_t y, real_t z)
{
    quat_t other = quat_from_axis(a, x, y, z);
    return quat_mul(q, other);
}
NOC_DEF void quat_irotate(quat_t *q, real_t a, real_t x, real_t y, real_t z)
{
    *q = quat_rotate(*q, a, x, y, z);
}

NOC_DEF quat_t quat_normalized(quat_t q)
{
    real_t norm = quat_norm(q);
    return quat(q.x / norm, q.y / norm, q.z / norm, q.w / norm);
}

NOC_DEF void quat_normalize(quat_t *q)
{
    *q = quat_normalized(*q);
}

NOC_DEF mat3_t quat_to_mat3(quat_t q)
{
    real_t x, y, z, w;
    x = q.x;
    y = q.y;
    z = q.z;
    w = q.w;
    return mat3(
            1-2*y*y-2*z*z,     2*x*y+2*z*w,     2*x*z-2*y*w,
              2*x*y-2*z*w,   1-2*x*x-2*z*z,     2*y*z+2*x*w,
              2*x*z+2*y*w,     2*y*z-2*x*w,   1-2*x*x-2*y*y);
}

NOC_DEF mat4_t quat_to_mat4(quat_t q)
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

NOC_DEF V3T quat_mul_vec3(quat_t q, V3T v)
{
    mat3_t m = quat_to_mat3(q);
    return mat3_mul_vec(m, v);
}
NOC_CPP(NOC_DEF V3T operator*(V3T v, quat_t q) {
   return quat_mul_vec3(q, v);})

NOC_DEF V4T quat_mul_vec4(quat_t q, V4T v)
{
    mat4_t m = quat_to_mat4(q);
    return mat4_mul_vec(m, v);
}
NOC_CPP(NOC_DEF V4T operator*(V4T v, quat_t q) {
   return quat_mul_vec4(q, v);})

NOC_DEF mat4_t mat4_mul_quat(mat4_t mat, quat_t q)
{
    mat4_t qm = quat_to_mat4(q);
    return mat4_mul(mat, qm);
}

NOC_DEF void mat4_imul_quat(mat4_t *mat, quat_t q)
{
    *mat = mat4_mul_quat(*mat, q);
}

NOC_DEF quat_t quat_neg(quat_t q)
{
    return quat(-q.x, -q.y, -q.z, -q.w);
}

NOC_DEF quat_t quat_slerp(quat_t a, quat_t b, real_t t)
{
    real_t dot, f1, f2, angle, sin_angle;
    if (t <= 0)
        return a;
    else if (t >= 1)
        return b;
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot <= 0) {
        b = quat_neg(b);
        dot = -dot;
    }
    f1 = 1 - t;
    f2 = t;
    if ((1 - dot) > 0.0000001) {
        angle = acos(dot);
        sin_angle = sin(angle);
        if (sin_angle > 0.0000001) {
            f1 = sin((1 - t) * angle) / sin_angle;
            f2 = sin(t * angle) / sin_angle;
        }
    }
    return quat(a.x * f1 + b.x * f2,
                a.y * f1 + b.y * f2,
                a.z * f1 + b.z * f2,
                a.w * f1 + b.w * f2);
}

NOC_DEF real_t quat_get_axis_angle(quat_t q, V3T *axis)
{
    if (axis) {
       axis->x = q.x / sqrt(1 - q.w * q.w);
       axis->y = q.y / sqrt(1 - q.w * q.w);
       axis->z = q.z / sqrt(1 - q.w * q.w);
    }
    return 2 * acos(q.w);
}

NOC_DEF mat4_t mat4_rotate(mat4_t m, real_t a, real_t x, real_t y, real_t z)
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
NOC_DEF void mat4_irotate(mat4_t *m, real_t a, real_t x, real_t y, real_t z)
{
    *m = mat4_rotate(*m, a, x, y, z);
}

#endif

#undef TAKEC
#undef TAKE
#undef NE2
#undef NE3
#undef NGE2
#undef NGE3
#undef NGE4
#undef real_t
#undef NOC_CPP

#endif
