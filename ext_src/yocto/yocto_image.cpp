//
// Implementation for Yocto/Image.
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

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_image.h"
#include "yocto_random.h"

#if !defined(_WIN32) && !defined(_WIN64)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif

// #ifndef _clang_analyzer__

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ext/stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "ext/stb_image_resize.h"

#define TINYEXR_IMPLEMENTATION
#include "ext/tinyexr.h"

// #endif

#if !defined(_WIN32) && !defined(_WIN64)
#pragma GCC diagnostic pop
#endif

#include <memory>

#include "ext/filesystem.hpp"
namespace fs = ghc::filesystem;

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR COLOR UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Approximate color of blackbody radiation from wavelength in nm.
vec3f blackbody_to_rgb(float temperature) {
  // https://github.com/neilbartlett/color-temperature
  auto rgb = zero3f;
  if ((temperature / 100) < 66) {
    rgb.x = 255;
  } else {
    // a + b x + c Log[x] /.
    // {a -> 351.97690566805693`,
    // b -> 0.114206453784165`,
    // c -> -40.25366309332127
    // x -> (kelvin/100) - 55}
    rgb.x = (temperature / 100) - 55;
    rgb.x = 351.97690566805693f + 0.114206453784165f * rgb.x -
            40.25366309332127f * log(rgb.x);
    if (rgb.x < 0) rgb.x = 0;
    if (rgb.x > 255) rgb.x = 255;
  }

  if ((temperature / 100) < 66) {
    // a + b x + c Log[x] /.
    // {a -> -155.25485562709179`,
    // b -> -0.44596950469579133`,
    // c -> 104.49216199393888`,
    // x -> (kelvin/100) - 2}
    rgb.y = (temperature / 100) - 2;
    rgb.y = -155.25485562709179f - 0.44596950469579133f * rgb.y +
            104.49216199393888f * log(rgb.y);
    if (rgb.y < 0) rgb.y = 0;
    if (rgb.y > 255) rgb.y = 255;
  } else {
    // a + b x + c Log[x] /.
    // {a -> 325.4494125711974`,
    // b -> 0.07943456536662342`,
    // c -> -28.0852963507957`,
    // x -> (kelvin/100) - 50}
    rgb.y = (temperature / 100) - 50;
    rgb.y = 325.4494125711974f + 0.07943456536662342f * rgb.y -
            28.0852963507957f * log(rgb.y);
    if (rgb.y < 0) rgb.y = 0;
    if (rgb.y > 255) rgb.y = 255;
  }

  if ((temperature / 100) >= 66) {
    rgb.z = 255;
  } else {
    if ((temperature / 100) <= 20) {
      rgb.z = 0;
    } else {
      // a + b x + c Log[x] /.
      // {a -> -254.76935184120902`,
      // b -> 0.8274096064007395`,
      // c -> 115.67994401066147`,
      // x -> kelvin/100 - 10}
      rgb.z = (temperature / 100) - 10;
      rgb.z = -254.76935184120902f + 0.8274096064007395f * rgb.z +
              115.67994401066147f * log(rgb.z);
      if (rgb.z < 0) rgb.z = 0;
      if (rgb.z > 255) rgb.z = 255;
    }
  }

  return srgb_to_rgb(rgb / 255);
}

// Convert HSV to RGB
vec3f hsv_to_rgb(const vec3f& hsv) {
  // from Imgui.cpp
  auto h = hsv.x, s = hsv.y, v = hsv.z;
  if (hsv.y == 0) return {v, v, v};

  h       = fmod(h, 1.0f) / (60.0f / 360.0f);
  int   i = (int)h;
  float f = h - (float)i;
  float p = v * (1 - s);
  float q = v * (1 - s * f);
  float t = v * (1 - s * (1 - f));

  switch (i) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    case 5: return {v, p, q};
    default: return {v, p, q};
  }
}
vec3f rgb_to_hsv(const vec3f& rgb) {
  // from Imgui.cpp
  auto r = rgb.x, g = rgb.y, b = rgb.z;
  auto K = 0.f;
  if (g < b) {
    swap(g, b);
    K = -1;
  }
  if (r < g) {
    swap(r, g);
    K = -2 / 6.0f - K;
  }

  auto chroma = r - (g < b ? g : b);
  return {abs(K + (g - b) / (6 * chroma + 1e-20f)), chroma / (r + 1e-20f), r};
}

// RGB color space definition. Various predefined color spaces are listed below.
struct color_space_params {
  // Curve type
  enum struct curve_t {
    linear,
    gamma,
    linear_gamma,
    aces_cc,
    aces_cct,
    pq,
    hlg
  };
  // primaries
  vec2f red_chromaticity;    // xy chromaticity of the red primary
  vec2f green_chromaticity;  // xy chromaticity of the green primary
  vec2f blue_chromaticity;   // xy chromaticity of the blue primary
  vec2f white_chromaticity;  // xy chromaticity of the white point
  mat3f rgb_to_xyz_mat;      // matrix from rgb to xyz
  mat3f xyz_to_rgb_mat;      // matrix from xyz to rgb
  // tone curve
  curve_t curve_type;
  float   curve_gamma;  // gamma for power curves
  vec4f   curve_abcd;   // tone curve values for linear_gamma curves
};

// Compute the rgb -> xyz matrix from the color space definition
// Input: red, green, blue, white (x,y) chromoticities
// Algorithm from: SMPTE Recommended Practice RP 177-1993
// http://car.france3.mars.free.fr/HD/INA-%2026%20jan%2006/SMPTE%20normes%20et%20confs/rp177.pdf
static inline mat3f rgb_to_xyz_mat(
    const vec2f& rc, const vec2f& gc, const vec2f& bc, const vec2f& wc) {
  auto rgb = mat3f{
      {rc.x, rc.y, 1 - rc.x - rc.y},
      {gc.x, gc.y, 1 - gc.x - gc.y},
      {bc.x, bc.y, 1 - bc.x - bc.y},
  };
  auto w = vec3f{wc.x, wc.y, 1 - wc.x - wc.y};
  auto c = inverse(rgb) * vec3f{w.x / w.y, 1, w.z / w.y};
  return mat3f{c.x * rgb.x, c.y * rgb.y, c.z * rgb.z};
}

// Construct an RGB color space. Predefined color spaces below
static inline color_space_params get_color_scape_params(color_space space) {
  static auto make_linear_rgb_space = [](const vec2f& red, const vec2f& green,
                                          const vec2f& blue,
                                          const vec2f& white) {
    return color_space_params{red, green, blue, white,
        rgb_to_xyz_mat(red, green, blue, white),
        inverse(rgb_to_xyz_mat(red, green, blue, white)),
        color_space_params::curve_t::linear};
  };
  static auto make_gamma_rgb_space =
      [](const vec2f& red, const vec2f& green, const vec2f& blue,
          const vec2f& white, float gamma, const vec4f& curve_abcd = zero4f) {
        return color_space_params{red, green, blue, white,
            rgb_to_xyz_mat(red, green, blue, white),
            inverse(rgb_to_xyz_mat(red, green, blue, white)),
            curve_abcd == zero4f ? color_space_params::curve_t::gamma
                                 : color_space_params::curve_t::linear_gamma};
      };
  static auto make_other_rgb_space =
      [](const vec2f& red, const vec2f& green, const vec2f& blue,
          const vec2f& white, color_space_params::curve_t curve_type) {
        return color_space_params{red, green, blue, white,
            rgb_to_xyz_mat(red, green, blue, white),
            inverse(rgb_to_xyz_mat(red, green, blue, white)), curve_type};
      };

  // color space parameters
  // https://en.wikipedia.org/wiki/Rec._709
  static auto rgb_params = make_linear_rgb_space(
      {0.6400, 0.3300}, {0.3000, 0.6000}, {0.1500, 0.0600}, {0.3127, 0.3290});
  // https://en.wikipedia.org/wiki/Rec._709
  static auto srgb_params = make_gamma_rgb_space({0.6400, 0.3300},
      {0.3000, 0.6000}, {0.1500, 0.0600}, {0.3127, 0.3290}, 2.4,
      {1.055, 0.055, 12.92, 0.0031308});
  // https://en.wikipedia.org/wiki/Academy_Color_Encoding_System
  static auto aces2065_params = make_linear_rgb_space({0.7347, 0.2653},
      {0.0000, 1.0000}, {0.0001, -0.0770}, {0.32168, 0.33767});
  // https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
  static auto acescg_params = make_linear_rgb_space({0.7130, 0.2930},
      {0.1650, 0.8300}, {0.1280, +0.0440}, {0.32168, 0.33767});
  // https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
  static auto acescc_params = make_other_rgb_space({0.7130, 0.2930},
      {0.1650, 0.8300}, {0.1280, +0.0440}, {0.32168, 0.33767},
      color_space_params::curve_t::aces_cc);
  // https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
  static auto acescct_params = make_other_rgb_space({0.7130, 0.2930},
      {0.1650, 0.8300}, {0.1280, +0.0440}, {0.32168, 0.33767},
      color_space_params::curve_t::aces_cct);
  // https://en.wikipedia.org/wiki/Adobe_RGB_color_space
  static auto adobe_params = make_gamma_rgb_space({0.6400, 0.3300},
      {0.2100, 0.7100}, {0.1500, 0.0600}, {0.3127, 0.3290}, 2.19921875);
  // https://en.wikipedia.org/wiki/Rec._709
  static auto rec709_params = make_gamma_rgb_space({0.6400, 0.3300},
      {0.3000, 0.6000}, {0.1500, 0.0600}, {0.3127, 0.3290}, 1 / 0.45,
      {1.099, 0.099, 4.500, 0.018});
  // https://en.wikipedia.org/wiki/Rec._2020
  static auto rec2020_params = make_gamma_rgb_space({0.7080, 0.2920},
      {0.1700, 0.7970}, {0.1310, 0.0460}, {0.3127, 0.3290}, 1 / 0.45,
      {1.09929682680944, 0.09929682680944, 4.5, 0.018053968510807});
  // https://en.wikipedia.org/wiki/Rec._2020
  static auto rec2100pq_params = make_other_rgb_space({0.7080, 0.2920},
      {0.1700, 0.7970}, {0.1310, 0.0460}, {0.3127, 0.3290},
      color_space_params::curve_t::pq);
  // https://en.wikipedia.org/wiki/Rec._2020
  static auto rec2100hlg_params = make_other_rgb_space({0.7080, 0.2920},
      {0.1700, 0.7970}, {0.1310, 0.0460}, {0.3127, 0.3290},
      color_space_params::curve_t::hlg);
  // https://en.wikipedia.org/wiki/DCI-P3
  static auto p3dci_params = make_gamma_rgb_space({0.6800, 0.3200},
      {0.2650, 0.6900}, {0.1500, 0.0600}, {0.3140, 0.3510}, 1.6);
  // https://en.wikipedia.org/wiki/DCI-P3
  static auto p3d60_params = make_gamma_rgb_space({0.6800, 0.3200},
      {0.2650, 0.6900}, {0.1500, 0.0600}, {0.32168, 0.33767}, 1.6);
  // https://en.wikipedia.org/wiki/DCI-P3
  static auto p3d65_params = make_gamma_rgb_space({0.6800, 0.3200},
      {0.2650, 0.6900}, {0.1500, 0.0600}, {0.3127, 0.3290}, 1.6);
  // https://en.wikipedia.org/wiki/DCI-P3
  static auto p3display_params = make_gamma_rgb_space({0.6800, 0.3200},
      {0.2650, 0.6900}, {0.1500, 0.0600}, {0.3127, 0.3290}, 2.4,
      {1.055, 0.055, 12.92, 0.0031308});
  // https://en.wikipedia.org/wiki/ProPhoto_RGB_color_space
  static auto prophoto_params = make_gamma_rgb_space({0.7347, 0.2653},
      {0.1596, 0.8404}, {0.0366, 0.0001}, {0.3457, 0.3585}, 1.8,
      {1, 0, 16, 0.001953125});

  // return values;
  switch (space) {
    case color_space::rgb: return rgb_params;
    case color_space::srgb: return srgb_params;
    case color_space::adobe: return adobe_params;
    case color_space::prophoto: return prophoto_params;
    case color_space::rec709: return rec709_params;
    case color_space::rec2020: return rec2020_params;
    case color_space::rec2100pq: return rec2100pq_params;
    case color_space::rec2100hlg: return rec2100hlg_params;
    case color_space::aces2065: return aces2065_params;
    case color_space::acescg: return acescg_params;
    case color_space::acescc: return acescc_params;
    case color_space::acescct: return acescct_params;
    case color_space::p3dci: return p3dci_params;
    case color_space::p3d60: return p3d60_params;
    case color_space::p3d65: return p3d65_params;
    case color_space::p3display: return p3display_params;
  }
}

// gamma to linear
static inline float gamma_display_to_linear(float x, float gamma) {
  return pow(x, gamma);
};
static inline float gamma_linear_to_display(float x, float gamma) {
  return pow(x, 1 / gamma);
};

// https://en.wikipedia.org/wiki/Rec._709
static inline float gamma_display_to_linear(
    float x, float gamma, const vec4f& abcd) {
  auto& [a, b, c, d] = abcd;
  if (x < 1 / d) {
    return x / c;
  } else {
    return pow((x + b) / a, gamma);
  }
};
static inline float gamma_linear_to_display(
    float x, float gamma, const vec4f& abcd) {
  auto& [a, b, c, d] = abcd;
  if (x < d) {
    return x * c;
  } else {
    return a * pow(x, 1 / gamma) - b;
  }
};

// https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
static inline float acescc_display_to_linear(float x) {
  if (x < -0.3013698630f) {  // (9.72-15)/17.52
    return (exp2(x * 17.52f - 9.72f) - exp2(-16.0f)) * 2;
  } else if (x < (log2(65504.0f) + 9.72f) / 17.52f) {
    return exp2(x * 17.52f - 9.72f);
  } else {  // (in >= (log2(65504)+9.72)/17.52)
    return 65504.0f;
  }
}
// https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
static inline float acescct_display_to_linear(float x) {
  if (x < 0.155251141552511f) {
    return (x - 0.0729055341958355f) / 10.5402377416545f;
  } else {
    return exp2(x * 17.52f - 9.72f);
  }
}
// https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
static inline float acescc_linear_to_display(float x) {
  if (x <= 0) {
    return -0.3584474886f;  // =(log2( pow(2.,-16.))+9.72)/17.52
  } else if (x < exp2(-15.0f)) {
    return (log2(exp2(-16.0f) + x * 0.5f) + 9.72f) / 17.52f;
  } else {  // (in >= pow(2.,-15))
    return (log2(x) + 9.72f) / 17.52f;
  }
}
// https://en.wikipedia.org/wiki/Academy_Color_Encoding_Systemx
static inline float acescct_linear_to_display(float x) {
  if (x <= 0.0078125f) {
    return 10.5402377416545f * x + 0.0729055341958355f;
  } else {
    return (log2(x) + 9.72f) / 17.52f;
  }
}

// https://en.wikipedia.org/wiki/High-dynamic-range_video#Perceptual_Quantizer
// https://github.com/ampas/aces-dev/blob/master/transforms/ctl/lib/ACESlib.Utilities_Color.ctl
// In PQ, we assume that the linear luminance in [0,1] corresponds to
// [0,10000] cd m^2
static inline float pq_display_to_linear(float x) {
  auto Np = pow(x, 1 / 78.84375f);
  auto L  = max(Np - 0.8359375f, 0.0f);
  L       = L / (18.8515625f - 18.6875f * Np);
  L       = pow(L, 1 / 0.1593017578125f);
  return L;
}
static inline float pq_linear_to_display(float x) {
  return pow((0.8359375 + 18.8515625 * pow(x, 0.1593017578125f)) /
                 (1 + 18.6875f * pow(x, 0.1593017578125f)),
      78.84375f);
}
// https://en.wikipedia.org/wiki/High-dynamic-range_video#Perceptual_Quantizer
// In HLG, we assume that the linear luminance in [0,1] corresponds to
// [0,1000] cd m^2. Note that the version we report here is scaled in [0,1]
// range for nominal luminance. But HLG was initially defined in the [0,12]
// range where it maps 1 to 0.5 and 12 to 1. For use in HDR tonemapping that is
// likely a better range to use.
static inline float hlg_display_to_linear(float x) {
  if (x < 0.5f) {
    return 3 * 3 * x * x;
  } else {
    return (exp((x - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12;
  }
}
static inline float hlg_linear_to_display(float x) {
  if (x < 1 / 12.0f) {
    return sqrt(3 * x);
  } else {
    return 0.17883277f * log(12 * x - 0.28466892f) + 0.55991073f;
  }
}

// Conversion to/from xyz
vec3f color_to_xyz(const vec3f& col, color_space from) {
  auto space = get_color_scape_params(from);
  auto rgb   = col;
  if (space.curve_type == color_space_params::curve_t::linear) {
    // do nothing
  } else if (space.curve_type == color_space_params::curve_t::gamma) {
    rgb = {
        gamma_linear_to_display(rgb.x, space.curve_gamma),
        gamma_linear_to_display(rgb.y, space.curve_gamma),
        gamma_linear_to_display(rgb.z, space.curve_gamma),
    };
  } else if (space.curve_type == color_space_params::curve_t::linear_gamma) {
    rgb = {
        gamma_linear_to_display(rgb.x, space.curve_gamma, space.curve_abcd),
        gamma_linear_to_display(rgb.y, space.curve_gamma, space.curve_abcd),
        gamma_linear_to_display(rgb.z, space.curve_gamma, space.curve_abcd),
    };
  } else if (space.curve_type == color_space_params::curve_t::aces_cc) {
    rgb = {
        acescc_linear_to_display(rgb.x),
        acescc_linear_to_display(rgb.y),
        acescc_linear_to_display(rgb.z),
    };
  } else if (space.curve_type == color_space_params::curve_t::aces_cct) {
    rgb = {
        acescct_linear_to_display(rgb.x),
        acescct_linear_to_display(rgb.y),
        acescct_linear_to_display(rgb.z),
    };
  } else if (space.curve_type == color_space_params::curve_t::pq) {
    rgb = {
        pq_linear_to_display(rgb.x),
        pq_linear_to_display(rgb.y),
        pq_linear_to_display(rgb.z),
    };
  } else if (space.curve_type == color_space_params::curve_t::hlg) {
    rgb = {
        hlg_linear_to_display(rgb.x),
        hlg_linear_to_display(rgb.y),
        hlg_linear_to_display(rgb.z),
    };
  } else {
    throw std::runtime_error("should not have gotten here");
  }
  return space.rgb_to_xyz_mat * rgb;
}
vec3f xyz_to_color(const vec3f& xyz, color_space to) {
  auto space = get_color_scape_params(to);
  auto rgb   = space.xyz_to_rgb_mat * xyz;
  if (space.curve_type == color_space_params::curve_t::linear) {
    // nothing
  } else if (space.curve_type == color_space_params::curve_t::gamma) {
    rgb = {
        gamma_display_to_linear(rgb.x, space.curve_gamma),
        gamma_display_to_linear(rgb.y, space.curve_gamma),
        gamma_display_to_linear(rgb.z, space.curve_gamma),
    };
  } else if (space.curve_type == color_space_params::curve_t::linear_gamma) {
    rgb = {
        gamma_display_to_linear(rgb.x, space.curve_gamma, space.curve_abcd),
        gamma_display_to_linear(rgb.y, space.curve_gamma, space.curve_abcd),
        gamma_display_to_linear(rgb.z, space.curve_gamma, space.curve_abcd),
    };
  } else if (space.curve_type == color_space_params::curve_t::aces_cc) {
    rgb = {
        acescc_display_to_linear(rgb.x),
        acescc_display_to_linear(rgb.y),
        acescc_display_to_linear(rgb.z),
    };
  } else if (space.curve_type == color_space_params::curve_t::aces_cct) {
    rgb = {
        acescct_display_to_linear(rgb.x),
        acescct_display_to_linear(rgb.y),
        acescct_display_to_linear(rgb.z),
    };
  } else if (space.curve_type == color_space_params::curve_t::pq) {
    rgb = {
        pq_display_to_linear(rgb.x),
        pq_display_to_linear(rgb.y),
        pq_display_to_linear(rgb.z),
    };
  } else if (space.curve_type == color_space_params::curve_t::hlg) {
    rgb = {
        hlg_display_to_linear(rgb.x),
        hlg_display_to_linear(rgb.y),
        hlg_display_to_linear(rgb.z),
    };
  } else {
    throw std::runtime_error("should not have gotten here");
  }
  return rgb;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR IMAGE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Splits an image into an array of regions
void make_regions(vector<image_region>& regions, const vec2i& size,
    int region_size, bool shuffled) {
  regions.clear();
  for (auto y = 0; y < size.y; y += region_size) {
    for (auto x = 0; x < size.x; x += region_size) {
      regions.push_back({{x, y},
          {min(x + region_size, size.x), min(y + region_size, size.y)}});
    }
  }
  if (shuffled) {
    auto rng = rng_state{};
    shuffle(regions, rng);
  }
}
vector<image_region> make_regions(
    const vec2i& size, int region_size, bool shuffled) {
  auto regions = vector<image_region>{};
  for (auto y = 0; y < size.y; y += region_size) {
    for (auto x = 0; x < size.x; x += region_size) {
      regions.push_back({{x, y},
          {min(x + region_size, size.x), min(y + region_size, size.y)}});
    }
  }
  if (shuffled) {
    auto rng = rng_state{};
    shuffle(regions, rng);
  }
  return regions;
}

// Conversion from/to floats.
void byte_to_float(image<vec4f>& fl, const image<vec4b>& bt) {
  fl.resize(bt.size());
  for (auto i = 0ull; i < fl.count(); i++) fl[i] = byte_to_float(bt[i]);
}
void float_to_byte(image<vec4b>& bt, const image<vec4f>& fl) {
  bt.resize(fl.size());
  for (auto i = 0ull; i < bt.count(); i++) bt[i] = float_to_byte(fl[i]);
}
image<vec4f> byte_to_float(const image<vec4b>& bt) {
  auto fl = image<vec4f>{bt.size()};
  for (auto i = 0ull; i < fl.count(); i++) fl[i] = byte_to_float(bt[i]);
  return fl;
}
image<vec4b> float_to_byte(const image<vec4f>& fl) {
  auto bt = image<vec4b>{fl.size()};
  for (auto i = 0ull; i < bt.count(); i++) bt[i] = float_to_byte(fl[i]);
  return bt;
}

// Conversion between linear and gamma-encoded images.
image<vec4f> srgb_to_rgb(const image<vec4f>& srgb) {
  auto rgb = image<vec4f>{srgb.size()};
  for (auto i = 0ull; i < rgb.count(); i++) rgb[i] = srgb_to_rgb(srgb[i]);
  return rgb;
}
image<vec4f> rgb_to_srgb(const image<vec4f>& rgb) {
  auto srgb = image<vec4f>{rgb.size()};
  for (auto i = 0ull; i < srgb.count(); i++) srgb[i] = rgb_to_srgb(rgb[i]);
  return srgb;
}
image<vec4f> srgb_to_rgb(const image<vec4b>& srgb) {
  auto rgb = image<vec4f>{srgb.size()};
  for (auto i = 0ull; i < rgb.count(); i++)
    rgb[i] = srgb_to_rgb(byte_to_float(srgb[i]));
  return rgb;
}
image<vec4b> rgb_to_srgbb(const image<vec4f>& rgb) {
  auto srgb = image<vec4b>{rgb.size()};
  for (auto i = 0ull; i < srgb.count(); i++)
    srgb[i] = float_to_byte(rgb_to_srgb(rgb[i]));
  return srgb;
}
void srgb_to_rgb(image<vec4f>& rgb, const image<vec4f>& srgb) {
  rgb.resize(srgb.size());
  for (auto i = 0ull; i < rgb.count(); i++) rgb[i] = srgb_to_rgb(srgb[i]);
}
void rgb_to_srgb(image<vec4f>& srgb, const image<vec4f>& rgb) {
  srgb.resize(rgb.size());
  for (auto i = 0ull; i < srgb.count(); i++) srgb[i] = rgb_to_srgb(rgb[i]);
}
void srgb_to_rgb(image<vec4f>& rgb, const image<vec4b>& srgb) {
  rgb.resize(srgb.size());
  for (auto i = 0ull; i < rgb.count(); i++)
    rgb[i] = srgb_to_rgb(byte_to_float(srgb[i]));
}
void rgb_to_srgb(image<vec4b>& srgb, const image<vec4f>& rgb) {
  srgb.resize(rgb.size());
  for (auto i = 0ull; i < srgb.count(); i++)
    srgb[i] = float_to_byte(rgb_to_srgb(rgb[i]));
}

// Filmic tonemapping
static vec3f tonemap_filmic(const vec3f& hdr_, bool accurate_fit = false) {
  if (!accurate_fit) {
    // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    auto hdr = hdr_ * 0.6f;  // brings it back to ACES range
    auto ldr = (hdr * hdr * 2.51f + hdr * 0.03f) /
               (hdr * hdr * 2.43f + hdr * 0.59f + 0.14f);
    return max(zero3f, ldr);
  } else {
    // https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    static const auto ACESInputMat = transpose(mat3f{
        {0.59719, 0.35458, 0.04823},
        {0.07600, 0.90834, 0.01566},
        {0.02840, 0.13383, 0.83777},
    });
    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    static const auto ACESOutputMat = transpose(mat3f{
        {1.60475, -0.53108, -0.07367},
        {-0.10208, 1.10813, -0.00605},
        {-0.00327, -0.07276, 1.07602},
    });
    // RRT => ODT
    auto RRTAndODTFit = [](const vec3f& v) -> vec3f {
      return (v * v + v * 0.0245786f - 0.000090537f) /
             (v * v * 0.983729f + v * 0.4329510f + 0.238081f);
    };

    auto ldr = ACESOutputMat * RRTAndODTFit(ACESInputMat * hdr_);
    return max(zero3f, ldr);
  }
}

static vec3f tonemap(const vec3f& hdr, const tonemap_params& params) {
  auto rgb = hdr;
  if (params.exposure != 0) rgb *= exp2(params.exposure);
  if (params.tint != vec3f{1, 1, 1}) rgb *= params.tint;
  if (params.contrast != 0.5f) rgb = contrast(rgb, params.contrast, 0.18f);
  if (params.logcontrast != 0.5f)
    rgb = logcontrast(rgb, params.logcontrast, 0.18f);
  if (params.saturation != 0.5f) rgb = saturate(rgb, params.saturation);
  if (params.filmic) rgb = tonemap_filmic(rgb);
  if (params.srgb) rgb = rgb_to_srgb(rgb);
  return rgb;
}
static vec4f tonemap(const vec4f& hdr, const tonemap_params& params) {
  return {tonemap(xyz(hdr), params), hdr.w};
}

// Apply exposure and filmic tone mapping
image<vec4f> tonemap(const image<vec4f>& hdr, const tonemap_params& params) {
  auto ldr = image<vec4f>{hdr.size()};
  for (auto i = 0ull; i < hdr.count(); i++) ldr[i] = tonemap(hdr[i], params);
  return ldr;
}
image<vec4b> tonemapb(const image<vec4f>& hdr, const tonemap_params& params) {
  auto ldr = image<vec4b>{hdr.size()};
  for (auto i = 0ull; i < hdr.count(); i++)
    ldr[i] = float_to_byte(tonemap(hdr[i], params));
  return ldr;
}
void tonemap(image<vec4f>& ldr, const image<vec4f>& hdr,
    const image_region& region, const tonemap_params& params) {
  for (auto j = region.min.y; j < region.max.y; j++)
    for (auto i = region.min.x; i < region.max.x; i++)
      ldr[{i, j}] = tonemap(hdr[{i, j}], params);
}

static vec3f colorgrade(const vec3f& ldr, const colorgrade_params& params) {
  auto rgb = ldr;
  if (params.contrast != 0.5f) {
    rgb = gain(ldr, 1 - params.contrast);
  }
  if (params.shadows != 0.5f || params.midtones != 0.5f ||
      params.highlights != 0.5f || params.shadows_color != vec3f{1, 1, 1} ||
      params.midtones_color != vec3f{1, 1, 1} ||
      params.highlights_color != vec3f{1, 1, 1}) {
    auto lift  = params.shadows_color;
    auto gamma = params.midtones_color;
    auto gain  = params.highlights_color;

    lift      = lift - mean(lift) + params.shadows - (float)0.5;
    gain      = gain - mean(gain) + params.highlights + (float)0.5;
    auto grey = gamma - mean(gamma) + params.midtones;
    gamma     = log(((float)0.5 - lift) / (gain - lift)) / log(grey);

    // apply_image
    auto lerp_value = clamp(pow(rgb, 1 / gamma), 0, 1);
    rgb             = gain * lerp_value + lift * (1 - lerp_value);
  }
  return rgb;
}
static vec4f colorgrade(const vec4f& ldr, const colorgrade_params& params) {
  return {colorgrade(xyz(ldr), params), ldr.w};
}

// Apply exposure and filmic tone mapping
image<vec4f> colorgrade(
    const image<vec4f>& ldr, const colorgrade_params& params) {
  auto corrected = image<vec4f>{ldr.size()};
  for (auto i = 0ull; i < ldr.count(); i++)
    corrected[i] = colorgrade(ldr[i], params);
  return corrected;
}
void colorgrade(image<vec4f>& corrected, const image<vec4f>& ldr,
    const image_region& region, const colorgrade_params& params) {
  for (auto j = region.min.y; j < region.max.y; j++)
    for (auto i = region.min.x; i < region.max.x; i++)
      corrected[{i, j}] = colorgrade(ldr[{i, j}], params);
}

// compute white balance
vec3f compute_white_balance(const image<vec4f>& img) {
  auto rgb = zero3f;
  for (auto& p : img) rgb += xyz(p);
  if (rgb == zero3f) return zero3f;
  return rgb / max(rgb);
}

static vec2i resize_size(const vec2i& img_size, const vec2i& size_) {
  auto size = size_;
  if (size == zero2i) {
    throw std::invalid_argument("bad image size in resize");
  }
  if (size.y == 0) {
    size.y = (int)round(size.x * (float)img_size.y / (float)img_size.x);
  } else if (size.x == 0) {
    size.x = (int)round(size.y * (float)img_size.x / (float)img_size.y);
  }
  return size;
}

image<vec4f> resize(const image<vec4f>& img, const vec2i& size_) {
  auto size    = resize_size(img.size(), size_);
  auto res_img = image<vec4f>{size};
  stbir_resize_float_generic((float*)img.data(), img.size().x, img.size().y,
      sizeof(vec4f) * img.size().x, (float*)res_img.data(), res_img.size().x,
      res_img.size().y, sizeof(vec4f) * res_img.size().x, 4, 3, 0,
      STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
  return res_img;
}
image<vec4b> resize(const image<vec4b>& img, const vec2i& size_) {
  auto size    = resize_size(img.size(), size_);
  auto res_img = image<vec4b>{size};
  stbir_resize_uint8_generic((byte*)img.data(), img.size().x, img.size().y,
      sizeof(vec4b) * img.size().x, (byte*)res_img.data(), res_img.size().x,
      res_img.size().y, sizeof(vec4b) * res_img.size().x, 4, 3, 0,
      STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
  return res_img;
}
void resize(
    image<vec4f>& res_img, const image<vec4f>& img, const vec2i& size_) {
  auto size = resize_size(img.size(), size_);
  res_img   = {size};
  stbir_resize_float_generic((float*)img.data(), img.size().x, img.size().y,
      sizeof(vec4f) * img.size().x, (float*)res_img.data(), res_img.size().x,
      res_img.size().y, sizeof(vec4f) * res_img.size().x, 4, 3, 0,
      STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
}
void resize(
    image<vec4b>& res_img, const image<vec4b>& img, const vec2i& size_) {
  auto size = resize_size(img.size(), size_);
  res_img   = {size};
  stbir_resize_uint8_generic((byte*)img.data(), img.size().x, img.size().y,
      sizeof(vec4b) * img.size().x, (byte*)res_img.data(), res_img.size().x,
      res_img.size().y, sizeof(vec4b) * res_img.size().x, 4, 3, 0,
      STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR IMAGE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Comvert a bump map to a normal map.
void bump_to_normal(image<vec4f>& norm, const image<vec4f>& img, float scale) {
  norm.resize(img.size());
  auto dx = 1.0f / img.size().x, dy = 1.0f / img.size().y;
  for (int j = 0; j < img.size().y; j++) {
    for (int i = 0; i < img.size().x; i++) {
      auto i1 = (i + 1) % img.size().x, j1 = (j + 1) % img.size().y;
      auto p00 = img[{i, j}], p10 = img[{i1, j}], p01 = img[{i, j1}];
      auto g00    = (p00.x + p00.y + p00.z) / 3;
      auto g01    = (p01.x + p01.y + p01.z) / 3;
      auto g10    = (p10.x + p10.y + p10.z) / 3;
      auto normal = vec3f{
          scale * (g00 - g10) / dx, scale * (g00 - g01) / dy, 1.0f};
      normal.y = -normal.y;  // make green pointing up, even if y axis
                             // points down
      normal       = normalize(normal) * 0.5f + vec3f{0.5f, 0.5f, 0.5f};
      norm[{i, j}] = {normal.x, normal.y, normal.z, 1};
    }
  }
}
image<vec4f> bump_to_normal(const image<vec4f>& img, float scale) {
  auto norm = image<vec4f>{img.size()};
  bump_to_normal(norm, img, scale);
  return norm;
}

// Make an image
void make_proc_image(image<vec4f>& img, const proc_image_params& params) {
  auto make_img = [&](const auto& shader) {
    img.resize(params.size);
    auto scale = 1.0f / max(params.size);
    for (auto j = 0; j < img.size().y; j++) {
      for (auto i = 0; i < img.size().x; i++) {
        auto uv     = vec2f{i * scale, j * scale};
        img[{i, j}] = shader(uv * params.scale);
        if (uv.x < params.borderw || uv.y < params.borderw ||
            uv.x > img.size().x * scale - params.borderw ||
            uv.y > img.size().y * scale - params.borderw) {
          img[{i, j}] = params.borderc;
        }
      }
    }
  };
  switch (params.type) {
    case proc_image_params::type_t::grid: {
      make_img([&params](vec2f uv) {
        uv *= 4;
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        auto thick = 0.01f / 2;
        auto c     = uv.x <= thick || uv.x >= 1 - thick || uv.y <= thick ||
                 uv.y >= 1 - thick ||
                 (uv.x >= 0.5f - thick && uv.x <= 0.5f + thick) ||
                 (uv.y >= 0.5f - thick && uv.y <= 0.5f + thick);
        return c ? params.color0 : params.color1;
      });
    } break;
    case proc_image_params::type_t::checker: {
      make_img([&params](vec2f uv) {
        uv *= 4;
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        auto c = uv.x <= 0.5f != uv.y <= 0.5f;
        return c ? params.color0 : params.color1;
      });
    } break;
    case proc_image_params::type_t::bumps: {
      make_img([&params](vec2f uv) {
        uv *= 4;
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        auto thick  = 0.125f;
        auto center = vec2f{
            uv.x <= 0.5f ? 0.25f : 0.75f,
            uv.y <= 0.5f ? 0.25f : 0.75f,
        };
        auto dist = clamp(length(uv - center), 0.0f, thick) / thick;
        auto val  = uv.x <= 0.5f != uv.y <= 0.5f ? (1 + sqrt(1 - dist)) / 2
                                                : (dist * dist) / 2;
        return lerp(params.color0, params.color1, val);
      });
    } break;
    case proc_image_params::type_t::ramp: {
      make_img([&params](vec2f uv) {
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        return lerp(params.color0, params.color1, uv.x);
      });
    } break;
    case proc_image_params::type_t::gammaramp: {
      make_img([&params](vec2f uv) {
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        if (uv.y < 1 / 3.0f) {
          return lerp(params.color0, params.color1, pow(uv.x, 2.2f));
        } else if (uv.y < 2 / 3.0f) {
          return lerp(params.color0, params.color1, uv.x);
        } else {
          return lerp(params.color0, params.color1, pow(uv.x, 1 / 2.2f));
        }
      });
    } break;
    case proc_image_params::type_t::uvramp: {
      make_img([](vec2f uv) {
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        return vec4f{uv.x, uv.y, 0, 1};
      });
    } break;
    case proc_image_params::type_t::uvgrid: {
      make_img([](vec2f uv) {
        auto colored = true;
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        uv.y     = 1 - uv.y;
        auto hsv = zero3f;
        hsv.x    = (clamp((int)(uv.x * 8), 0, 7) +
                    (clamp((int)(uv.y * 8), 0, 7) + 5) % 8 * 8) /
                64.0f;
        auto vuv = uv * 4;
        vuv -= vec2f{(float)(int)vuv.x, (float)(int)vuv.y};
        auto vc  = vuv.x <= 0.5f != vuv.y <= 0.5f;
        hsv.z    = vc ? 0.5f - 0.05f : 0.5f + 0.05f;
        auto suv = uv * 16;
        suv -= vec2f{(float)(int)suv.x, (float)(int)suv.y};
        auto st = 0.01f / 2;
        auto sc = suv.x <= st || suv.x >= 1 - st || suv.y <= st ||
                  suv.y >= 1 - st;
        if (sc) {
          hsv.y = 0.2f;
          hsv.z = 0.8f;
        } else {
          hsv.y = 0.8f;
        }
        auto rgb = (colored) ? hsv_to_rgb(hsv) : vec3f{hsv.z};
        return vec4f{rgb, 1};
      });
    } break;
    case proc_image_params::type_t::blackbody: {
      make_img([](vec2f uv) {
        uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
        return vec4f{blackbody_to_rgb(lerp(1000, 12000, uv.x)), 1};
      });
    } break;
    case proc_image_params::type_t::noise: {
      make_img([&params](vec2f uv) {
        uv *= 8;
        auto v = perlin_noise({uv.x, uv.y, 0.5f});
        v      = clamp(0.5f + 0.5f * v, 0.0f, 1.0f);
        return lerp(params.color0, params.color1, v);
      });
    } break;
    case proc_image_params::type_t::turbulence: {
      make_img([&params](vec2f uv) {
        uv *= 8;
        auto v = perlin_turbulence({uv.x, uv.y, 0.5f}, params.noise.x,
            params.noise.y, (int)params.noise.z);
        v      = clamp(0.5f + 0.5f * v, 0.0f, 1.0f);
        return lerp(params.color0, params.color1, v);
      });
    } break;
    case proc_image_params::type_t::fbm: {
      make_img([&params](vec2f uv) {
        uv *= 8;
        auto v = perlin_fbm({uv.x, uv.y, 0.5f}, params.noise.x, params.noise.y,
            (int)params.noise.z);
        v      = clamp(0.5f + 0.5f * v, 0.0f, 1.0f);
        return lerp(params.color0, params.color1, v);
      });
    } break;
    case proc_image_params::type_t::ridge: {
      make_img([&params](vec2f uv) {
        uv *= 8;
        auto v = perlin_ridge({uv.x, uv.y, 0.5f}, params.noise.x,
            params.noise.y, (int)params.noise.z, params.noise.w);
        v      = clamp(0.5f + 0.5f * v, 0.0f, 1.0f);
        return lerp(params.color0, params.color1, v);
      });
    } break;
  }
}
image<vec4f> make_proc_image(const proc_image_params& params) {
  auto img = image<vec4f>{params.size};
  make_proc_image(img, params);
  return img;
}

// Add a border to an image
image<vec4f> add_border(image<vec4f>& img, int width, const vec4f& color) {
  auto bordered = img;
  for (auto j = 0; j < img.size().y; j++) {
    for (auto b = 0; b < width; b++) {
      bordered[{b, j}]                    = color;
      bordered[{img.size().x - 1 - b, j}] = color;
    }
  }
  for (auto i = 0; i < img.size().x; i++) {
    for (auto b = 0; b < width; b++) {
      bordered[{i, b}]                    = color;
      bordered[{i, img.size().y - 1 - b}] = color;
    }
  }
  return bordered;
}
void add_border(
    image<vec4f>& bordered, image<vec4f>& img, int width, const vec4f& color) {
  bordered = img;
  for (auto j = 0; j < img.size().y; j++) {
    for (auto b = 0; b < width; b++) {
      bordered[{b, j}]                    = color;
      bordered[{img.size().x - 1 - b, j}] = color;
    }
  }
  for (auto i = 0; i < img.size().x; i++) {
    for (auto b = 0; b < width; b++) {
      bordered[{i, b}]                    = color;
      bordered[{i, img.size().y - 1 - b}] = color;
    }
  }
}

#if 0

// Implementation of sunsky modified heavily from pbrt
void make_sunsky(image<vec4f>& img, const vec2i& size, float theta_sun,
    float turbidity, bool has_sun, float sun_intensity, float sun_temperature,
    const vec3f& ground_albedo) {
  // idea adapted from pbrt

  // initialize model
  double wavelengths[9] = {630, 680, 710, 500, 530, 560, 460, 480, 490};
  ArHosekSkyModelState* skymodel_state[9];
  if (sun_temperature) {
    sun_temperature = clamp(sun_temperature, 2000.0f, 14000.0f);
    for (int i = 0; i < 9; ++i) {
      skymodel_state[i] = arhosekskymodelstate_alienworld_alloc_init(theta_sun,
          sun_intensity, sun_temperature, turbidity, ground_albedo[i / 3]);
    }
  } else {
    for (int i = 0; i < 9; ++i) {
      skymodel_state[i] = arhosekskymodelstate_alloc_init(
          theta_sun, turbidity, ground_albedo[i / 3]);
    }
  }

  // clear image
  img.resize(size);
  for (auto& p : img) p = {0, 0, 0, 1};

  // sun-sky
  auto sun_direction = vec3f{0, sin(theta_sun), cos(theta_sun)};
  auto integral      = zero3f;
  for (auto j = 0; j < img.size().y / 2; j++) {
    auto theta = (j + 0.5f) * pif / img.size().y;
    if (theta > pif / 2) continue;
    for (auto i = 0; i < img.size().x; i++) {
      auto phi       = (i + 0.5f) * 2 * pif / img.size().x;
      auto direction = vec3f{
          cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta)};
      auto gamma = acos(clamp(dot(direction, sun_direction), -1.0f, 1.0f));
      for (int c = 0; c < 9; ++c) {
        auto val = (has_sun) ? arhosekskymodel_solar_radiance(skymodel_state[c],
                                   theta, gamma, wavelengths[c])
                             : arhosekskymodel_radiance(skymodel_state[c],
                                   theta, gamma, wavelengths[c]);
        // average channel over wavelengths
        img[{i, j}][c / 3] += (float)val / 3;
      }
      integral += xyz(img[{i, j}]) * sin(theta) /
                  (img.size().x * img.size().y / 2);
    }
  }

  // ground
  auto ground = ground_albedo * integral;
  for (auto j = img.size().y / 2; j < img.size().y; j++) {
    for (auto i = 0; i < img.size().x; i++) {
      img[{i, j}] = {ground.x, ground.y, ground.z, 1};
    }
  }

  // cleanup
  for (auto i = 0; i < 9; i++) arhosekskymodelstate_free(skymodel_state[i]);
}

image<vec4f> make_sunsky(const vec2i& size, float theta_sun, float turbidity,
    bool has_sun, float sun_intensity, float sun_temperature,
    const vec3f& ground_albedo) {
  auto img = image<vec4f>{size};
  make_sunsky(img, size, theta_sun, turbidity, has_sun, sun_intensity,
      sun_temperature, ground_albedo);
  return img;
}

#else

// Implementation of sunsky modified heavily from pbrt
void make_sunsky(image<vec4f>& img, const vec2i& size, float theta_sun,
    float turbidity, bool has_sun, float sun_intensity, float sun_radius,
    const vec3f& ground_albedo) {
  auto zenith_xyY = vec3f{
      (+0.00165f * pow(theta_sun, 3.f) - 0.00374f * pow(theta_sun, 2.f) +
          0.00208f * theta_sun + 0.00000f) *
              pow(turbidity, 2.f) +
          (-0.02902f * pow(theta_sun, 3.f) + 0.06377f * pow(theta_sun, 2.f) -
              0.03202f * theta_sun + 0.00394f) *
              turbidity +
          (+0.11693f * pow(theta_sun, 3.f) - 0.21196f * pow(theta_sun, 2.f) +
              0.06052f * theta_sun + 0.25885f),
      (+0.00275f * pow(theta_sun, 3.f) - 0.00610f * pow(theta_sun, 2.f) +
          0.00316f * theta_sun + 0.00000f) *
              pow(turbidity, 2.f) +
          (-0.04214f * pow(theta_sun, 3.f) + 0.08970f * pow(theta_sun, 2.f) -
              0.04153f * theta_sun + 0.00515f) *
              turbidity +
          (+0.15346f * pow(theta_sun, 3.f) - 0.26756f * pow(theta_sun, 2.f) +
              0.06669f * theta_sun + 0.26688f),
      1000 * (4.0453f * turbidity - 4.9710f) *
              tan((4.0f / 9.0f - turbidity / 120.0f) * (pif - 2 * theta_sun)) -
          .2155f * turbidity + 2.4192f,
  };

  auto perez_A_xyY = vec3f{-0.01925f * turbidity - 0.25922f,
      -0.01669f * turbidity - 0.26078f, +0.17872f * turbidity - 1.46303f};
  auto perez_B_xyY = vec3f{-0.06651f * turbidity + 0.00081f,
      -0.09495f * turbidity + 0.00921f, -0.35540f * turbidity + 0.42749f};
  auto perez_C_xyY = vec3f{-0.00041f * turbidity + 0.21247f,
      -0.00792f * turbidity + 0.21023f, -0.02266f * turbidity + 5.32505f};
  auto perez_D_xyY = vec3f{-0.06409f * turbidity - 0.89887f,
      -0.04405f * turbidity - 1.65369f, +0.12064f * turbidity - 2.57705f};
  auto perez_E_xyY = vec3f{-0.00325f * turbidity + 0.04517f,
      -0.01092f * turbidity + 0.05291f, -0.06696f * turbidity + 0.37027f};

  auto perez_f = [](vec3f A, vec3f B, vec3f C, vec3f D, vec3f E, float theta,
                     float gamma, float theta_sun, vec3f zenith) -> vec3f {
    auto num = ((1 + A * exp(B / cos(theta))) *
                (1 + C * exp(D * gamma) + E * cos(gamma) * cos(gamma)));
    auto den = ((1 + A * exp(B)) * (1 + C * exp(D * theta_sun) +
                                       E * cos(theta_sun) * cos(theta_sun)));
    return zenith * num / den;
  };

  auto sky = [&perez_f, perez_A_xyY, perez_B_xyY, perez_C_xyY, perez_D_xyY,
                 perez_E_xyY, zenith_xyY](
                 float theta, float gamma, float theta_sun) -> vec3f {
    return xyz_to_rgb(xyY_to_xyz(
               perez_f(perez_A_xyY, perez_B_xyY, perez_C_xyY, perez_D_xyY,
                   perez_E_xyY, theta, gamma, theta_sun, zenith_xyY))) /
           10000;
  };

  // compute sun luminance
  // TODO: how this relates to zenith intensity?
  auto sun_ko     = vec3f{0.48f, 0.75f, 0.14f};
  auto sun_kg     = vec3f{0.1f, 0.0f, 0.0f};
  auto sun_kwa    = vec3f{0.02f, 0.0f, 0.0f};
  auto sun_sol    = vec3f{20000.0f, 27000.0f, 30000.0f};
  auto sun_lambda = vec3f{680, 530, 480};
  auto sun_beta   = 0.04608365822050f * turbidity - 0.04586025928522f;
  auto sun_m      = 1.0f /
               (cos(theta_sun) + 0.000940f * pow(1.6386f - theta_sun, -1.253f));

  auto tauR = exp(-sun_m * 0.008735f * pow(sun_lambda / 1000, -4.08f));
  auto tauA = exp(-sun_m * sun_beta * pow(sun_lambda / 1000, -1.3f));
  auto tauO = exp(-sun_m * sun_ko * .35f);
  auto tauG = exp(
      -1.41f * sun_kg * sun_m / pow(1 + 118.93f * sun_kg * sun_m, 0.45f));
  auto tauWA  = exp(-0.2385f * sun_kwa * 2.0f * sun_m /
                   pow(1 + 20.07f * sun_kwa * 2.0f * sun_m, 0.45f));
  auto sun_le = sun_sol * tauR * tauA * tauO * tauG * tauWA * 10000;

  // rescale by user
  sun_le *= sun_intensity;

  // sun scale from Wikipedia scaled by user quantity and rescaled to at
  // the minimum 5 pixel diamater
  auto sun_angular_radius = 9.35e-03f / 2;  // Wikipedia
  sun_angular_radius *= sun_radius;
  sun_angular_radius = max(sun_angular_radius, 2 * pif / size.y);

  // sun direction
  auto sun_direction = vec3f{0, cos(theta_sun), sin(theta_sun)};

  auto sun = [has_sun, sun_angular_radius, sun_le](auto theta, auto gamma) {
    return (has_sun && gamma < sun_angular_radius) ? sun_le / 10000 : zero3f;
  };

  // Make the sun sky image
  img.resize(size);
  auto sky_integral = 0.0f, sun_integral = 0.0f;
  for (auto j = 0; j < img.size().y / 2; j++) {
    auto theta = pif * ((j + 0.5f) / img.size().y);
    theta      = clamp(theta, 0.0f, pif / 2 - flt_eps);
    for (int i = 0; i < img.size().x; i++) {
      auto phi = 2 * pif * (float(i + 0.5f) / img.size().x);
      auto w = vec3f{cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta)};
      auto gamma   = acos(clamp(dot(w, sun_direction), -1.0f, 1.0f));
      auto sky_col = sky(theta, gamma, theta_sun);
      auto sun_col = sun(theta, gamma);
      sky_integral += mean(sky_col) * sin(theta);
      sun_integral += mean(sun_col) * sin(theta);
      auto col    = sky_col + sun_col;
      img[{i, j}] = {col.x, col.y, col.z, 1};
    }
  }

  if (ground_albedo != zero3f) {
    auto ground = zero3f;
    for (auto j = 0; j < img.size().y / 2; j++) {
      auto theta = pif * ((j + 0.5f) / img.size().y);
      for (int i = 0; i < img.size().x; i++) {
        auto pxl   = img[{i, j}];
        auto le    = vec3f{pxl.x, pxl.y, pxl.z};
        auto angle = sin(theta) * 4 * pif / (img.size().x * img.size().y);
        ground += le * (ground_albedo / pif) * cos(theta) * angle;
      }
    }
    for (auto j = img.size().y / 2; j < img.size().y; j++) {
      for (int i = 0; i < img.size().x; i++) {
        img[{i, j}] = {ground.x, ground.y, ground.z, 1};
      }
    }
  } else {
    for (auto j = img.size().y / 2; j < img.size().y; j++) {
      for (int i = 0; i < img.size().x; i++) {
        img[{i, j}] = {0, 0, 0, 1};
      }
    }
  }
}

image<vec4f> make_sunsky(const vec2i& size, float theta_sun, float turbidity,
    bool has_sun, float sun_intensity, float sun_radius,
    const vec3f& ground_albedo) {
  auto img = image<vec4f>{size};
  make_sunsky(img, size, theta_sun, turbidity, has_sun, sun_intensity,
      sun_radius, ground_albedo);
  return img;
}

#endif

// Make an image of multiple lights.
void make_lights(image<vec4f>& img, const vec2i& size, const vec3f& le,
    int nlights, float langle, float lwidth, float lheight) {
  img.resize(size);
  for (auto j = 0; j < img.size().y / 2; j++) {
    auto theta = pif * ((j + 0.5f) / img.size().y);
    theta      = clamp(theta, 0.0f, pif / 2 - 0.00001f);
    if (fabs(theta - langle) > lheight / 2) continue;
    for (int i = 0; i < img.size().x; i++) {
      auto phi     = 2 * pif * (float(i + 0.5f) / img.size().x);
      auto inlight = false;
      for (auto l = 0; l < nlights; l++) {
        auto lphi = 2 * pif * (l + 0.5f) / nlights;
        inlight   = inlight || fabs(phi - lphi) < lwidth / 2;
      }
      img[{i, j}] = {le, 1};
    }
  }
}
image<vec4f> make_lights(const vec2i& size, const vec3f& le, int nlights,
    float langle, float lwidth, float lheight) {
  auto img = image<vec4f>{size};
  make_lights(img, size, le, nlights, langle, lwidth, lheight);
  return img;
}

void make_logo(image<vec4b>& img, const string& type) {
  static const auto logo_medium_size = vec2i{102, 36};
  static const auto logo_small_size  = vec2i{72, 28};
  // clang-format off
  static const auto logo_medium = vector<byte>{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 139, 193, 55, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 209, 255, 43, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 253, 239, 1, 0, 0, 0, 0, 0, 0, 0, 0, 30, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 167, 234, 190, 0, 0, 0, 0, 0, 0, 59, 234, 233, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 69, 255, 183, 0, 0, 0, 0, 0, 0, 31, 169, 230, 255, 255, 224, 152, 15, 0, 0, 0, 0, 203, 234, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 73, 255, 255, 52, 0, 0, 0, 0, 0, 164, 255, 189, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 54, 228, 253, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 255, 125, 0, 0, 0, 0, 0, 66, 235, 255, 252, 206, 217, 254, 255, 225, 45, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 1, 213, 255, 157, 0, 0, 0, 0, 20, 248, 255, 73, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 185, 255, 66, 0, 0, 0, 0, 43, 251, 255, 182, 15, 0, 0, 22, 166, 188, 8, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 99, 255, 245, 16, 0, 0, 0, 117, 255, 213, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 241, 252, 12, 0, 0, 0, 0, 174, 255, 198, 4, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 7, 232, 255, 111, 0, 0, 1, 219, 255, 99, 0, 0, 0, 29, 152, 206, 238, 193, 128, 6, 0, 0, 0, 0, 0, 0, 25, 149, 207, 239, 199, 118, 4, 43, 179, 199, 255, 255, 181, 179, 136, 0, 0, 0, 0, 61, 166, 220, 231, 180, 95, 0, 0, 0, 0, 0, 0, 0, 0, 45, 255, 206, 0, 0, 0, 0, 52, 255, 255, 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 125, 255, 215, 0, 0, 69, 255, 232, 7, 0, 0, 56, 232, 255, 254, 235, 255, 255, 198, 21, 0, 0, 0, 0, 46, 226, 255, 254, 234, 255, 255, 170, 61, 255, 255, 255, 255, 255, 255, 166, 0, 0, 0, 101, 250, 255, 248, 241, 255, 255, 153, 3, 0, 0, 0, 0, 0, 0, 104, 255, 148, 0, 0, 0, 0, 137, 255, 232, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 20, 246, 255, 65, 0, 173, 255, 124, 0, 0, 3, 221, 255, 183, 17, 0, 49, 232, 255, 151, 0, 0, 0, 1, 213, 255, 198, 22, 0, 26, 153, 46, 5, 23, 84, 255, 255, 29, 23, 13, 0, 0, 36, 253, 255, 129, 7, 2, 90, 251, 255, 87, 0, 0, 0, 0, 0, 0, 162, 255, 90, 0, 0, 0, 0, 173, 255, 194, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 151, 255, 170, 26, 251, 245, 19, 0, 0, 89, 255, 248, 15, 0, 0, 0, 91, 255, 247, 23, 0, 0, 77, 255, 253, 27, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 0, 153, 255, 199, 0, 0, 0, 0, 155, 255, 206, 0, 0, 0, 0, 0, 0, 220, 255, 32, 0, 0, 0, 0, 208, 255, 167, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 254, 250, 150, 255, 149, 0, 0, 0, 174, 255, 177, 0, 0, 0, 0, 13, 250, 255, 95, 0, 0, 168, 255, 191, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 0, 238, 255, 113, 0, 0, 0, 0, 71, 255, 255, 31, 0, 0, 0, 0, 22, 255, 230, 0, 0, 0, 0, 0, 243, 255, 140, 0, 0, 0, 108, 231, 231, 231, 231, 149, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 177, 255, 255, 253, 36, 0, 0, 0, 205, 255, 140, 0, 0, 0, 0, 0, 227, 255, 125, 0, 0, 201, 255, 149, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 14, 255, 255, 76, 0, 0, 0, 0, 35, 255, 255, 61, 0, 0, 0, 0, 80, 255, 172, 0, 0, 0, 0, 1, 248, 255, 135, 0, 0, 0, 86, 255, 255, 255, 255, 165, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 255, 255, 175, 0, 0, 0, 0, 236, 255, 119, 0, 0, 0, 0, 0, 207, 255, 156, 0, 0, 232, 255, 128, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 44, 255, 255, 55, 0, 0, 0, 0, 15, 255, 255, 92, 0, 0, 0, 0, 138, 255, 113, 0, 0, 0, 0, 0, 222, 255, 155, 0, 0, 0, 1, 4, 4, 166, 255, 165, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 245, 255, 105, 0, 0, 0, 0, 245, 255, 113, 0, 0, 0, 0, 0, 202, 255, 163, 0, 0, 246, 255, 120, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 53, 255, 255, 49, 0, 0, 0, 0, 10, 255, 255, 99, 0, 0, 0, 0, 196, 255, 55, 0, 0, 0, 0, 0, 194, 255, 175, 0, 0, 0, 0, 0, 0, 164, 255, 165, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 244, 255, 104, 0, 0, 0, 0, 215, 255, 133, 0, 0, 0, 0, 0, 221, 255, 132, 0, 0, 218, 255, 141, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 23, 255, 255, 69, 0, 0, 0, 0, 29, 255, 255, 68, 0, 0, 0, 6, 248, 247, 6, 0, 0, 0, 0, 0, 166, 255, 206, 0, 0, 0, 0, 0, 0, 164, 255, 165, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 244, 255, 104, 0, 0, 0, 0, 185, 255, 159, 0, 0, 0, 0, 3, 243, 255, 100, 0, 0, 187, 255, 167, 0, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 6, 0, 0, 0, 1, 247, 255, 95, 0, 0, 0, 0, 54, 255, 255, 36, 0, 0, 0, 57, 255, 195, 0, 0, 0, 0, 0, 0, 103, 255, 255, 32, 0, 0, 0, 0, 0, 164, 255, 165, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 244, 255, 104, 0, 0, 0, 0, 123, 255, 232, 2, 0, 0, 0, 65, 255, 253, 36, 0, 0, 133, 255, 239, 7, 0, 0, 0, 0, 0, 0, 0, 67, 255, 255, 7, 0, 0, 0, 0, 187, 255, 170, 0, 0, 0, 0, 129, 255, 222, 3, 0, 0, 0, 115, 255, 137, 0, 0, 0, 0, 0, 0, 9, 236, 255, 127, 0, 0, 0, 0, 0, 164, 255, 165, 0, 0, 0, 222, 255, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 244, 255, 104, 0, 0, 0, 0, 17, 243, 255, 113, 0, 0, 8, 191, 255, 170, 0, 0, 0, 23, 247, 255, 135, 0, 0, 1, 91, 18, 0, 0, 42, 255, 255, 51, 0, 1, 0, 0, 69, 255, 247, 58, 0, 0, 32, 231, 255, 106, 0, 0, 0, 0, 173, 255, 79, 0, 0, 0, 0, 0, 0, 0, 132, 255, 252, 91, 0, 0, 0, 21, 197, 255, 165, 0, 0, 0, 222, 255, 137, 20, 20, 20, 20, 20, 15, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 244, 255, 104, 0, 0, 0, 0, 0, 111, 254, 255, 187, 153, 216, 255, 230, 39, 0, 0, 0, 0, 124, 255, 255, 208, 170, 219, 255, 170, 0, 0, 5, 226, 255, 233, 172, 234, 59, 0, 0, 174, 255, 244, 171, 162, 234, 255, 197, 9, 0, 0, 0, 0, 231, 255, 21, 0, 0, 0, 0, 0, 0, 0, 9, 169, 255, 255, 224, 176, 195, 246, 255, 255, 145, 0, 0, 0, 222, 255, 255, 255, 255, 255, 255, 255, 176, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 244, 255, 104, 0, 0, 0, 0, 0, 0, 87, 233, 255, 255, 253, 196, 29, 0, 0, 0, 0, 0, 0, 94, 237, 255, 255, 255, 188, 34, 0, 0, 0, 65, 233, 255, 255, 246, 103, 0, 0, 2, 138, 244, 255, 255, 247, 160, 7, 0, 0, 0, 0, 33, 255, 219, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 126, 227, 255, 255, 255, 250, 194, 73, 0, 0, 0, 0, 222, 255, 255, 255, 255, 255, 255, 255, 144, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 63, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 36, 64, 18, 0, 0, 0, 0, 0, 0, 11, 56, 60, 15, 0, 0, 0, 0, 0, 3, 47, 55, 6, 0, 0, 0, 0, 0, 0, 91, 255, 160, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 67, 40, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 149, 255, 102, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 69, 140, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  };
static const auto logo_small = vector<byte> {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 165, 221, 125, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 223, 255, 139, 0, 0, 0, 0, 7, 55, 39, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 8, 236, 255, 241, 6, 0, 0, 132, 255, 255, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 52, 84, 88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 255, 255, 81, 0, 0, 10, 160, 249, 255, 255, 242, 120, 3, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 138, 255, 255, 72, 0, 0, 214, 255, 226, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 235, 255, 204, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 83, 255, 255, 24, 0, 9, 207, 255, 255, 247, 249, 255, 253, 57, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 34, 254, 255, 153, 0, 40, 255, 255, 124, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 235, 255, 204, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 140, 255, 222, 0, 0, 115, 255, 255, 160, 9, 9, 129, 129, 0, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 182, 255, 232, 2, 122, 255, 250, 23, 0, 90, 199, 242, 214, 135, 6, 0, 0, 0, 36, 175, 231, 229, 164, 24, 173, 249, 255, 239, 176, 112, 0, 4, 130, 212, 243, 202, 95, 0, 0, 0, 0, 0, 198, 255, 165, 0, 7, 234, 255, 243, 9, 0, 0, 0, 0, 0, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 76, 255, 255, 60, 204, 255, 168, 0, 111, 255, 255, 255, 255, 255, 182, 0, 0, 41, 239, 255, 255, 255, 255, 135, 251, 255, 255, 255, 255, 130, 0, 175, 255, 255, 255, 255, 255, 118, 0, 0, 0, 7, 248, 255, 107, 0, 42, 255, 255, 172, 0, 0, 0, 0, 0, 0, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 2, 224, 255, 172, 255, 255, 61, 14, 237, 255, 208, 31, 152, 255, 255, 65, 0, 170, 255, 254, 90, 63, 143, 7, 58, 240, 255, 216, 59, 24, 60, 255, 255, 157, 31, 204, 255, 240, 16, 0, 0, 57, 255, 255, 50, 0, 77, 255, 255, 141, 0, 43, 61, 61, 61, 30, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 121, 255, 255, 255, 211, 0, 72, 255, 255, 116, 0, 47, 255, 255, 137, 5, 252, 255, 200, 0, 0, 0, 0, 0, 235, 255, 204, 0, 0, 136, 255, 255, 52, 0, 111, 255, 255, 73, 0, 0, 115, 255, 244, 3, 0, 108, 255, 255, 122, 0, 160, 255, 255, 255, 129, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 22, 249, 255, 255, 105, 0, 105, 255, 255, 89, 0, 20, 255, 255, 169, 37, 255, 255, 166, 0, 0, 0, 0, 0, 235, 255, 204, 0, 0, 169, 255, 255, 25, 0, 84, 255, 255, 105, 0, 0, 172, 255, 191, 0, 0, 94, 255, 255, 129, 0, 103, 211, 255, 255, 129, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 193, 255, 255, 29, 0, 115, 255, 255, 84, 0, 15, 255, 255, 179, 52, 255, 255, 159, 0, 0, 0, 0, 0, 235, 255, 204, 0, 0, 179, 255, 255, 20, 0, 79, 255, 255, 115, 0, 0, 230, 255, 133, 0, 0, 67, 255, 255, 145, 0, 0, 40, 255, 255, 129, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 189, 255, 255, 26, 0, 83, 255, 255, 101, 0, 33, 255, 255, 147, 19, 255, 255, 182, 0, 0, 0, 0, 0, 235, 255, 204, 0, 0, 147, 255, 255, 37, 0, 97, 255, 255, 83, 0, 32, 255, 255, 76, 0, 0, 32, 255, 255, 195, 0, 0, 40, 255, 255, 129, 0, 213, 255, 254, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 189, 255, 255, 26, 0, 36, 254, 255, 164, 0, 98, 255, 255, 94, 0, 221, 255, 245, 28, 8, 61, 0, 0, 214, 255, 224, 2, 6, 98, 255, 255, 100, 0, 162, 255, 253, 33, 0, 89, 255, 255, 19, 0, 0, 0, 185, 255, 251, 56, 0, 58, 255, 255, 129, 0, 213, 255, 254, 63, 63, 63, 63, 7, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 189, 255, 255, 26, 0, 0, 168, 255, 255, 197, 244, 255, 222, 5, 0, 93, 255, 255, 244, 239, 255, 77, 0, 136, 255, 255, 242, 178, 6, 225, 255, 244, 197, 255, 255, 163, 0, 0, 147, 255, 217, 0, 0, 0, 0, 61, 248, 255, 251, 216, 254, 255, 255, 129, 0, 213, 255, 255, 255, 255, 255, 254, 9, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 189, 255, 255, 26, 0, 0, 17, 174, 255, 255, 255, 208, 36, 0, 0, 0, 110, 248, 255, 255, 239, 90, 0, 41, 231, 255, 255, 230, 21, 43, 212, 255, 255, 255, 168, 12, 0, 0, 204, 255, 159, 0, 0, 0, 0, 0, 55, 219, 255, 255, 255, 241, 131, 17, 0, 213, 255, 255, 255, 255, 255, 229, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 65, 34, 0, 0, 0, 0, 0, 0, 8, 54, 52, 8, 0, 0, 0, 11, 59, 56, 8, 0, 0, 1, 36, 65, 21, 0, 0, 0, 11, 251, 255, 102, 0, 0, 0, 0, 0, 0, 0, 21, 61, 35, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 161, 221, 44, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  };
  // clang-format on
  if (type == "logo-medium") {
    img.resize(logo_medium_size);
    for (auto i = 0; i < img.count(); i++)
      img[i] = vec4b{logo_medium[i], logo_medium[i], logo_medium[i], (byte)255};
  } else if (type == "logo-small") {
    img.resize(logo_small_size);
    for (auto i = 0; i < img.count(); i++)
      img[i] = vec4b{logo_small[i], logo_small[i], logo_small[i], (byte)255};
  } else {
    throw std::runtime_error("unknown builtin image " + type);
  }
}

image<vec4b> make_logo(const string& type) {
  auto img = image<vec4b>{};
  make_logo(img, type);
  return img;
}

void make_logo(image<vec4f>& img, const string& type) {
  auto img8 = image<vec4b>();
  make_logo(img8, type);
  img.resize(img8.size());
  srgb_to_rgb(img, img8);
}

image<vec4f> add_logo(const image<vec4f>& img, const string& type) {
  auto logo   = srgb_to_rgb(make_logo(type));
  auto offset = img.size() - logo.size() - 8;
  auto wlogo  = img;
  set_region(wlogo, logo, offset);
  return wlogo;
}

image<vec4b> add_logo(const image<vec4b>& img, const string& type) {
  auto logo   = make_logo(type);
  auto offset = img.size() - logo.size() - 8;
  auto wlogo  = img;
  set_region(wlogo, logo, offset);
  return wlogo;
}

void add_logo(
    image<vec4f>& wlogo, const image<vec4f>& img, const string& type) {
  auto logo   = srgb_to_rgb(make_logo(type));
  auto offset = img.size() - logo.size() - 8;
  wlogo       = img;
  set_region(wlogo, logo, offset);
}

void add_logo(
    image<vec4b>& wlogo, const image<vec4b>& img, const string& type) {
  auto logo   = make_logo(type);
  auto offset = img.size() - logo.size() - 8;
  wlogo       = img;
  set_region(wlogo, logo, offset);
}

void make_image_preset(image<vec4f>& img, const string& type) {
  auto size = vec2i{1024, 1024};
  if (type.find("sky") != type.npos) size = {2048, 1024};
  if (type == "grid") {
    auto params   = proc_image_params{};
    params.type   = proc_image_params::type_t::grid;
    params.color0 = vec4f{0.2, 0.2, 0.2, 1};
    params.color1 = vec4f{0.7, 0.7, 0.7, 1};
    make_proc_image(img, params);
  } else if (type == "checker") {
    auto params   = proc_image_params{};
    params.type   = proc_image_params::type_t::checker;
    params.color0 = vec4f{0.2, 0.2, 0.2, 1};
    params.color1 = vec4f{0.7, 0.7, 0.7, 1};
    make_proc_image(img, params);
  } else if (type == "bumps") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::bumps;
    make_proc_image(img, params);
  } else if (type == "uvramp") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::uvramp;
    make_proc_image(img, params);
  } else if (type == "gammaramp") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::gammaramp;
    make_proc_image(img, params);
  } else if (type == "blackbodyramp") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::blackbody;
    make_proc_image(img, params);
  } else if (type == "uvgrid") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::uvgrid;
    make_proc_image(img, params);
  } else if (type == "sky") {
    make_sunsky(
        img, size, pif / 4, 3.0f, false, 1.0f, 1.0f, vec3f{0.7f, 0.7f, 0.7f});
  } else if (type == "sunsky") {
    make_sunsky(
        img, size, pif / 4, 3.0f, true, 1.0f, 1.0f, vec3f{0.7f, 0.7f, 0.7f});
  } else if (type == "noise") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::noise;
    make_proc_image(img, params);
  } else if (type == "fbm") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::fbm;
    make_proc_image(img, params);
  } else if (type == "ridge") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::ridge;
    make_proc_image(img, params);
  } else if (type == "turbulence") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::turbulence;
    make_proc_image(img, params);
  } else if (type == "bump-normal") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::bumps;
    make_proc_image(img, params);
    auto bump = img;
    bump_to_normal(img, bump, 0.05f);
  } else if (type == "logo-medium") {
    make_logo(img, "logo-medium");
  } else if (type == "images1") {
    auto sub_types = vector<string>{"grid", "uvgrid", "checker", "gammaramp",
        "bumps", "bump-normal", "noise", "fbm", "blackbodyramp"};
    auto sub_imgs  = vector<image<vec4f>>(sub_types.size());
    for (auto i = 0; i < sub_imgs.size(); i++) {
      sub_imgs.at(i).resize(img.size());
      make_image_preset(sub_imgs.at(i), sub_types.at(i));
    }
    auto montage_size = zero2i;
    for (auto& sub_img : sub_imgs) {
      montage_size.x += sub_img.size().x;
      montage_size.y = max(montage_size.y, sub_img.size().y);
    }
    img.resize(montage_size);
    auto pos = 0;
    for (auto& sub_img : sub_imgs) {
      set_region(img, sub_img, {pos, 0});
      pos += sub_img.size().x;
    }
  } else if (type == "images2") {
    auto sub_types = vector<string>{"sky", "sunsky"};
    auto sub_imgs  = vector<image<vec4f>>(sub_types.size());
    for (auto i = 0; i < sub_imgs.size(); i++) {
      make_image_preset(sub_imgs.at(i), sub_types.at(i));
    }
    auto montage_size = zero2i;
    for (auto& sub_img : sub_imgs) {
      montage_size.x += sub_img.size().x;
      montage_size.y = max(montage_size.y, sub_img.size().y);
    }
    img.resize(montage_size);
    auto pos = 0;
    for (auto& sub_img : sub_imgs) {
      set_region(img, sub_img, {pos, 0});
      pos += sub_img.size().x;
    }
  } else if (type == "test-floor") {
    auto params    = proc_image_params{};
    params.type    = proc_image_params::type_t::grid;
    params.color0  = vec4f{0.2, 0.2, 0.2, 1};
    params.color1  = vec4f{0.5, 0.5, 0.5, 1};
    params.borderw = 0.0025;
    make_proc_image(img, params);
  } else if (type == "test-grid") {
    auto params   = proc_image_params{};
    params.type   = proc_image_params::type_t::grid;
    params.color0 = vec4f{0.2, 0.2, 0.2, 1};
    params.color1 = vec4f{0.5, 0.5, 0.5, 1};
    make_proc_image(img, params);
  } else if (type == "test-checker") {
    auto params   = proc_image_params{};
    params.type   = proc_image_params::type_t::checker;
    params.color0 = vec4f{0.2, 0.2, 0.2, 1};
    params.color1 = vec4f{0.5, 0.5, 0.5, 1};
    make_proc_image(img, params);
  } else if (type == "test-bumps") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::bumps;
    make_proc_image(img, params);
  } else if (type == "test-uvramp") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::uvramp;
    make_proc_image(img, params);
  } else if (type == "test-gammaramp") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::gammaramp;
    make_proc_image(img, params);
  } else if (type == "test-blackbodyramp") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::blackbody;
    make_proc_image(img, params);
  } else if (type == "test-uvgrid") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::uvgrid;
    make_proc_image(img, params);
  } else if (type == "test-sky") {
    make_sunsky(
        img, size, pif / 4, 3.0f, false, 1.0f, 1.0f, vec3f{0.7f, 0.7f, 0.7f});
  } else if (type == "test-sunsky") {
    make_sunsky(
        img, size, pif / 4, 3.0f, true, 1.0f, 1.0f, vec3f{0.7f, 0.7f, 0.7f});
  } else if (type == "test-noise") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::noise;
    make_proc_image(img, params);
  } else if (type == "test-fbm") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::fbm;
    make_proc_image(img, params);
  } else if (type == "test-bumps-normal") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::bumps;
    make_proc_image(img, params);
    auto bump = img;
    bump_to_normal(img, bump, 0.05f);
  } else if (type == "test-bumps-displacement") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::bumps;
    make_proc_image(img, params);
  } else if (type == "test-fbm-displacement") {
    auto params = proc_image_params{};
    params.type = proc_image_params::type_t::fbm;
    make_proc_image(img, params);
  } else {
    throw std::invalid_argument("unknown image preset " + type);
  }
}
image<vec4f> make_image_preset(const string& type) {
  auto img = image<vec4f>{};
  make_image_preset(img, type);
  return img;
}

void make_image_preset(image<vec4b>& img, const string& type) {
  auto imgf = image<vec4f>{};
  make_image_preset(imgf, type);
  if (type.find("-normal") == type.npos &&
      type.find("-displacement") == type.npos) {
    rgb_to_srgb(img, imgf);
  } else {
    float_to_byte(img, imgf);
  }
}

void make_image_preset(
    image<vec4f>& hdr, image<vec4b>& ldr, const string& type) {
  if (type.find("sky") == type.npos) {
    auto imgf = image<vec4f>{};
    make_image_preset(imgf, type);
    if (type.find("-normal") == type.npos) {
      rgb_to_srgb(ldr, imgf);
    } else {
      float_to_byte(ldr, imgf);
    }
  } else {
    make_image_preset(hdr, type);
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR VOLUME
// -----------------------------------------------------------------------------
namespace yocto {

// make a simple example volume
void make_test(
    volume<float>& vol, const vec3i& size, float scale, float exponent) {
  vol.resize(size);
  for (auto k = 0; k < vol.size().z; k++) {
    for (auto j = 0; j < vol.size().y; j++) {
      for (auto i = 0; i < vol.size().x; i++) {
        auto p     = vec3f{i / (float)vol.size().x, j / (float)vol.size().y,
            k / (float)vol.size().z};
        auto value = pow(
            max(max(cos(scale * p.x), cos(scale * p.y)), 0.0f), exponent);
        vol[{i, j, k}] = clamp(value, 0.0f, 1.0f);
      }
    }
  }
}
volume<float> make_test(const vec3i& size, float scale, float exponent) {
  auto vol = volume<float>{};
  make_test(vol, size, scale, exponent);
  return vol;
}

void make_volpreset(volume<float>& vol, const string& type) {
  auto size = vec3i{256, 256, 256};
  if (type == "test-volume") {
    make_test(vol, size, 6, 10);
  } else {
    throw std::runtime_error("unknown volume preset " + type);
  }
}
volume<float> make_volpreset(const string& type) {
  auto vol = volume<float>{};
  make_volpreset(vol, type);
  return vol;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR IMAGEIO
// -----------------------------------------------------------------------------
namespace yocto {

// Split a string
static inline vector<string> split_string(const string& str) {
  auto ret = vector<string>();
  if (str.empty()) return ret;
  auto lpos = (size_t)0;
  while (lpos != str.npos) {
    auto pos = str.find_first_of(" \t\n\r", lpos);
    if (pos != str.npos) {
      if (pos > lpos) ret.push_back(str.substr(lpos, pos - lpos));
      lpos = pos + 1;
    } else {
      if (lpos < str.size()) ret.push_back(str.substr(lpos));
      lpos = pos;
    }
  }
  return ret;
}

// Pfm load
static inline float* load_pfm(
    const char* filename, int* w, int* h, int* nc, int req) {
  auto fs = fopen(filename, "rb");
  if (!fs) return nullptr;
  auto fs_guard = std::unique_ptr<FILE, void (*)(FILE*)>{
      fs, [](FILE* f) { fclose(f); }};

  // buffer
  char buffer[4096];
  auto toks = vector<string>();

  // read magic
  if (!fgets(buffer, sizeof(buffer), fs)) return nullptr;
  toks = split_string(buffer);
  if (toks[0] == "Pf")
    *nc = 1;
  else if (toks[0] == "PF")
    *nc = 3;
  else
    return nullptr;

  // read w, h
  if (!fgets(buffer, sizeof(buffer), fs)) return nullptr;
  toks = split_string(buffer);
  *w   = atoi(toks[0].c_str());
  *h   = atoi(toks[1].c_str());

  // read scale
  if (!fgets(buffer, sizeof(buffer), fs)) return nullptr;
  toks   = split_string(buffer);
  auto s = atof(toks[0].c_str());

  // read the data (flip y)
  auto npixels = (size_t)(*w) * (size_t)(*h);
  auto nvalues = npixels * (size_t)(*nc);
  auto nrow    = (size_t)(*w) * (size_t)(*nc);
  auto pixels  = std::unique_ptr<float[]>(new float[nvalues]);
  for (auto j = *h - 1; j >= 0; j--) {
    if (fread(pixels.get() + j * nrow, sizeof(float), nrow, fs) != nrow)
      return nullptr;
  }

  // endian conversion
  if (s > 0) {
    for (auto i = 0; i < nvalues; ++i) {
      auto dta = (uint8_t*)(pixels.get() + i);
      std::swap(dta[0], dta[3]);
      std::swap(dta[1], dta[2]);
    }
  }

  // scale
  auto scl = (s > 0) ? s : -s;
  if (scl != 1) {
    for (auto i = 0; i < nvalues; i++) pixels[i] *= scl;
  }

  // proper number of channels
  if (!req || *nc == req) return pixels.release();

  // pack into channels
  if (req < 0 || req > 4) {
    return nullptr;
  }
  auto cpixels = std::unique_ptr<float[]>(new float[req * npixels]);
  for (auto i = 0ull; i < npixels; i++) {
    auto vp = pixels.get() + i * (*nc);
    auto cp = cpixels.get() + i * req;
    if (*nc == 1) {
      switch (req) {
        case 1: cp[0] = vp[0]; break;
        case 2:
          cp[0] = vp[0];
          cp[1] = vp[0];
          break;
        case 3:
          cp[0] = vp[0];
          cp[1] = vp[0];
          cp[2] = vp[0];
          break;
        case 4:
          cp[0] = vp[0];
          cp[1] = vp[0];
          cp[2] = vp[0];
          cp[3] = 1;
          break;
      }
    } else {
      switch (req) {
        case 1: cp[0] = vp[0]; break;
        case 2:
          cp[0] = vp[0];
          cp[1] = vp[1];
          break;
        case 3:
          cp[0] = vp[0];
          cp[1] = vp[1];
          cp[2] = vp[2];
          break;
        case 4:
          cp[0] = vp[0];
          cp[1] = vp[1];
          cp[2] = vp[2];
          cp[3] = 1;
          break;
      }
    }
  }
  return cpixels.release();
}

// save pfm
static inline bool save_pfm(
    const char* filename, int w, int h, int nc, const float* pixels) {
  auto fs = fopen(filename, "wb");
  if (!fs) return false;
  auto fs_guard = std::unique_ptr<FILE, void (*)(FILE*)>{
      fs, [](FILE* f) { fclose(f); }};

  if (fprintf(fs, "%s\n", (nc == 1) ? "Pf" : "PF") < 0) return false;
  if (fprintf(fs, "%d %d\n", w, h) < 0) return false;
  if (fprintf(fs, "-1\n") < 0) return false;
  if (nc == 1 || nc == 3) {
    if (fwrite(pixels, sizeof(float), w * h * nc, fs) != w * h * nc)
      return false;
  } else {
    for (auto i = 0; i < w * h; i++) {
      auto vz = 0.0f;
      auto v  = pixels + i * nc;
      if (fwrite(v + 0, sizeof(float), 1, fs) != 1) return false;
      if (fwrite(v + 1, sizeof(float), 1, fs) != 1) return false;
      if (nc == 2) {
        if (fwrite(&vz, sizeof(float), 1, fs) != 1) return false;
      } else {
        if (fwrite(v + 2, sizeof(float), 1, fs) != 1) return false;
      }
    }
  }

  return true;
}

// load exr image weith tiny exr
static inline const char* get_tinyexr_error(int error) {
  switch (error) {
    case TINYEXR_ERROR_INVALID_MAGIC_NUMBER: return "INVALID_MAGIC_NUMBER";
    case TINYEXR_ERROR_INVALID_EXR_VERSION: return "INVALID_EXR_VERSION";
    case TINYEXR_ERROR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case TINYEXR_ERROR_INVALID_DATA: return "INVALID_DATA";
    case TINYEXR_ERROR_INVALID_FILE: return "INVALID_FILE";
    // case TINYEXR_ERROR_INVALID_PARAMETER: return "INVALID_PARAMETER";
    case TINYEXR_ERROR_CANT_OPEN_FILE: return "CANT_OPEN_FILE";
    case TINYEXR_ERROR_UNSUPPORTED_FORMAT: return "UNSUPPORTED_FORMAT";
    case TINYEXR_ERROR_INVALID_HEADER: return "INVALID_HEADER";
    default: throw std::runtime_error("unknown tinyexr error");
  }
}

// Return the preset type and the remaining filename
static inline bool is_preset_filename(const string& filename) {
  return filename.find("::yocto::") == 0;
}
// Return the preset type and the filename. Call only if this is a preset.
static inline pair<string, string> get_preset_type(const string& filename) {
  if (filename.find("::yocto::") == 0) {
    auto aux = filename.substr(string("::yocto::").size());
    auto pos = aux.find("::");
    if (pos == aux.npos) throw std::runtime_error("bad preset name" + filename);
    return {aux.substr(0, pos), aux.substr(pos + 2)};
  } else {
    return {"", filename};
  }
}

static inline void load_image_preset(
    const string& filename, image<vec4f>& img) {
  auto [type, nfilename] = get_preset_type(filename);
  img.resize({1024, 1024});
  if (type == "images2") img.resize({2048, 1024});
  make_image_preset(img, type);
}
static inline void load_image_preset(
    const string& filename, image<vec4b>& img) {
  auto imgf = image<vec4f>{};
  load_image_preset(filename, imgf);
  img.resize(imgf.size());
  rgb_to_srgb(img, imgf);
}

// Check if an image is HDR based on filename.
bool is_hdr_filename(const string& filename) {
  auto ext = fs::path(filename).extension().string();
  return ext == ".hdr" || ext == ".exr" || ext == ".pfm";
}

// Loads an hdr image.
image<vec4f> load_image(const string& filename) {
  auto img = image<vec4f>{};
  load_image(filename, img);
  return img;
}

// Loads an hdr image.
void load_image(const string& filename, image<vec4f>& img) {
  if (is_preset_filename(filename)) return load_image_preset(filename, img);
  auto ext = fs::path(filename).extension().string();
  if (ext == ".exr" || ext == ".EXR") {
    auto width = 0, height = 0;
    auto pixels = (float*)nullptr;
    if (auto error = LoadEXR(
            &pixels, &width, &height, filename.c_str(), nullptr);
        error < 0)
      throw std::runtime_error("error loading image " + filename + "("s +
                               get_tinyexr_error(error) + ")"s);
    if (!pixels) throw std::runtime_error("error loading image " + filename);
    img = image{{width, height}, (const vec4f*)pixels};
    free(pixels);
  } else if (ext == ".pfm" || ext == ".PFM") {
    auto width = 0, height = 0, ncomp = 0;
    auto pixels = load_pfm(filename.c_str(), &width, &height, &ncomp, 4);
    if (!pixels) throw std::runtime_error("error loading image " + filename);
    img = image{{width, height}, (const vec4f*)pixels};
    delete[] pixels;
  } else if (ext == ".hdr" || ext == ".HDR") {
    auto width = 0, height = 0, ncomp = 0;
    auto pixels = stbi_loadf(filename.c_str(), &width, &height, &ncomp, 4);
    if (!pixels) throw std::runtime_error("error loading image " + filename);
    img = image{{width, height}, (const vec4f*)pixels};
    free(pixels);
  } else if (!is_hdr_filename(filename)) {
    img = srgb_to_rgb(load_imageb(filename));
  } else {
    throw std::runtime_error("unsupported image format " + ext);
  }
}

// Saves an hdr image.
void save_image(const string& filename, const image<vec4f>& img) {
  auto ext = fs::path(filename).extension().string();
  if (ext == ".hdr" || ext == ".HDR") {
    if (!stbi_write_hdr(filename.c_str(), img.size().x, img.size().y, 4,
            (float*)img.data()))
      throw std::runtime_error("error saving image " + filename);
  } else if (ext == ".pfm" || ext == ".PFM") {
    if (!save_pfm(filename.c_str(), img.size().x, img.size().y, 4,
            (float*)img.data()))
      throw std::runtime_error("error saving image " + filename);
  } else if (ext == ".exr" || ext == ".EXR") {
    if (SaveEXR((float*)img.data(), img.size().x, img.size().y, 4,
            filename.c_str()) < 0)
      throw std::runtime_error("error saving image " + filename);
  } else if (!is_hdr_filename(filename)) {
    save_imageb(filename, rgb_to_srgbb(img));
  } else {
    throw std::runtime_error("unsupported image format " + ext);
  }
}

// Loads an ldr image.
image<vec4b> load_imageb(const string& filename) {
  auto img = image<vec4b>{};
  load_imageb(filename, img);
  return img;
}

// Loads an ldr image.
void load_imageb(const string& filename, image<vec4b>& img) {
  if (is_preset_filename(filename)) return load_image_preset(filename, img);
  auto ext = fs::path(filename).extension().string();
  if (ext == ".png" || ext == ".PNG" || ext == ".jpg" || ext == ".JPG" ||
      ext == ".tga" || ext == ".TGA" || ext == ".bmp" || ext == ".BMP") {
    auto width = 0, height = 0, ncomp = 0;
    auto pixels = stbi_load(filename.c_str(), &width, &height, &ncomp, 4);
    if (!pixels) throw std::runtime_error("error loading image " + filename);
    img = image{{width, height}, (const vec4b*)pixels};
    free(pixels);
  } else if (is_hdr_filename(filename)) {
    img = rgb_to_srgbb(load_image(filename));
  } else {
    throw std::runtime_error("unsupported image format " + ext);
  }
}

// Saves an ldr image.
void save_imageb(const string& filename, const image<vec4b>& img) {
  auto ext = fs::path(filename).extension().string();
  if (ext == ".png" || ext == ".PNG") {
    if (!stbi_write_png(filename.c_str(), img.size().x, img.size().y, 4,
            img.data(), img.size().x * 4))
      throw std::runtime_error("error saving image " + filename);
  } else if (ext == ".jpg" || ext == ".JPG") {
    if (!stbi_write_jpg(
            filename.c_str(), img.size().x, img.size().y, 4, img.data(), 75))
      throw std::runtime_error("error saving image " + filename);
  } else if (ext == ".tga" || ext == ".TGA") {
    if (!stbi_write_tga(
            filename.c_str(), img.size().x, img.size().y, 4, img.data()))
      throw std::runtime_error("error saving image " + filename);
  } else if (ext == ".bmp" || ext == ".BMP") {
    if (!stbi_write_bmp(
            filename.c_str(), img.size().x, img.size().y, 4, img.data()))
      throw std::runtime_error("error saving image " + filename);
  } else if (is_hdr_filename(filename)) {
    save_image(filename, srgb_to_rgb(img));
  } else {
    throw std::runtime_error("unsupported image format " + ext);
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR VOLUME IMAGE IO
// -----------------------------------------------------------------------------
namespace yocto {

namespace impl {

// Volume load
static inline float* load_yvol(
    const char* filename, int* w, int* h, int* d, int* nc, int req) {
  auto fs = fopen(filename, "rb");
  if (!fs) return nullptr;
  auto fs_guard = std::unique_ptr<FILE, void (*)(FILE*)>{
      fs, [](FILE* f) { fclose(f); }};

  // buffer
  char buffer[4096];
  auto toks = vector<string>();

  // read magic
  if (!fgets(buffer, sizeof(buffer), fs)) return nullptr;
  toks = split_string(buffer);
  if (toks[0] != "YVOL") return nullptr;

  // read w, h
  if (!fgets(buffer, sizeof(buffer), fs)) return nullptr;
  toks = split_string(buffer);
  *w   = atoi(toks[0].c_str());
  *h   = atoi(toks[1].c_str());
  *d   = atoi(toks[2].c_str());
  *nc  = atoi(toks[3].c_str());

  // read data
  auto nvoxels = (size_t)(*w) * (size_t)(*h) * (size_t)(*d);
  auto nvalues = nvoxels * (size_t)(*nc);
  auto voxels  = std::unique_ptr<float[]>(new float[nvalues]);
  if (fread(voxels.get(), sizeof(float), nvalues, fs) != nvalues)
    return nullptr;

  // proper number of channels
  if (!req || *nc == req) return voxels.release();

  // pack into channels
  if (req < 0 || req > 4) {
    return nullptr;
  }
  auto cvoxels = std::unique_ptr<float[]>(new float[req * nvoxels]);
  for (auto i = 0; i < nvoxels; i++) {
    auto vp = voxels.get() + i * (*nc);
    auto cp = cvoxels.get() + i * req;
    if (*nc == 1) {
      switch (req) {
        case 1: cp[0] = vp[0]; break;
        case 2:
          cp[0] = vp[0];
          cp[1] = vp[0];
          break;
        case 3:
          cp[0] = vp[0];
          cp[1] = vp[0];
          cp[2] = vp[0];
          break;
        case 4:
          cp[0] = vp[0];
          cp[1] = vp[0];
          cp[2] = vp[0];
          cp[3] = 1;
          break;
      }
    } else if (*nc == 2) {
      switch (req) {
        case 1: cp[0] = vp[0]; break;
        case 2:
          cp[0] = vp[0];
          cp[1] = vp[1];
          break;
        case 3:
          cp[0] = vp[0];
          cp[1] = vp[1];
          break;
        case 4:
          cp[0] = vp[0];
          cp[1] = vp[1];
          break;
      }
    } else if (*nc == 3) {
      switch (req) {
        case 1: cp[0] = vp[0]; break;
        case 2:
          cp[0] = vp[0];
          cp[1] = vp[1];
          break;
        case 3:
          cp[0] = vp[0];
          cp[1] = vp[1];
          cp[2] = vp[2];
          break;
        case 4:
          cp[0] = vp[0];
          cp[1] = vp[1];
          cp[2] = vp[2];
          cp[3] = 1;
          break;
      }
    } else if (*nc == 4) {
      switch (req) {
        case 1: cp[0] = vp[0]; break;
        case 2:
          cp[0] = vp[0];
          cp[1] = vp[1];
          break;
        case 3:
          cp[0] = vp[0];
          cp[1] = vp[1];
          cp[2] = vp[2];
          break;
        case 4:
          cp[0] = vp[0];
          cp[1] = vp[1];
          cp[2] = vp[2];
          cp[3] = vp[3];
          break;
      }
    }
  }
  return cvoxels.release();
}

// save pfm
static inline bool save_yvol(
    const char* filename, int w, int h, int d, int nc, const float* voxels) {
  auto fs = fopen(filename, "wb");
  if (!fs) return false;
  auto fs_guard = std::unique_ptr<FILE, void (*)(FILE*)>{
      fs, [](FILE* f) { fclose(f); }};

  if (fprintf(fs, "YVOL\n") < 0) return false;
  if (fprintf(fs, "%d %d %d %d\n", w, h, d, nc) < 0) return false;
  auto nvalues = (size_t)w * (size_t)h * (size_t)d * (size_t)nc;
  if (fwrite(voxels, sizeof(float), nvalues, fs) != nvalues) return false;

  return true;
}

// Loads volume data from binary format.
void load_volume(const string& filename, volume<float>& vol) {
  auto width = 0, height = 0, depth = 0, ncomp = 0;
  auto voxels = load_yvol(filename.c_str(), &width, &height, &depth, &ncomp, 1);
  if (!voxels) {
    throw std::runtime_error("error loading volume " + filename);
  }
  vol = volume{{width, height, depth}, (const float*)voxels};
  delete[] voxels;
}

// Saves volume data in binary format.
void save_volume(const string& filename, const volume<float>& vol) {
  if (!save_yvol(filename.c_str(), vol.size().x, vol.size().y, vol.size().z, 1,
          vol.data())) {
    throw std::runtime_error("error saving volume " + filename);
  }
}

}  // namespace impl

// Loads volume data from binary format.
void load_volume(const string& filename, volume<float>& vol) {
  impl::load_volume(filename, vol);
}

// Saves volume data in binary format.
void save_volume(const string& filename, const volume<float>& vol) {
  impl::save_volume(filename, vol);
}

}  // namespace yocto
