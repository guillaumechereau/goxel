//
// # Yocto/Random: Tiny library for random number generatio and Monte Carlo.
//
// Yocto/Random provides utilities for random number generation, hasing,
// Monte Carlo integration and sampling and Perloin noise.
//
//
// ## Random Number Generation, Noise, and Monte Carlo support
//
// This library supports many facilities helpful in writing sampling
// functions targeting path tracing and shape generations. Implementation of
// Perlin noise is include based on stb libraries.
//
// 1. Random number generation with PCG32:
//     1. initialize the random number generator with `make_rng()`
//     2. advance the random number state with `advance_rng()`
//     3. if necessary, you can reseed the rng with `seed_rng()`
//     4. generate random integers in an interval with `get_random_int()`
//     5. generate random floats and double in the [0,1) range with
//        `get_random_float()`, `get_random_vec2f()`, `get_random_vec3f()`,
//        `next_rand1d()`
// 2. Perlin noise: `perlin_noise()` to generate Perlin noise with optional
//    wrapping, with fractal variations `perlin_ridge_noise()`,
//    `perlin_fbm_noise()`, `perlin_turbulence_noise()`
// 3. Monte Carlo support: warp functions from [0,1)^k domains to domains
//    commonly used in path tracing. In particular, use
//    `sample_hemisphere_direction()`, `sample_sphere_direction()`,
//    `sample_hemisphere_direction_cosine()`,
//    `sample_hemisphere_direction_cospower()`. `sample_disk_point()`.
//    `sample_cylinder_point()`. `sample_triangle()`,
//    `sample_discrete_distribution()`. For each warp, you can compute
//     the PDF with `sample_xxx_pdf()`.
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
// LICENSE OF INCLUDED SOFTWARE for Pcg random number generator
//
// This code also includes a small exerpt from http://www.pcg-random.org/
// licensed as follows
// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//
//
// LICENCE OF INCLUDED SOFTWARE FOR PERLIN NOISE
// https://github.com/nothings/stb/blob/master/stb_perlin.h
//
// ------------------------------------------------------------------------------
// ALTERNATIVE B - Public Domain (www.unlicense.org)
// This is free and unencumbered software released into the public domain.
// Anyone is free to copy, modify, publish, use, compile, sell, or distribute
// this software, either in source code form or as a compiled binary, for any
// purpose, commercial or non-commercial, and by any means. In jurisdictions
// that recognize copyright laws, the author or authors of this software
// dedicate any and all copyright interest in the software to the public domain.
// We make this dedication for the benefit of the public at large and to the
// detriment of our heirs and successors. We intend this dedication to be an
// overt act of relinquishment in perpetuity of all present and future rights to
// this software under copyright law.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// ------------------------------------------------------------------------------
//
//

#ifndef _YOCTO_RANDOM_H_
#define _YOCTO_RANDOM_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_math.h"
#include "yocto_utils.h"

// -----------------------------------------------------------------------------
// RANDOM NUMBER GENERATION
// -----------------------------------------------------------------------------
namespace yocto {

// PCG random numbers from http://www.pcg-random.org/
struct rng_state {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    rng_state() : state{0x853c49e6748fea9bULL}, inc{0xda3e39cb94b95bdbULL} {}
    rng_state(uint64_t state, uint64_t inc) : state{state}, inc{inc} {}
};

// Next random number.
inline uint32_t advance_rng(rng_state& rng) {
    uint64_t oldstate   = rng.state;
    rng.state           = oldstate * 6364136223846793005ULL + rng.inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot        = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

// Init a random number generator with a state state from the sequence seq.
inline rng_state make_rng(uint64_t seed, uint64_t seq = 1) {
    auto rng  = rng_state();
    rng.state = 0U;
    rng.inc   = (seq << 1u) | 1u;
    advance_rng(rng);
    rng.state += seed;
    advance_rng(rng);
    return rng;
}

// Next random numbers: floats in [0,1), ints in [0,n).
inline int get_random_int(rng_state& rng, int n) {
    return advance_rng(rng) % n;
}
inline float get_random_float(rng_state& rng) {
    union {
        uint32_t u;
        float    f;
    } x;
    x.u = (advance_rng(rng) >> 9) | 0x3f800000u;
    return x.f - 1.0f;
    // alternate implementation
    // const static auto scale = (float)(1.0 / numeric_limits<uint32_t>::max());
    // return advance_rng(rng) * scale;
}
inline vec2f get_random_vec2f(rng_state& rng) {
    // force order of evaluation by using separate assignments.
    auto x = get_random_float(rng);
    auto y = get_random_float(rng);
    return {x, y};
}
inline vec3f get_random_vec3f(rng_state& rng) {
    // force order of evaluation by using separate assignments.
    auto x = get_random_float(rng);
    auto y = get_random_float(rng);
    auto z = get_random_float(rng);
    return {x, y, z};
}

// Shuffles a sequence of elements
template <typename T>
inline void random_shuffle(vector<T>& vals, rng_state& rng) {
    // https://en.wikipedia.org/wiki/Fisher–Yates_shuffle
    using std::swap;
    for (auto i = (int)vals.size() - 1; i > 0; i--) {
        auto j = get_random_int(rng, i + 1);
        swap(vals[j], vals[i]);
    }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// MONETACARLO SAMPLING FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Sample an hemispherical direction with uniform distribution.
inline vec3f sample_hemisphere_direction(const vec2f& ruv) {
    auto z   = ruv.y;
    auto r   = sqrt(clamp01(1 - z * z));
    auto phi = 2 * pif * ruv.x;
    return {r * cos(phi), r * sin(phi), z};
}
inline float sample_hemisphere_direction_pdf(const vec3f& direction) {
    return (direction.z <= 0) ? 0 : 1 / (2 * pif);
}

// Sample an hemispherical direction with uniform distribution.
inline vec3f sample_hemisphere_direction(
    const vec3f& normal, const vec2f& ruv) {
    auto z               = ruv.y;
    auto r               = sqrt(clamp01(1 - z * z));
    auto phi             = 2 * pif * ruv.x;
    auto local_direction = vec3f{r * cos(phi), r * sin(phi), z};
    return transform_direction(make_basis_fromz(normal), local_direction);
}
inline float sample_hemisphere_direction_pdf(
    const vec3f& normal, const vec3f& direction) {
    return (dot(normal, direction) <= 0) ? 0 : 1 / (2 * pif);
}

// Sample a spherical direction with uniform distribution.
inline vec3f sample_sphere_direction(const vec2f& ruv) {
    auto z   = 2 * ruv.y - 1;
    auto r   = sqrt(clamp01(1 - z * z));
    auto phi = 2 * pif * ruv.x;
    return {r * cos(phi), r * sin(phi), z};
}
inline float sample_sphere_direction_pdf(const vec3f& w) {
    return 1 / (4 * pif);
}

// Sample an hemispherical direction with cosine distribution.
inline vec3f sample_hemisphere_direction_cosine(const vec2f& ruv) {
    auto z   = sqrt(ruv.y);
    auto r   = sqrt(1 - z * z);
    auto phi = 2 * pif * ruv.x;
    return {r * cos(phi), r * sin(phi), z};
}
inline float sample_hemisphere_direction_cosine_pdf(const vec3f& direction) {
    return (direction.z <= 0) ? 0 : direction.z / pif;
}

// Sample an hemispherical direction with cosine power distribution.
inline vec3f sample_hemisphere_direction_cospower(
    float exponent, const vec2f& ruv) {
    auto z   = pow(ruv.y, 1 / (exponent + 1));
    auto r   = sqrt(1 - z * z);
    auto phi = 2 * pif * ruv.x;
    return {r * cos(phi), r * sin(phi), z};
}
inline float sample_hemisphere_direction_cospower_pdf(
    float exponent, const vec3f& direction) {
    return (direction.z <= 0)
               ? 0
               : pow(direction.z, exponent) * (exponent + 1) / (2 * pif);
}

// Sample a point uniformly on a disk.
inline vec2f sample_disk_point(const vec2f& ruv) {
    auto r   = sqrt(ruv.y);
    auto phi = 2 * pif * ruv.x;
    return {cos(phi) * r, sin(phi) * r};
}
inline float sample_disk_point_pdf() { return 1 / pif; }

// Sample a point uniformly on a cylinder, without caps.
inline vec3f sample_cylinder_point(const vec2f& ruv) {
    auto phi = 2 * pif * ruv.x;
    return {sin(phi), cos(phi), ruv.y * 2 - 1};
}
inline float sample_cylinder_point_pdf() { return 1 / pif; }

// Sample a point uniformly on a triangle returning the baricentric coordinates.
inline vec2f sample_triangle_coordinates(const vec2f& ruv) {
    return {1 - sqrt(ruv.x), ruv.y * sqrt(ruv.x)};
}

// Sample a point uniformly on a triangle.
inline vec3f sample_triangle_point(
    const vec3f& p0, const vec3f& p1, const vec3f& p2, const vec2f& ruv) {
    auto uv = sample_triangle_coordinates(ruv);
    return p0 * (1 - uv.x - uv.y) + p1 * uv.x + p2 * uv.y;
}
// Pdf for uniform triangle sampling, i.e. triangle area.
inline float sample_triangle_point_pdf(
    const vec3f& p0, const vec3f& p1, const vec3f& p2) {
    return 2 / length(cross(p1 - p0, p2 - p0));
}

// Sample an index with uniform distribution.
inline int sample_uniform_index(int size, float r) {
    return clamp((int)(r * size), 0, size - 1);
}
inline float sample_uniform_index_pdf(int size) { return 1.0f / size; }

// Sample an index with uniform distribution.
template <typename T>
inline T sample_uniform_element(const vector<T>& elements, float r) {
    if (elements.empty()) return {};
    auto size = (int)elements.size();
    return elements[clamp((int)(r * size), 0, size - 1)];
}
template <typename T>
inline float sample_uniform_element_pdf(const vector<T>& elements) {
    if (elements.empty()) return 0;
    return 1.0f / (int)elements.size();
}

// Sample a discrete distribution represented by its cdf.
inline int sample_discrete_distribution(const vector<float>& cdf, float r) {
    r        = clamp(r * cdf.back(), 0.0f, cdf.back() - 0.00001f);
    auto idx = (int)(std::upper_bound(cdf.data(), cdf.data() + cdf.size(), r) -
                     cdf.data());
    return clamp(idx, 0, (int)cdf.size() - 1);
}
// Pdf for uniform discrete distribution sampling.
inline float sample_discrete_distribution_pdf(
    const vector<float>& cdf, int idx) {
    if (idx == 0) return cdf.at(0);
    return cdf.at(idx) - cdf.at(idx - 1);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// PERLIN NOISE FUNCTION
// -----------------------------------------------------------------------------
namespace yocto {

// Compute the revised Perlin noise function. Wrap provides a wrapping noise
// but must be power of two (wraps at 256 anyway). For octave based noise,
// good values are obtained with octaves=6 (numerber of noise calls),
// lacunarity=~2.0 (spacing between successive octaves: 2.0 for warpping
// output), gain=0.5 (relative weighting applied to each successive octave),
// offset=1.0 (used to invert the ridges).
inline float perlin_noise(const vec3f& p, const vec3i& wrap = zero3i);
inline float perlin_ridge_noise(const vec3f& p, float lacunarity = 2.0f,
    float gain = 0.5f, float offset = 1.0f, int octaves = 6,
    const vec3i& wrap = zero3i);
inline float perlin_fbm_noise(const vec3f& p, float lacunarity = 2.0f,
    float gain = 0.5f, int octaves = 6, const vec3i& wrap = zero3i);
inline float perlin_turbulence_noise(const vec3f& p, float lacunarity = 2.0f,
    float gain = 0.5f, int octaves = 6, const vec3i& wrap = zero3i);

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PERLIN NOISE
// -----------------------------------------------------------------------------
namespace yocto {

// clang-format off
inline float stb__perlin_lerp(float a, float b, float t)
{
   return a + (b-a) * t;
}

inline int stb__perlin_fastfloor(float a)
{
    int ai = (int) a;
    return (a < ai) ? ai-1 : ai;
}

// different grad function from Perlin's, but easy to modify to match reference
inline float stb__perlin_grad(int hash, float x, float y, float z)
{
   static float basis[12][4] =
   {
      {  1, 1, 0 },
      { -1, 1, 0 },
      {  1,-1, 0 },
      { -1,-1, 0 },
      {  1, 0, 1 },
      { -1, 0, 1 },
      {  1, 0,-1 },
      { -1, 0,-1 },
      {  0, 1, 1 },
      {  0,-1, 1 },
      {  0, 1,-1 },
      {  0,-1,-1 },
   };

   // perlin's gradient has 12 cases so some get used 1/16th of the time
   // and some 2/16ths. We reduce bias by changing those fractions
   // to 5/64ths and 6/64ths, and the same 4 cases get the extra weight.
   static unsigned char indices[64] =
   {
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,9,1,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
   };

   // if you use reference permutation table, change 63 below to 15 to match reference
   // (this is why the ordering of the table above is funky)
   float *grad = basis[indices[hash & 63]];
   return grad[0]*x + grad[1]*y + grad[2]*z;
}

inline float stb_perlin_noise3(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap)
{
    // not same permutation table as Perlin's reference to avoid copyright issues;
    // Perlin's table can be found at http://mrl.nyu.edu/~perlin/noise/
    // @OPTIMIZE: should this be unsigned char instead of int for cache?
    static unsigned char stb__perlin_randtab[512] =
    {
    23, 125, 161, 52, 103, 117, 70, 37, 247, 101, 203, 169, 124, 126, 44, 123,
    152, 238, 145, 45, 171, 114, 253, 10, 192, 136, 4, 157, 249, 30, 35, 72,
    175, 63, 77, 90, 181, 16, 96, 111, 133, 104, 75, 162, 93, 56, 66, 240,
    8, 50, 84, 229, 49, 210, 173, 239, 141, 1, 87, 18, 2, 198, 143, 57,
    225, 160, 58, 217, 168, 206, 245, 204, 199, 6, 73, 60, 20, 230, 211, 233,
    94, 200, 88, 9, 74, 155, 33, 15, 219, 130, 226, 202, 83, 236, 42, 172,
    165, 218, 55, 222, 46, 107, 98, 154, 109, 67, 196, 178, 127, 158, 13, 243,
    65, 79, 166, 248, 25, 224, 115, 80, 68, 51, 184, 128, 232, 208, 151, 122,
    26, 212, 105, 43, 179, 213, 235, 148, 146, 89, 14, 195, 28, 78, 112, 76,
    250, 47, 24, 251, 140, 108, 186, 190, 228, 170, 183, 139, 39, 188, 244, 246,
    132, 48, 119, 144, 180, 138, 134, 193, 82, 182, 120, 121, 86, 220, 209, 3,
    91, 241, 149, 85, 205, 150, 113, 216, 31, 100, 41, 164, 177, 214, 153, 231,
    38, 71, 185, 174, 97, 201, 29, 95, 7, 92, 54, 254, 191, 118, 34, 221,
    131, 11, 163, 99, 234, 81, 227, 147, 156, 176, 17, 142, 69, 12, 110, 62,
    27, 255, 0, 194, 59, 116, 242, 252, 19, 21, 187, 53, 207, 129, 64, 135,
    61, 40, 167, 237, 102, 223, 106, 159, 197, 189, 215, 137, 36, 32, 22, 5,

    // and a second copy so we don't need an extra mask or static initializer
    23, 125, 161, 52, 103, 117, 70, 37, 247, 101, 203, 169, 124, 126, 44, 123,
    152, 238, 145, 45, 171, 114, 253, 10, 192, 136, 4, 157, 249, 30, 35, 72,
    175, 63, 77, 90, 181, 16, 96, 111, 133, 104, 75, 162, 93, 56, 66, 240,
    8, 50, 84, 229, 49, 210, 173, 239, 141, 1, 87, 18, 2, 198, 143, 57,
    225, 160, 58, 217, 168, 206, 245, 204, 199, 6, 73, 60, 20, 230, 211, 233,
    94, 200, 88, 9, 74, 155, 33, 15, 219, 130, 226, 202, 83, 236, 42, 172,
    165, 218, 55, 222, 46, 107, 98, 154, 109, 67, 196, 178, 127, 158, 13, 243,
    65, 79, 166, 248, 25, 224, 115, 80, 68, 51, 184, 128, 232, 208, 151, 122,
    26, 212, 105, 43, 179, 213, 235, 148, 146, 89, 14, 195, 28, 78, 112, 76,
    250, 47, 24, 251, 140, 108, 186, 190, 228, 170, 183, 139, 39, 188, 244, 246,
    132, 48, 119, 144, 180, 138, 134, 193, 82, 182, 120, 121, 86, 220, 209, 3,
    91, 241, 149, 85, 205, 150, 113, 216, 31, 100, 41, 164, 177, 214, 153, 231,
    38, 71, 185, 174, 97, 201, 29, 95, 7, 92, 54, 254, 191, 118, 34, 221,
    131, 11, 163, 99, 234, 81, 227, 147, 156, 176, 17, 142, 69, 12, 110, 62,
    27, 255, 0, 194, 59, 116, 242, 252, 19, 21, 187, 53, 207, 129, 64, 135,
    61, 40, 167, 237, 102, 223, 106, 159, 197, 189, 215, 137, 36, 32, 22, 5,
    };

   float u,v,w;
   float n000,n001,n010,n011,n100,n101,n110,n111;
   float n00,n01,n10,n11;
   float n0,n1;

   unsigned int x_mask = (x_wrap-1) & 255;
   unsigned int y_mask = (y_wrap-1) & 255;
   unsigned int z_mask = (z_wrap-1) & 255;
   int px = stb__perlin_fastfloor(x);
   int py = stb__perlin_fastfloor(y);
   int pz = stb__perlin_fastfloor(z);
   int x0 = px & x_mask, x1 = (px+1) & x_mask;
   int y0 = py & y_mask, y1 = (py+1) & y_mask;
   int z0 = pz & z_mask, z1 = (pz+1) & z_mask;
   int r0,r1, r00,r01,r10,r11;

   #define stb__perlin_ease(a)   (((a*6-15)*a + 10) * a * a * a)

   x -= px; u = stb__perlin_ease(x);
   y -= py; v = stb__perlin_ease(y);
   z -= pz; w = stb__perlin_ease(z);

   r0 = stb__perlin_randtab[x0];
   r1 = stb__perlin_randtab[x1];

   r00 = stb__perlin_randtab[r0+y0];
   r01 = stb__perlin_randtab[r0+y1];
   r10 = stb__perlin_randtab[r1+y0];
   r11 = stb__perlin_randtab[r1+y1];

   n000 = stb__perlin_grad(stb__perlin_randtab[r00+z0], x  , y  , z   );
   n001 = stb__perlin_grad(stb__perlin_randtab[r00+z1], x  , y  , z-1 );
   n010 = stb__perlin_grad(stb__perlin_randtab[r01+z0], x  , y-1, z   );
   n011 = stb__perlin_grad(stb__perlin_randtab[r01+z1], x  , y-1, z-1 );
   n100 = stb__perlin_grad(stb__perlin_randtab[r10+z0], x-1, y  , z   );
   n101 = stb__perlin_grad(stb__perlin_randtab[r10+z1], x-1, y  , z-1 );
   n110 = stb__perlin_grad(stb__perlin_randtab[r11+z0], x-1, y-1, z   );
   n111 = stb__perlin_grad(stb__perlin_randtab[r11+z1], x-1, y-1, z-1 );

   n00 = stb__perlin_lerp(n000,n001,w);
   n01 = stb__perlin_lerp(n010,n011,w);
   n10 = stb__perlin_lerp(n100,n101,w);
   n11 = stb__perlin_lerp(n110,n111,w);

   n0 = stb__perlin_lerp(n00,n01,v);
   n1 = stb__perlin_lerp(n10,n11,v);

   return stb__perlin_lerp(n0,n1,u);
}

inline float stb_perlin_ridge_noise3(float x, float y, float z,float lacunarity, float gain, float offset, int octaves,int x_wrap, int y_wrap, int z_wrap)
{
   int i;
   float frequency = 1.0f;
   float prev = 1.0f;
   float amplitude = 0.5f;
   float sum = 0.0f;

   for (i = 0; i < octaves; i++) {
      float r = (float)(stb_perlin_noise3(x*frequency,y*frequency,z*frequency,x_wrap,y_wrap,z_wrap));
      r = r<0 ? -r : r; // fabs()
      r = offset - r;
      r = r*r;
      sum += r*amplitude*prev;
      prev = r;
      frequency *= lacunarity;
      amplitude *= gain;
   }
   return sum;
}

inline float stb_perlin_fbm_noise3(float x, float y, float z,float lacunarity, float gain, int octaves,int x_wrap, int y_wrap, int z_wrap)
{
   int i;
   float frequency = 1.0f;
   float amplitude = 1.0f;
   float sum = 0.0f;

   for (i = 0; i < octaves; i++) {
      sum += stb_perlin_noise3(x*frequency,y*frequency,z*frequency,x_wrap,y_wrap,z_wrap)*amplitude;
      frequency *= lacunarity;
      amplitude *= gain;
   }
   return sum;
}

inline float stb_perlin_turbulence_noise3(float x, float y, float z, float lacunarity, float gain, int octaves,int x_wrap, int y_wrap, int z_wrap)
{
   int i;
   float frequency = 1.0f;
   float amplitude = 1.0f;
   float sum = 0.0f;

   for (i = 0; i < octaves; i++) {
      float r = stb_perlin_noise3(x*frequency,y*frequency,z*frequency,x_wrap,y_wrap,z_wrap)*amplitude;
      r = r<0 ? -r : r; // fabs()
      sum += r;
      frequency *= lacunarity;
      amplitude *= gain;
   }
   return sum;
}
// clang-format on

// adapeted  stb_perlin.h
inline float perlin_noise(const vec3f& p, const vec3i& wrap) {
    return stb_perlin_noise3(p.x, p.y, p.z, wrap.x, wrap.y, wrap.z);
}

// adapeted  stb_perlin.h
inline float perlin_ridge_noise(const vec3f& p, float lacunarity, float gain,
    float offset, int octaves, const vec3i& wrap) {
    return stb_perlin_ridge_noise3(p.x, p.y, p.z, lacunarity, gain, offset,
        octaves, wrap.x, wrap.y, wrap.z);
}

// adapeted  stb_perlin.h
inline float perlin_fbm_noise(const vec3f& p, float lacunarity, float gain,
    int octaves, const vec3i& wrap) {
    return stb_perlin_fbm_noise3(
        p.x, p.y, p.z, lacunarity, gain, octaves, wrap.x, wrap.y, wrap.z);
}

// adapeted  stb_perlin.h
inline float perlin_turbulence_noise(const vec3f& p, float lacunarity,
    float gain, int octaves, const vec3i& wrap) {
    return stb_perlin_turbulence_noise3(
        p.x, p.y, p.z, lacunarity, gain, octaves, wrap.x, wrap.y, wrap.z);
}

}  // namespace yocto

#endif
