//
// # Yocto/Math: Tiny library for math support in graphics applications.
//
// Yocto/Math provides the basic math primitives used in grahics, including
// small-sized vectors and matrixes, frames, bounding boxes and transforms.
//
//
// ## Small Vectors and Matrices, Frames, Bounding Boxes and Transforms
//
// We provide common operations for small vectors and matrices typically used
// in graphics. In particular, we support 1-4 dimensional vectors
// coordinates (`vec<T, 1>`, `vec<T, 2>`, `vec<T, 3>`, `vec<T, 4>`).
//
// We support 2-4 dimensional matrices (`mat<T, 2, 2>`, `mat<T, 3, 3>`,
// `mat<T, 4, 4>`) with matrix-matrix and matrix-vector products, transposes and
// inverses. Matrices are stored in column-major order and are accessed and
// constructed by column. The one dimensional version is for completeness only.
//
// To represent transformations, most of the library facilities prefer the use
// coordinate frames, aka rigid transforms, represented as `frame<T, 2>` and
// `frame<T, 3>`. The structure store three coordinate axes and the origin.
// This is equivalent to a rigid transform written as a column-major affine
// matrix. Transform operations are better behaved with this representation.
//
// We represent ranges of values in 1-4 dimensions with `bbox<T, 1>`, `bbox<T,
// 2>`, `bbox<T, 3>`, `bbox<T, 4>`. Each range support construction from points
// and other ranges. These can be used to represent generic ranges and
// axis-aligned bounding boxes, for which we define the aliases `bbox<T, 1>`,
// `bbox<T, 2>`, `bbox<T, 3>`,`bbox<T, 4>`. We provide operations to compute
// bounds for points, lines, triangles and quads.
//
// For both matrices and frames we support transform operations for points,
// vectors and directions (`transform_point()`, `transform_vector()`,
// `transform_direction()`). For frames we also the support inverse operations
// (`transform_xxx_inverse()`). Transform matrices and frames can be
// constructed from basic translation, rotation and scaling, e.g. with
// `translation_mat()` or `make_translation_frame()` respectively, etc.
// For rotation we support axis-angle and quaternions, with slerp.
//
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2019 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#ifndef _YOCTO_MATH_H_
#define _YOCTO_MATH_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <algorithm>
#include <array>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

using std::abs;
using std::acos;
using std::asin;
using std::atan;
using std::atan2;
using std::cos;
using std::exp;
using std::exp2;
using std::fabs;
using std::floor;
using std::fmod;
using std::isfinite;
using std::log;
using std::pow;
using std::round;
using std::sin;
using std::sqrt;
using std::swap;
using std::tan;

using std::array;
using std::pair;
using std::numeric_limits;
using std::exception;
using std::out_of_range;
using std::runtime_error;
using std::invalid_argument;

}  // namespace yocto

// -----------------------------------------------------------------------------
// MATH CONSTANTS AND FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

using byte = unsigned char;
using uint = unsigned int;

const auto pi  = 3.14159265358979323846;
const auto pif = 3.14159265f;

template <typename T>
constexpr inline T type_max() {
    return numeric_limits<T>::max();
}
template <typename T>
constexpr inline T type_min() {
    return numeric_limits<T>::lowest();
}

constexpr const auto int_max       = type_max<int>();
constexpr const auto int_min       = type_min<int>();
constexpr const auto float_max     = type_max<float>();
constexpr const auto float_min     = type_min<float>();
constexpr const auto float_epsilon = FLT_EPSILON;

template <typename T>
constexpr inline T min(T x, T y) {
    return (x < y) ? x : y;
}
template <typename T>
constexpr inline T max(T x, T y) {
    return (x > y) ? x : y;
}
template <typename T>
constexpr inline T clamp(T x, T min_, T max_) {
    return min(max(x, min_), max_);
}
template <typename T>
constexpr inline T clamp01(T x) {
    return min(max(x, (T)0), (T)1);
}
template <typename T>
constexpr inline T lerp(T a, T b, T u) {
    return a * (1 - u) + b * u;
}
constexpr inline int pow2(int x) { return 1 << x; }

}  // namespace yocto

// -----------------------------------------------------------------------------
// VECTORS
// -----------------------------------------------------------------------------
namespace yocto {

// Small size vectors.
template <typename T, int N>
struct vec;

template <typename T>
struct vec<T, 1> {
    T x = 0;

    constexpr vec() : x{0} {}
    constexpr vec(T x) : x{x} {}

    constexpr T&       operator[](int i) { return (&x)[i]; }
    constexpr const T& operator[](int i) const { return (&x)[i]; }
};

template <typename T>
struct vec<T, 2> {
    T x = 0;
    T y = 0;

    constexpr vec() : x{0}, y{0} {}
    constexpr vec(T x, T y) : x{x}, y{y} {}
    constexpr explicit vec(T v) : x{v}, y{v} {}

    constexpr T&       operator[](int i) { return (&x)[i]; }
    constexpr const T& operator[](int i) const { return (&x)[i]; }
};

template <typename T>
struct vec<T, 3> {
    T x = 0;
    T y = 0;
    T z = 0;

    constexpr vec() : x{0}, y{0}, z{0} {}
    constexpr vec(T x, T y, T z) : x{x}, y{y}, z{z} {}
    constexpr explicit vec(T v) : x{v}, y{v}, z{v} {}

    constexpr T&       operator[](int i) { return (&x)[i]; }
    constexpr const T& operator[](int i) const { return (&x)[i]; }
};

template <typename T>
struct vec<T, 4> {
    T x = 0;
    T y = 0;
    T z = 0;
    T w = 0;

    constexpr vec() : x{0}, y{0}, z{0}, w{0} {}
    constexpr vec(T x, T y, T z, T w) : x{x}, y{y}, z{z}, w{w} {}
    constexpr explicit vec(T v) : x{v}, y{v}, z{v}, w{v} {}

    constexpr T&       operator[](int i) { return (&x)[i]; }
    constexpr const T& operator[](int i) const { return (&x)[i]; }
};

// Typedefs
using vec1f = vec<float, 1>;
using vec2f = vec<float, 2>;
using vec3f = vec<float, 3>;
using vec4f = vec<float, 4>;
using vec1i = vec<int, 1>;
using vec2i = vec<int, 2>;
using vec3i = vec<int, 3>;
using vec4i = vec<int, 4>;
using vec4b = vec<byte, 4>;

// Zero vector constants.
constexpr const auto zero1f = vec1f{0};
constexpr const auto zero2f = vec2f{0, 0};
constexpr const auto zero3f = vec3f{0, 0, 0};
constexpr const auto zero4f = vec4f{0, 0, 0, 0};
constexpr const auto zero1i = vec1i{0};
constexpr const auto zero2i = vec2i{0, 0};
constexpr const auto zero3i = vec3i{0, 0, 0};
constexpr const auto zero4i = vec4i{0, 0, 0, 0};
constexpr const auto zero4b = vec4b{0, 0, 0, 0};

// Access xyz component of a vec4 typically used for color operation.
template <typename T>
constexpr inline vec<T, 3>& xyz(const vec<T, 4>& a) {
    return (vec<T, 3>&)a;
}
template <typename T>
constexpr inline vec<T, 3>& xyz(vec<T, 4>& a) {
    return (vec<T, 3>&)a;
}

// Vector comparison operations.
template <typename T, int N>
constexpr inline bool operator==(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return a.x == b.x;
    } else if constexpr (N == 2) {
        return a.x == b.x && a.y == b.y;
    } else if constexpr (N == 3) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    } else if constexpr (N == 4) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    } else {
        for (auto i = 0; i < N; i++)
            if (a[i] != b[i]) return false;
        return true;
    }
}
template <typename T, int N>
constexpr inline bool operator!=(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return a.x != b.x;
    } else if constexpr (N == 2) {
        return a.x != b.x || a.y != b.y;
    } else if constexpr (N == 3) {
        return a.x != b.x || a.y != b.y || a.z != b.z;
    } else if constexpr (N == 4) {
        return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
    } else {
        return !(a == b);
    }
}

// Vector operations.
template <typename T, int N>
constexpr inline vec<T, N> operator+(const vec<T, N>& a) {
    return a;
}
template <typename T, int N>
constexpr inline vec<T, N> operator-(const vec<T, N>& a) {
    if constexpr (N == 1) {
        return {-a.x};
    } else if constexpr (N == 2) {
        return {-a.x, -a.y};
    } else if constexpr (N == 3) {
        return {-a.x, -a.y, -a.z};
    } else if constexpr (N == 4) {
        return {-a.x, -a.y, -a.z, -a.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = -a[i];
        return c;
    }
}
template <typename T, int N>
constexpr inline vec<T, N> operator+(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a.x + b.x};
    } else if constexpr (N == 2) {
        return {a.x + b.x, a.y + b.y};
    } else if constexpr (N == 3) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    } else if constexpr (N == 4) {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] + b[i];
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator+(const vec<T, N>& a, T1 b) {
    if constexpr (N == 1) {
        return {a.x + b};
    } else if constexpr (N == 2) {
        return {a.x + b, a.y + b};
    } else if constexpr (N == 3) {
        return {a.x + b, a.y + b, a.z + b};
    } else if constexpr (N == 4) {
        return {a.x + b, a.y + b, a.z + b, a.w + b};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] + b;
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator+(T1 a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a + b.x};
    } else if constexpr (N == 2) {
        return {a + b.x, a + b.y};
    } else if constexpr (N == 3) {
        return {a + b.x, a + b.y, a + b.z};
    } else if constexpr (N == 4) {
        return {a + b.x, a + b.y, a + b.z, a + b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a + b[i];
        return c;
    }
}
template <typename T, int N>
constexpr inline vec<T, N> operator-(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a.x - b.x};
    } else if constexpr (N == 2) {
        return {a.x - b.x, a.y - b.y};
    } else if constexpr (N == 3) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    } else if constexpr (N == 4) {
        return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] - b[i];
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator-(const vec<T, N>& a, T1 b) {
    if constexpr (N == 1) {
        return {a.x - b};
    } else if constexpr (N == 2) {
        return {a.x - b, a.y - b};
    } else if constexpr (N == 3) {
        return {a.x - b, a.y - b, a.z - b};
    } else if constexpr (N == 4) {
        return {a.x - b, a.y - b, a.z - b, a.w - b};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] - b;
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator-(T1 a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a - b.x};
    } else if constexpr (N == 2) {
        return {a - b.x, a - b.y};
    } else if constexpr (N == 3) {
        return {a - b.x, a - b.y, a - b.z};
    } else if constexpr (N == 4) {
        return {a - b.x, a - b.y, a - b.z, a - b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a - b[i];
        return c;
    }
}
template <typename T, int N>
constexpr inline vec<T, N> operator*(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a.x * b.x};
    } else if constexpr (N == 2) {
        return {a.x * b.x, a.y * b.y};
    } else if constexpr (N == 3) {
        return {a.x * b.x, a.y * b.y, a.z * b.z};
    } else if constexpr (N == 4) {
        return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] * b[i];
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator*(const vec<T, N>& a, T1 b) {
    if constexpr (N == 1) {
        return {a.x * b};
    } else if constexpr (N == 2) {
        return {a.x * b, a.y * b};
    } else if constexpr (N == 3) {
        return {a.x * b, a.y * b, a.z * b};
    } else if constexpr (N == 4) {
        return {a.x * b, a.y * b, a.z * b, a.w * b};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] * b;
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator*(T1 a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a * b.x};
    } else if constexpr (N == 2) {
        return {a * b.x, a * b.y};
    } else if constexpr (N == 3) {
        return {a * b.x, a * b.y, a * b.z};
    } else if constexpr (N == 4) {
        return {a * b.x, a * b.y, a * b.z, a * b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a * b[i];
        return c;
    }
}
template <typename T, int N>
constexpr inline vec<T, N> operator/(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a.x / b.x};
    } else if constexpr (N == 2) {
        return {a.x / b.x, a.y / b.y};
    } else if constexpr (N == 3) {
        return {a.x / b.x, a.y / b.y, a.z / b.z};
    } else if constexpr (N == 4) {
        return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] / b[i];
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator/(const vec<T, N>& a, T1 b) {
    if constexpr (N == 1) {
        return {a.x / b};
    } else if constexpr (N == 2) {
        return {a.x / b, a.y / b};
    } else if constexpr (N == 3) {
        return {a.x / b, a.y / b, a.z / b};
    } else if constexpr (N == 4) {
        return {a.x / b, a.y / b, a.z / b, a.w / b};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a[i] / b;
        return c;
    }
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N> operator/(T1 a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {a / b.x};
    } else if constexpr (N == 2) {
        return {a / b.x, a / b.y};
    } else if constexpr (N == 3) {
        return {a / b.x, a / b.y, a / b.z};
    } else if constexpr (N == 4) {
        return {a / b.x, a / b.y, a / b.z, a / b.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = a / b[i];
        return c;
    }
}

// Vector assignments
template <typename T, int N>
constexpr inline vec<T, N>& operator+=(vec<T, N>& a, const vec<T, N>& b) {
    return a = a + b;
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N>& operator+=(vec<T, N>& a, T1 b) {
    return a = a + b;
}
template <typename T, int N>
constexpr inline vec<T, N>& operator-=(vec<T, N>& a, const vec<T, N>& b) {
    return a = a - b;
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N>& operator-=(vec<T, N>& a, T1 b) {
    return a = a - b;
}
template <typename T, int N>
constexpr inline vec<T, N>& operator*=(vec<T, N>& a, const vec<T, N>& b) {
    return a = a * b;
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N>& operator*=(vec<T, N>& a, T1 b) {
    return a = a * b;
}
template <typename T, int N>
constexpr inline vec<T, N>& operator/=(vec<T, N>& a, const vec<T, N>& b) {
    return a = a / b;
}
template <typename T, int N, typename T1>
constexpr inline vec<T, N>& operator/=(vec<T, N>& a, T1 b) {
    return a = a / b;
}

// Vector products and lengths.
template <typename T, int N>
constexpr inline T dot(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return a.x * b.x;
    } else if constexpr (N == 2) {
        return a.x * b.x + a.y * b.y;
    } else if constexpr (N == 3) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    } else if constexpr (N == 4) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    } else {
        auto c = T{0};
        for (auto i = 0; i < N; i++) c += a[i] * b[i];
        return c;
    }
}
template <typename T>
constexpr inline T cross(const vec<T, 2>& a, const vec<T, 2>& b) {
    return a.x * b.y - a.y * b.x;
}
template <typename T>
constexpr inline vec<T, 3> cross(const vec<T, 3>& a, const vec<T, 3>& b) {
    return {
        a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

template <typename T, int N>
constexpr inline T length(const vec<T, N>& a) {
    return sqrt(dot(a, a));
}
template <typename T, int N>
constexpr inline vec<T, N> normalize(const vec<T, N>& a) {
    auto l = length(a);
    return (l != 0) ? a / l : a;
}
template <typename T, int N>
constexpr inline T distance(const vec<T, N>& a, const vec<T, N>& b) {
    return length(a - b);
}
template <typename T, int N>
constexpr inline T distance_squared(const vec<T, N>& a, const vec<T, N>& b) {
    return dot(a - b, a - b);
}

template <typename T>
constexpr inline T angle(const vec<T, 3>& a, const vec<T, 3>& b) {
    return acos(clamp(dot(normalize(a), normalize(b)), (T)-1, (T)1));
}
template <typename T, typename T1>
constexpr inline vec<T, 4> slerp(const vec<T, 4>& a, const vec<T, 4>& b, T1 u) {
    // https://en.wikipedia.org/wiki/Slerp
    auto an = normalize(a), bn = normalize(b);
    auto d = dot(an, bn);
    if (d < 0) {
        bn = -bn;
        d  = -d;
    }
    if (d > (T)0.9995) return normalize(an + u * (bn - an));
    auto th = acos(clamp(d, (T)-1, (T)1));
    if (!th) return an;
    return an * (sin(th * (1 - u)) / sin(th)) + bn * (sin(th * u) / sin(th));
}

// Orthogonal vectors.
template <typename T>
constexpr inline vec<T, 3> orthogonal(const vec<T, 3>& v) {
    // http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts)
    return abs(v.x) > abs(v.z) ? vec<T, 3>{-v.y, v.x, 0}
                               : vec<T, 3>{0, -v.z, v.y};
}
template <typename T>
constexpr inline vec<T, 3> orthonormalize(
    const vec<T, 3>& a, const vec<T, 3>& b) {
    return normalize(a - b * dot(a, b));
}

// Reflected and refracted vector.
template <typename T>
constexpr inline vec<T, 3> reflect(const vec<T, 3>& w, const vec<T, 3>& n) {
    return -w + 2 * dot(n, w) * n;
}
template <typename T, typename T1>
constexpr inline vec<T, 3> refract(
    const vec<T, 3>& w, const vec<T, 3>& n, T1 eta) {
    // auto k = 1.0 - eta * eta * (1.0 - dot(n, w) * dot(n, w));
    auto k = 1 - eta * eta * max((T)0, 1 - dot(n, w) * dot(n, w));
    if (k < 0) return {0, 0, 0};  // tir
    return -w * eta + (eta * dot(n, w) - sqrt(k)) * n;
}

// Max element and clamp.
template <typename T, int N>
constexpr inline vec<T, N> max(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {max(a.x, b.x)};
    } else if constexpr (N == 2) {
        return {max(a.x, b.x), max(a.y, b.y)};
    } else if constexpr (N == 3) {
        return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
    } else if constexpr (N == 4) {
        return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w)};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = max(a[i], b[i]);
        return c;
    }
}
template <typename T, int N>
constexpr inline vec<T, N> min(const vec<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {min(a.x, b.x)};
    } else if constexpr (N == 2) {
        return {min(a.x, b.x), min(a.y, b.y)};
    } else if constexpr (N == 3) {
        return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
    } else if constexpr (N == 4) {
        return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w)};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = min(a[i], b[i]);
        return c;
    }
}
template <typename T, int N, typename T1, typename T2>
constexpr inline vec<T, N> clamp(const vec<T, N>& x, T1 min, T2 max) {
    if constexpr (N == 1) {
        return {clamp(x.x, min, max)};
    } else if constexpr (N == 2) {
        return {clamp(x.x, min, max), clamp(x.y, min, max)};
    } else if constexpr (N == 3) {
        return {
            clamp(x.x, min, max), clamp(x.y, min, max), clamp(x.z, min, max)};
    } else if constexpr (N == 4) {
        return {clamp(x.x, min, max), clamp(x.y, min, max),
            clamp(x.z, min, max), clamp(x.w, min, max)};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = clamp(x[i], min, max);
        return c;
    }
}
template <typename T, int N>
constexpr inline vec<T, N> clamp01(const vec<T, N>& x) {
    if constexpr (N == 1) {
        return {clamp01(x.x)};
    } else if constexpr (N == 2) {
        return {clamp01(x.x), clamp01(x.y)};
    } else if constexpr (N == 3) {
        return {clamp01(x.x), clamp01(x.y), clamp01(x.z)};
    } else if constexpr (N == 4) {
        return {clamp01(x.x), clamp01(x.y), clamp01(x.z), clamp01(x.w)};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = clamp01(x[i]);
        return c;
    }
}
template <typename T, int N>
constexpr inline T max(const vec<T, N>& a) {
    if constexpr (N == 1) {
        return a.x;
    } else if constexpr (N == 2) {
        return max(a.x, a.y);
    } else if constexpr (N == 3) {
        return max(max(a.x, a.y), a.z);
    } else if constexpr (N == 4) {
        return max(max(max(a.x, a.y), a.z), a.w);
    } else {
        auto m = a[0];
        for (auto i = 1; i < N; i++) m = max(a[i], m);
        return m;
    }
}
template <typename T, int N>
constexpr inline T min(const vec<T, N>& a) {
    if constexpr (N == 1) {
        return a.x;
    } else if constexpr (N == 2) {
        return min(a.x, a.y);
    } else if constexpr (N == 3) {
        return min(min(a.x, a.y), a.z);
    } else if constexpr (N == 4) {
        return min(min(min(a.x, a.y), a.z), a.w);
    } else {
        auto m = a[0];
        for (auto i = 1; i < N; i++) m = min(a[i], m);
        return m;
    }
}
template <typename T, int N>
constexpr inline T mean(const vec<T, N>& a) {
    if constexpr (N == 1) {
        return a.x;
    } else if constexpr (N == 2) {
        return (a.x + a.y) / 2;
    } else if constexpr (N == 3) {
        return (a.x + a.y + a.z) / 3;
    } else if constexpr (N == 4) {
        return (a.x + a.y + a.z + a.w) / 4;
    } else {
        auto m = a[0];
        for (auto i = 1; i < N; i++) m += a[i];
        return m / N;
    }
}
template <typename T, int N>
constexpr inline int max_element(const vec<T, N>& a) {
    auto pos = 0;
    for (auto i = 1; i < N; i++) {
        if (a[i] > a[pos]) pos = i;
    }
    return pos;
}
template <typename T, int N>
constexpr inline int min_element(const vec<T, N>& a) {
    auto pos = 0;
    for (auto i = 1; i < N; i++) {
        if (a[i] < a[pos]) pos = i;
    }
    return pos;
}

// Apply a unary function to all vector elements
template <typename T, int N>
constexpr inline vec<T, N> apply(T (*func)(T), const vec<T, N>& a) {
    if constexpr (N == 1) {
        return {func(a.x)};
    } else if constexpr (N == 2) {
        return {func(a.x), func(a.y)};
    } else if constexpr (N == 3) {
        return {func(a.x), func(a.y), func(a.z)};
    } else if constexpr (N == 4) {
        return {func(a.x), func(a.y), func(a.z), func(a.w)};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = func(a[i]);
        return c;
    }
}
// Apply a binary function to all vector elements
template <typename T, int N>
constexpr inline vec<T, N> apply(T (*func)(T, T), const vec<T, N>& a, T b) {
    if constexpr (N == 1) {
        return {func(a.x, b)};
    } else if constexpr (N == 2) {
        return {func(a.x, b), func(a.y, b)};
    } else if constexpr (N == 3) {
        return {func(a.x, b), func(a.y, b), func(a.z, b)};
    } else if constexpr (N == 4) {
        return {func(a.x, b), func(a.y, b), func(a.z, b), func(a.w, b)};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] = func(a[i], b);
        return c;
    }
}

// Functions applied to vector elements
template <typename T, int N>
constexpr inline vec<T, N> exp(const vec<T, N>& a) {
    return apply(exp, a);
};
template <typename T, int N, typename T1>
constexpr inline vec<T, N> pow(const vec<T, N>& a, T1 b) {
    return apply(pow, a, b);
};

template <typename T, int N>
constexpr inline bool isfinite(const vec<T, N>& a) {
    if constexpr (N == 1) {
        return isfinite(a.x);
    } else if constexpr (N == 2) {
        return isfinite(a.x) && isfinite(a.y);
    } else if constexpr (N == 3) {
        return isfinite(a.x) && isfinite(a.y) && isfinite(a.z);
    } else if constexpr (N == 4) {
        return isfinite(a.x) && isfinite(a.y) && isfinite(a.z) && isfinite(a.w);
    } else {
        throw invalid_argument("not implemented");
    }
};

// Quaternion operatons represented as xi + yj + zk + w
// const auto identity_quat4f = vec<float, 4>{0, 0, 0, 1};
template <typename T, typename T1>
constexpr inline vec<T, 4> quat_mul(const vec<T, 4>& a, T1 b) {
    return {a.x * b, a.y * b, a.z * b, a.w * b};
}
template <typename T>
constexpr inline vec<T, 4> quat_mul(const vec<T, 4>& a, const vec<T, 4>& b) {
    return {a.x * b.w + a.w * b.x + a.y * b.w - a.z * b.y,
        a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
        a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
template <typename T>
constexpr inline vec<T, 4> quat_conjugate(const vec<T, 4>& a) {
    return {-a.x, -a.y, -a.z, a.w};
}
template <typename T>
constexpr inline vec<T, 4> quat_inverse(const vec<T, 4>& a) {
    return quat_conjugate(a) / dot(a, a);
}

}  // namespace yocto

namespace std {

// Hash functor for vector for use with unordered_map
template <typename T, int N>
struct hash<yocto::vec<T, N>> {
    static constexpr std::hash<T> hasher = std::hash<T>();

    size_t operator()(const yocto::vec<T, N>& v) const {
        auto h = (size_t)0;
        for (auto i = 0; i < N; i++)
            h ^= hasher((&v.x)[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

}  // namespace std

// -----------------------------------------------------------------------------
// MATRICES
// -----------------------------------------------------------------------------
namespace yocto {

// Small Fixed-size matrices stored in column major format.
template <typename T, int N, int M>
struct mat;

// Small Fixed-size matrices stored in column major format.
template <typename T, int N>
struct mat<T, N, 1> {
    vec<T, N> x = {};

    constexpr mat() : x{} {}
    constexpr mat(const vec<T, N>& x) : x{x} {}

    constexpr vec<T, N>&       operator[](int i) { return (&x)[i]; }
    constexpr const vec<T, N>& operator[](int i) const { return (&x)[i]; }
};

// Small Fixed-size matrices stored in column major format.
template <typename T, int N>
struct mat<T, N, 2> {
    vec<T, N> x = {};
    vec<T, N> y = {};

    constexpr mat() : x{}, y{} {}
    constexpr mat(const vec<T, N>& x, const vec<T, N>& y) : x{x}, y{y} {}

    constexpr vec<T, N>&       operator[](int i) { return (&x)[i]; }
    constexpr const vec<T, N>& operator[](int i) const { return (&x)[i]; }
};

// Small Fixed-size matrices stored in column major format.
template <typename T, int N>
struct mat<T, N, 3> {
    vec<T, N> x = {};
    vec<T, N> y = {};
    vec<T, N> z = {};

    constexpr mat() : x{}, y{}, z{} {}
    constexpr mat(const vec<T, N>& x, const vec<T, N>& y, const vec<T, N>& z)
        : x{x}, y{y}, z{z} {}

    constexpr vec<T, N>&       operator[](int i) { return (&x)[i]; }
    constexpr const vec<T, N>& operator[](int i) const { return (&x)[i]; }
};

// Small Fixed-size matrices stored in column major format.
template <typename T, int N>
struct mat<T, N, 4> {
    vec<T, N> x = {};
    vec<T, N> y = {};
    vec<T, N> z = {};
    vec<T, N> w = {};

    constexpr mat() : x{}, y{}, z{}, w{} {}
    constexpr mat(const vec<T, N>& x, const vec<T, N>& y, const vec<T, N>& z,
        const vec<T, N>& w)
        : x{x}, y{y}, z{z}, w{w} {}

    constexpr vec<T, N>&       operator[](int i) { return (&x)[i]; }
    constexpr const vec<T, N>& operator[](int i) const { return (&x)[i]; }
};

// Typedefs
using mat2f = mat<float, 2, 2>;
using mat3f = mat<float, 3, 3>;
using mat4f = mat<float, 4, 4>;

// Identity matrix
template <typename T, int N>
constexpr inline mat<T, N, N> make_identity_mat() {
    if constexpr (N == 1) {
        return {{1}};
    } else if constexpr (N == 2) {
        return {{1, 0}, {0, 1}};
    } else if constexpr (N == 3) {
        return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    } else if constexpr (N == 4) {
        return {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    } else {
        auto m = mat<T, N, N>{};
        for (auto i = 0; i < N; i++) m[i][i] = 1;
        return m;
    }
}

// Identity matrices constants.
constexpr const auto identity_mat2f = mat2f{{1, 0}, {0, 1}};
constexpr const auto identity_mat3f = mat3f{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
constexpr const auto identity_mat4f = mat4f{
    {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

// Matrix comparisons.
template <typename T, int N, int M>
constexpr inline bool operator==(const mat<T, N, M>& a, const mat<T, N, M>& b) {
    if constexpr (M == 1) {
        return a.x == b.x;
    } else if constexpr (M == 2) {
        return a.x == b.x && a.y == b.y;
    } else if constexpr (M == 3) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    } else if constexpr (M == 4) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    } else {
        for (auto i = 0; i < M; i++)
            if (a[i] != b[i]) return false;
        return true;
    }
}
template <typename T, int N, int M>
constexpr inline bool operator!=(const mat<T, N, M>& a, const mat<T, N, M>& b) {
    return !(a == b);
}

// Matrix operations.
template <typename T, int N, int M>
constexpr inline mat<T, N, M> operator+(
    const mat<T, N, M>& a, const mat<T, N, M>& b) {
    if constexpr (M == 1) {
        return {a.x + b.x};
    } else if constexpr (M == 2) {
        return {a.x + b.x, a.y + b.y};
    } else if constexpr (M == 3) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    } else if constexpr (M == 4) {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    } else {
        auto c = mat<T, N, M>{};
        for (auto i = 0; i < M; i++) c[i] = a[i] + b[i];
        return c;
    }
}
template <typename T, int N, int M, typename T1>
constexpr inline mat<T, N, M> operator*(const mat<T, N, M>& a, T1 b) {
    if constexpr (M == 1) {
        return {a.x * b};
    } else if constexpr (M == 2) {
        return {a.x * b, a.y * b};
    } else if constexpr (M == 3) {
        return {a.x * b, a.y * b, a.z * b};
    } else if constexpr (M == 4) {
        return {a.x * b, a.y * b, a.z * b, a.w * b};
    } else {
        auto c = mat<T, N, M>{};
        for (auto i = 0; i < M; i++) c[i] = a[i] * b;
        return c;
    }
}
template <typename T, int N, int M>
constexpr inline vec<T, N> operator*(
    const mat<T, N, M>& a, const vec<T, M>& b) {
    if constexpr (M == 1) {
        return a.x * b.x;
    } else if constexpr (M == 2) {
        return a.x * b.x + a.y * b.y;
    } else if constexpr (M == 3) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    } else if constexpr (M == 4) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < M; i++) c += a[i] + b[i];
        return c;
    }
}
template <typename T, int N, int M>
constexpr inline vec<T, M> operator*(
    const vec<T, N>& a, const mat<T, N, M>& b) {
    if constexpr (M == 1) {
        return {dot(a, b.x)};
    } else if constexpr (M == 2) {
        return {dot(a, b.x), dot(a, b.y)};
    } else if constexpr (M == 3) {
        return {dot(a, b.x), dot(a, b.y), dot(a, b.z)};
    } else if constexpr (M == 4) {
        return {dot(a, b.x), dot(a, b.y), dot(a, b.z), dot(a, b.w)};
    } else {
        auto c = vec<T, M>{};
        for (auto i = 0; i < M; i++) c[i] = dot(a, b[i]);
        return c;
    }
}
template <typename T, int N, int M, int K>
constexpr inline mat<T, N, M> operator*(
    const mat<T, N, K>& a, const mat<T, K, M>& b) {
    if constexpr (M == 1) {
        return {dot(a, b.x)};
    } else if constexpr (M == 2) {
        return {a * b.x, a * b.y};
    } else if constexpr (M == 3) {
        return {a * b.x, a * b.y, a * b.z};
    } else if constexpr (M == 4) {
        return {a * b.x, a * b.y, a * b.z, a * b.w};
    } else {
        auto c = mat<T, N, M>{};
        for (auto i = 0; i < M; i++) c[i] = a * b[i];
        return c;
    }
}

// Matrix assignments.
template <typename T, int N, int M>
constexpr inline mat<T, N, M>& operator+=(
    mat<T, N, M>& a, const mat<T, N, M>& b) {
    return a = a + b;
}
template <typename T, int N>
constexpr inline mat<T, N, N>& operator*=(
    mat<T, N, N>& a, const mat<T, N, N>& b) {
    return a = a * b;
}
template <typename T, int N, int M, typename T1>
constexpr inline mat<T, N, M>& operator*=(mat<T, N, M>& a, T1 b) {
    return a = a * b;
}

// Matrix diagonals and transposes.
template <typename T, int N>
constexpr inline vec<T, N> diagonal(const mat<T, N, N>& a) {
    if constexpr (N == 1) {
        return {a.x.x};
    } else if constexpr (N == 2) {
        return {a.x.x, a.y.y};
    } else if constexpr (N == 3) {
        return {a.x.x, a.y.y, a.z.z};
    } else if constexpr (N == 4) {
        return {a.x.x, a.y.y, a.z.z, a.w.w};
    } else {
        auto c = vec<T, N>{};
        for (auto i = 0; i < N; i++) c[i] += a[i][i];
        return c;
    }
}
template <typename T, int N, int M>
constexpr inline mat<T, M, N> transpose(const mat<T, N, M>& a);

// Matrix adjugates, determinant and inverses.
template <typename T, int N>
constexpr inline mat<T, N, N> adjugate(const mat<T, N, N>& a);
template <typename T, int N>
constexpr inline T determinant(const mat<T, N, N>& a);
template <typename T, int N>
constexpr inline mat<T, N, N> inverse(const mat<T, N, N>& a) {
    return adjugate(a) * (1 / determinant(a));
}

// Constructs a basis from a direction
template <typename T>
constexpr inline mat<T, 3, 3> make_basis_fromz(const vec<T, 3>& v) {
    // https://graphics.pixar.com/library/OrthonormalB/paper.pdf
    auto z    = normalize(v);
    auto sign = copysignf(1.0f, z.z);
    auto a    = -1.0f / (sign + z.z);
    auto b    = z.x * z.y * a;
    auto x    = vec<T, 3>{1.0f + sign * z.x * z.x * a, sign * b, -sign * z.x};
    auto y    = vec<T, 3>{b, sign + z.y * z.y * a, -z.y};
    return {x, y, z};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// RIGID BODY TRANSFORMS/FRAMES
// -----------------------------------------------------------------------------
namespace yocto {

// Rigid frames stored as a column-major affine transform matrix.
template <typename T, int N>
struct frame;

// Rigid frames stored as a column-major affine transform matrix.
template <typename T>
struct frame<T, 2> {
    vec<T, 2> x = {1, 0};
    vec<T, 2> y = {0, 1};
    vec<T, 2> o = {0, 0};

    constexpr frame() : x{}, y{}, o{} {}
    constexpr frame(const vec<T, 2>& x, const vec<T, 2>& y, const vec<T, 2>& o)
        : x{x}, y{y}, o{o} {}
    constexpr frame(const mat<T, 2, 2>& m, const vec<T, 2>& t)
        : x{m.x}, y{m.y}, o{t} {}

    constexpr vec<T, 2>&       operator[](int i) { return (&x)[i]; }
    constexpr const vec<T, 2>& operator[](int i) const { return (&x)[i]; }
};

// Rigid frames stored as a column-major affine transform matrix.
template <typename T>
struct frame<T, 3> {
    vec<T, 3> x = {1, 0, 0};
    vec<T, 3> y = {0, 1, 0};
    vec<T, 3> z = {0, 0, 1};
    vec<T, 3> o = {0, 0, 0};

    constexpr frame() : x{}, y{}, z{}, o{} {}
    constexpr frame(const vec<T, 3>& x, const vec<T, 3>& y, const vec<T, 3>& z,
        const vec<T, 3>& o)
        : x{x}, y{y}, z{z}, o{o} {}
    constexpr frame(const mat<T, 3, 3>& m, const vec<T, 3>& t)
        : x{m.x}, y{m.y}, z{m.z}, o{t} {}

    constexpr vec<T, 3>&       operator[](int i) { return (&x)[i]; }
    constexpr const vec<T, 3>& operator[](int i) const { return (&x)[i]; }
};

// Typedefs
using frame2f = frame<float, 2>;
using frame3f = frame<float, 3>;

// Indentity frames.
constexpr const auto identity_frame2f = frame2f{{1, 0}, {0, 1}, {0, 0}};
constexpr const auto identity_frame3f = frame3f{
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};

// Access rotation and translation.
template <typename T, int N>
constexpr inline mat<T, N, N>& frame_rotation(frame<T, N>& f) {
    return (mat<T, N, N>&)f;
}
template <typename T, int N>
constexpr inline const mat<T, N, N>& frame_rotation(const frame<T, N>& f) {
    return (mat<T, N, N>&)f;
}
template <typename T, int N>
constexpr inline vec<T, N>& frame_translation(frame<T, N>& f) {
    return f.o;
}
template <typename T, int N>
constexpr inline const vec<T, N>& frame_translation(const frame<T, N>& f) {
    return f.o;
}

// Frame construction from axis.
template <typename T>
constexpr inline frame<T, 3> make_frame_fromz(
    const vec<T, 3>& o, const vec<T, 3>& v) {
    // https://graphics.pixar.com/library/OrthonormalB/paper.pdf
    auto z    = normalize(v);
    auto sign = copysignf(1.0f, z.z);
    auto a    = -1.0f / (sign + z.z);
    auto b    = z.x * z.y * a;
    auto x    = vec<T, 3>{1.0f + sign * z.x * z.x * a, sign * b, -sign * z.x};
    auto y    = vec<T, 3>{b, sign + z.y * z.y * a, -z.y};
    return {x, y, z, o};
}
template <typename T>
constexpr inline frame<T, 3> make_frame_fromzx(
    const vec<T, 3>& o, const vec<T, 3>& z_, const vec<T, 3>& x_) {
    auto z = normalize(z_);
    auto x = orthonormalize(x_, z);
    auto y = normalize(cross(z, x));
    return {x, y, z, o};
}

// Frame to matrix conversion.
template <typename T>
constexpr inline mat<T, 4, 4> frame_to_mat(const frame<T, 3>& a) {
    return {
        {a.x.x, a.x.y, a.x.z, 0},
        {a.y.x, a.y.y, a.y.z, 0},
        {a.z.x, a.z.y, a.z.z, 0},
        {a.o.x, a.o.y, a.o.z, 1},
    };
}
template <typename T>
constexpr inline frame<T, 3> mat_to_frame(const mat<T, 4, 4>& a) {
    return {
        {a.x.x, a.x.y, a.x.z},
        {a.y.x, a.y.y, a.y.z},
        {a.z.x, a.z.y, a.z.z},
        {a.w.x, a.w.y, a.w.z},
    };
}

// Frame comparisons.
template <typename T, int N>
constexpr inline bool operator==(const frame<T, N>& a, const frame<T, N>& b) {
    if constexpr (N == 2) {
        return a.x == b.x && a.y == b.y && a.o == b.o;
    } else if constexpr (N == 3) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.o == b.o;
    } else {
        for (auto i = 0; i < N; i++)
            if (a[i] != b[i]) return false;
        return a[N] == b[N];
    }
}
template <typename T, int N>
constexpr inline bool operator!=(const frame<T, N>& a, const frame<T, N>& b) {
    return !(a == b);
}

// Frame composition, equivalent to affine matrix product.
template <typename T, int N>
constexpr inline frame<T, N> operator*(
    const frame<T, N>& a, const frame<T, N>& b) {
    return {
        frame_rotation(a) * frame_rotation(b), frame_rotation(a) * b.o + a.o};
}
// Frame inverse, equivalent to rigid affine inverse.
template <typename T, int N>
constexpr inline frame<T, N> inverse(
    const frame<T, N>& a, bool is_rigid = true) {
    auto minv = (is_rigid) ? transpose(frame_rotation(a))
                           : inverse(frame_rotation(a));
    return {minv, -(minv * a.o)};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// QUATERNIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Quaternions to represent rotations
template <typename T, int N>
struct quat;

// Quaternions to represent rotations
template <typename T>
struct quat<T, 4> {
    T x = 0;
    T y = 0;
    T z = 0;
    T w = 1;

    // constructors
    constexpr quat() : x{0}, y{0}, z{0}, w{1} {}
    constexpr quat(T x, T y, T z, T w) : x{x}, y{y}, z{z}, w{w} {}
};

// Typedefs
using quat4f = quat<float, 4>;

// Constants
constexpr const auto identity_quat4f = quat4f{0, 0, 0, 1};

// Quaternion operatons
template <typename T, typename T1>
constexpr inline quat<T, 4> operator*(const quat<T, 4>& a, T1 b) {
    return {a.x * b, a.y * b, a.z * b, a.w * b};
}
template <typename T>
constexpr inline quat<T, 4> operator*(
    const quat<T, 4>& a, const quat<T, 4>& b) {
    return {a.x * b.w + a.w * b.x + a.y * b.w - a.z * b.y,
        a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
        a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// Quaterion operations
template <typename T>
constexpr inline T dot(const quat<T, 4>& a, const quat<T, 4>& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
template <typename T>
constexpr inline T length(const quat<T, 4>& a) {
    return sqrt(dot(a, a));
}
template <typename T>
constexpr inline quat<T, 4> normalize(const quat<T, 4>& a) {
    auto l = length(a);
    return (l != 0) ? a / l : a;
}
template <typename T>
constexpr inline quat<T, 4> conjugate(const quat<T, 4>& a) {
    return {-a.x, -a.y, -a.z, a.w};
}
template <typename T>
constexpr inline quat<T, 4> inverse(const quat<T, 4>& a) {
    return conjugate(a) / dot(a, a);
}
template <typename T>
constexpr inline T uangle(const quat<T, 4>& a, const quat<T, 4>& b) {
    T d = dot(a, b);
    return d > 1 ? 0 : std::acos(d < -1 ? -1 : d);
}
template <typename T>
constexpr inline quat<T, 4> lerp(
    const quat<T, 4>& a, const quat<T, 4>& b, T t) {
    return a * (1 - t) + b * t;
}
template <typename T>
inline quat<T, 4> nlerp(const quat<T, 4>& a, const quat<T, 4>& b, T t) {
    return normalize(lerp(a, b, t));
}
template <typename T>
constexpr inline quat<T, 4> slerp(
    const quat<T, 4>& a, const quat<T, 4>& b, T t) {
    auto th = uangle(a, b);
    return th == 0 ? a
                   : a * (sin(th * (1 - t)) / sin(th)) +
                         b * (sin(th * t) / sin(th));
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// AXIS ALIGNED BOUNDING BOXES
// -----------------------------------------------------------------------------
namespace yocto {

// Axis aligned bounding box represented as a min/max vector pairs.
template <typename T, int N>
struct bbox {
    vec<T, N> min = vec<T, N>{type_max<T>()};
    vec<T, N> max = vec<T, N>{type_min<T>()};

    constexpr bbox() : min{type_max<T>()}, max{type_min<T>()} {}
    constexpr bbox(const vec<T, N>& min, const vec<T, N>& max)
        : min{min}, max{max} {}

    constexpr vec<T, N>&       operator[](int i) { return (&min)[i]; }
    constexpr const vec<T, N>& operator[](int i) const { return (&min)[i]; }
};

// Typedefs
using bbox1f = bbox<float, 1>;
using bbox2f = bbox<float, 2>;
using bbox3f = bbox<float, 3>;
using bbox4f = bbox<float, 4>;
using bbox1i = bbox<int, 1>;
using bbox2i = bbox<int, 2>;
using bbox3i = bbox<int, 3>;
using bbox4i = bbox<int, 4>;

// Empty bbox constant.
constexpr const auto invalid_bbox1f = bbox1f{};
constexpr const auto invalid_bbox2f = bbox2f{};
constexpr const auto invalid_bbox3f = bbox3f{};
constexpr const auto invalid_bbox4f = bbox4f{};
constexpr const auto invalid_bbox1i = bbox1i{};
constexpr const auto invalid_bbox2i = bbox2i{};
constexpr const auto invalid_bbox3i = bbox3i{};
constexpr const auto invalid_bbox4i = bbox4i{};

// Bounding box values
template <typename T>
constexpr inline T bbox_size(const bbox<T, 1>& a) {
    return a.max - a.min;
}
template <typename T, int N>
constexpr inline vec<T, N> bbox_size(const bbox<T, N>& a) {
    return a.max - a.min;
}
template <typename T>
constexpr inline T bbox_center(const bbox<T, 1>& a) {
    return (a.max + a.min) / 2;
}
template <typename T, int N>
constexpr inline vec<T, N> bbox_center(const bbox<T, N>& a) {
    return (a.max + a.min) / 2;
}

// Bounding box comparisons.
template <typename T, int N>
constexpr inline bool operator==(const bbox<T, N>& a, const bbox<T, N>& b) {
    return a.min == b.min && a.max == b.max;
}
template <typename T, int N>
constexpr inline bool operator!=(const bbox<T, N>& a, const bbox<T, N>& b) {
    return a.min != b.min || a.max != b.max;
}

// Bounding box expansions with points and other boxes.
template <typename T, int N>
constexpr inline bbox<T, N> operator+(const bbox<T, N>& a, const vec<T, N>& b) {
    return {min(a.min, b), max(a.max, b)};
}
template <typename T, int N>
constexpr inline bbox<T, N> operator+(
    const bbox<T, N>& a, const bbox<T, N>& b) {
    return {min(a.min, b.min), max(a.max, b.max)};
}
template <typename T, int N>
constexpr inline bbox<T, N>& operator+=(bbox<T, N>& a, const vec<T, N>& b) {
    return a = a + b;
}
template <typename T, int N>
constexpr inline bbox<T, N>& operator+=(bbox<T, N>& a, const bbox<T, N>& b) {
    return a = a + b;
}

// Primitive bounds.
template <typename T, int N, typename T1>
constexpr inline bbox<T, N> point_bounds(const vec<T, N>& p, T1 r = 0) {
    auto a = bbox<T, N>{};
    a += p - vec<T, N>{r};
    a += p + vec<T, N>{r};
    return a;
}
template <typename T, int N, typename T1>
constexpr inline bbox<T, N> line_bounds(
    const vec<T, N>& p0, const vec<T, N>& p1, T1 r0 = 0, T1 r1 = 0) {
    auto a = bbox<T, N>{};
    a += p0 - vec<T, N>{r0, r0, r0};
    a += p0 + vec<T, N>{r0, r0, r0};
    a += p1 - vec<T, N>{r1, r1, r1};
    a += p1 + vec<T, N>{r1, r1, r1};
    return a;
}
template <typename T, int N>
constexpr inline bbox<T, N> triangle_bounds(
    const vec<T, N>& p0, const vec<T, N>& p1, const vec<T, N>& p2) {
    auto a = bbox<T, 3>{};
    a += p0;
    a += p1;
    a += p2;
    return a;
}
template <typename T, int N>
constexpr inline bbox<T, N> quad_bounds(const vec<T, N>& p0,
    const vec<T, N>& p1, const vec<T, N>& p2, const vec<T, N>& p3) {
    auto a = bbox<T, N>{};
    a += p0;
    a += p1;
    a += p2;
    a += p3;
    return a;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// RAYS
// -----------------------------------------------------------------------------
namespace yocto {

// Rays with origin, direction and min/max t value.
template <typename T, int N>
struct ray;

template <typename T>
struct ray<T, 2> {
    vec<T, 2> o    = {0, 0};
    vec<T, 2> d    = {0, 1};
    T         tmin = 0;
    T         tmax = type_max<T>();

    constexpr ray() : o{0, 0}, d{0, 1}, tmin{0}, tmax{type_max<T>} {}
    constexpr ray(const vec<T, 2>& o, const vec<T, 2>& d, T tmin, T tmax)
        : o{o}, d{d}, tmin{tmin}, tmax{tmax} {}
};

// Rays with origin, direction and min/max t value.
template <typename T>
struct ray<T, 3> {
    vec<T, 3> o    = {0, 0, 0};
    vec<T, 3> d    = {0, 0, 1};
    T         tmin = 0;
    T         tmax = type_max<T>();

    constexpr ray() : o{0, 0, 0}, d{0, 0, 1}, tmin{0}, tmax{type_max<T>} {}
    constexpr ray(const vec<T, 3>& o, const vec<T, 3>& d, T tmin, T tmax)
        : o{o}, d{d}, tmin{tmin}, tmax{tmax} {}
};

// Ray esplison
constexpr const auto ray_eps = 1e-4f;

// Typedefs
using ray2f = ray<float, 2>;
using ray3f = ray<float, 3>;

// Construct a ray from direction or segments using a default epsilon.
template <typename T, int N>
constexpr inline ray<T, N> make_ray(
    const vec<T, N>& o, const vec<T, N>& d, T eps = (T)ray_eps) {
    return {o, d, eps, type_max<T>()};
}
template <typename T, int N>
constexpr inline ray<T, N> make_segment(
    const vec<T, N>& p1, const vec<T, N>& p2, T eps = (T)ray_eps) {
    return {p1, normalize(p2 - p1), eps, length(p2 - p1) - 2 * eps};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// TRANSFORMS
// -----------------------------------------------------------------------------
namespace yocto {

// Transforms points, vectors and directions by matrices.
template <typename T, int N>
constexpr inline vec<T, N> transform_point(
    const mat<T, N + 1, N + 1>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        throw invalid_argument("not well defined");
    } else if constexpr (N == 2) {
        auto tvb = a * vec<T, 3>{b.x, b.y, 1};
        return vec<T, 2>{tvb.x, tvb.y} / tvb.z;
    } else if constexpr (N == 3) {
        auto tvb = a * vec<T, 4>{b.x, b.y, b.z, 1};
        return vec<T, 3>{tvb.x, tvb.y, tvb.z} / tvb.w;
    } else {
    }
}
template <typename T, int N>
constexpr inline vec<T, N> transform_vector(
    const mat<T, N + 1, N + 1>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        throw invalid_argument("not well defined");
    } else if constexpr (N == 2) {
        auto tvb = a * vec<T, 3>{b.x, b.y, 0};
        return vec<T, 2>{tvb.x, tvb.y} / tvb.z;
    } else if constexpr (N == 3) {
        auto tvb = a * vec<T, 4>{b.x, b.y, b.z, 0};
        return vec<T, 3>{tvb.x, tvb.y, tvb.z};
    } else {
    }
}
template <typename T, int N>
constexpr inline vec<T, N> transform_direction(
    const mat<T, N + 1, N + 1>& a, const vec<T, N>& b) {
    return normalize(transform_vector(a, b));
}
template <typename T, int N>
constexpr inline vec<T, N> transform_vector(
    const mat<T, N, N>& a, const vec<T, N>& b) {
    return a * b;
}
template <typename T, int N>
constexpr inline vec<T, N> transform_direction(
    const mat<T, N, N>& a, const vec<T, N>& b) {
    return normalize(transform_vector(a, b));
}

// Transforms points, vectors and directions by frames.
template <typename T, int N>
constexpr inline vec<T, N> transform_point(
    const frame<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return a.x * b.x + a.o;
    } else if constexpr (N == 2) {
        return a.x * b.x + a.y * b.y + a.o;
    } else if constexpr (N == 3) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.o;
    } else {
    }
}
template <typename T, int N>
constexpr inline vec<T, N> transform_vector(
    const frame<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return a.x * b.x;
    } else if constexpr (N == 2) {
        return a.x * b.x + a.y * b.y;
    } else if constexpr (N == 3) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    } else {
    }
}
template <typename T, int N>
constexpr inline vec<T, N> transform_direction(
    const frame<T, N>& a, const vec<T, N>& b) {
    return normalize(transform_vector(a, b));
}

// Transforms rays and bounding boxes by matrices.
template <typename T, int N>
constexpr inline ray<T, N> transform_ray(
    const frame<T, N>& a, const ray<T, N>& b) {
    return {transform_point(a, b.o), transform_vector(a, b.d), b.tmin, b.tmax};
}
template <typename T, int N>
constexpr inline ray<T, N> transform_ray(
    const mat<T, N + 1, N + 1>& a, const ray<T, N>& b) {
    return {transform_point(a, b.o), transform_vector(a, b.d), b.tmin, b.tmax};
}
template <typename T>
constexpr inline bbox<T, 3> transform_bbox(
    const frame<T, 3>& a, const bbox<T, 3>& b) {
    auto corners = {vec<T, 3>{b.min.x, b.min.y, b.min.z},
        vec<T, 3>{b.min.x, b.min.y, b.max.z},
        vec<T, 3>{b.min.x, b.max.y, b.min.z},
        vec<T, 3>{b.min.x, b.max.y, b.max.z},
        vec<T, 3>{b.max.x, b.min.y, b.min.z},
        vec<T, 3>{b.max.x, b.min.y, b.max.z},
        vec<T, 3>{b.max.x, b.max.y, b.min.z},
        vec<T, 3>{b.max.x, b.max.y, b.max.z}};
    auto xformed = bbox<T, 3>();
    for (auto& corner : corners) xformed += transform_point(a, corner);
    return xformed;
}
template <typename T>
constexpr inline bbox<T, 3> transform_bbox(
    const mat<T, 4, 4>& a, const bbox<T, 3>& b) {
    auto corners = {vec<T, 3>{b.min.x, b.min.y, b.min.z},
        vec<T, 3>{b.min.x, b.min.y, b.max.z},
        vec<T, 3>{b.min.x, b.max.y, b.min.z},
        vec<T, 3>{b.min.x, b.max.y, b.max.z},
        vec<T, 3>{b.max.x, b.min.y, b.min.z},
        vec<T, 3>{b.max.x, b.min.y, b.max.z},
        vec<T, 3>{b.max.x, b.max.y, b.min.z},
        vec<T, 3>{b.max.x, b.max.y, b.max.z}};
    auto xformed = bbox<T, 3>();
    for (auto& corner : corners) xformed += transform_point(a, corner);
    return xformed;
}

// Inverse transforms by frames, assuming they are rigid transforms.
template <typename T, int N>
constexpr inline vec<T, N> transform_point_inverse(
    const frame<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {dot(b - a.o, a.x)};
    } else if constexpr (N == 2) {
        return {dot(b - a.o, a.x), dot(b - a.o, a.y)};
    } else if constexpr (N == 3) {
        return {dot(b - a.o, a.x), dot(b - a.o, a.y), dot(b - a.o, a.z)};
    } else {
    }
}
template <typename T, int N>
constexpr inline vec<T, N> transform_vector_inverse(
    const frame<T, N>& a, const vec<T, N>& b) {
    if constexpr (N == 1) {
        return {dot(b, a.x)};
    } else if constexpr (N == 2) {
        return {dot(b, a.x), dot(b, a.y)};
    } else if constexpr (N == 3) {
        return {dot(b, a.x), dot(b, a.y), dot(b, a.z)};
    } else {
    }
}
template <typename T, int N>
constexpr inline vec<T, N> transform_direction_inverse(
    const frame<T, N>& a, const vec<T, N>& b) {
    return normalize(transform_vector_inverse(a, b));
}
template <typename T, int N>
constexpr inline ray<T, N> transform_ray_inverse(
    const frame<T, N>& a, const ray<T, N>& b) {
    return {transform_point_inverse(a, b.o),
        transform_direction_inverse(a, b.d), b.tmin, b.tmax};
}
template <typename T, int N>
constexpr inline bbox<T, N> transform_bbox_inverse(
    const frame<T, N>& a, const bbox<T, N>& b) {
    return transform_bbox(inverse(a), b);
}

// Translation, scaling and rotations transforms.
template <typename T>
constexpr inline frame<T, 3> make_translation_frame(const vec<T, 3>& a) {
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, a};
}
template <typename T>
constexpr inline frame<T, 3> make_scaling_frame(const vec<T, 3>& a) {
    return {{a.x, 0, 0}, {0, a.y, 0}, {0, 0, a.z}, {0, 0, 0}};
}
template <typename T, typename T1>
constexpr inline frame<T, 3> make_rotation_frame(
    const vec<T, 3>& axis, T1 angle) {
    auto s = sin(angle), c = cos(angle);
    auto vv = normalize(axis);
    return {{c + (1 - c) * vv.x * vv.x, (1 - c) * vv.x * vv.y + s * vv.z,
                (1 - c) * vv.x * vv.z - s * vv.y},
        {(1 - c) * vv.x * vv.y - s * vv.z, c + (1 - c) * vv.y * vv.y,
            (1 - c) * vv.y * vv.z + s * vv.x},
        {(1 - c) * vv.x * vv.z + s * vv.y, (1 - c) * vv.y * vv.z - s * vv.x,
            c + (1 - c) * vv.z * vv.z},
        {0, 0, 0}};
}
template <typename T>
constexpr inline frame<T, 3> make_rotation_frame(const vec<T, 4>& quat) {
    auto v = quat;
    return {{v.w * v.w + v.x * v.x - v.y * v.y - v.z * v.z,
                (v.x * v.y + v.z * v.w) * 2, (v.z * v.x - v.y * v.w) * 2},
        {(v.x * v.y - v.z * v.w) * 2,
            v.w * v.w - v.x * v.x + v.y * v.y - v.z * v.z,
            (v.y * v.z + v.x * v.w) * 2},
        {(v.z * v.x + v.y * v.w) * 2, (v.y * v.z - v.x * v.w) * 2,
            v.w * v.w - v.x * v.x - v.y * v.y + v.z * v.z},
        {0, 0, 0}};
}
template <typename T>
constexpr inline frame<T, 3> make_rotation_frame(const quat<T, 4>& quat) {
    auto v = quat;
    return {{v.w * v.w + v.x * v.x - v.y * v.y - v.z * v.z,
                (v.x * v.y + v.z * v.w) * 2, (v.z * v.x - v.y * v.w) * 2},
        {(v.x * v.y - v.z * v.w) * 2,
            v.w * v.w - v.x * v.x + v.y * v.y - v.z * v.z,
            (v.y * v.z + v.x * v.w) * 2},
        {(v.z * v.x + v.y * v.w) * 2, (v.y * v.z - v.x * v.w) * 2,
            v.w * v.w - v.x * v.x - v.y * v.y + v.z * v.z},
        {0, 0, 0}};
}
template <typename T>
constexpr inline frame<T, 3> make_rotation_frame(const mat<T, 3, 3>& rot) {
    return {rot.x, rot.y, rot.z, {0, 0, 0}};
}

// Lookat frame. Z-axis can be inverted with inv_xz.
template <typename T>
constexpr inline frame<T, 3> make_lookat_frame(const vec<T, 3>& eye,
    const vec<T, 3>& center, const vec<T, 3>& up, bool inv_xz = false) {
    auto w = normalize(eye - center);
    auto u = normalize(cross(up, w));
    auto v = normalize(cross(w, u));
    if (inv_xz) {
        w = -w;
        u = -u;
    }
    return {u, v, w, eye};
}

// OpenGL frustum, ortho and perspecgive matrices.
template <typename T>
constexpr inline mat<T, 4, 4> make_frustum_mat(T l, T r, T b, T t, T n, T f) {
    return {{2 * n / (r - l), 0, 0, 0}, {0, 2 * n / (t - b), 0, 0},
        {(r + l) / (r - l), (t + b) / (t - b), -(f + n) / (f - n), -1},
        {0, 0, -2 * f * n / (f - n), 0}};
}
template <typename T>
constexpr inline mat<T, 4, 4> make_ortho_mat(T l, T r, T b, T t, T n, T f) {
    return {{2 / (r - l), 0, 0, 0}, {0, 2 / (t - b), 0, 0},
        {0, 0, -2 / (f - n), 0},
        {-(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1}};
}
template <typename T>
constexpr inline mat<T, 4, 4> make_ortho2d_mat(
    T left, T right, T bottom, T top) {
    return make_ortho_mat(left, right, bottom, top, -1, 1);
}
template <typename T>
constexpr inline mat<T, 4, 4> make_ortho_mat(T xmag, T ymag, T near, T far) {
    return {{1 / xmag, 0, 0, 0}, {0, 1 / ymag, 0, 0},
        {0, 0, 2 / (near - far), 0}, {0, 0, (far + near) / (near - far), 1}};
}
template <typename T>
constexpr inline mat<T, 4, 4> make_perspective_mat(
    T fovy, T aspect, T near, T far) {
    auto tg = tan(fovy / 2);
    return {{1 / (aspect * tg), 0, 0, 0}, {0, 1 / tg, 0, 0},
        {0, 0, (far + near) / (near - far), -1},
        {0, 0, 2 * far * near / (near - far), 0}};
}
template <typename T>
constexpr inline mat<T, 4, 4> perspective_mat(T fovy, T aspect, T near) {
    auto tg = tan(fovy / 2);
    return {{1 / (aspect * tg), 0, 0, 0}, {0, 1 / tg, 0, 0}, {0, 0, -1, -1},
        {0, 0, 2 * near, 0}};
}

// Rotation conversions.
template <typename T>
constexpr inline pair<vec<T, 3>, T> make_rotation_axisangle(
    const vec<T, 4>& quat) {
    return {normalize(vec<T, 3>{quat.x, quat.y, quat.z}), 2 * acos(quat.w)};
}
template <typename T, typename T1>
constexpr inline vec<T, 4> make_rotation_quat(const vec<T, 3>& axis, T1 angle) {
    auto len = length(axis);
    if (!len) return {0, 0, 0, 1};
    return vec<T, 4>{sin(angle / 2) * axis.x / len,
        sin(angle / 2) * axis.y / len, sin(angle / 2) * axis.z / len,
        cos(angle / 2)};
}
template <typename T>
constexpr inline vec<T, 4> make_rotation_quat(const vec<T, 4>& axisangle) {
    return make_rotation_quat(
        vec<T, 3>{axisangle.x, axisangle.y, axisangle.z}, axisangle.w);
}

// Turntable and FPS Camera navigation.
template <typename T, typename T1>
constexpr inline void update_camera_turntable(vec<T, 3>& from, vec<T, 3>& to,
    vec<T, 3>& up, const vec<T, 2>& rotate, T1 dolly, const vec<T, 2>& pan);
template <typename T, typename T1>
constexpr inline void update_camera_turntable(frame<T, 3>& frame, T& focus,
    const vec<T, 2>& rotate, T1 dolly, const vec<T, 2>& pan);
template <typename T>
constexpr inline void update_camera_fps(
    frame<T, 3>& frame, const vec<T, 3>& transl, const vec<T, 2>& rotate);

// Computes the image uv coordinates corresponding to the view parameters.
// Returns negative coordinates if out of the image.
template <typename T, typename T1>
constexpr inline vec2i get_image_coords(const vec<T, 2>& mouse_pos,
    const vec<T, 2>& center, T1 scale, const vec2i& txt_size) {
    auto xyf = (mouse_pos - center) / scale;
    return vec2i{(int)round(xyf.x + txt_size.x / 2.0f),
        (int)round(xyf.y + txt_size.y / 2.0f)};
}

// Center image and autofit.
template <typename T>
constexpr inline void update_image_view(vec<T, 2>& center, T& scale,
    const vec2i& imsize, const vec2i& winsize, bool zoom_to_fit) {
    if (zoom_to_fit) {
        scale  = min(winsize.x / (T)imsize.x, winsize.y / (T)imsize.y);
        center = {(T)winsize.x / 2, (T)winsize.y / 2};
    } else {
        if (winsize.x >= imsize.x * scale) center.x = winsize.x / 2;
        if (winsize.y >= imsize.y * scale) center.y = winsize.y / 2;
    }
}

}  // namespace yocto

// ---------------------------------------------------------------------------//
//                                                                            //
//                             IMPLEMENTATION                                 //
//                                                                            //
// ---------------------------------------------------------------------------//

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF MATRICES
// -----------------------------------------------------------------------------
namespace yocto {

// Matrix diagonals and transposes.
template <typename T, int N, int M>
constexpr inline mat<T, M, N> transpose(const mat<T, N, M>& a) {
    if constexpr (N == 1 && M == 1) {
        return a;
    } else if constexpr (N == 2 && M == 2) {
        return {{a.x.x, a.y.x}, {a.x.y, a.y.y}};
    } else if constexpr (N == 3 && M == 3) {
        return {
            {a.x.x, a.y.x, a.z.x},
            {a.x.y, a.y.y, a.z.y},
            {a.x.z, a.y.z, a.z.z},
        };
    } else if constexpr (N == 4 && M == 4) {
        return {
            {a.x.x, a.y.x, a.z.x, a.w.x},
            {a.x.y, a.y.y, a.z.y, a.w.y},
            {a.x.z, a.y.z, a.z.z, a.w.z},
            {a.x.w, a.y.w, a.z.w, a.w.w},
        };
    } else {
        auto c = mat<T, M, N>{};
        for (auto i = 0; i < M; i++)
            for (auto j = 0; j < N; j++) c[j][i] = a[i][j];
    }
}

// Matrix adjugates, determinant and inverses.
template <typename T, int N>
constexpr inline mat<T, N, N> adjugate(const mat<T, N, N>& a) {
    if constexpr (N == 1) {
        return {a.x};
    } else if constexpr (N == 2) {
        return {{a.y.y, -a.x.y}, {-a.y.x, a.x.x}};
    } else if constexpr (N == 3) {
        return {
            {
                a.y.y * a.z.z - a.z.y * a.y.z,
                a.z.y * a.x.z - a.x.y * a.z.z,
                a.x.y * a.y.z - a.y.y * a.x.z,
            },
            {
                a.y.z * a.z.x - a.z.z * a.y.x,
                a.z.z * a.x.x - a.x.z * a.z.x,
                a.x.z * a.y.x - a.y.z * a.x.x,
            },
            {
                a.y.x * a.z.y - a.z.x * a.y.y,
                a.z.x * a.x.y - a.x.x * a.z.y,
                a.x.x * a.y.y - a.y.x * a.x.y,
            },
        };
    } else if constexpr (N == 4) {
        return {
            {
                a.y.y * a.z.z * a.w.w + a.w.y * a.y.z * a.z.w +
                    a.z.y * a.w.z * a.y.w - a.y.y * a.w.z * a.z.w -
                    a.z.y * a.y.z * a.w.w - a.w.y * a.z.z * a.y.w,
                a.x.y * a.w.z * a.z.w + a.z.y * a.x.z * a.w.w +
                    a.w.y * a.z.z * a.x.w - a.w.y * a.x.z * a.z.w -
                    a.z.y * a.w.z * a.x.w - a.x.y * a.z.z * a.w.w,
                a.x.y * a.y.z * a.w.w + a.w.y * a.x.z * a.y.w +
                    a.y.y * a.w.z * a.x.w - a.x.y * a.w.z * a.y.w -
                    a.y.y * a.x.z * a.w.w - a.w.y * a.y.z * a.x.w,
                a.x.y * a.z.z * a.y.w + a.y.y * a.x.z * a.z.w +
                    a.z.y * a.y.z * a.x.w - a.x.y * a.y.z * a.z.w -
                    a.z.y * a.x.z * a.y.w - a.y.y * a.z.z * a.x.w,
            },
            {
                a.y.z * a.w.w * a.z.x + a.z.z * a.y.w * a.w.x +
                    a.w.z * a.z.w * a.y.x - a.y.z * a.z.w * a.w.x -
                    a.w.z * a.y.w * a.z.x - a.z.z * a.w.w * a.y.x,
                a.x.z * a.z.w * a.w.x + a.w.z * a.x.w * a.z.x +
                    a.z.z * a.w.w * a.x.x - a.x.z * a.w.w * a.z.x -
                    a.z.z * a.x.w * a.w.x - a.w.z * a.z.w * a.x.x,
                a.x.z * a.w.w * a.y.x + a.y.z * a.x.w * a.w.x +
                    a.w.z * a.y.w * a.x.x - a.x.z * a.y.w * a.w.x -
                    a.w.z * a.x.w * a.y.x - a.y.z * a.w.w * a.x.x,
                a.x.z * a.y.w * a.z.x + a.z.z * a.x.w * a.y.x +
                    a.y.z * a.z.w * a.x.x - a.x.z * a.z.w * a.y.x -
                    a.y.z * a.x.w * a.z.x - a.z.z * a.y.w * a.x.x,
            },
            {
                a.y.w * a.z.x * a.w.y + a.w.w * a.y.x * a.z.y +
                    a.z.w * a.w.x * a.y.y - a.y.w * a.w.x * a.z.y -
                    a.z.w * a.y.x * a.w.y - a.w.w * a.z.x * a.y.y,
                a.x.w * a.w.x * a.z.y + a.z.w * a.x.x * a.w.y +
                    a.w.w * a.z.x * a.x.y - a.x.w * a.z.x * a.w.y -
                    a.w.w * a.x.x * a.z.y - a.z.w * a.w.x * a.x.y,
                a.x.w * a.y.x * a.w.y + a.w.w * a.x.x * a.y.y +
                    a.y.w * a.w.x * a.x.y - a.x.w * a.w.x * a.y.y -
                    a.y.w * a.x.x * a.w.y - a.w.w * a.y.x * a.x.y,
                a.x.w * a.z.x * a.y.y + a.y.w * a.x.x * a.z.y +
                    a.z.w * a.y.x * a.x.y - a.x.w * a.y.x * a.z.y -
                    a.z.w * a.x.x * a.y.y - a.y.w * a.z.x * a.x.y,
            },
            {
                a.y.x * a.w.y * a.z.z + a.z.x * a.y.y * a.w.z +
                    a.w.x * a.z.y * a.y.z - a.y.x * a.z.y * a.w.z -
                    a.w.x * a.y.y * a.z.z - a.z.x * a.w.y * a.y.z,
                a.x.x * a.z.y * a.w.z + a.w.x * a.x.y * a.z.z +
                    a.z.x * a.w.y * a.x.z - a.x.x * a.w.y * a.z.z -
                    a.z.x * a.x.y * a.w.z - a.w.x * a.z.y * a.x.z,
                a.x.x * a.w.y * a.y.z + a.y.x * a.x.y * a.w.z +
                    a.w.x * a.y.y * a.x.z - a.x.x * a.y.y * a.w.z -
                    a.w.x * a.x.y * a.y.z - a.y.x * a.w.y * a.x.z,
                a.x.x * a.y.y * a.z.z + a.z.x * a.x.y * a.y.z +
                    a.y.x * a.z.y * a.x.z - a.x.x * a.z.y * a.y.z -
                    a.y.x * a.x.y * a.z.z - a.z.x * a.y.y * a.x.z,
            },
        };
    } else {
        throw invalid_argument("matrix size not supported");
    }
}
template <typename T, int N>
constexpr inline T determinant(const mat<T, N, N>& a) {
    if constexpr (N == 1) {
        return {a.x};
    } else if constexpr (N == 2) {
        return a.x.x * a.y.y - a.x.y * a.y.x;
    } else if constexpr (N == 3) {
        return a.x.x * (a.y.y * a.z.z - a.z.y * a.y.z) +
               a.x.y * (a.y.z * a.z.x - a.z.z * a.y.x) +
               a.x.z * (a.y.x * a.z.y - a.z.x * a.y.y);
    } else if constexpr (N == 4) {
        return a.x.x * (a.y.y * a.z.z * a.w.w + a.w.y * a.y.z * a.z.w +
                           a.z.y * a.w.z * a.y.w - a.y.y * a.w.z * a.z.w -
                           a.z.y * a.y.z * a.w.w - a.w.y * a.z.z * a.y.w) +
               a.x.y * (a.y.z * a.w.w * a.z.x + a.z.z * a.y.w * a.w.x +
                           a.w.z * a.z.w * a.y.x - a.y.z * a.z.w * a.w.x -
                           a.w.z * a.y.w * a.z.x - a.z.z * a.w.w * a.y.x) +
               a.x.z * (a.y.w * a.z.x * a.w.y + a.w.w * a.y.x * a.z.y +
                           a.z.w * a.w.x * a.y.y - a.y.w * a.w.x * a.z.y -
                           a.z.w * a.y.x * a.w.y - a.w.w * a.z.x * a.y.y) +
               a.x.w * (a.y.x * a.w.y * a.z.z + a.z.x * a.y.y * a.w.z +
                           a.w.x * a.z.y * a.y.z - a.y.x * a.z.y * a.w.z -
                           a.w.x * a.y.y * a.z.z - a.z.x * a.w.y * a.y.z);
    } else {
        throw invalid_argument("matrix size not supported");
    }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF UI UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Turntable for UI navigation.
template <typename T, typename T1>
constexpr inline void update_camera_turntable(vec<T, 3>& from, vec<T, 3>& to,
    vec<T, 3>& up, const vec<T, 2>& rotate, T1 dolly, const vec<T, 2>& pan) {
    // rotate if necessary
    if (rotate.x || rotate.y) {
        auto z     = normalize(to - from);
        auto lz    = length(to - from);
        auto phi   = atan2(z.z, z.x) + rotate.x;
        auto theta = acos(z.y) + rotate.y;
        theta      = clamp(theta, 0.001f, pi - 0.001f);
        auto nz    = vec<T, 3>{sin(theta) * cos(phi) * lz, cos(theta) * lz,
            sin(theta) * sin(phi) * lz};
        from       = to - nz;
    }

    // dolly if necessary
    if (dolly) {
        auto z  = normalize(to - from);
        auto lz = max(0.001f, length(to - from) * (1 + dolly));
        z *= lz;
        from = to - z;
    }

    // pan if necessary
    if (pan.x || pan.y) {
        auto z = normalize(to - from);
        auto x = normalize(cross(up, z));
        auto y = normalize(cross(z, x));
        auto t = vec<T, 3>{pan.x * x.x + pan.y * y.x, pan.x * x.y + pan.y * y.y,
            pan.x * x.z + pan.y * y.z};
        from += t;
        to += t;
    }
}

// Turntable for UI navigation.
template <typename T, typename T1>
constexpr inline void update_camera_turntable(frame<T, 3>& frame, T& focus,
    const vec<T, 2>& rotate, T1 dolly, const vec<T, 2>& pan) {
    // rotate if necessary
    if (rotate != zero2f) {
        auto phi   = atan2(frame.z.z, frame.z.x) + rotate.x;
        auto theta = acos(frame.z.y) + rotate.y;
        theta      = clamp(theta, (T)0.001, (T)pi - (T)0.001);
        auto new_z = vec<T, 3>{
            sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)};
        auto new_center = frame.o - frame.z * focus;
        auto new_o      = new_center + new_z * focus;
        frame           = make_lookat_frame(new_o, new_center, {0, 1, 0});
        focus           = length(new_o - new_center);
    }

    // pan if necessary
    if (dolly) {
        auto c  = frame.o - frame.z * focus;
        focus   = max(focus * (1 + dolly), 0.001f);
        frame.o = c + frame.z * focus;
    }

    // pan if necessary
    if (pan.x || pan.y) {
        frame.o += frame.x * pan.x + frame.y * pan.y;
    }
}

// FPS camera for UI navigation for a frame parametrization.
template <typename T>
constexpr inline void update_camera_first_person(
    frame<T, 3>& frame, const vec<T, 3>& transl, const vec<T, 2>& rotate) {
    // https://gamedev.stackexchange.com/questions/30644/how-to-keep-my-quaternion-using-fps-camera-from-tilting-and-messing-up
    auto y = vec<T, 3>{0, 1, 0};
    auto z = orthonormalize(frame.z, y);
    auto x = cross(y, z);

    auto rot = make_rotation_frame(vec<T, 3>{1, 0, 0}, rotate.y) *
               yocto::frame<T, 3>{
                   frame.x, frame.y, frame.z, vec<T, 3>{0, 0, 0}} *
               make_rotation_frame(vec<T, 3>{0, 1, 0}, rotate.x);
    auto pos = frame.o + transl.x * x + transl.y * y + transl.z * z;

    frame = {rot.x, rot.y, rot.z, pos};
}

}  // namespace yocto

#endif
