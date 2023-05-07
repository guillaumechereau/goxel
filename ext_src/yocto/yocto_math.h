//
// # Yocto/Math: Math types
//
// Yocto/Math defines the basic math primitives used in graphics, including
// small-sized vectors, matrices, frames, quaternions, rays, bounding boxes
// and their transforms. Yocto/Math is implemented in `yocto_math.h`.
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2022 Fabio Pellacini
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

#ifndef _YOCTO_MATH_H_
#define _YOCTO_MATH_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::pair;

}  // namespace yocto

// -----------------------------------------------------------------------------
// CUDA SUPPORT
// -----------------------------------------------------------------------------
#ifdef __CUDACC__
#define inline inline __device__ __forceinline__
#endif

// -----------------------------------------------------------------------------
// MATH CONSTANTS AND FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

using byte   = unsigned char;
using uint   = unsigned int;
using ushort = unsigned short;

constexpr auto pi  = 3.14159265358979323846;
constexpr auto pif = (float)pi;

constexpr auto int_max = std::numeric_limits<int>::max();
constexpr auto int_min = std::numeric_limits<int>::lowest();
constexpr auto flt_max = std::numeric_limits<float>::max();
constexpr auto flt_min = std::numeric_limits<float>::lowest();
constexpr auto flt_eps = std::numeric_limits<float>::epsilon();

inline float abs(float a);
inline float min(float a, float b);
inline float max(float a, float b);
inline float clamp(float a, float min, float max);
inline float sign(float a);
inline float sqr(float a);
inline float sqrt(float a);
inline float sin(float a);
inline float cos(float a);
inline float tan(float a);
inline float asin(float a);
inline float acos(float a);
inline float atan(float a);
inline float log(float a);
inline float exp(float a);
inline float log2(float a);
inline float exp2(float a);
inline float pow(float a, float b);
inline bool  isfinite(float a);
inline float atan2(float a, float b);
inline float fmod(float a, float b);
inline float radians(float a);
inline float degrees(float a);
inline float lerp(float a, float b, float u);
inline void  swap(float& a, float& b);
inline float smoothstep(float a, float b, float u);
inline float bias(float a, float bias);
inline float gain(float a, float gain);

inline int  abs(int a);
inline int  min(int a, int b);
inline int  max(int a, int b);
inline int  clamp(int a, int min, int max);
inline int  sign(int a);
inline int  pow2(int a);
inline void swap(int& a, int& b);

inline size_t min(size_t a, size_t b);
inline size_t max(size_t a, size_t b);

}  // namespace yocto

// -----------------------------------------------------------------------------
// VECTORS
// -----------------------------------------------------------------------------
namespace yocto {

struct vec2f {
  float x = 0;
  float y = 0;

  inline float&       operator[](int i);
  inline const float& operator[](int i) const;
};

struct vec3f {
  float x = 0;
  float y = 0;
  float z = 0;

  inline float&       operator[](int i);
  inline const float& operator[](int i) const;
};

struct vec4f {
  float x = 0;
  float y = 0;
  float z = 0;
  float w = 0;

  inline float&       operator[](int i);
  inline const float& operator[](int i) const;
};

// Zero vector constants.
constexpr auto zero2f = vec2f{0, 0};
constexpr auto zero3f = vec3f{0, 0, 0};
constexpr auto zero4f = vec4f{0, 0, 0, 0};

// One vector constants.
constexpr auto one2f = vec2f{1, 1};
constexpr auto one3f = vec3f{1, 1, 1};
constexpr auto one4f = vec4f{1, 1, 1, 1};

// Element access
inline vec3f xyz(const vec4f& a);

// Vector sequence operations.
inline int          size(const vec2f& a);
inline const float* begin(const vec2f& a);
inline const float* end(const vec2f& a);
inline float*       begin(vec2f& a);
inline float*       end(vec2f& a);
inline const float* data(const vec2f& a);
inline float*       data(vec2f& a);

// Vector comparison operations.
inline bool operator==(const vec2f& a, const vec2f& b);
inline bool operator!=(const vec2f& a, const vec2f& b);

// Vector operations.
inline vec2f operator+(const vec2f& a);
inline vec2f operator-(const vec2f& a);
inline vec2f operator+(const vec2f& a, const vec2f& b);
inline vec2f operator+(const vec2f& a, float b);
inline vec2f operator+(float a, const vec2f& b);
inline vec2f operator-(const vec2f& a, const vec2f& b);
inline vec2f operator-(const vec2f& a, float b);
inline vec2f operator-(float a, const vec2f& b);
inline vec2f operator*(const vec2f& a, const vec2f& b);
inline vec2f operator*(const vec2f& a, float b);
inline vec2f operator*(float a, const vec2f& b);
inline vec2f operator/(const vec2f& a, const vec2f& b);
inline vec2f operator/(const vec2f& a, float b);
inline vec2f operator/(float a, const vec2f& b);

// Vector assignments
inline vec2f& operator+=(vec2f& a, const vec2f& b);
inline vec2f& operator+=(vec2f& a, float b);
inline vec2f& operator-=(vec2f& a, const vec2f& b);
inline vec2f& operator-=(vec2f& a, float b);
inline vec2f& operator*=(vec2f& a, const vec2f& b);
inline vec2f& operator*=(vec2f& a, float b);
inline vec2f& operator/=(vec2f& a, const vec2f& b);
inline vec2f& operator/=(vec2f& a, float b);

// Vector products and lengths.
inline float dot(const vec2f& a, const vec2f& b);
inline float cross(const vec2f& a, const vec2f& b);

inline float length(const vec2f& a);
inline float length_squared(const vec2f& a);
inline vec2f normalize(const vec2f& a);
inline float distance(const vec2f& a, const vec2f& b);
inline float distance_squared(const vec2f& a, const vec2f& b);
inline float angle(const vec2f& a, const vec2f& b);

// Max element and clamp.
inline vec2f max(const vec2f& a, float b);
inline vec2f min(const vec2f& a, float b);
inline vec2f max(const vec2f& a, const vec2f& b);
inline vec2f min(const vec2f& a, const vec2f& b);
inline vec2f clamp(const vec2f& x, float min, float max);
inline vec2f lerp(const vec2f& a, const vec2f& b, float u);
inline vec2f lerp(const vec2f& a, const vec2f& b, const vec2f& u);

inline float max(const vec2f& a);
inline float min(const vec2f& a);
inline float sum(const vec2f& a);
inline float mean(const vec2f& a);

// Functions applied to vector elements
inline vec2f abs(const vec2f& a);
inline vec2f sqr(const vec2f& a);
inline vec2f sqrt(const vec2f& a);
inline vec2f exp(const vec2f& a);
inline vec2f log(const vec2f& a);
inline vec2f exp2(const vec2f& a);
inline vec2f log2(const vec2f& a);
inline bool  isfinite(const vec2f& a);
inline vec2f pow(const vec2f& a, float b);
inline vec2f pow(const vec2f& a, const vec2f& b);
inline vec2f gain(const vec2f& a, float b);
inline void  swap(vec2f& a, vec2f& b);

// Vector sequence operations.
inline int          size(const vec3f& a);
inline const float* begin(const vec3f& a);
inline const float* end(const vec3f& a);
inline float*       begin(vec3f& a);
inline float*       end(vec3f& a);
inline const float* data(const vec3f& a);
inline float*       data(vec3f& a);

// Vector comparison operations.
inline bool operator==(const vec3f& a, const vec3f& b);
inline bool operator!=(const vec3f& a, const vec3f& b);

// Vector operations.
inline vec3f operator+(const vec3f& a);
inline vec3f operator-(const vec3f& a);
inline vec3f operator+(const vec3f& a, const vec3f& b);
inline vec3f operator+(const vec3f& a, float b);
inline vec3f operator+(float a, const vec3f& b);
inline vec3f operator-(const vec3f& a, const vec3f& b);
inline vec3f operator-(const vec3f& a, float b);
inline vec3f operator-(float a, const vec3f& b);
inline vec3f operator*(const vec3f& a, const vec3f& b);
inline vec3f operator*(const vec3f& a, float b);
inline vec3f operator*(float a, const vec3f& b);
inline vec3f operator/(const vec3f& a, const vec3f& b);
inline vec3f operator/(const vec3f& a, float b);
inline vec3f operator/(float a, const vec3f& b);

// Vector assignments
inline vec3f& operator+=(vec3f& a, const vec3f& b);
inline vec3f& operator+=(vec3f& a, float b);
inline vec3f& operator-=(vec3f& a, const vec3f& b);
inline vec3f& operator-=(vec3f& a, float b);
inline vec3f& operator*=(vec3f& a, const vec3f& b);
inline vec3f& operator*=(vec3f& a, float b);
inline vec3f& operator/=(vec3f& a, const vec3f& b);
inline vec3f& operator/=(vec3f& a, float b);

// Vector products and lengths.
inline float dot(const vec3f& a, const vec3f& b);
inline vec3f cross(const vec3f& a, const vec3f& b);

inline float length(const vec3f& a);
inline float length_squared(const vec3f& a);
inline vec3f normalize(const vec3f& a);
inline float distance(const vec3f& a, const vec3f& b);
inline float distance_squared(const vec3f& a, const vec3f& b);
inline float angle(const vec3f& a, const vec3f& b);

// Orthogonal vectors.
inline vec3f orthogonal(const vec3f& v);
inline vec3f orthonormalize(const vec3f& a, const vec3f& b);

// Reflected and refracted vector.
inline vec3f reflect(const vec3f& w, const vec3f& n);
inline vec3f refract(const vec3f& w, const vec3f& n, float inv_eta);

// Max element and clamp.
inline vec3f max(const vec3f& a, float b);
inline vec3f min(const vec3f& a, float b);
inline vec3f max(const vec3f& a, const vec3f& b);
inline vec3f min(const vec3f& a, const vec3f& b);
inline vec3f clamp(const vec3f& x, float min, float max);
inline vec3f lerp(const vec3f& a, const vec3f& b, float u);
inline vec3f lerp(const vec3f& a, const vec3f& b, const vec3f& u);

inline float max(const vec3f& a);
inline float min(const vec3f& a);
inline float sum(const vec3f& a);
inline float mean(const vec3f& a);

// Functions applied to vector elements
inline vec3f abs(const vec3f& a);
inline vec3f sqr(const vec3f& a);
inline vec3f sqrt(const vec3f& a);
inline vec3f exp(const vec3f& a);
inline vec3f log(const vec3f& a);
inline vec3f exp2(const vec3f& a);
inline vec3f log2(const vec3f& a);
inline vec3f pow(const vec3f& a, float b);
inline vec3f pow(const vec3f& a, const vec3f& b);
inline vec3f gain(const vec3f& a, float b);
inline bool  isfinite(const vec3f& a);
inline void  swap(vec3f& a, vec3f& b);

// Vector sequence operations.
inline int          size(const vec4f& a);
inline const float* begin(const vec4f& a);
inline const float* end(const vec4f& a);
inline float*       begin(vec4f& a);
inline float*       end(vec4f& a);
inline const float* data(const vec4f& a);
inline float*       data(vec4f& a);

// Vector comparison operations.
inline bool operator==(const vec4f& a, const vec4f& b);
inline bool operator!=(const vec4f& a, const vec4f& b);

// Vector operations.
inline vec4f operator+(const vec4f& a);
inline vec4f operator-(const vec4f& a);
inline vec4f operator+(const vec4f& a, const vec4f& b);
inline vec4f operator+(const vec4f& a, float b);
inline vec4f operator+(float a, const vec4f& b);
inline vec4f operator-(const vec4f& a, const vec4f& b);
inline vec4f operator-(const vec4f& a, float b);
inline vec4f operator-(float a, const vec4f& b);
inline vec4f operator*(const vec4f& a, const vec4f& b);
inline vec4f operator*(const vec4f& a, float b);
inline vec4f operator*(float a, const vec4f& b);
inline vec4f operator/(const vec4f& a, const vec4f& b);
inline vec4f operator/(const vec4f& a, float b);
inline vec4f operator/(float a, const vec4f& b);

// Vector assignments
inline vec4f& operator+=(vec4f& a, const vec4f& b);
inline vec4f& operator+=(vec4f& a, float b);
inline vec4f& operator-=(vec4f& a, const vec4f& b);
inline vec4f& operator-=(vec4f& a, float b);
inline vec4f& operator*=(vec4f& a, const vec4f& b);
inline vec4f& operator*=(vec4f& a, float b);
inline vec4f& operator/=(vec4f& a, const vec4f& b);
inline vec4f& operator/=(vec4f& a, float b);

// Vector products and lengths.
inline float dot(const vec4f& a, const vec4f& b);
inline float length(const vec4f& a);
inline float length_squared(const vec4f& a);
inline vec4f normalize(const vec4f& a);
inline float distance(const vec4f& a, const vec4f& b);
inline float distance_squared(const vec4f& a, const vec4f& b);
inline float angle(const vec4f& a, const vec4f& b);

inline vec4f slerp(const vec4f& a, const vec4f& b, float u);

// Max element and clamp.
inline vec4f max(const vec4f& a, float b);
inline vec4f min(const vec4f& a, float b);
inline vec4f max(const vec4f& a, const vec4f& b);
inline vec4f min(const vec4f& a, const vec4f& b);
inline vec4f clamp(const vec4f& x, float min, float max);
inline vec4f lerp(const vec4f& a, const vec4f& b, float u);
inline vec4f lerp(const vec4f& a, const vec4f& b, const vec4f& u);

inline float max(const vec4f& a);
inline float min(const vec4f& a);
inline float sum(const vec4f& a);
inline float mean(const vec4f& a);

// Functions applied to vector elements
inline vec4f abs(const vec4f& a);
inline vec4f sqr(const vec4f& a);
inline vec4f sqrt(const vec4f& a);
inline vec4f exp(const vec4f& a);
inline vec4f log(const vec4f& a);
inline vec4f exp2(const vec4f& a);
inline vec4f log2(const vec4f& a);
inline vec4f pow(const vec4f& a, float b);
inline vec4f pow(const vec4f& a, const vec4f& b);
inline vec4f gain(const vec4f& a, float b);
inline bool  isfinite(const vec4f& a);
inline void  swap(vec4f& a, vec4f& b);

// Quaternion operatons represented as xi + yj + zk + w
// const auto identity_quat4f = vec4f{0, 0, 0, 1};
inline vec4f quat_mul(const vec4f& a, float b);
inline vec4f quat_mul(const vec4f& a, const vec4f& b);
inline vec4f quat_conjugate(const vec4f& a);
inline vec4f quat_inverse(const vec4f& a);

}  // namespace yocto

// -----------------------------------------------------------------------------
// INTEGER VECTORS
// -----------------------------------------------------------------------------
namespace yocto {

struct vec2i {
  int x = 0;
  int y = 0;

  inline int&       operator[](int i);
  inline const int& operator[](int i) const;
};

struct vec3i {
  int x = 0;
  int y = 0;
  int z = 0;

  inline int&       operator[](int i);
  inline const int& operator[](int i) const;
};

struct vec4i {
  int x = 0;
  int y = 0;
  int z = 0;
  int w = 0;

  inline int&       operator[](int i);
  inline const int& operator[](int i) const;
};

struct vec4b {
  byte x = 0;
  byte y = 0;
  byte z = 0;
  byte w = 0;

  inline byte&       operator[](int i);
  inline const byte& operator[](int i) const;
};

// Zero vector constants.
constexpr auto zero2i = vec2i{0, 0};
constexpr auto zero3i = vec3i{0, 0, 0};
constexpr auto zero4i = vec4i{0, 0, 0, 0};
constexpr auto zero4b = vec4b{0, 0, 0, 0};

// Element access
inline vec3i xyz(const vec4i& a);

// Vector sequence operations.
inline int        size(const vec2i& a);
inline const int* begin(const vec2i& a);
inline const int* end(const vec2i& a);
inline int*       begin(vec2i& a);
inline int*       end(vec2i& a);
inline const int* data(const vec2i& a);
inline int*       data(vec2i& a);

// Vector comparison operations.
inline bool operator==(const vec2i& a, const vec2i& b);
inline bool operator!=(const vec2i& a, const vec2i& b);

// Vector operations.
inline vec2i operator+(const vec2i& a);
inline vec2i operator-(const vec2i& a);
inline vec2i operator+(const vec2i& a, const vec2i& b);
inline vec2i operator+(const vec2i& a, int b);
inline vec2i operator+(int a, const vec2i& b);
inline vec2i operator-(const vec2i& a, const vec2i& b);
inline vec2i operator-(const vec2i& a, int b);
inline vec2i operator-(int a, const vec2i& b);
inline vec2i operator*(const vec2i& a, const vec2i& b);
inline vec2i operator*(const vec2i& a, int b);
inline vec2i operator*(int a, const vec2i& b);
inline vec2i operator/(const vec2i& a, const vec2i& b);
inline vec2i operator/(const vec2i& a, int b);
inline vec2i operator/(int a, const vec2i& b);

// Vector assignments
inline vec2i& operator+=(vec2i& a, const vec2i& b);
inline vec2i& operator+=(vec2i& a, int b);
inline vec2i& operator-=(vec2i& a, const vec2i& b);
inline vec2i& operator-=(vec2i& a, int b);
inline vec2i& operator*=(vec2i& a, const vec2i& b);
inline vec2i& operator*=(vec2i& a, int b);
inline vec2i& operator/=(vec2i& a, const vec2i& b);
inline vec2i& operator/=(vec2i& a, int b);

// Max element and clamp.
inline vec2i max(const vec2i& a, int b);
inline vec2i min(const vec2i& a, int b);
inline vec2i max(const vec2i& a, const vec2i& b);
inline vec2i min(const vec2i& a, const vec2i& b);
inline vec2i clamp(const vec2i& x, int min, int max);

inline int max(const vec2i& a);
inline int min(const vec2i& a);
inline int sum(const vec2i& a);

// Functions applied to vector elements
inline vec2i abs(const vec2i& a);
inline void  swap(vec2i& a, vec2i& b);

// Vector sequence operations.
inline int        size(const vec3i& a);
inline const int* begin(const vec3i& a);
inline const int* end(const vec3i& a);
inline int*       begin(vec3i& a);
inline int*       end(vec3i& a);
inline const int* data(const vec3i& a);
inline int*       data(vec3i& a);

// Vector comparison operations.
inline bool operator==(const vec3i& a, const vec3i& b);
inline bool operator!=(const vec3i& a, const vec3i& b);

// Vector operations.
inline vec3i operator+(const vec3i& a);
inline vec3i operator-(const vec3i& a);
inline vec3i operator+(const vec3i& a, const vec3i& b);
inline vec3i operator+(const vec3i& a, int b);
inline vec3i operator+(int a, const vec3i& b);
inline vec3i operator-(const vec3i& a, const vec3i& b);
inline vec3i operator-(const vec3i& a, int b);
inline vec3i operator-(int a, const vec3i& b);
inline vec3i operator*(const vec3i& a, const vec3i& b);
inline vec3i operator*(const vec3i& a, int b);
inline vec3i operator*(int a, const vec3i& b);
inline vec3i operator/(const vec3i& a, const vec3i& b);
inline vec3i operator/(const vec3i& a, int b);
inline vec3i operator/(int a, const vec3i& b);

// Vector assignments
inline vec3i& operator+=(vec3i& a, const vec3i& b);
inline vec3i& operator+=(vec3i& a, int b);
inline vec3i& operator-=(vec3i& a, const vec3i& b);
inline vec3i& operator-=(vec3i& a, int b);
inline vec3i& operator*=(vec3i& a, const vec3i& b);
inline vec3i& operator*=(vec3i& a, int b);
inline vec3i& operator/=(vec3i& a, const vec3i& b);
inline vec3i& operator/=(vec3i& a, int b);

// Max element and clamp.
inline vec3i max(const vec3i& a, int b);
inline vec3i min(const vec3i& a, int b);
inline vec3i max(const vec3i& a, const vec3i& b);
inline vec3i min(const vec3i& a, const vec3i& b);
inline vec3i clamp(const vec3i& x, int min, int max);

inline int max(const vec3i& a);
inline int min(const vec3i& a);
inline int sum(const vec3i& a);

// Functions applied to vector elements
inline vec3i abs(const vec3i& a);
inline void  swap(vec3i& a, vec3i& b);

// Vector sequence operations.
inline int        size(const vec4i& a);
inline const int* begin(const vec4i& a);
inline const int* end(const vec4i& a);
inline int*       begin(vec4i& a);
inline int*       end(vec4i& a);
inline const int* data(const vec4i& a);
inline int*       data(vec4i& a);

// Vector comparison operations.
inline bool operator==(const vec4i& a, const vec4i& b);
inline bool operator!=(const vec4i& a, const vec4i& b);

// Vector operations.
inline vec4i operator+(const vec4i& a);
inline vec4i operator-(const vec4i& a);
inline vec4i operator+(const vec4i& a, const vec4i& b);
inline vec4i operator+(const vec4i& a, int b);
inline vec4i operator+(int a, const vec4i& b);
inline vec4i operator-(const vec4i& a, const vec4i& b);
inline vec4i operator-(const vec4i& a, int b);
inline vec4i operator-(int a, const vec4i& b);
inline vec4i operator*(const vec4i& a, const vec4i& b);
inline vec4i operator*(const vec4i& a, int b);
inline vec4i operator*(int a, const vec4i& b);
inline vec4i operator/(const vec4i& a, const vec4i& b);
inline vec4i operator/(const vec4i& a, int b);
inline vec4i operator/(int a, const vec4i& b);

// Vector assignments
inline vec4i& operator+=(vec4i& a, const vec4i& b);
inline vec4i& operator+=(vec4i& a, int b);
inline vec4i& operator-=(vec4i& a, const vec4i& b);
inline vec4i& operator-=(vec4i& a, int b);
inline vec4i& operator*=(vec4i& a, const vec4i& b);
inline vec4i& operator*=(vec4i& a, int b);
inline vec4i& operator/=(vec4i& a, const vec4i& b);
inline vec4i& operator/=(vec4i& a, int b);

// Max element and clamp.
inline vec4i max(const vec4i& a, int b);
inline vec4i min(const vec4i& a, int b);
inline vec4i max(const vec4i& a, const vec4i& b);
inline vec4i min(const vec4i& a, const vec4i& b);
inline vec4i clamp(const vec4i& x, int min, int max);

inline int max(const vec4i& a);
inline int min(const vec4i& a);
inline int sum(const vec4i& a);

// Functions applied to vector elements
inline vec4i abs(const vec4i& a);
inline void  swap(vec4i& a, vec4i& b);

// Vector comparison operations.
inline bool operator==(const vec4b& a, const vec4b& b);
inline bool operator!=(const vec4b& a, const vec4b& b);

}  // namespace yocto

// -----------------------------------------------------------------------------
// MATRICES
// -----------------------------------------------------------------------------
namespace yocto {

// Small Fixed-size matrices stored in column major format.
struct mat2f {
  vec2f x = {1, 0};
  vec2f y = {0, 1};

  inline vec2f&       operator[](int i);
  inline const vec2f& operator[](int i) const;
};

// Small Fixed-size matrices stored in column major format.
struct mat3f {
  vec3f x = {1, 0, 0};
  vec3f y = {0, 1, 0};
  vec3f z = {0, 0, 1};

  inline vec3f&       operator[](int i);
  inline const vec3f& operator[](int i) const;
};

// Small Fixed-size matrices stored in column major format.
struct mat4f {
  vec4f x = {1, 0, 0, 0};
  vec4f y = {0, 1, 0, 0};
  vec4f z = {0, 0, 1, 0};
  vec4f w = {0, 0, 0, 1};

  inline vec4f&       operator[](int i);
  inline const vec4f& operator[](int i) const;
};

// Identity matrices constants.
constexpr auto identity2x2f = mat2f{{1, 0}, {0, 1}};
constexpr auto identity3x3f = mat3f{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
constexpr auto identity4x4f = mat4f{
    {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

// Matrix comparisons.
inline bool operator==(const mat2f& a, const mat2f& b);
inline bool operator!=(const mat2f& a, const mat2f& b);

// Matrix operations.
inline mat2f operator+(const mat2f& a, const mat2f& b);
inline mat2f operator*(const mat2f& a, float b);
inline vec2f operator*(const mat2f& a, const vec2f& b);
inline vec2f operator*(const vec2f& a, const mat2f& b);
inline mat2f operator*(const mat2f& a, const mat2f& b);

// Matrix assignments.
inline mat2f& operator+=(mat2f& a, const mat2f& b);
inline mat2f& operator*=(mat2f& a, const mat2f& b);
inline mat2f& operator*=(mat2f& a, float b);

// Matrix diagonals and transposes.
inline vec2f diagonal(const mat2f& a);
inline mat2f transpose(const mat2f& a);

// Matrix adjoints, determinants and inverses.
inline float determinant(const mat2f& a);
inline mat2f adjoint(const mat2f& a);
inline mat2f inverse(const mat2f& a);

// Matrix comparisons.
inline bool operator==(const mat3f& a, const mat3f& b);
inline bool operator!=(const mat3f& a, const mat3f& b);

// Matrix operations.
inline mat3f operator+(const mat3f& a, const mat3f& b);
inline mat3f operator*(const mat3f& a, float b);
inline vec3f operator*(const mat3f& a, const vec3f& b);
inline vec3f operator*(const vec3f& a, const mat3f& b);
inline mat3f operator*(const mat3f& a, const mat3f& b);

// Matrix assignments.
inline mat3f& operator+=(mat3f& a, const mat3f& b);
inline mat3f& operator*=(mat3f& a, const mat3f& b);
inline mat3f& operator*=(mat3f& a, float b);

// Matrix diagonals and transposes.
inline vec3f diagonal(const mat3f& a);
inline mat3f transpose(const mat3f& a);

// Matrix adjoints, determinants and inverses.
inline float determinant(const mat3f& a);
inline mat3f adjoint(const mat3f& a);
inline mat3f inverse(const mat3f& a);

// Constructs a basis from a direction
inline mat3f basis_fromz(const vec3f& v);

// Matrix comparisons.
inline bool operator==(const mat4f& a, const mat4f& b);
inline bool operator!=(const mat4f& a, const mat4f& b);

// Matrix operations.
inline mat4f operator+(const mat4f& a, const mat4f& b);
inline mat4f operator*(const mat4f& a, float b);
inline vec4f operator*(const mat4f& a, const vec4f& b);
inline vec4f operator*(const vec4f& a, const mat4f& b);
inline mat4f operator*(const mat4f& a, const mat4f& b);

// Matrix assignments.
inline mat4f& operator+=(mat4f& a, const mat4f& b);
inline mat4f& operator*=(mat4f& a, const mat4f& b);
inline mat4f& operator*=(mat4f& a, float b);

// Matrix diagonals and transposes.
inline vec4f diagonal(const mat4f& a);
inline mat4f transpose(const mat4f& a);

}  // namespace yocto

// -----------------------------------------------------------------------------
// RIGID BODY TRANSFORMS/FRAMES
// -----------------------------------------------------------------------------
namespace yocto {

// Rigid frames stored as a column-major affine transform matrix.
struct frame2f {
  vec2f x = {1, 0};
  vec2f y = {0, 1};
  vec2f o = {0, 0};

  inline vec2f&       operator[](int i);
  inline const vec2f& operator[](int i) const;
};

// Rigid frames stored as a column-major affine transform matrix.
struct frame3f {
  vec3f x = {1, 0, 0};
  vec3f y = {0, 1, 0};
  vec3f z = {0, 0, 1};
  vec3f o = {0, 0, 0};

  inline vec3f&       operator[](int i);
  inline const vec3f& operator[](int i) const;
};

// Identity frames.
constexpr auto identity2x3f = frame2f{{1, 0}, {0, 1}, {0, 0}};
constexpr auto identity3x4f = frame3f{
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};

// Frame properties
inline mat2f rotation(const frame2f& a);
inline vec2f translation(const frame2f& a);

// Frame construction
inline frame2f make_frame(const mat2f& m, const vec2f& t);

// Conversion between frame and mat
inline mat3f   frame_to_mat(const frame2f& f);
inline frame2f mat_to_frame(const mat3f& ma);

// Frame comparisons.
inline bool operator==(const frame2f& a, const frame2f& b);
inline bool operator!=(const frame2f& a, const frame2f& b);

// Frame composition, equivalent to affine matrix product.
inline frame2f  operator*(const frame2f& a, const frame2f& b);
inline frame2f& operator*=(frame2f& a, const frame2f& b);

// Frame inverse, equivalent to rigid affine inverse.
inline frame2f inverse(const frame2f& a, bool non_rigid = false);

// Frame properties
inline mat3f rotation(const frame3f& a);
inline vec3f translation(const frame3f& a);

// Frame construction
inline frame3f make_frame(const mat3f& m, const vec3f& t);

// Conversion between frame and mat
inline mat4f   frame_to_mat(const frame3f& f);
inline frame3f mat_to_frame(const mat4f& m);

// Frame comparisons.
inline bool operator==(const frame3f& a, const frame3f& b);
inline bool operator!=(const frame3f& a, const frame3f& b);

// Frame composition, equivalent to affine matrix product.
inline frame3f  operator*(const frame3f& a, const frame3f& b);
inline frame3f& operator*=(frame3f& a, const frame3f& b);

// Frame inverse, equivalent to rigid affine inverse.
inline frame3f inverse(const frame3f& a, bool non_rigid = false);

// Frame construction from axis.
inline frame3f frame_fromz(const vec3f& o, const vec3f& v);
inline frame3f frame_fromzx(const vec3f& o, const vec3f& z_, const vec3f& x_);

}  // namespace yocto

// -----------------------------------------------------------------------------
// QUATERNIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Quaternions to represent rotations
struct quat4f {
  float x = 0;
  float y = 0;
  float z = 0;
  float w = 1;
};

// Constants
constexpr auto identity_quat4f = quat4f{0, 0, 0, 1};

// Quaternion operatons
inline quat4f operator+(const quat4f& a, const quat4f& b);
inline quat4f operator*(const quat4f& a, float b);
inline quat4f operator/(const quat4f& a, float b);
inline quat4f operator*(const quat4f& a, const quat4f& b);

// Quaterion operations
inline float  dot(const quat4f& a, const quat4f& b);
inline float  length(const quat4f& a);
inline quat4f normalize(const quat4f& a);
inline quat4f conjugate(const quat4f& a);
inline quat4f inverse(const quat4f& a);
inline float  uangle(const quat4f& a, const quat4f& b);
inline quat4f lerp(const quat4f& a, const quat4f& b, float t);
inline quat4f nlerp(const quat4f& a, const quat4f& b, float t);
inline quat4f slerp(const quat4f& a, const quat4f& b, float t);

}  // namespace yocto

// -----------------------------------------------------------------------------
// TRANSFORMS
// -----------------------------------------------------------------------------
namespace yocto {

// Transforms points, vectors and directions by matrices.
inline vec2f transform_point(const mat3f& a, const vec2f& b);
inline vec2f transform_vector(const mat3f& a, const vec2f& b);
inline vec2f transform_direction(const mat3f& a, const vec2f& b);
inline vec2f transform_normal(const mat3f& a, const vec2f& b);
inline vec2f transform_vector(const mat2f& a, const vec2f& b);
inline vec2f transform_direction(const mat2f& a, const vec2f& b);
inline vec2f transform_normal(const mat2f& a, const vec2f& b);

inline vec3f transform_point(const mat4f& a, const vec3f& b);
inline vec3f transform_vector(const mat4f& a, const vec3f& b);
inline vec3f transform_direction(const mat4f& a, const vec3f& b);
inline vec3f transform_vector(const mat3f& a, const vec3f& b);
inline vec3f transform_direction(const mat3f& a, const vec3f& b);
inline vec3f transform_normal(const mat3f& a, const vec3f& b);

// Transforms points, vectors and directions by frames.
inline vec2f transform_point(const frame2f& a, const vec2f& b);
inline vec2f transform_vector(const frame2f& a, const vec2f& b);
inline vec2f transform_direction(const frame2f& a, const vec2f& b);
inline vec2f transform_normal(
    const frame2f& a, const vec2f& b, bool non_rigid = false);

// Transforms points, vectors and directions by frames.
inline vec3f transform_point(const frame3f& a, const vec3f& b);
inline vec3f transform_vector(const frame3f& a, const vec3f& b);
inline vec3f transform_direction(const frame3f& a, const vec3f& b);
inline vec3f transform_normal(
    const frame3f& a, const vec3f& b, bool non_rigid = false);

// Transforms points, vectors and directions by frames.
inline vec2f transform_point_inverse(const frame2f& a, const vec2f& b);
inline vec2f transform_vector_inverse(const frame2f& a, const vec2f& b);
inline vec2f transform_direction_inverse(const frame2f& a, const vec2f& b);

// Transforms points, vectors and directions by frames.
inline vec3f transform_point_inverse(const frame3f& a, const vec3f& b);
inline vec3f transform_vector_inverse(const frame3f& a, const vec3f& b);
inline vec3f transform_direction_inverse(const frame3f& a, const vec3f& b);

// Translation, scaling and rotations transforms.
inline frame3f translation_frame(const vec3f& a);
inline frame3f scaling_frame(const vec3f& a);
inline frame3f rotation_frame(const vec3f& axis, float angle);
inline frame3f rotation_frame(const vec4f& quat);
inline frame3f rotation_frame(const quat4f& quat);
inline frame3f rotation_frame(const mat3f& rot);

// Lookat frame. Z-axis can be inverted with inv_xz.
inline frame3f lookat_frame(const vec3f& eye, const vec3f& center,
    const vec3f& up, bool inv_xz = false);

// OpenGL frustum, ortho and perspecgive matrices.
inline mat4f frustum_mat(float l, float r, float b, float t, float n, float f);
inline mat4f ortho_mat(float l, float r, float b, float t, float n, float f);
inline mat4f ortho2d_mat(float left, float right, float bottom, float top);
inline mat4f ortho_mat(float xmag, float ymag, float near, float far);
inline mat4f perspective_mat(float fovy, float aspect, float near, float far);
inline mat4f perspective_mat(float fovy, float aspect, float near);

// Rotation conversions.
inline pair<vec3f, float> rotation_axisangle(const vec4f& quat);
inline vec4f              rotation_quat(const vec3f& axis, float angle);
inline vec4f              rotation_quat(const vec4f& axisangle);

}  // namespace yocto

// -----------------------------------------------------------------------------
// USER INTERFACE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Computes the image uv coordinates corresponding to the view parameters.
// Returns negative coordinates if out of the image.
inline vec2i image_coords(const vec2f& mouse_pos, const vec2f& center,
    float scale, const vec2i& txt_size);

// Center image and autofit. Returns center and scale.
inline pair<vec2f, float> camera_imview(const vec2f& center, float scale,
    const vec2i& imsize, const vec2i& winsize, bool zoom_to_fit);

// Turntable for UI navigation. Returns from and to.
inline pair<vec3f, vec3f> camera_turntable(const vec3f& from, const vec3f& to,
    const vec3f& up, const vec2f& rotate, float dolly, const vec2f& pan);

// Turntable for UI navigation. Returns frame and focus.
inline pair<frame3f, float> camera_turntable(const frame3f& frame, float focus,
    const vec2f& rotate, float dolly, const vec2f& pan);

// FPS camera for UI navigation for a frame parametrization. Returns frame.
inline frame3f camera_fpscam(
    const frame3f& frame, const vec3f& transl, const vec2f& rotate);

// Computes the image uv coordinates corresponding to the view parameters.
// Returns negative coordinates if out of the image.
[[deprecated]] inline vec2i get_image_coords(const vec2f& mouse_pos,
    const vec2f& center, float scale, const vec2i& txt_size);

// Center image and autofit.
[[deprecated]] inline void update_imview(vec2f& center, float& scale,
    const vec2i& imsize, const vec2i& winsize, bool zoom_to_fit);

// Turntable for UI navigation.
[[deprecated]] inline void update_turntable(vec3f& from, vec3f& to,
    const vec3f& up, const vec2f& rotate, float dolly, const vec2f& pan);

// Turntable for UI navigation.
[[deprecated]] inline void update_turntable(frame3f& frame, float& focus,
    const vec2f& rotate, float dolly, const vec2f& pan);

// FPS camera for UI navigation for a frame parametrization.
[[deprecated]] inline void update_fpscam(
    frame3f& frame, const vec3f& transl, const vec2f& rotate);

}  // namespace yocto

// -----------------------------------------------------------------------------
// PYTHON-LIKE ITERATORS
// -----------------------------------------------------------------------------
namespace yocto {

// Python range. Construct an object that iterates over an integer sequence.
template <typename T>
constexpr auto range(T max);
template <typename T>
constexpr auto range(T min, T max);
template <typename T>
constexpr auto range(T min, T max, T step);

// Python enumerate
template <typename Sequence, typename T = size_t>
constexpr auto enumerate(const Sequence& sequence, T start = 0);
template <typename Sequence, typename T = size_t>
constexpr auto enumerate(Sequence& sequence, T start = 0);

// Python zip
template <typename Sequence1, typename Sequence2>
constexpr auto zip(const Sequence1& sequence1, const Sequence2& sequence2);
template <typename Sequence1, typename Sequence2>
constexpr auto zip(Sequence1& sequence1, Sequence2& sequence2);
template <typename Sequence1, typename Sequence2>
constexpr auto zip(const Sequence1& sequence1, Sequence2& sequence2);
template <typename Sequence1, typename Sequence2>
constexpr auto zip(Sequence1& sequence1, const Sequence2& sequence2);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SIGNED-SIZE
// -----------------------------------------------------------------------------
namespace yocto {

template <typename T>
inline std::ptrdiff_t ssize(const T& container);

}

// -----------------------------------------------------------------------------
//
//
// IMPLEMENTATION
//
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// MATH CONSTANTS AND FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

inline float abs(float a) { return a < 0 ? -a : a; }
inline float min(float a, float b) { return (a < b) ? a : b; }
inline float max(float a, float b) { return (a > b) ? a : b; }
inline float clamp(float a, float min_, float max_) {
  return min(max(a, min_), max_);
}
inline float sign(float a) { return a < 0 ? -1.0f : 1.0f; }
inline float sqr(float a) { return a * a; }
inline float sqrt(float a) { return std::sqrt(a); }
inline float sin(float a) { return std::sin(a); }
inline float cos(float a) { return std::cos(a); }
inline float tan(float a) { return std::tan(a); }
inline float asin(float a) { return std::asin(a); }
inline float acos(float a) { return std::acos(a); }
inline float atan(float a) { return std::atan(a); }
inline float log(float a) { return std::log(a); }
inline float exp(float a) { return std::exp(a); }
inline float log2(float a) { return std::log2(a); }
inline float exp2(float a) { return std::exp2(a); }
inline float pow(float a, float b) { return std::pow(a, b); }
#ifndef __CUDACC__
inline bool isfinite(float a) { return std::isfinite(a); }
#else
inline bool isfinite(float a) { return ::isfinite(a); }
#endif
inline float atan2(float a, float b) { return std::atan2(a, b); }
inline float fmod(float a, float b) { return std::fmod(a, b); }
inline void  swap(float& a, float& b) { std::swap(a, b); }
inline float radians(float a) { return a * pif / 180; }
inline float degrees(float a) { return a * 180 / pif; }
inline float lerp(float a, float b, float u) { return a * (1 - u) + b * u; }
inline float step(float a, float u) { return u < a ? 0.0f : 1.0f; }
inline float smoothstep(float a, float b, float u) {
  auto t = clamp((u - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}
inline float bias(float a, float bias) {
  return a / ((1 / bias - 2) * (1 - a) + 1);
}
inline float gain(float a, float gain) {
  return (a < 0.5f) ? bias(a * 2, gain) / 2
                    : bias(a * 2 - 1, 1 - gain) / 2 + 0.5f;
}

inline int  abs(int a) { return a < 0 ? -a : a; }
inline int  min(int a, int b) { return (a < b) ? a : b; }
inline int  max(int a, int b) { return (a > b) ? a : b; }
inline int  clamp(int a, int min_, int max_) { return min(max(a, min_), max_); }
inline int  sign(int a) { return a < 0 ? -1 : 1; }
inline int  pow2(int a) { return 1 << a; }
inline void swap(int& a, int& b) { std::swap(a, b); }

inline size_t min(size_t a, size_t b) { return (a < b) ? a : b; }
inline size_t max(size_t a, size_t b) { return (a > b) ? a : b; }

}  // namespace yocto

// -----------------------------------------------------------------------------
// VECTORS
// -----------------------------------------------------------------------------
namespace yocto {

// Vec2
inline float& vec2f::operator[](int i) { return (&x)[i]; }
inline const float& vec2f::operator[](int i) const { return (&x)[i]; }

// Vec3
inline float& vec3f::operator[](int i) { return (&x)[i]; }
inline const float& vec3f::operator[](int i) const { return (&x)[i]; }

// Vec4
inline float& vec4f::operator[](int i) { return (&x)[i]; }
inline const float& vec4f::operator[](int i) const { return (&x)[i]; }

// Element access
inline vec3f xyz(const vec4f& a) { return {a.x, a.y, a.z}; }

// Vector sequence operations.
inline int          size(const vec2f& a) { return 2; }
inline const float* begin(const vec2f& a) { return &a.x; }
inline const float* end(const vec2f& a) { return &a.x + 2; }
inline float*       begin(vec2f& a) { return &a.x; }
inline float*       end(vec2f& a) { return &a.x + 2; }
inline const float* data(const vec2f& a) { return &a.x; }
inline float*       data(vec2f& a) { return &a.x; }

// Vector comparison operations.
inline bool operator==(const vec2f& a, const vec2f& b) {
  return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const vec2f& a, const vec2f& b) {
  return a.x != b.x || a.y != b.y;
}

// Vector operations.
inline vec2f operator+(const vec2f& a) { return a; }
inline vec2f operator-(const vec2f& a) { return {-a.x, -a.y}; }
inline vec2f operator+(const vec2f& a, const vec2f& b) {
  return {a.x + b.x, a.y + b.y};
}
inline vec2f operator+(const vec2f& a, float b) { return {a.x + b, a.y + b}; }
inline vec2f operator+(float a, const vec2f& b) { return {a + b.x, a + b.y}; }
inline vec2f operator-(const vec2f& a, const vec2f& b) {
  return {a.x - b.x, a.y - b.y};
}
inline vec2f operator-(const vec2f& a, float b) { return {a.x - b, a.y - b}; }
inline vec2f operator-(float a, const vec2f& b) { return {a - b.x, a - b.y}; }
inline vec2f operator*(const vec2f& a, const vec2f& b) {
  return {a.x * b.x, a.y * b.y};
}
inline vec2f operator*(const vec2f& a, float b) { return {a.x * b, a.y * b}; }
inline vec2f operator*(float a, const vec2f& b) { return {a * b.x, a * b.y}; }
inline vec2f operator/(const vec2f& a, const vec2f& b) {
  return {a.x / b.x, a.y / b.y};
}
inline vec2f operator/(const vec2f& a, float b) { return {a.x / b, a.y / b}; }
inline vec2f operator/(float a, const vec2f& b) { return {a / b.x, a / b.y}; }

// Vector assignments
inline vec2f& operator+=(vec2f& a, const vec2f& b) { return a = a + b; }
inline vec2f& operator+=(vec2f& a, float b) { return a = a + b; }
inline vec2f& operator-=(vec2f& a, const vec2f& b) { return a = a - b; }
inline vec2f& operator-=(vec2f& a, float b) { return a = a - b; }
inline vec2f& operator*=(vec2f& a, const vec2f& b) { return a = a * b; }
inline vec2f& operator*=(vec2f& a, float b) { return a = a * b; }
inline vec2f& operator/=(vec2f& a, const vec2f& b) { return a = a / b; }
inline vec2f& operator/=(vec2f& a, float b) { return a = a / b; }

// Vector products and lengths.
inline float dot(const vec2f& a, const vec2f& b) {
  return a.x * b.x + a.y * b.y;
}
inline float cross(const vec2f& a, const vec2f& b) {
  return a.x * b.y - a.y * b.x;
}

inline float length(const vec2f& a) { return sqrt(dot(a, a)); }
inline float length_squared(const vec2f& a) { return dot(a, a); }
inline vec2f normalize(const vec2f& a) {
  auto l = length(a);
  return (l != 0) ? a / l : a;
}
inline float distance(const vec2f& a, const vec2f& b) { return length(a - b); }
inline float distance_squared(const vec2f& a, const vec2f& b) {
  return dot(a - b, a - b);
}
inline float angle(const vec2f& a, const vec2f& b) {
  return acos(clamp(dot(normalize(a), normalize(b)), (float)-1, (float)1));
}

// Max element and clamp.
inline vec2f max(const vec2f& a, float b) { return {max(a.x, b), max(a.y, b)}; }
inline vec2f min(const vec2f& a, float b) { return {min(a.x, b), min(a.y, b)}; }
inline vec2f max(const vec2f& a, const vec2f& b) {
  return {max(a.x, b.x), max(a.y, b.y)};
}
inline vec2f min(const vec2f& a, const vec2f& b) {
  return {min(a.x, b.x), min(a.y, b.y)};
}
inline vec2f clamp(const vec2f& x, float min, float max) {
  return {clamp(x.x, min, max), clamp(x.y, min, max)};
}
inline vec2f lerp(const vec2f& a, const vec2f& b, float u) {
  return a * (1 - u) + b * u;
}
inline vec2f lerp(const vec2f& a, const vec2f& b, const vec2f& u) {
  return a * (1 - u) + b * u;
}

inline float max(const vec2f& a) { return max(a.x, a.y); }
inline float min(const vec2f& a) { return min(a.x, a.y); }
inline float sum(const vec2f& a) { return a.x + a.y; }
inline float mean(const vec2f& a) { return sum(a) / 2; }

// Functions applied to vector elements
inline vec2f abs(const vec2f& a) { return {abs(a.x), abs(a.y)}; }
inline vec2f sqr(const vec2f& a) { return {sqr(a.x), sqr(a.y)}; }
inline vec2f sqrt(const vec2f& a) { return {sqrt(a.x), sqrt(a.y)}; }
inline vec2f exp(const vec2f& a) { return {exp(a.x), exp(a.y)}; }
inline vec2f log(const vec2f& a) { return {log(a.x), log(a.y)}; }
inline vec2f exp2(const vec2f& a) { return {exp2(a.x), exp2(a.y)}; }
inline vec2f log2(const vec2f& a) { return {log2(a.x), log2(a.y)}; }
inline bool  isfinite(const vec2f& a) { return isfinite(a.x) && isfinite(a.y); }
inline vec2f pow(const vec2f& a, float b) { return {pow(a.x, b), pow(a.y, b)}; }
inline vec2f pow(const vec2f& a, const vec2f& b) {
  return {pow(a.x, b.x), pow(a.y, b.y)};
}
inline vec2f gain(const vec2f& a, float b) {
  return {gain(a.x, b), gain(a.y, b)};
}
inline void swap(vec2f& a, vec2f& b) { std::swap(a, b); }

// Vector sequence operations.
inline int          size(const vec3f& a) { return 3; }
inline const float* begin(const vec3f& a) { return &a.x; }
inline const float* end(const vec3f& a) { return &a.x + 3; }
inline float*       begin(vec3f& a) { return &a.x; }
inline float*       end(vec3f& a) { return &a.x + 3; }
inline const float* data(const vec3f& a) { return &a.x; }
inline float*       data(vec3f& a) { return &a.x; }

// Vector comparison operations.
inline bool operator==(const vec3f& a, const vec3f& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
inline bool operator!=(const vec3f& a, const vec3f& b) {
  return a.x != b.x || a.y != b.y || a.z != b.z;
}

// Vector operations.
inline vec3f operator+(const vec3f& a) { return a; }
inline vec3f operator-(const vec3f& a) { return {-a.x, -a.y, -a.z}; }
inline vec3f operator+(const vec3f& a, const vec3f& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline vec3f operator+(const vec3f& a, float b) {
  return {a.x + b, a.y + b, a.z + b};
}
inline vec3f operator+(float a, const vec3f& b) {
  return {a + b.x, a + b.y, a + b.z};
}
inline vec3f operator-(const vec3f& a, const vec3f& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline vec3f operator-(const vec3f& a, float b) {
  return {a.x - b, a.y - b, a.z - b};
}
inline vec3f operator-(float a, const vec3f& b) {
  return {a - b.x, a - b.y, a - b.z};
}
inline vec3f operator*(const vec3f& a, const vec3f& b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}
inline vec3f operator*(const vec3f& a, float b) {
  return {a.x * b, a.y * b, a.z * b};
}
inline vec3f operator*(float a, const vec3f& b) {
  return {a * b.x, a * b.y, a * b.z};
}
inline vec3f operator/(const vec3f& a, const vec3f& b) {
  return {a.x / b.x, a.y / b.y, a.z / b.z};
}
inline vec3f operator/(const vec3f& a, float b) {
  return {a.x / b, a.y / b, a.z / b};
}
inline vec3f operator/(float a, const vec3f& b) {
  return {a / b.x, a / b.y, a / b.z};
}

// Vector assignments
inline vec3f& operator+=(vec3f& a, const vec3f& b) { return a = a + b; }
inline vec3f& operator+=(vec3f& a, float b) { return a = a + b; }
inline vec3f& operator-=(vec3f& a, const vec3f& b) { return a = a - b; }
inline vec3f& operator-=(vec3f& a, float b) { return a = a - b; }
inline vec3f& operator*=(vec3f& a, const vec3f& b) { return a = a * b; }
inline vec3f& operator*=(vec3f& a, float b) { return a = a * b; }
inline vec3f& operator/=(vec3f& a, const vec3f& b) { return a = a / b; }
inline vec3f& operator/=(vec3f& a, float b) { return a = a / b; }

// Vector products and lengths.
inline float dot(const vec3f& a, const vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline vec3f cross(const vec3f& a, const vec3f& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(const vec3f& a) { return sqrt(dot(a, a)); }
inline float length_squared(const vec3f& a) { return dot(a, a); }
inline vec3f normalize(const vec3f& a) {
  auto l = length(a);
  return (l != 0) ? a / l : a;
}
inline float distance(const vec3f& a, const vec3f& b) { return length(a - b); }
inline float distance_squared(const vec3f& a, const vec3f& b) {
  return dot(a - b, a - b);
}
inline float angle(const vec3f& a, const vec3f& b) {
  return acos(clamp(dot(normalize(a), normalize(b)), (float)-1, (float)1));
}

// Orthogonal vectors.
inline vec3f orthogonal(const vec3f& v) {
  // http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts)
  return abs(v.x) > abs(v.z) ? vec3f{-v.y, v.x, 0} : vec3f{0, -v.z, v.y};
}
inline vec3f orthonormalize(const vec3f& a, const vec3f& b) {
  return normalize(a - b * dot(a, b));
}

// Reflected and refracted vector.
inline vec3f reflect(const vec3f& w, const vec3f& n) {
  return -w + 2 * dot(n, w) * n;
}
inline vec3f refract(const vec3f& w, const vec3f& n, float inv_eta) {
  auto cosine = dot(n, w);
  auto k      = 1 + inv_eta * inv_eta * (cosine * cosine - 1);
  if (k < 0) return {0, 0, 0};  // tir
  return -w * inv_eta + (inv_eta * cosine - sqrt(k)) * n;
}

// Max element and clamp.
inline vec3f max(const vec3f& a, float b) {
  return {max(a.x, b), max(a.y, b), max(a.z, b)};
}
inline vec3f min(const vec3f& a, float b) {
  return {min(a.x, b), min(a.y, b), min(a.z, b)};
}
inline vec3f max(const vec3f& a, const vec3f& b) {
  return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}
inline vec3f min(const vec3f& a, const vec3f& b) {
  return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}
inline vec3f clamp(const vec3f& x, float min, float max) {
  return {clamp(x.x, min, max), clamp(x.y, min, max), clamp(x.z, min, max)};
}
inline vec3f lerp(const vec3f& a, const vec3f& b, float u) {
  return a * (1 - u) + b * u;
}
inline vec3f lerp(const vec3f& a, const vec3f& b, const vec3f& u) {
  return a * (1 - u) + b * u;
}

inline float max(const vec3f& a) { return max(max(a.x, a.y), a.z); }
inline float min(const vec3f& a) { return min(min(a.x, a.y), a.z); }
inline float sum(const vec3f& a) { return a.x + a.y + a.z; }
inline float mean(const vec3f& a) { return sum(a) / 3; }

// Functions applied to vector elements
inline vec3f abs(const vec3f& a) { return {abs(a.x), abs(a.y), abs(a.z)}; }
inline vec3f sqr(const vec3f& a) { return {sqr(a.x), sqr(a.y), sqr(a.z)}; }
inline vec3f sqrt(const vec3f& a) { return {sqrt(a.x), sqrt(a.y), sqrt(a.z)}; }
inline vec3f exp(const vec3f& a) { return {exp(a.x), exp(a.y), exp(a.z)}; }
inline vec3f log(const vec3f& a) { return {log(a.x), log(a.y), log(a.z)}; }
inline vec3f exp2(const vec3f& a) { return {exp2(a.x), exp2(a.y), exp2(a.z)}; }
inline vec3f log2(const vec3f& a) { return {log2(a.x), log2(a.y), log2(a.z)}; }
inline vec3f pow(const vec3f& a, float b) {
  return {pow(a.x, b), pow(a.y, b), pow(a.z, b)};
}
inline vec3f pow(const vec3f& a, const vec3f& b) {
  return {pow(a.x, b.x), pow(a.y, b.y), pow(a.z, b.z)};
}
inline vec3f gain(const vec3f& a, float b) {
  return {gain(a.x, b), gain(a.y, b), gain(a.z, b)};
}
inline bool isfinite(const vec3f& a) {
  return isfinite(a.x) && isfinite(a.y) && isfinite(a.z);
}
inline void swap(vec3f& a, vec3f& b) { std::swap(a, b); }

// Vector sequence operations.
inline int          size(const vec4f& a) { return 4; }
inline const float* begin(const vec4f& a) { return &a.x; }
inline const float* end(const vec4f& a) { return &a.x + 4; }
inline float*       begin(vec4f& a) { return &a.x; }
inline float*       end(vec4f& a) { return &a.x + 4; }
inline const float* data(const vec4f& a) { return &a.x; }
inline float*       data(vec4f& a) { return &a.x; }

// Vector comparison operations.
inline bool operator==(const vec4f& a, const vec4f& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
inline bool operator!=(const vec4f& a, const vec4f& b) {
  return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}

// Vector operations.
inline vec4f operator+(const vec4f& a) { return a; }
inline vec4f operator-(const vec4f& a) { return {-a.x, -a.y, -a.z, -a.w}; }
inline vec4f operator+(const vec4f& a, const vec4f& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
inline vec4f operator+(const vec4f& a, float b) {
  return {a.x + b, a.y + b, a.z + b, a.w + b};
}
inline vec4f operator+(float a, const vec4f& b) {
  return {a + b.x, a + b.y, a + b.z, a + b.w};
}
inline vec4f operator-(const vec4f& a, const vec4f& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
inline vec4f operator-(const vec4f& a, float b) {
  return {a.x - b, a.y - b, a.z - b, a.w - b};
}
inline vec4f operator-(float a, const vec4f& b) {
  return {a - b.x, a - b.y, a - b.z, a - b.w};
}
inline vec4f operator*(const vec4f& a, const vec4f& b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}
inline vec4f operator*(const vec4f& a, float b) {
  return {a.x * b, a.y * b, a.z * b, a.w * b};
}
inline vec4f operator*(float a, const vec4f& b) {
  return {a * b.x, a * b.y, a * b.z, a * b.w};
}
inline vec4f operator/(const vec4f& a, const vec4f& b) {
  return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
}
inline vec4f operator/(const vec4f& a, float b) {
  return {a.x / b, a.y / b, a.z / b, a.w / b};
}
inline vec4f operator/(float a, const vec4f& b) {
  return {a / b.x, a / b.y, a / b.z, a / b.w};
}

// Vector assignments
inline vec4f& operator+=(vec4f& a, const vec4f& b) { return a = a + b; }
inline vec4f& operator+=(vec4f& a, float b) { return a = a + b; }
inline vec4f& operator-=(vec4f& a, const vec4f& b) { return a = a - b; }
inline vec4f& operator-=(vec4f& a, float b) { return a = a - b; }
inline vec4f& operator*=(vec4f& a, const vec4f& b) { return a = a * b; }
inline vec4f& operator*=(vec4f& a, float b) { return a = a * b; }
inline vec4f& operator/=(vec4f& a, const vec4f& b) { return a = a / b; }
inline vec4f& operator/=(vec4f& a, float b) { return a = a / b; }

// Vector products and lengths.
inline float dot(const vec4f& a, const vec4f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
inline float length(const vec4f& a) { return sqrt(dot(a, a)); }
inline float length_squared(const vec4f& a) { return dot(a, a); }
inline vec4f normalize(const vec4f& a) {
  auto l = length(a);
  return (l != 0) ? a / l : a;
}
inline float distance(const vec4f& a, const vec4f& b) { return length(a - b); }
inline float distance_squared(const vec4f& a, const vec4f& b) {
  return dot(a - b, a - b);
}
inline float angle(const vec4f& a, const vec4f& b) {
  return acos(clamp(dot(normalize(a), normalize(b)), (float)-1, (float)1));
}

inline vec4f slerp(const vec4f& a, const vec4f& b, float u) {
  // https://en.wikipedia.org/wiki/Slerp
  auto an = normalize(a), bn = normalize(b);
  auto d = dot(an, bn);
  if (d < 0) {
    bn = -bn;
    d  = -d;
  }
  if (d > (float)0.9995) return normalize(an + u * (bn - an));
  auto th = acos(clamp(d, (float)-1, (float)1));
  if (th == 0) return an;
  return an * (sin(th * (1 - u)) / sin(th)) + bn * (sin(th * u) / sin(th));
}

// Max element and clamp.
inline vec4f max(const vec4f& a, float b) {
  return {max(a.x, b), max(a.y, b), max(a.z, b), max(a.w, b)};
}
inline vec4f min(const vec4f& a, float b) {
  return {min(a.x, b), min(a.y, b), min(a.z, b), min(a.w, b)};
}
inline vec4f max(const vec4f& a, const vec4f& b) {
  return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w)};
}
inline vec4f min(const vec4f& a, const vec4f& b) {
  return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w)};
}
inline vec4f clamp(const vec4f& x, float min, float max) {
  return {clamp(x.x, min, max), clamp(x.y, min, max), clamp(x.z, min, max),
      clamp(x.w, min, max)};
}
inline vec4f lerp(const vec4f& a, const vec4f& b, float u) {
  return a * (1 - u) + b * u;
}
inline vec4f lerp(const vec4f& a, const vec4f& b, const vec4f& u) {
  return a * (1 - u) + b * u;
}

inline float max(const vec4f& a) { return max(max(max(a.x, a.y), a.z), a.w); }
inline float min(const vec4f& a) { return min(min(min(a.x, a.y), a.z), a.w); }
inline float sum(const vec4f& a) { return a.x + a.y + a.z + a.w; }
inline float mean(const vec4f& a) { return sum(a) / 4; }

// Functions applied to vector elements
inline vec4f abs(const vec4f& a) {
  return {abs(a.x), abs(a.y), abs(a.z), abs(a.w)};
}
inline vec4f sqr(const vec4f& a) {
  return {sqr(a.x), sqr(a.y), sqr(a.z), sqr(a.w)};
}
inline vec4f sqrt(const vec4f& a) {
  return {sqrt(a.x), sqrt(a.y), sqrt(a.z), sqrt(a.w)};
}
inline vec4f exp(const vec4f& a) {
  return {exp(a.x), exp(a.y), exp(a.z), exp(a.w)};
}
inline vec4f log(const vec4f& a) {
  return {log(a.x), log(a.y), log(a.z), log(a.w)};
}
inline vec4f exp2(const vec4f& a) {
  return {exp2(a.x), exp2(a.y), exp2(a.z), exp2(a.w)};
}
inline vec4f log2(const vec4f& a) {
  return {log2(a.x), log2(a.y), log2(a.z), log2(a.w)};
}
inline vec4f pow(const vec4f& a, float b) {
  return {pow(a.x, b), pow(a.y, b), pow(a.z, b), pow(a.w, b)};
}
inline vec4f pow(const vec4f& a, const vec4f& b) {
  return {pow(a.x, b.x), pow(a.y, b.y), pow(a.z, b.z), pow(a.w, b.w)};
}
inline vec4f gain(const vec4f& a, float b) {
  return {gain(a.x, b), gain(a.y, b), gain(a.z, b), gain(a.w, b)};
}
inline bool isfinite(const vec4f& a) {
  return isfinite(a.x) && isfinite(a.y) && isfinite(a.z) && isfinite(a.w);
}
inline void swap(vec4f& a, vec4f& b) { std::swap(a, b); }

// Quaternion operatons represented as xi + yj + zk + w
// const auto identity_quat4f = vec4f{0, 0, 0, 1};
inline vec4f quat_mul(const vec4f& a, float b) {
  return {a.x * b, a.y * b, a.z * b, a.w * b};
}
inline vec4f quat_mul(const vec4f& a, const vec4f& b) {
  return {a.x * b.w + a.w * b.x + a.y * b.w - a.z * b.y,
      a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
      a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline vec4f quat_conjugate(const vec4f& a) { return {-a.x, -a.y, -a.z, a.w}; }
inline vec4f quat_inverse(const vec4f& a) {
  return quat_conjugate(a) / dot(a, a);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// INTEGER VECTORS
// -----------------------------------------------------------------------------
namespace yocto {

// Vector data types
inline int& vec2i::operator[](int i) { return (&x)[i]; }
inline const int& vec2i::operator[](int i) const { return (&x)[i]; }

// Vector data types
inline int& vec3i::operator[](int i) { return (&x)[i]; }
inline const int& vec3i::operator[](int i) const { return (&x)[i]; }

// Vector data types
inline int& vec4i::operator[](int i) { return (&x)[i]; }
inline const int& vec4i::operator[](int i) const { return (&x)[i]; }

// Vector data types
inline byte& vec4b::operator[](int i) { return (&x)[i]; }
inline const byte& vec4b::operator[](int i) const { return (&x)[i]; }

// Element access
inline vec3i xyz(const vec4i& a) { return {a.x, a.y, a.z}; }

// Vector sequence operations.
inline int        size(const vec2i& a) { return 2; }
inline const int* begin(const vec2i& a) { return &a.x; }
inline const int* end(const vec2i& a) { return &a.x + 2; }
inline int*       begin(vec2i& a) { return &a.x; }
inline int*       end(vec2i& a) { return &a.x + 2; }
inline const int* data(const vec2i& a) { return &a.x; }
inline int*       data(vec2i& a) { return &a.x; }

// Vector comparison operations.
inline bool operator==(const vec2i& a, const vec2i& b) {
  return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const vec2i& a, const vec2i& b) {
  return a.x != b.x || a.y != b.y;
}

// Vector operations.
inline vec2i operator+(const vec2i& a) { return a; }
inline vec2i operator-(const vec2i& a) { return {-a.x, -a.y}; }
inline vec2i operator+(const vec2i& a, const vec2i& b) {
  return {a.x + b.x, a.y + b.y};
}
inline vec2i operator+(const vec2i& a, int b) { return {a.x + b, a.y + b}; }
inline vec2i operator+(int a, const vec2i& b) { return {a + b.x, a + b.y}; }
inline vec2i operator-(const vec2i& a, const vec2i& b) {
  return {a.x - b.x, a.y - b.y};
}
inline vec2i operator-(const vec2i& a, int b) { return {a.x - b, a.y - b}; }
inline vec2i operator-(int a, const vec2i& b) { return {a - b.x, a - b.y}; }
inline vec2i operator*(const vec2i& a, const vec2i& b) {
  return {a.x * b.x, a.y * b.y};
}
inline vec2i operator*(const vec2i& a, int b) { return {a.x * b, a.y * b}; }
inline vec2i operator*(int a, const vec2i& b) { return {a * b.x, a * b.y}; }
inline vec2i operator/(const vec2i& a, const vec2i& b) {
  return {a.x / b.x, a.y / b.y};
}
inline vec2i operator/(const vec2i& a, int b) { return {a.x / b, a.y / b}; }
inline vec2i operator/(int a, const vec2i& b) { return {a / b.x, a / b.y}; }

// Vector assignments
inline vec2i& operator+=(vec2i& a, const vec2i& b) { return a = a + b; }
inline vec2i& operator+=(vec2i& a, int b) { return a = a + b; }
inline vec2i& operator-=(vec2i& a, const vec2i& b) { return a = a - b; }
inline vec2i& operator-=(vec2i& a, int b) { return a = a - b; }
inline vec2i& operator*=(vec2i& a, const vec2i& b) { return a = a * b; }
inline vec2i& operator*=(vec2i& a, int b) { return a = a * b; }
inline vec2i& operator/=(vec2i& a, const vec2i& b) { return a = a / b; }
inline vec2i& operator/=(vec2i& a, int b) { return a = a / b; }

// Max element and clamp.
inline vec2i max(const vec2i& a, int b) { return {max(a.x, b), max(a.y, b)}; }
inline vec2i min(const vec2i& a, int b) { return {min(a.x, b), min(a.y, b)}; }
inline vec2i max(const vec2i& a, const vec2i& b) {
  return {max(a.x, b.x), max(a.y, b.y)};
}
inline vec2i min(const vec2i& a, const vec2i& b) {
  return {min(a.x, b.x), min(a.y, b.y)};
}
inline vec2i clamp(const vec2i& x, int min, int max) {
  return {clamp(x.x, min, max), clamp(x.y, min, max)};
}

inline int max(const vec2i& a) { return max(a.x, a.y); }
inline int min(const vec2i& a) { return min(a.x, a.y); }
inline int sum(const vec2i& a) { return a.x + a.y; }

// Functions applied to vector elements
inline vec2i abs(const vec2i& a) { return {abs(a.x), abs(a.y)}; }
inline void  swap(vec2i& a, vec2i& b) { std::swap(a, b); }

// Vector sequence operations.
inline int        size(const vec3i& a) { return 3; }
inline const int* begin(const vec3i& a) { return &a.x; }
inline const int* end(const vec3i& a) { return &a.x + 3; }
inline int*       begin(vec3i& a) { return &a.x; }
inline int*       end(vec3i& a) { return &a.x + 3; }
inline const int* data(const vec3i& a) { return &a.x; }
inline int*       data(vec3i& a) { return &a.x; }

// Vector comparison operations.
inline bool operator==(const vec3i& a, const vec3i& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
inline bool operator!=(const vec3i& a, const vec3i& b) {
  return a.x != b.x || a.y != b.y || a.z != b.z;
}

// Vector operations.
inline vec3i operator+(const vec3i& a) { return a; }
inline vec3i operator-(const vec3i& a) { return {-a.x, -a.y, -a.z}; }
inline vec3i operator+(const vec3i& a, const vec3i& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline vec3i operator+(const vec3i& a, int b) {
  return {a.x + b, a.y + b, a.z + b};
}
inline vec3i operator+(int a, const vec3i& b) {
  return {a + b.x, a + b.y, a + b.z};
}
inline vec3i operator-(const vec3i& a, const vec3i& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline vec3i operator-(const vec3i& a, int b) {
  return {a.x - b, a.y - b, a.z - b};
}
inline vec3i operator-(int a, const vec3i& b) {
  return {a - b.x, a - b.y, a - b.z};
}
inline vec3i operator*(const vec3i& a, const vec3i& b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}
inline vec3i operator*(const vec3i& a, int b) {
  return {a.x * b, a.y * b, a.z * b};
}
inline vec3i operator*(int a, const vec3i& b) {
  return {a * b.x, a * b.y, a * b.z};
}
inline vec3i operator/(const vec3i& a, const vec3i& b) {
  return {a.x / b.x, a.y / b.y, a.z / b.z};
}
inline vec3i operator/(const vec3i& a, int b) {
  return {a.x / b, a.y / b, a.z / b};
}
inline vec3i operator/(int a, const vec3i& b) {
  return {a / b.x, a / b.y, a / b.z};
}

// Vector assignments
inline vec3i& operator+=(vec3i& a, const vec3i& b) { return a = a + b; }
inline vec3i& operator+=(vec3i& a, int b) { return a = a + b; }
inline vec3i& operator-=(vec3i& a, const vec3i& b) { return a = a - b; }
inline vec3i& operator-=(vec3i& a, int b) { return a = a - b; }
inline vec3i& operator*=(vec3i& a, const vec3i& b) { return a = a * b; }
inline vec3i& operator*=(vec3i& a, int b) { return a = a * b; }
inline vec3i& operator/=(vec3i& a, const vec3i& b) { return a = a / b; }
inline vec3i& operator/=(vec3i& a, int b) { return a = a / b; }

// Max element and clamp.
inline vec3i max(const vec3i& a, int b) {
  return {max(a.x, b), max(a.y, b), max(a.z, b)};
}
inline vec3i min(const vec3i& a, int b) {
  return {min(a.x, b), min(a.y, b), min(a.z, b)};
}
inline vec3i max(const vec3i& a, const vec3i& b) {
  return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}
inline vec3i min(const vec3i& a, const vec3i& b) {
  return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}
inline vec3i clamp(const vec3i& x, int min, int max) {
  return {clamp(x.x, min, max), clamp(x.y, min, max), clamp(x.z, min, max)};
}

inline int max(const vec3i& a) { return max(max(a.x, a.y), a.z); }
inline int min(const vec3i& a) { return min(min(a.x, a.y), a.z); }
inline int sum(const vec3i& a) { return a.x + a.y + a.z; }

// Functions applied to vector elements
inline vec3i abs(const vec3i& a) { return {abs(a.x), abs(a.y), abs(a.z)}; }
inline void  swap(vec3i& a, vec3i& b) { std::swap(a, b); }

// Vector sequence operations.
inline int        size(const vec4i& a) { return 4; }
inline const int* begin(const vec4i& a) { return &a.x; }
inline const int* end(const vec4i& a) { return &a.x + 4; }
inline int*       begin(vec4i& a) { return &a.x; }
inline int*       end(vec4i& a) { return &a.x + 4; }
inline const int* data(const vec4i& a) { return &a.x; }
inline int*       data(vec4i& a) { return &a.x; }

// Vector comparison operations.
inline bool operator==(const vec4i& a, const vec4i& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
inline bool operator!=(const vec4i& a, const vec4i& b) {
  return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}

// Vector operations.
inline vec4i operator+(const vec4i& a) { return a; }
inline vec4i operator-(const vec4i& a) { return {-a.x, -a.y, -a.z, -a.w}; }
inline vec4i operator+(const vec4i& a, const vec4i& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
inline vec4i operator+(const vec4i& a, int b) {
  return {a.x + b, a.y + b, a.z + b, a.w + b};
}
inline vec4i operator+(int a, const vec4i& b) {
  return {a + b.x, a + b.y, a + b.z, a + b.w};
}
inline vec4i operator-(const vec4i& a, const vec4i& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
inline vec4i operator-(const vec4i& a, int b) {
  return {a.x - b, a.y - b, a.z - b, a.w - b};
}
inline vec4i operator-(int a, const vec4i& b) {
  return {a - b.x, a - b.y, a - b.z, a - b.w};
}
inline vec4i operator*(const vec4i& a, const vec4i& b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}
inline vec4i operator*(const vec4i& a, int b) {
  return {a.x * b, a.y * b, a.z * b, a.w * b};
}
inline vec4i operator*(int a, const vec4i& b) {
  return {a * b.x, a * b.y, a * b.z, a * b.w};
}
inline vec4i operator/(const vec4i& a, const vec4i& b) {
  return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
}
inline vec4i operator/(const vec4i& a, int b) {
  return {a.x / b, a.y / b, a.z / b, a.w / b};
}
inline vec4i operator/(int a, const vec4i& b) {
  return {a / b.x, a / b.y, a / b.z, a / b.w};
}

// Vector assignments
inline vec4i& operator+=(vec4i& a, const vec4i& b) { return a = a + b; }
inline vec4i& operator+=(vec4i& a, int b) { return a = a + b; }
inline vec4i& operator-=(vec4i& a, const vec4i& b) { return a = a - b; }
inline vec4i& operator-=(vec4i& a, int b) { return a = a - b; }
inline vec4i& operator*=(vec4i& a, const vec4i& b) { return a = a * b; }
inline vec4i& operator*=(vec4i& a, int b) { return a = a * b; }
inline vec4i& operator/=(vec4i& a, const vec4i& b) { return a = a / b; }
inline vec4i& operator/=(vec4i& a, int b) { return a = a / b; }

// Max element and clamp.
inline vec4i max(const vec4i& a, int b) {
  return {max(a.x, b), max(a.y, b), max(a.z, b), max(a.w, b)};
}
inline vec4i min(const vec4i& a, int b) {
  return {min(a.x, b), min(a.y, b), min(a.z, b), min(a.w, b)};
}
inline vec4i max(const vec4i& a, const vec4i& b) {
  return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w)};
}
inline vec4i min(const vec4i& a, const vec4i& b) {
  return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w)};
}
inline vec4i clamp(const vec4i& x, int min, int max) {
  return {clamp(x.x, min, max), clamp(x.y, min, max), clamp(x.z, min, max),
      clamp(x.w, min, max)};
}

inline int max(const vec4i& a) { return max(max(max(a.x, a.y), a.z), a.w); }
inline int min(const vec4i& a) { return min(min(min(a.x, a.y), a.z), a.w); }
inline int sum(const vec4i& a) { return a.x + a.y + a.z + a.w; }

// Functions applied to vector elements
inline vec4i abs(const vec4i& a) {
  return {abs(a.x), abs(a.y), abs(a.z), abs(a.w)};
}
inline void swap(vec4i& a, vec4i& b) { std::swap(a, b); }

// Vector comparison operations.
inline bool operator==(const vec4b& a, const vec4b& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
inline bool operator!=(const vec4b& a, const vec4b& b) {
  return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// MATRICES
// -----------------------------------------------------------------------------
namespace yocto {

// Small Fixed-size matrices stored in column major format.
inline vec2f& mat2f::operator[](int i) { return (&x)[i]; }
inline const vec2f& mat2f::operator[](int i) const { return (&x)[i]; }

// Small Fixed-size matrices stored in column major format.
inline vec3f& mat3f::operator[](int i) { return (&x)[i]; }
inline const vec3f& mat3f::operator[](int i) const { return (&x)[i]; }

// Small Fixed-size matrices stored in column major format.
inline vec4f& mat4f::operator[](int i) { return (&x)[i]; }
inline const vec4f& mat4f::operator[](int i) const { return (&x)[i]; }

// Matrix comparisons.
inline bool operator==(const mat2f& a, const mat2f& b) {
  return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const mat2f& a, const mat2f& b) { return !(a == b); }

// Matrix operations.
inline mat2f operator+(const mat2f& a, const mat2f& b) {
  return {a.x + b.x, a.y + b.y};
}
inline mat2f operator*(const mat2f& a, float b) { return {a.x * b, a.y * b}; }
inline vec2f operator*(const mat2f& a, const vec2f& b) {
  return a.x * b.x + a.y * b.y;
}
inline vec2f operator*(const vec2f& a, const mat2f& b) {
  return {dot(a, b.x), dot(a, b.y)};
}
inline mat2f operator*(const mat2f& a, const mat2f& b) {
  return {a * b.x, a * b.y};
}

// Matrix assignments.
inline mat2f& operator+=(mat2f& a, const mat2f& b) { return a = a + b; }
inline mat2f& operator*=(mat2f& a, const mat2f& b) { return a = a * b; }
inline mat2f& operator*=(mat2f& a, float b) { return a = a * b; }

// Matrix diagonals and transposes.
inline vec2f diagonal(const mat2f& a) { return {a.x.x, a.y.y}; }
inline mat2f transpose(const mat2f& a) {
  return {{a.x.x, a.y.x}, {a.x.y, a.y.y}};
}

// Matrix adjoints, determinants and inverses.
inline float determinant(const mat2f& a) { return cross(a.x, a.y); }
inline mat2f adjoint(const mat2f& a) {
  return {{a.y.y, -a.x.y}, {-a.y.x, a.x.x}};
}
inline mat2f inverse(const mat2f& a) {
  return adjoint(a) * (1 / determinant(a));
}

// Matrix comparisons.
inline bool operator==(const mat3f& a, const mat3f& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
inline bool operator!=(const mat3f& a, const mat3f& b) { return !(a == b); }

// Matrix operations.
inline mat3f operator+(const mat3f& a, const mat3f& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline mat3f operator*(const mat3f& a, float b) {
  return {a.x * b, a.y * b, a.z * b};
}
inline vec3f operator*(const mat3f& a, const vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline vec3f operator*(const vec3f& a, const mat3f& b) {
  return {dot(a, b.x), dot(a, b.y), dot(a, b.z)};
}
inline mat3f operator*(const mat3f& a, const mat3f& b) {
  return {a * b.x, a * b.y, a * b.z};
}

// Matrix assignments.
inline mat3f& operator+=(mat3f& a, const mat3f& b) { return a = a + b; }
inline mat3f& operator*=(mat3f& a, const mat3f& b) { return a = a * b; }
inline mat3f& operator*=(mat3f& a, float b) { return a = a * b; }

// Matrix diagonals and transposes.
inline vec3f diagonal(const mat3f& a) { return {a.x.x, a.y.y, a.z.z}; }
inline mat3f transpose(const mat3f& a) {
  return {
      {a.x.x, a.y.x, a.z.x},
      {a.x.y, a.y.y, a.z.y},
      {a.x.z, a.y.z, a.z.z},
  };
}

// Matrix adjoints, determinants and inverses.
inline float determinant(const mat3f& a) { return dot(a.x, cross(a.y, a.z)); }
inline mat3f adjoint(const mat3f& a) {
  return transpose(mat3f{cross(a.y, a.z), cross(a.z, a.x), cross(a.x, a.y)});
}
inline mat3f inverse(const mat3f& a) {
  return adjoint(a) * (1 / determinant(a));
}

// Constructs a basis from a direction
inline mat3f basis_fromz(const vec3f& v) {
  // https://graphics.pixar.com/library/OrthonormalB/paper.pdf
  auto z    = normalize(v);
  auto sign = copysignf(1.0f, z.z);
  auto a    = -1.0f / (sign + z.z);
  auto b    = z.x * z.y * a;
  auto x    = vec3f{1.0f + sign * z.x * z.x * a, sign * b, -sign * z.x};
  auto y    = vec3f{b, sign + z.y * z.y * a, -z.y};
  return {x, y, z};
}

// Matrix comparisons.
inline bool operator==(const mat4f& a, const mat4f& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
inline bool operator!=(const mat4f& a, const mat4f& b) { return !(a == b); }

// Matrix operations.
inline mat4f operator+(const mat4f& a, const mat4f& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
inline mat4f operator*(const mat4f& a, float b) {
  return {a.x * b, a.y * b, a.z * b, a.w * b};
}
inline vec4f operator*(const mat4f& a, const vec4f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
inline vec4f operator*(const vec4f& a, const mat4f& b) {
  return {dot(a, b.x), dot(a, b.y), dot(a, b.z), dot(a, b.w)};
}
inline mat4f operator*(const mat4f& a, const mat4f& b) {
  return {a * b.x, a * b.y, a * b.z, a * b.w};
}

// Matrix assignments.
inline mat4f& operator+=(mat4f& a, const mat4f& b) { return a = a + b; }
inline mat4f& operator*=(mat4f& a, const mat4f& b) { return a = a * b; }
inline mat4f& operator*=(mat4f& a, float b) { return a = a * b; }

// Matrix diagonals and transposes.
inline vec4f diagonal(const mat4f& a) { return {a.x.x, a.y.y, a.z.z, a.w.w}; }
inline mat4f transpose(const mat4f& a) {
  return {
      {a.x.x, a.y.x, a.z.x, a.w.x},
      {a.x.y, a.y.y, a.z.y, a.w.y},
      {a.x.z, a.y.z, a.z.z, a.w.z},
      {a.x.w, a.y.w, a.z.w, a.w.w},
  };
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// RIGID BODY TRANSFORMS/FRAMES
// -----------------------------------------------------------------------------
namespace yocto {

// Rigid frames stored as a column-major affine transform matrix.
inline vec2f& frame2f::operator[](int i) { return (&x)[i]; }
inline const vec2f& frame2f::operator[](int i) const { return (&x)[i]; }

// Rigid frames stored as a column-major affine transform matrix.
inline vec3f& frame3f::operator[](int i) { return (&x)[i]; }
inline const vec3f& frame3f::operator[](int i) const { return (&x)[i]; }

// Frame properties
inline mat2f rotation(const frame2f& a) { return {a.x, a.y}; }
inline vec2f translation(const frame2f& a) { return a.o; }

// Frame construction
inline frame2f make_frame(const mat2f& m, const vec2f& t) {
  return {m.x, m.y, t};
}

// Frame/mat conversion
inline frame2f mat_to_frame(const mat3f& m) {
  return {{m.x.x, m.x.y}, {m.y.x, m.y.y}, {m.z.x, m.z.y}};
}
inline mat3f frame_to_mat(const frame2f& f) {
  return {{f.x.x, f.x.y, 0}, {f.y.x, f.y.y, 0}, {f.o.x, f.o.y, 1}};
}

// Frame comparisons.
inline bool operator==(const frame2f& a, const frame2f& b) {
  return a.x == b.x && a.y == b.y && a.o == b.o;
}
inline bool operator!=(const frame2f& a, const frame2f& b) { return !(a == b); }

// Frame composition, equivalent to affine matrix product.
inline frame2f operator*(const frame2f& a, const frame2f& b) {
  return make_frame(rotation(a) * rotation(b), rotation(a) * b.o + a.o);
}
inline frame2f& operator*=(frame2f& a, const frame2f& b) { return a = a * b; }

// Frame inverse, equivalent to rigid affine inverse.
inline frame2f inverse(const frame2f& a, bool non_rigid) {
  if (non_rigid) {
    auto minv = inverse(rotation(a));
    return make_frame(minv, -(minv * a.o));
  } else {
    auto minv = transpose(rotation(a));
    return make_frame(minv, -(minv * a.o));
  }
}

// Frame properties
inline mat3f rotation(const frame3f& a) { return {a.x, a.y, a.z}; }
inline vec3f translation(const frame3f& a) { return a.o; }

// Frame construction
inline frame3f make_frame(const mat3f& m, const vec3f& t) {
  return {m.x, m.y, m.z, t};
}

// frame/mat conversion
inline frame3f mat_to_frame(const mat4f& m) {
  return {{m.x.x, m.x.y, m.x.z}, {m.y.x, m.y.y, m.y.z}, {m.z.x, m.z.y, m.z.z},
      {m.w.x, m.w.y, m.w.z}};
}
inline mat4f frame_to_mat(const frame3f& f) {
  return {{f.x.x, f.x.y, f.x.z, 0}, {f.y.x, f.y.y, f.y.z, 0},
      {f.z.x, f.z.y, f.z.z, 0}, {f.o.x, f.o.y, f.o.z, 1}};
}

// Frame comparisons.
inline bool operator==(const frame3f& a, const frame3f& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.o == b.o;
}
inline bool operator!=(const frame3f& a, const frame3f& b) { return !(a == b); }

// Frame composition, equivalent to affine matrix product.
inline frame3f operator*(const frame3f& a, const frame3f& b) {
  return make_frame(rotation(a) * rotation(b), rotation(a) * b.o + a.o);
}
inline frame3f& operator*=(frame3f& a, const frame3f& b) { return a = a * b; }

// Frame inverse, equivalent to rigid affine inverse.
inline frame3f inverse(const frame3f& a, bool non_rigid) {
  if (non_rigid) {
    auto minv = inverse(rotation(a));
    return make_frame(minv, -(minv * a.o));
  } else {
    auto minv = transpose(rotation(a));
    return make_frame(minv, -(minv * a.o));
  }
}

// Frame construction from axis.
inline frame3f frame_fromz(const vec3f& o, const vec3f& v) {
  // https://graphics.pixar.com/library/OrthonormalB/paper.pdf
  auto z    = normalize(v);
  auto sign = copysignf(1.0f, z.z);
  auto a    = -1.0f / (sign + z.z);
  auto b    = z.x * z.y * a;
  auto x    = vec3f{1.0f + sign * z.x * z.x * a, sign * b, -sign * z.x};
  auto y    = vec3f{b, sign + z.y * z.y * a, -z.y};
  return {x, y, z, o};
}
inline frame3f frame_fromzx(const vec3f& o, const vec3f& z_, const vec3f& x_) {
  auto z = normalize(z_);
  auto x = orthonormalize(x_, z);
  auto y = normalize(cross(z, x));
  return {x, y, z, o};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// QUATERNIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Quaternion operatons
inline quat4f operator+(const quat4f& a, const quat4f& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
inline quat4f operator*(const quat4f& a, float b) {
  return {a.x * b, a.y * b, a.z * b, a.w * b};
}
inline quat4f operator/(const quat4f& a, float b) {
  return {a.x / b, a.y / b, a.z / b, a.w / b};
}
inline quat4f operator*(const quat4f& a, const quat4f& b) {
  return {a.x * b.w + a.w * b.x + a.y * b.w - a.z * b.y,
      a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
      a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// Quaterion operations
inline float dot(const quat4f& a, const quat4f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
inline float  length(const quat4f& a) { return sqrt(dot(a, a)); }
inline quat4f normalize(const quat4f& a) {
  auto l = length(a);
  return (l != 0) ? a / l : a;
}
inline quat4f conjugate(const quat4f& a) { return {-a.x, -a.y, -a.z, a.w}; }
inline quat4f inverse(const quat4f& a) { return conjugate(a) / dot(a, a); }
inline float  uangle(const quat4f& a, const quat4f& b) {
   auto d = dot(a, b);
   return d > 1 ? 0 : acos(d < -1 ? -1 : d);
}
inline quat4f lerp(const quat4f& a, const quat4f& b, float t) {
  return a * (1 - t) + b * t;
}
inline quat4f nlerp(const quat4f& a, const quat4f& b, float t) {
  return normalize(lerp(a, b, t));
}
inline quat4f slerp(const quat4f& a, const quat4f& b, float t) {
  auto th = uangle(a, b);
  return th == 0
             ? a
             : a * (sin(th * (1 - t)) / sin(th)) + b * (sin(th * t) / sin(th));
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// TRANSFORMS
// -----------------------------------------------------------------------------
namespace yocto {

// Transforms points, vectors and directions by matrices.
inline vec2f transform_point(const mat3f& a, const vec2f& b) {
  auto tvb = a * vec3f{b.x, b.y, 1};
  return vec2f{tvb.x, tvb.y} / tvb.z;
}
inline vec2f transform_vector(const mat3f& a, const vec2f& b) {
  auto tvb = a * vec3f{b.x, b.y, 0};
  return vec2f{tvb.x, tvb.y} / tvb.z;
}
inline vec2f transform_direction(const mat3f& a, const vec2f& b) {
  return normalize(transform_vector(a, b));
}
inline vec2f transform_normal(const mat3f& a, const vec2f& b) {
  return normalize(transform_vector(transpose(inverse(a)), b));
}
inline vec2f transform_vector(const mat2f& a, const vec2f& b) { return a * b; }
inline vec2f transform_direction(const mat2f& a, const vec2f& b) {
  return normalize(transform_vector(a, b));
}
inline vec2f transform_normal(const mat2f& a, const vec2f& b) {
  return normalize(transform_vector(transpose(inverse(a)), b));
}

inline vec3f transform_point(const mat4f& a, const vec3f& b) {
  auto tvb = a * vec4f{b.x, b.y, b.z, 1};
  return vec3f{tvb.x, tvb.y, tvb.z} / tvb.w;
}
inline vec3f transform_vector(const mat4f& a, const vec3f& b) {
  auto tvb = a * vec4f{b.x, b.y, b.z, 0};
  return vec3f{tvb.x, tvb.y, tvb.z};
}
inline vec3f transform_direction(const mat4f& a, const vec3f& b) {
  return normalize(transform_vector(a, b));
}
inline vec3f transform_vector(const mat3f& a, const vec3f& b) { return a * b; }
inline vec3f transform_direction(const mat3f& a, const vec3f& b) {
  return normalize(transform_vector(a, b));
}
inline vec3f transform_normal(const mat3f& a, const vec3f& b) {
  return normalize(transform_vector(transpose(inverse(a)), b));
}

// Transforms points, vectors and directions by frames.
inline vec2f transform_point(const frame2f& a, const vec2f& b) {
  return a.x * b.x + a.y * b.y + a.o;
}
inline vec2f transform_vector(const frame2f& a, const vec2f& b) {
  return a.x * b.x + a.y * b.y;
}
inline vec2f transform_direction(const frame2f& a, const vec2f& b) {
  return normalize(transform_vector(a, b));
}
inline vec2f transform_normal(
    const frame2f& a, const vec2f& b, bool non_rigid) {
  if (non_rigid) {
    return transform_normal(rotation(a), b);
  } else {
    return normalize(transform_vector(a, b));
  }
}

// Transforms points, vectors and directions by frames.
inline vec3f transform_point(const frame3f& a, const vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.o;
}
inline vec3f transform_vector(const frame3f& a, const vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline vec3f transform_direction(const frame3f& a, const vec3f& b) {
  return normalize(transform_vector(a, b));
}
inline vec3f transform_normal(
    const frame3f& a, const vec3f& b, bool non_rigid) {
  if (non_rigid) {
    return transform_normal(rotation(a), b);
  } else {
    return normalize(transform_vector(a, b));
  }
}

// Transforms points, vectors and directions by frames.
inline vec2f transform_point_inverse(const frame2f& a, const vec2f& b) {
  return {dot(a.x, b - a.o), dot(a.y, b - a.o)};
}
inline vec2f transform_vector_inverse(const frame2f& a, const vec2f& b) {
  return {dot(a.x, b), dot(a.y, b)};
}
inline vec2f transform_direction_inverse(const frame2f& a, const vec2f& b) {
  return normalize(transform_vector_inverse(a, b));
}

// Transforms points, vectors and directions by frames.
inline vec3f transform_point_inverse(const frame3f& a, const vec3f& b) {
  return {dot(a.x, b - a.o), dot(a.y, b - a.o), dot(a.z, b - a.o)};
}
inline vec3f transform_vector_inverse(const frame3f& a, const vec3f& b) {
  return {dot(a.x, b), dot(a.y, b), dot(a.z, b)};
}
inline vec3f transform_direction_inverse(const frame3f& a, const vec3f& b) {
  return normalize(transform_vector_inverse(a, b));
}

// Translation, scaling and rotations transforms.
inline frame3f translation_frame(const vec3f& a) {
  return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, a};
}
inline frame3f scaling_frame(const vec3f& a) {
  return {{a.x, 0, 0}, {0, a.y, 0}, {0, 0, a.z}, {0, 0, 0}};
}
inline frame3f rotation_frame(const vec3f& axis, float angle) {
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
inline frame3f rotation_frame(const vec4f& quat) {
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
inline frame3f rotation_frame(const quat4f& quat) {
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
inline frame3f rotation_frame(const mat3f& rot) {
  return {rot.x, rot.y, rot.z, {0, 0, 0}};
}

// Lookat frame. Z-axis can be inverted with inv_xz.
inline frame3f lookat_frame(
    const vec3f& eye, const vec3f& center, const vec3f& up, bool inv_xz) {
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
inline mat4f frustum_mat(float l, float r, float b, float t, float n, float f) {
  return {{2 * n / (r - l), 0, 0, 0}, {0, 2 * n / (t - b), 0, 0},
      {(r + l) / (r - l), (t + b) / (t - b), -(f + n) / (f - n), -1},
      {0, 0, -2 * f * n / (f - n), 0}};
}
inline mat4f ortho_mat(float l, float r, float b, float t, float n, float f) {
  return {{2 / (r - l), 0, 0, 0}, {0, 2 / (t - b), 0, 0},
      {0, 0, -2 / (f - n), 0},
      {-(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1}};
}
inline mat4f ortho2d_mat(float left, float right, float bottom, float top) {
  return ortho_mat(left, right, bottom, top, -1, 1);
}
inline mat4f ortho_mat(float xmag, float ymag, float near, float far) {
  return {{1 / xmag, 0, 0, 0}, {0, 1 / ymag, 0, 0}, {0, 0, 2 / (near - far), 0},
      {0, 0, (far + near) / (near - far), 1}};
}
inline mat4f perspective_mat(float fovy, float aspect, float near, float far) {
  auto tg = tan(fovy / 2);
  return {{1 / (aspect * tg), 0, 0, 0}, {0, 1 / tg, 0, 0},
      {0, 0, (far + near) / (near - far), -1},
      {0, 0, 2 * far * near / (near - far), 0}};
}
inline mat4f perspective_mat(float fovy, float aspect, float near) {
  auto tg = tan(fovy / 2);
  return {{1 / (aspect * tg), 0, 0, 0}, {0, 1 / tg, 0, 0}, {0, 0, -1, -1},
      {0, 0, 2 * near, 0}};
}

// Rotation conversions.
inline pair<vec3f, float> rotation_axisangle(const vec4f& quat) {
  return {normalize(vec3f{quat.x, quat.y, quat.z}), 2 * acos(quat.w)};
}
inline vec4f rotation_quat(const vec3f& axis, float angle) {
  auto len = length(axis);
  if (len == 0) return {0, 0, 0, 1};
  return vec4f{sin(angle / 2) * axis.x / len, sin(angle / 2) * axis.y / len,
      sin(angle / 2) * axis.z / len, cos(angle / 2)};
}
inline vec4f rotation_quat(const vec4f& axisangle) {
  return rotation_quat(
      vec3f{axisangle.x, axisangle.y, axisangle.z}, axisangle.w);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// USER INTERFACE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Computes the image uv coordinates corresponding to the view parameters.
// Returns negative coordinates if out of the image.
inline vec2i image_coords(const vec2f& mouse_pos, const vec2f& center,
    float scale, const vec2i& txt_size) {
  auto xyf = (mouse_pos - center) / scale;
  return vec2i{(int)round(xyf.x + txt_size.x / 2.0f),
      (int)round(xyf.y + txt_size.y / 2.0f)};
}

// Center image and autofit. Returns center and scale.
inline pair<vec2f, float> camera_imview(const vec2f& center, float scale,
    const vec2i& imsize, const vec2i& winsize, bool zoom_to_fit) {
  if (zoom_to_fit) {
    return {{(float)winsize.x / 2, (float)winsize.y / 2},
        min(winsize.x / (float)imsize.x, winsize.y / (float)imsize.y)};
  } else {
    return {
        {(winsize.x >= imsize.x * scale) ? (float)winsize.x / 2 : center.x,
            (winsize.y >= imsize.y * scale) ? (float)winsize.y / 2 : center.y},
        scale};
  }
}

// Turntable for UI navigation. Returns from and to.
inline pair<vec3f, vec3f> camera_turntable(const vec3f& from_, const vec3f& to_,
    const vec3f& up, const vec2f& rotate, float dolly, const vec2f& pan) {
  // copy values
  auto from = from_, to = to_;

  // rotate if necessary
  if (rotate != vec2f{0, 0}) {
    auto z     = normalize(to - from);
    auto lz    = length(to - from);
    auto phi   = atan2(z.z, z.x) + rotate.x;
    auto theta = acos(z.y) + rotate.y;
    theta      = clamp(theta, 0.001f, pif - 0.001f);
    auto nz    = vec3f{sin(theta) * cos(phi) * lz, cos(theta) * lz,
        sin(theta) * sin(phi) * lz};
    from       = to - nz;
  }

  // dolly if necessary
  if (dolly != 0) {
    auto z  = normalize(to - from);
    auto lz = max(0.001f, length(to - from) * (1 + dolly));
    z *= lz;
    from = to - z;
  }

  // pan if necessary
  if (pan != vec2f{0, 0}) {
    auto z = normalize(to - from);
    auto x = normalize(cross(up, z));
    auto y = normalize(cross(z, x));
    auto t = vec3f{pan.x * x.x + pan.y * y.x, pan.x * x.y + pan.y * y.y,
        pan.x * x.z + pan.y * y.z};
    from += t;
    to += t;
  }

  // done
  return {from, to};
}

// Turntable for UI navigation. Returns frame and focus.
inline pair<frame3f, float> camera_turntable(const frame3f& frame_, float focus,
    const vec2f& rotate, float dolly, const vec2f& pan) {
  // copy values
  auto frame = frame_;

  // rotate if necessary
  if (rotate != vec2f{0, 0}) {
    auto phi   = atan2(frame.z.z, frame.z.x) + rotate.x;
    auto theta = acos(frame.z.y) + rotate.y;
    theta      = clamp(theta, 0.001f, pif - 0.001f);
    auto new_z = vec3f{
        sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)};
    auto new_center = frame.o - frame.z * focus;
    auto new_o      = new_center + new_z * focus;
    frame           = lookat_frame(new_o, new_center, {0, 1, 0});
    focus           = length(new_o - new_center);
  }

  // pan if necessary
  if (dolly != 0) {
    auto c  = frame.o - frame.z * focus;
    focus   = max(focus * (1 + dolly), 0.001f);
    frame.o = c + frame.z * focus;
  }

  // pan if necessary
  if (pan != vec2f{0, 0}) {
    frame.o += frame.x * pan.x + frame.y * pan.y;
  }

  // done
  return {frame, focus};
}

// FPS camera for UI navigation for a frame parametrization. Returns frame.
inline frame3f camera_fpscam(
    const frame3f& frame, const vec3f& transl, const vec2f& rotate) {
  // https://gamedev.stackexchange.com/questions/30644/how-to-keep-my-quaternion-using-fps-camera-from-tilting-and-messing-up
  auto y = vec3f{0, 1, 0};
  auto z = orthonormalize(frame.z, y);
  auto x = cross(y, z);

  auto rot = rotation_frame(vec3f{1, 0, 0}, rotate.y) *
             frame3f{frame.x, frame.y, frame.z, vec3f{0, 0, 0}} *
             rotation_frame(vec3f{0, 1, 0}, rotate.x);
  auto pos = frame.o + transl.x * x + transl.y * y + transl.z * z;

  return {rot.x, rot.y, rot.z, pos};
}

// Computes the image uv coordinates corresponding to the view parameters.
// Returns negative coordinates if out of the image.
inline vec2i get_image_coords(const vec2f& mouse_pos, const vec2f& center,
    float scale, const vec2i& txt_size) {
  auto xyf = (mouse_pos - center) / scale;
  return vec2i{(int)round(xyf.x + txt_size.x / 2.0f),
      (int)round(xyf.y + txt_size.y / 2.0f)};
}

// Center image and autofit.
inline void update_imview(vec2f& center, float& scale, const vec2i& imsize,
    const vec2i& winsize, bool zoom_to_fit) {
  if (zoom_to_fit) {
    scale  = min(winsize.x / (float)imsize.x, winsize.y / (float)imsize.y);
    center = {(float)winsize.x / 2, (float)winsize.y / 2};
  } else {
    if (winsize.x >= imsize.x * scale) center.x = (float)winsize.x / 2;
    if (winsize.y >= imsize.y * scale) center.y = (float)winsize.y / 2;
  }
}

// Turntable for UI navigation.
inline void update_turntable(vec3f& from, vec3f& to, vec3f& up,
    const vec2f& rotate, float dolly, const vec2f& pan) {
  // rotate if necessary
  if (rotate != vec2f{0, 0}) {
    auto z     = normalize(to - from);
    auto lz    = length(to - from);
    auto phi   = atan2(z.z, z.x) + rotate.x;
    auto theta = acos(z.y) + rotate.y;
    theta      = clamp(theta, 0.001f, pif - 0.001f);
    auto nz    = vec3f{sin(theta) * cos(phi) * lz, cos(theta) * lz,
        sin(theta) * sin(phi) * lz};
    from       = to - nz;
  }

  // dolly if necessary
  if (dolly != 0) {
    auto z  = normalize(to - from);
    auto lz = max(0.001f, length(to - from) * (1 + dolly));
    z *= lz;
    from = to - z;
  }

  // pan if necessary
  if (pan != vec2f{0, 0}) {
    auto z = normalize(to - from);
    auto x = normalize(cross(up, z));
    auto y = normalize(cross(z, x));
    auto t = vec3f{pan.x * x.x + pan.y * y.x, pan.x * x.y + pan.y * y.y,
        pan.x * x.z + pan.y * y.z};
    from += t;
    to += t;
  }
}

// Turntable for UI navigation.
inline void update_turntable(frame3f& frame, float& focus, const vec2f& rotate,
    float dolly, const vec2f& pan) {
  // rotate if necessary
  if (rotate != vec2f{0, 0}) {
    auto phi   = atan2(frame.z.z, frame.z.x) + rotate.x;
    auto theta = acos(frame.z.y) + rotate.y;
    theta      = clamp(theta, 0.001f, pif - 0.001f);
    auto new_z = vec3f{
        sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)};
    auto new_center = frame.o - frame.z * focus;
    auto new_o      = new_center + new_z * focus;
    frame           = lookat_frame(new_o, new_center, {0, 1, 0});
    focus           = length(new_o - new_center);
  }

  // pan if necessary
  if (dolly != 0) {
    auto c  = frame.o - frame.z * focus;
    focus   = max(focus * (1 + dolly), 0.001f);
    frame.o = c + frame.z * focus;
  }

  // pan if necessary
  if (pan != vec2f{0, 0}) {
    frame.o += frame.x * pan.x + frame.y * pan.y;
  }
}

// FPS camera for UI navigation for a frame parametrization.
inline void update_fpscam(
    frame3f& frame, const vec3f& transl, const vec2f& rotate) {
  // https://gamedev.stackexchange.com/questions/30644/how-to-keep-my-quaternion-using-fps-camera-from-tilting-and-messing-up
  auto y = vec3f{0, 1, 0};
  auto z = orthonormalize(frame.z, y);
  auto x = cross(y, z);

  auto rot = rotation_frame(vec3f{1, 0, 0}, rotate.y) *
             frame3f{frame.x, frame.y, frame.z, vec3f{0, 0, 0}} *
             rotation_frame(vec3f{0, 1, 0}, rotate.x);
  auto pos = frame.o + transl.x * x + transl.y * y + transl.z * z;

  frame = {rot.x, rot.y, rot.z, pos};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// PYTHON-LIKE ITERATORS
// -----------------------------------------------------------------------------
namespace yocto {

// Python `range()` equivalent. Construct an object to iterate over a sequence.
template <typename T>
constexpr auto range(T max) {
  return range((T)0, max);
}
template <typename T>
constexpr auto range(T min, T max) {
  struct range_iterator {
    T    index;
    void operator++() { ++index; }
    bool operator!=(const range_iterator& other) const {
      return index != other.index;
    }
    T operator*() const { return index; }
  };
  struct range_helper {
    T              begin_ = 0, end_ = 0;
    range_iterator begin() const { return {begin_}; }
    range_iterator end() const { return {end_}; }
  };
  return range_helper{min, max};
}
template <typename T>
constexpr auto range(T min, T max, T step) {
  struct range_iterator {
    T    index;
    T    step;
    void operator++() { index += step; }
    bool operator!=(const range_iterator& other) const {
      return index != other.index;
    }
    T operator*() const { return index; }
  };
  struct range_helper {
    T              begin_ = 0, end_ = 0, step_ = 0;
    range_iterator begin() const { return {begin_, step_}; }
    range_iterator end() const {
      return {begin_ + ((end_ - begin_) / step_) * step_, step_};
    }
  };
  return range_helper{min, max, step};
}

// Python enumerate
template <typename Sequence, typename T>
constexpr auto enumerate(const Sequence& sequence, T start) {
  using Iterator  = typename Sequence::const_iterator;
  using Reference = typename Sequence::const_reference;
  struct enumerate_iterator {
    T        index;
    Iterator iterator;
    bool     operator!=(const enumerate_iterator& other) const {
          return index != other.index;
    }
    void operator++() {
      ++index;
      ++iterator;
    }
    pair<const T&, Reference> operator*() const { return {index, *iterator}; }
  };
  struct enumerate_helper {
    const Sequence& sequence;
    T               begin_, end_;
    auto begin() { return enumerate_iterator{begin_, std::begin(sequence)}; }
    auto end() { return enumerate_iterator{end_, std::end(sequence)}; }
  };
  return enumerate_helper{sequence, 0, size(sequence)};
}

// Python enumerate
template <typename Sequence, typename T>
constexpr auto enumerate(Sequence& sequence, T start) {
  using Iterator  = typename Sequence::iterator;
  using Reference = typename Sequence::reference;
  struct enumerate_iterator {
    T        index;
    Iterator iterator;
    bool     operator!=(const enumerate_iterator& other) const {
          return index != other.index;
    }
    void operator++() {
      ++index;
      ++iterator;
    }
    pair<T&, Reference> operator*() const { return {index, *iterator}; }
  };
  struct enumerate_helper {
    Sequence& sequence;
    T         begin_, end_;
    auto begin() { return enumerate_iterator{begin_, std::begin(sequence)}; }
    auto end() { return enumerate_iterator{end_, std::end(sequence)}; }
  };
  return enumerate_helper{sequence, 0, size(sequence)};
}

// Python zip
template <typename Sequence1, typename Sequence2>
constexpr auto zip(const Sequence1& sequence1, const Sequence2& sequence2) {
  using Iterator1  = typename Sequence1::const_iterator;
  using Reference1 = typename Sequence1::const_reference;
  using Iterator2  = typename Sequence2::const_iterator;
  using Reference2 = typename Sequence2::const_reference;
  struct zip_iterator {
    Iterator1 iterator1;
    Iterator2 iterator2;
    bool      operator!=(const zip_iterator& other) const {
           return iterator1 != other.iterator1;
    }
    void operator++() {
      ++iterator1;
      ++iterator2;
    }
    pair<Reference1, Reference2> operator*() const {
      return {*iterator1, *iterator2};
    }
  };
  struct zip_helper {
    const Sequence1& sequence1;
    const Sequence2& sequence2;
    auto             begin() {
                  return zip_iterator{std::begin(sequence1), std::begin(sequence2)};
    }
    auto end() {
      return zip_iterator{std::end(sequence1), std::end(sequence2)};
    }
  };
  return zip_helper{sequence1, sequence2};
}

// Python zip
template <typename Sequence1, typename Sequence2>
constexpr auto zip(Sequence1& sequence1, Sequence2& sequence2) {
  using Iterator1  = typename Sequence1::iterator;
  using Reference1 = typename Sequence1::reference;
  using Iterator2  = typename Sequence2::iterator;
  using Reference2 = typename Sequence2::reference;
  struct zip_iterator {
    Iterator1 iterator1;
    Iterator2 iterator2;
    bool      operator!=(const zip_iterator& other) const {
           return iterator1 != other.iterator1;
    }
    void operator++() {
      ++iterator1;
      ++iterator2;
    }
    pair<Reference1, Reference2> operator*() const {
      return {*iterator1, *iterator2};
    }
  };
  struct zip_helper {
    Sequence1& sequence1;
    Sequence2& sequence2;
    auto       begin() {
            return zip_iterator{std::begin(sequence1), std::begin(sequence2)};
    }
    auto end() {
      return zip_iterator{std::end(sequence1), std::end(sequence2)};
    }
  };
  return zip_helper{sequence1, sequence2};
}

// Python zip
template <typename Sequence1, typename Sequence2>
constexpr auto zip(const Sequence1& sequence1, Sequence2& sequence2) {
  using Iterator1  = typename Sequence1::const_iterator;
  using Reference1 = typename Sequence1::const_reference;
  using Iterator2  = typename Sequence2::iterator;
  using Reference2 = typename Sequence2::reference;
  struct zip_iterator {
    Iterator1 iterator1;
    Iterator2 iterator2;
    bool      operator!=(const zip_iterator& other) const {
           return iterator1 != other.iterator1;
    }
    void operator++() {
      ++iterator1;
      ++iterator2;
    }
    pair<Reference1, Reference2> operator*() const {
      return {*iterator1, *iterator2};
    }
  };
  struct zip_helper {
    const Sequence1& sequence1;
    Sequence2&       sequence2;
    auto             begin() {
                  return zip_iterator{std::begin(sequence1), std::begin(sequence2)};
    }
    auto end() {
      return zip_iterator{std::end(sequence1), std::end(sequence2)};
    }
  };
  return zip_helper{sequence1, sequence2};
}

// Python zip
template <typename Sequence1, typename Sequence2>
constexpr auto zip(Sequence1& sequence1, const Sequence2& sequence2) {
  using Iterator1  = typename Sequence1::iterator;
  using Reference1 = typename Sequence1::reference;
  using Iterator2  = typename Sequence2::const_iterator;
  using Reference2 = typename Sequence2::const_reference;
  struct zip_iterator {
    Iterator1 iterator1;
    Iterator2 iterator2;
    bool      operator!=(const zip_iterator& other) const {
           return iterator1 != other.iterator1;
    }
    void operator++() {
      ++iterator1;
      ++iterator2;
    }
    pair<Reference1, Reference2> operator*() const {
      return {*iterator1, *iterator2};
    }
  };
  struct zip_helper {
    Sequence1&       sequence1;
    const Sequence2& sequence2;
    auto             begin() {
                  return zip_iterator{std::begin(sequence1), std::begin(sequence2)};
    }
    auto end() {
      return zip_iterator{std::end(sequence1), std::end(sequence2)};
    }
  };
  return zip_helper{sequence1, sequence2};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// SIGNED-SIZE
// -----------------------------------------------------------------------------
namespace yocto {

template <typename T>
inline std::ptrdiff_t ssize(const T& container) {
  return (std::ptrdiff_t)std::size(container);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// CUDA SUPPORT
// -----------------------------------------------------------------------------
#ifdef __CUDACC__
#undef inline
#endif

#endif
