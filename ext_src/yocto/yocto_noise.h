//
// # Yocto/Noise: Noise functions
//
// Yocto/Noise provides a Perlin noise implementation.
// Yocto/Noise is implemented in `yocto_noise.h`.
// The noise implementation is a standalone port of `stb_perlin.h`,
// released in the public domain.
// We expect the implementation to change in the next releases.
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

#ifndef _YOCTO_NOISE_H_
#define _YOCTO_NOISE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <array>

#include "yocto_math.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// Using directives
using std::array;

}  // namespace yocto

// -----------------------------------------------------------------------------
// PERLIN NOISE FUNCTION
// -----------------------------------------------------------------------------
namespace yocto {

// Compute the revised Perlin noise function with returned values in the range
// [0,1]. Wrap provides a wrapping noise but must be power of two (wraps at 256
// anyway).
inline float perlin_noise(float p, int wrap = 0);
inline float perlin_noise(const vec2f& p, const vec2i& wrap = {0, 0});
inline float perlin_noise(const vec3f& p, const vec3i& wrap = {0, 0, 0});
inline float perlin_noise(const vec4f& p, const vec4i& wrap = {0, 0, 0, 0});

// Fractal noise variations. Good values are obtained with
// octaves=6 (numerber of noise calls),
// lacunarity=~2.0 (spacing between successive octaves: 2.0 for warpping),
// gain=0.5 (relative weighting applied to each successive octave),
// offset=1.0 (used to invert the ridges).
inline float perlin_ridge(const vec3f& p, float lacunarity = 2,
    float gain = 0.5, int octaves = 6, float offset = 1,
    const vec3i& wrap = {0, 0, 0});
inline float perlin_fbm(const vec3f& p, float lacunarity = 2, float gain = 0.5,
    int octaves = 6, const vec3i& wrap = {0, 0, 0});
inline float perlin_turbulence(const vec3f& p, float lacunarity = 2,
    float gain = 0.5, int octaves = 6, const vec3i& wrap = {0, 0, 0});

}  // namespace yocto

// -----------------------------------------------------------------------------
//
//
// IMPLEMENTATION
//
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PERLIN NOISE
// -----------------------------------------------------------------------------
namespace yocto {

// Code to generate permutation tables
// import random, textwrap
// permutation = [i for i in range(256)]
// random.seed(12876289)
// random.shuffle(permutation)
// print('inline const unsigned char __perlin_permutation[512] = {')
// print('  // clang-format off')
// out = ''
// for i in permutation:
//     out += str(i)+', '
// out = textwrap.wrap(out, 78)
// for line in out:
//     print(' ', line)
// print('  // repeat')
// for line in out:
//     print(' ', line)
// print('  // clang-format on')
// print('};')

// not same permutation table as Perlin's reference to avoid copyright issues;
constexpr auto __perlin_permutation = array<unsigned char, 512>{
    // clang-format off
  124, 56, 113, 233, 69, 219, 244, 236, 246, 92, 26, 82, 218, 176, 78, 143, 238,
  145, 119, 38, 132, 112, 51, 7, 27, 81, 158, 241, 98, 37, 91, 230, 198, 205,
  178, 149, 152, 140, 190, 193, 234, 157, 6, 239, 249, 16, 155, 75, 162, 90,
  114, 43, 55, 28, 232, 183, 31, 12, 177, 74, 148, 186, 169, 20, 116, 45, 103,
  242, 135, 66, 163, 8, 221, 63, 102, 121, 39, 58, 201, 35, 217, 120, 144, 3,
  47, 203, 153, 213, 105, 210, 197, 79, 160, 34, 76, 248, 187, 180, 89, 17, 181,
  252, 60, 24, 21, 71, 164, 0, 138, 33, 188, 195, 223, 128, 65, 229, 247, 189,
  129, 88, 204, 42, 54, 32, 165, 118, 80, 96, 150, 199, 130, 64, 141, 156, 94,
  222, 255, 216, 194, 182, 25, 139, 111, 83, 108, 226, 227, 122, 220, 29, 52,
  207, 5, 174, 77, 133, 191, 67, 200, 4, 192, 250, 161, 172, 59, 117, 127, 136,
  225, 106, 251, 10, 154, 240, 171, 179, 126, 100, 19, 70, 2, 97, 159, 73, 104,
  53, 184, 137, 101, 72, 22, 185, 211, 243, 49, 175, 170, 93, 57, 62, 30, 131,
  115, 110, 46, 208, 11, 231, 13, 50, 254, 125, 237, 87, 206, 84, 86, 196, 167,
  41, 1, 151, 212, 224, 99, 147, 23, 40, 134, 95, 253, 123, 85, 235, 107, 142,
  44, 215, 146, 9, 48, 173, 168, 214, 68, 18, 15, 61, 202, 245, 36, 228, 109,
  209, 166, 14,
  // repeat
  124, 56, 113, 233, 69, 219, 244, 236, 246, 92, 26, 82, 218, 176, 78, 143, 238,
  145, 119, 38, 132, 112, 51, 7, 27, 81, 158, 241, 98, 37, 91, 230, 198, 205,
  178, 149, 152, 140, 190, 193, 234, 157, 6, 239, 249, 16, 155, 75, 162, 90,
  114, 43, 55, 28, 232, 183, 31, 12, 177, 74, 148, 186, 169, 20, 116, 45, 103,
  242, 135, 66, 163, 8, 221, 63, 102, 121, 39, 58, 201, 35, 217, 120, 144, 3,
  47, 203, 153, 213, 105, 210, 197, 79, 160, 34, 76, 248, 187, 180, 89, 17, 181,
  252, 60, 24, 21, 71, 164, 0, 138, 33, 188, 195, 223, 128, 65, 229, 247, 189,
  129, 88, 204, 42, 54, 32, 165, 118, 80, 96, 150, 199, 130, 64, 141, 156, 94,
  222, 255, 216, 194, 182, 25, 139, 111, 83, 108, 226, 227, 122, 220, 29, 52,
  207, 5, 174, 77, 133, 191, 67, 200, 4, 192, 250, 161, 172, 59, 117, 127, 136,
  225, 106, 251, 10, 154, 240, 171, 179, 126, 100, 19, 70, 2, 97, 159, 73, 104,
  53, 184, 137, 101, 72, 22, 185, 211, 243, 49, 175, 170, 93, 57, 62, 30, 131,
  115, 110, 46, 208, 11, 231, 13, 50, 254, 125, 237, 87, 206, 84, 86, 196, 167,
  41, 1, 151, 212, 224, 99, 147, 23, 40, 134, 95, 253, 123, 85, 235, 107, 142,
  44, 215, 146, 9, 48, 173, 168, 214, 68, 18, 15, 61, 202, 245, 36, 228, 109,
  209, 166, 14,
    // clang-format on
};

inline float perlin_noise(float p, int w) {
  auto ease   = [](float a) { return ((a * 6 - 15) * a + 10) * a * a * a; };
  auto ifloor = [](float a) -> int {
    int ai = (int)a;
    return (a < ai) ? ai - 1 : ai;
  };
  auto grad = [m = (w - 1) & 255](int i, float f) -> float {
    auto& _p   = __perlin_permutation;
    auto  hash = (int)_p[i & m];
    auto  h    = hash & 15;
    auto  grad = 1.0f + (h & 7);     // Gradient value 1.0, 2.0, ..., 8.0
    if ((h & 8) != 0) grad = -grad;  // and a random sign for the gradient
    return (grad * f);               // Multiply the gradient with the distance
  };

  auto i = ifloor(p);
  auto f = p - i;

  auto u = ease(f);

  auto n0 = grad(i + 0, f + 0);
  auto n1 = grad(i + 1, f - 1);

  return lerp(n0, n1, u) * 0.5f + 0.5f;
}

inline float perlin_noise(const vec2f& p, const vec2i& w) {
  auto ease   = [](float a) { return ((a * 6 - 15) * a + 10) * a * a * a; };
  auto ifloor = [](float a) -> int {
    auto ai = (int)a;
    return (a < ai) ? ai - 1 : ai;
  };
  auto grad = [m = vec2i{(w.x - 1) & 255, (w.y - 1) & 255}](
                  const vec2i& i, const vec2f& f) -> float {
    auto& _p   = __perlin_permutation;
    auto  hash = (int)_p[_p[i.x & m.x] + i.y & m.y];
    auto  h    = hash & 7;           // Convert low 3 bits of hash code
    float u    = h < 4 ? f.x : f.y;  // into 8 simple gradient directions,
    float v = h < 4 ? f.y : f.x;  // and compute the dot product with (f.x,f.y).
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -2 * v : 2 * v);
  };

  auto i = vec2i{ifloor(p.x), ifloor(p.y)};
  auto f = vec2f{p.x - i.x, p.y - i.y};
  auto u = vec2f{ease(f.x), ease(f.y)};

  auto n00 = grad({i.x + 0, i.y + 0}, {f.x + 0, f.y + 0});
  auto n01 = grad({i.x + 0, i.y + 1}, {f.x + 0, f.y - 1});
  auto n10 = grad({i.x + 1, i.y + 0}, {f.x - 1, f.y + 0});
  auto n11 = grad({i.x + 1, i.y + 1}, {f.x - 1, f.y - 1});

  auto n0 = lerp(n00, n01, u.y);
  auto n1 = lerp(n10, n11, u.y);

  return lerp(n0, n1, u.x) * 0.5f + 0.5f;
}

inline float perlin_noise(const vec3f& p, const vec3i& w) {
  auto ease   = [](float a) { return ((a * 6 - 15) * a + 10) * a * a * a; };
  auto ifloor = [](float a) -> int {
    int ai = (int)a;
    return (a < ai) ? ai - 1 : ai;
  };
  auto grad = [m = vec3i{(w.x - 1) & 255, (w.y - 1) & 255, (w.z - 1) & 255}](
                  const vec3i& i, const vec3f& f) -> float {
    auto& _p   = __perlin_permutation;
    auto  hash = (int)_p[_p[_p[i.x & m.x] + i.y & m.y] + i.z & m.z];
    // Convert low 4 bits of hash code into 12 simple
    // gradient directions, and compute dot product.
    auto h = hash & 15;
    auto u = h < 8 ? f.x : f.y;
    auto v = h < 4 ? f.y : h == 12 || h == 14 ? f.x : f.z;
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -v : v);
  };

  auto i = vec3i{ifloor(p.x), ifloor(p.y), ifloor(p.z)};
  auto f = vec3f{p.x - i.x, p.y - i.y, p.z - i.z};
  auto u = vec3f{ease(f.x), ease(f.y), ease(f.z)};

  auto n000 = grad({i.x + 0, i.y + 0, i.z + 0}, {f.x + 0, f.y + 0, f.z + 0});
  auto n001 = grad({i.x + 0, i.y + 0, i.z + 1}, {f.x + 0, f.y + 0, f.z - 1});
  auto n010 = grad({i.x + 0, i.y + 1, i.z + 0}, {f.x + 0, f.y - 1, f.z + 0});
  auto n011 = grad({i.x + 0, i.y + 1, i.z + 1}, {f.x + 0, f.y - 1, f.z - 1});
  auto n100 = grad({i.x + 1, i.y + 0, i.z + 0}, {f.x - 1, f.y + 0, f.z + 0});
  auto n101 = grad({i.x + 1, i.y + 0, i.z + 1}, {f.x - 1, f.y + 0, f.z - 1});
  auto n110 = grad({i.x + 1, i.y + 1, i.z + 0}, {f.x - 1, f.y - 1, f.z + 0});
  auto n111 = grad({i.x + 1, i.y + 1, i.z + 1}, {f.x - 1, f.y - 1, f.z - 1});

  auto n00 = lerp(n000, n001, u.z);
  auto n01 = lerp(n010, n011, u.z);
  auto n10 = lerp(n100, n101, u.z);
  auto n11 = lerp(n110, n111, u.z);

  auto n0 = lerp(n00, n01, u.y);
  auto n1 = lerp(n10, n11, u.y);

  return lerp(n0, n1, u.x) * 0.5f + 0.5f;
}

inline float perlin_noise(const vec4f& p, const vec4i& w) {
  auto ease   = [](float a) { return ((a * 6 - 15) * a + 10) * a * a * a; };
  auto ifloor = [](float a) -> int {
    int ai = (int)a;
    return (a < ai) ? ai - 1 : ai;
  };
  auto grad = [m = vec4i{(w.x - 1) & 255, (w.y - 1) & 255, (w.z - 1) & 255,
                   (w.w - 1) & 255}](const vec4i& i, const vec4f& f) -> float {
    auto& _p = __perlin_permutation;
    auto  hash =
        (int)_p[_p[_p[_p[i.x & m.x] + i.y & m.y] + i.z & m.y] + i.w & m.w];
    // Convert low 5 bits of hash code into 32 simple
    // gradient directions, and compute dot product.
    auto h = hash & 31;
    auto u = h < 24 ? f.x : f.y;
    auto v = h < 16 ? f.y : f.z;
    auto w = h < 8 ? f.z : f.w;
    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -v : v) +
           ((h & 4) != 0 ? -w : w);
  };

  auto i = vec4i{ifloor(p.x), ifloor(p.y), ifloor(p.z), ifloor(p.w)};
  auto f = vec4f{p.x - i.x, p.y - i.y, p.z - i.z, p.w - i.w};
  auto u = vec4f{ease(f.x), ease(f.y), ease(f.z), ease(f.z)};

  auto n0000 = grad({i.x + 0, i.y + 0, i.z + 0, i.w + 0},
      {f.x + 0, f.y + 0, f.z + 0, f.z + 0});
  auto n0001 = grad({i.x + 0, i.y + 0, i.z + 0, i.w + 1},
      {f.x + 0, f.y + 0, f.z + 0, f.z - 1});
  auto n0010 = grad({i.x + 0, i.y + 0, i.z + 1, i.w + 0},
      {f.x + 0, f.y + 0, f.z - 1, f.z + 0});
  auto n0011 = grad({i.x + 0, i.y + 0, i.z + 1, i.w + 1},
      {f.x + 0, f.y + 0, f.z - 1, f.z - 1});
  auto n0100 = grad({i.x + 0, i.y + 1, i.z + 0, i.w + 0},
      {f.x + 0, f.y - 1, f.z + 0, f.z + 0});
  auto n0101 = grad({i.x + 0, i.y + 1, i.z + 0, i.w + 1},
      {f.x + 0, f.y - 1, f.z + 0, f.z - 1});
  auto n0110 = grad({i.x + 0, i.y + 1, i.z + 1, i.w + 0},
      {f.x + 0, f.y - 1, f.z - 1, f.z + 0});
  auto n0111 = grad({i.x + 0, i.y + 1, i.z + 1, i.w + 1},
      {f.x + 0, f.y - 1, f.z - 1, f.z - 1});
  auto n1000 = grad({i.x + 1, i.y + 0, i.z + 0, i.w + 0},
      {f.x - 1, f.y + 0, f.z + 0, f.z + 0});
  auto n1001 = grad({i.x + 1, i.y + 0, i.z + 0, i.w + 1},
      {f.x - 1, f.y + 0, f.z + 0, f.z - 1});
  auto n1010 = grad({i.x + 1, i.y + 0, i.z + 1, i.w + 0},
      {f.x - 1, f.y + 0, f.z - 1, f.z + 0});
  auto n1011 = grad({i.x + 1, i.y + 0, i.z + 1, i.w + 1},
      {f.x - 1, f.y + 0, f.z - 1, f.z - 1});
  auto n1100 = grad({i.x + 1, i.y + 1, i.z + 0, i.w + 0},
      {f.x - 1, f.y - 1, f.z + 0, f.z + 0});
  auto n1101 = grad({i.x + 1, i.y + 1, i.z + 0, i.w + 1},
      {f.x - 1, f.y - 1, f.z + 0, f.z - 1});
  auto n1110 = grad({i.x + 1, i.y + 1, i.z + 1, i.w + 0},
      {f.x - 1, f.y - 1, f.z - 1, f.z + 0});
  auto n1111 = grad({i.x + 1, i.y + 1, i.z + 1, i.w + 1},
      {f.x - 1, f.y - 1, f.z - 1, f.z - 1});

  auto n000 = lerp(n0000, n0001, u.w);
  auto n001 = lerp(n0010, n0011, u.w);
  auto n010 = lerp(n0100, n0101, u.w);
  auto n011 = lerp(n0110, n0111, u.w);
  auto n100 = lerp(n1000, n1001, u.w);
  auto n101 = lerp(n1010, n1011, u.w);
  auto n110 = lerp(n1100, n1101, u.w);
  auto n111 = lerp(n1110, n1111, u.w);

  auto n00 = lerp(n000, n001, u.z);
  auto n01 = lerp(n010, n011, u.z);
  auto n10 = lerp(n100, n101, u.z);
  auto n11 = lerp(n110, n111, u.z);

  auto n0 = lerp(n00, n01, u.y);
  auto n1 = lerp(n10, n11, u.y);

  return lerp(n0, n1, u.x) * 0.5f + 0.5f;
}

// ridge
inline float perlin_ridge(const vec3f& p, float lacunarity, float gain,
    int octaves, float offset, const vec3i& wrap) {
  auto frequency = 1.0f;
  auto prev      = 1.0f;
  auto amplitude = 0.5f;
  auto sum       = 0.0f;
  for (auto i : range(octaves)) {
    auto r = offset - abs(perlin_noise(p * frequency, wrap) * 2 - 1);
    r      = r * r;
    sum += r * amplitude * prev;
    prev = r;
    frequency *= lacunarity;
    amplitude *= gain;
  }
  return sum;
}

// fmb
inline float perlin_fbm(const vec3f& p, float lacunarity, float gain,
    int octaves, const vec3i& wrap) {
  auto frequency = 1.0f;
  auto amplitude = 1.0f;
  auto sum       = 0.0f;
  for (auto i : range(octaves)) {
    sum += perlin_noise(p * frequency, wrap) * amplitude;
    frequency *= lacunarity;
    amplitude *= gain;
  }
  return sum;
}

// turbulence
inline float perlin_turbulence(const vec3f& p, float lacunarity, float gain,
    int octaves, const vec3i& wrap) {
  auto frequency = 1.0f;
  auto amplitude = 1.0f;
  auto sum       = 0.0f;
  for (auto i : range(octaves)) {
    sum += abs(perlin_noise(p * frequency, wrap) * 2 - 1) * amplitude;
    frequency *= lacunarity;
    amplitude *= gain;
  }
  return sum;
}

}  // namespace yocto

#endif
