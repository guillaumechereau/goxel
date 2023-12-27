//
// Implementation for Yocto/Image.
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

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_image.h"

// #include <stb_image/stb_image_resize.h>
#include "../stb/stb_image_resize.h"

#include <future>
#include <memory>
#include <stdexcept>

#include "yocto_color.h"
#include "yocto_noise.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::unique_ptr;

}  // namespace yocto

// -----------------------------------------------------------------------------
// PARALLEL HELPERS
// -----------------------------------------------------------------------------
namespace yocto {

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index.
template <typename T, typename Func>
inline void parallel_for_batch(T num, T batch, Func&& func) {
  auto              futures  = vector<std::future<void>>{};
  auto              nthreads = std::thread::hardware_concurrency();
  std::atomic<T>    next_idx(0);
  std::atomic<bool> has_error(false);
  for (auto thread_id = 0; thread_id < (int)nthreads; thread_id++) {
    futures.emplace_back(std::async(
        std::launch::async, [&func, &next_idx, &has_error, num, batch]() {
          try {
            while (true) {
              auto start = next_idx.fetch_add(batch);
              if (start >= num) break;
              if (has_error) break;
              auto end = std::min(num, start + batch);
              for (auto i = (T)start; i < end; i++) func(i);
            }
          } catch (...) {
            has_error = true;
            throw;
          }
        }));
  }
  for (auto& f : futures) f.get();
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF IMAGE DATA AND UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// image creation
image_data make_image(int width, int height, bool linear) {
  return image_data{
      width, height, linear, vector<vec4f>(width * height, vec4f{0, 0, 0, 0})};
}

// equality
bool operator==(const image_data& a, const image_data& b) {
  return a.width == b.width && a.height == b.height && a.linear == b.linear &&
         a.pixels == b.pixels;
}
bool operator!=(const image_data& a, const image_data& b) {
  return a.width != b.width || a.height != b.height || a.linear != b.linear ||
         a.pixels != b.pixels;
}

// swap
void swap(image_data& a, image_data& b) {
  std::swap(a.width, b.width);
  std::swap(a.height, b.height);
  std::swap(a.linear, b.linear);
  std::swap(a.pixels, b.pixels);
}

// conversions
image_data convert_image(const image_data& image, bool linear) {
  if (image.linear == linear) return image;
  auto result = make_image(image.width, image.height, linear);
  convert_image(result, image);
  return result;
}
void convert_image(image_data& result, const image_data& image) {
  if (image.width != result.width || image.height != result.height)
    throw std::invalid_argument{"image have to be the same size"};
  if (image.linear == result.linear) {
    result.pixels = image.pixels;
  } else {
    for (auto idx : range(image.pixels.size())) {
      result.pixels[idx] = image.linear ? rgb_to_srgb(image.pixels[idx])
                                        : srgb_to_rgb(image.pixels[idx]);
    }
  }
}

// Lookup pixel for evaluation
static vec4f lookup_image(
    const image_data& image, int i, int j, bool as_linear) {
  if (as_linear && !image.linear) {
    return srgb_to_rgb(image.pixels[j * image.width + i]);
  } else {
    return image.pixels[j * image.width + i];
  }
}

// Evaluates an image at a point `uv`.
vec4f eval_image(const image_data& image, const vec2f& uv, bool as_linear,
    bool no_interpolation, bool clamp_to_edge) {
  if (image.width == 0 || image.height == 0) return {0, 0, 0, 0};

  // get image width/height
  auto size = vec2i{image.width, image.height};

  // get coordinates normalized for tiling
  auto s = 0.0f, t = 0.0f;
  if (clamp_to_edge) {
    s = clamp(uv.x, 0.0f, 1.0f) * size.x;
    t = clamp(uv.y, 0.0f, 1.0f) * size.y;
  } else {
    s = fmod(uv.x, 1.0f) * size.x;
    if (s < 0) s += size.x;
    t = fmod(uv.y, 1.0f) * size.y;
    if (t < 0) t += size.y;
  }

  // get image coordinates and residuals
  auto i = clamp((int)s, 0, size.x - 1), j = clamp((int)t, 0, size.y - 1);
  auto ii = (i + 1) % size.x, jj = (j + 1) % size.y;
  auto u = s - i, v = t - j;

  // handle interpolation
  if (no_interpolation) {
    return lookup_image(image, i, j, as_linear);
  } else {
    return lookup_image(image, i, j, as_linear) * (1 - u) * (1 - v) +
           lookup_image(image, i, jj, as_linear) * (1 - u) * v +
           lookup_image(image, ii, j, as_linear) * u * (1 - v) +
           lookup_image(image, ii, jj, as_linear) * u * v;
  }
}

// Apply tone mapping returning a float or byte image.
image_data tonemap_image(const image_data& image, float exposure, bool filmic) {
  if (!image.linear) return image;
  auto result = make_image(image.width, image.height, false);
  for (auto idx = 0; idx < image.width * image.height; idx++) {
    result.pixels[idx] = tonemap(image.pixels[idx], exposure, filmic, true);
  }
  return result;
}

// Apply tone mapping. If the input image is an ldr, does nothing.
void tonemap_image(
    image_data& result, const image_data& image, float exposure, bool filmic) {
  if (image.width != result.width || image.height != result.height)
    throw std::invalid_argument{"image should be the same size"};
  if (result.linear) throw std::invalid_argument{"ldr expected"};
  if (image.linear) {
    for (auto idx : range(image.pixels.size())) {
      result.pixels[idx] = tonemap(image.pixels[idx], exposure, filmic);
    }
  } else {
    auto scale = vec4f{
        pow(2.0f, exposure), pow(2.0f, exposure), pow(2.0f, exposure), 1};
    for (auto idx : range(image.pixels.size())) {
      result.pixels[idx] = image.pixels[idx] * scale;
    }
  }
}
// Apply tone mapping using multithreading for speed.
void tonemap_image_mt(
    image_data& result, const image_data& image, float exposure, bool filmic) {
  if (image.width != result.width || image.height != result.height)
    throw std::invalid_argument{"image should be the same size"};
  if (result.linear) throw std::invalid_argument{"ldr expected"};
  if (image.linear) {
    parallel_for_batch((size_t)image.width * (size_t)image.height,
        (size_t)image.width, [&result, &image, exposure, filmic](size_t idx) {
          result.pixels[idx] = tonemap(image.pixels[idx], exposure, filmic);
        });
  } else {
    auto scale = vec4f{
        pow(2.0f, exposure), pow(2.0f, exposure), pow(2.0f, exposure), 1};
    parallel_for_batch((size_t)image.width * (size_t)image.height,
        (size_t)image.width, [&result, &image, scale](size_t idx) {
          result.pixels[idx] = image.pixels[idx] * scale;
        });
  }
}

// Resize an image.
image_data resize_image(
    const image_data& image, int res_width, int res_height) {
  if (res_width == 0 && res_height == 0) {
    throw std::invalid_argument{"bad image size in resize"};
  }
  if (res_height == 0) {
    res_height = (int)round(
        res_width * (double)image.height / (double)image.width);
  } else if (res_width == 0) {
    res_width = (int)round(
        res_height * (double)image.width / (double)image.height);
  }
  auto result = make_image(res_width, res_height, image.linear);
  stbir_resize_float_generic((float*)image.pixels.data(), (int)image.width,
      (int)image.height, (int)(sizeof(vec4f) * image.width),
      (float*)result.pixels.data(), (int)result.width, (int)result.height,
      (int)(sizeof(vec4f) * result.width), 4, 3, 0, STBIR_EDGE_CLAMP,
      STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
  return result;
}

// Compute the difference between two images.
image_data image_difference(
    const image_data& image1, const image_data& image2, bool display) {
  // check sizes
  if (image1.width != image2.width || image1.height != image2.height) {
    throw std::invalid_argument{"image sizes are different"};
  }

  // check types
  if (image1.linear != image2.linear) {
    throw std::invalid_argument{"image types are different"};
  }

  // compute diff
  auto difference = make_image(image1.width, image1.height, image1.linear);
  for (auto idx : range(difference.pixels.size())) {
    auto diff              = abs(image1.pixels[idx] - image2.pixels[idx]);
    difference.pixels[idx] = display ? vec4f{max(diff), max(diff), max(diff), 1}
                                     : diff;
  }
  return difference;
}

void set_region(image_data& image, const image_data& region, int x, int y) {
  for (auto j : range(region.height)) {
    for (auto i : range(region.width)) {
      image.pixels[(j + y) * image.width + (i + x)] =
          region.pixels[j * region.width + i];
    }
  }
}

void get_region(image_data& region, const image_data& image, int x, int y,
    int width, int height) {
  if (region.width != width || region.height != height) {
    region = make_image(width, height, image.linear);
  }
  for (auto j : range(height)) {
    for (auto i : range(width)) {
      region.pixels[j * region.width + i] =
          image.pixels[(j + y) * image.width + (i + x)];
    }
  }
}

// Composite two images together.
image_data composite_image(
    const image_data& image_a, const image_data& image_b) {
  if (image_a.width != image_b.width || image_a.height != image_b.height)
    throw std::invalid_argument{"image should be the same size"};
  if (image_a.linear != image_b.linear)
    throw std::invalid_argument{"image should be of the same type"};
  auto result = make_image(image_a.width, image_a.height, image_a.linear);
  for (auto idx : range(result.pixels.size())) {
    result.pixels[idx] = composite(image_a.pixels[idx], image_b.pixels[idx]);
  }
  return result;
}

// Composite two images together.
void composite_image(
    image_data& result, const image_data& image_a, const image_data& image_b) {
  if (image_a.width != image_b.width || image_a.height != image_b.height)
    throw std::invalid_argument{"image should be the same size"};
  if (image_a.linear != image_b.linear)
    throw std::invalid_argument{"image should be of the same type"};
  if (image_a.width != result.width || image_a.height != result.height)
    throw std::invalid_argument{"image should be the same size"};
  if (image_a.linear != result.linear)
    throw std::invalid_argument{"image should be of the same type"};
  for (auto idx : range(result.pixels.size())) {
    result.pixels[idx] = composite(image_a.pixels[idx], image_b.pixels[idx]);
  }
}

// Apply color grading from a linear or srgb color to an srgb color.
vec4f colorgradeb(
    const vec4f& color, bool linear, const colorgrade_params& params) {
  auto rgb   = xyz(color);
  auto alpha = color.w;
  if (linear) {
    if (params.exposure != 0) rgb *= exp2(params.exposure);
    if (params.tint != vec3f{1, 1, 1}) rgb *= params.tint;
    if (params.lincontrast != 0.5f)
      rgb = lincontrast(rgb, params.lincontrast, 0.18f);
    if (params.logcontrast != 0.5f)
      rgb = logcontrast(rgb, params.logcontrast, 0.18f);
    if (params.linsaturation != 0.5f) rgb = saturate(rgb, params.linsaturation);
    if (params.filmic) rgb = tonemap_filmic(rgb);
    if (params.srgb) rgb = rgb_to_srgb(rgb);
  }
  if (params.contrast != 0.5f) rgb = contrast(rgb, params.contrast);
  if (params.saturation != 0.5f) rgb = saturate(rgb, params.saturation);
  if (params.shadows != 0.5f || params.midtones != 0.5f ||
      params.highlights != 0.5f || params.shadows_color != vec3f{1, 1, 1} ||
      params.midtones_color != vec3f{1, 1, 1} ||
      params.highlights_color != vec3f{1, 1, 1}) {
    auto lift  = params.shadows_color;
    auto gamma = params.midtones_color;
    auto gain  = params.highlights_color;
    lift       = lift - mean(lift) + params.shadows - (float)0.5;
    gain       = gain - mean(gain) + params.highlights + (float)0.5;
    auto grey  = gamma - mean(gamma) + params.midtones;
    gamma      = log(((float)0.5 - lift) / (gain - lift)) / log(grey);
    // apply_image
    auto lerp_value = clamp(pow(rgb, 1 / gamma), (float)0, (float)1);
    rgb             = gain * lerp_value + lift * (1 - lerp_value);
  }
  return vec4f{rgb.x, rgb.y, rgb.z, alpha};
}

// Color grade an hsr or ldr image to an ldr image.
image_data colorgrade_image(
    const image_data& image, const colorgrade_params& params) {
  auto result = make_image(image.width, image.height, false);
  for (auto idx : range(image.pixels.size())) {
    result.pixels[idx] = colorgrade(image.pixels[idx], image.linear, params);
  }
  return result;
}

// Color grade an hsr or ldr image to an ldr image.
// Uses multithreading for speed.
void colorgrade_image(image_data& result, const image_data& image,
    const colorgrade_params& params) {
  if (image.width != result.width || image.height != result.height)
    throw std::invalid_argument{"image should be the same size"};
  if (!!result.linear) throw std::invalid_argument{"non linear expected"};
  for (auto idx : range(image.pixels.size())) {
    result.pixels[idx] = colorgrade(image.pixels[idx], image.linear, params);
  }
}

// Color grade an hsr or ldr image to an ldr image.
// Uses multithreading for speed.
void colorgrade_image_mt(image_data& result, const image_data& image,
    const colorgrade_params& params) {
  if (image.width != result.width || image.height != result.height)
    throw std::invalid_argument{"image should be the same size"};
  if (!!result.linear) throw std::invalid_argument{"non linear expected"};
  parallel_for_batch((size_t)image.width * (size_t)image.height,
      (size_t)image.width, [&result, &image, &params](size_t idx) {
        result.pixels[idx] = colorgrade(
            image.pixels[idx], image.linear, params);
      });
}

// determine white balance colors
vec4f compute_white_balance(const image_data& image) {
  auto rgb = vec3f{0, 0, 0};
  for (auto idx = (size_t)0; image.pixels.size(); idx++) {
    rgb += xyz(image.pixels[idx]);
  }
  if (rgb == vec3f{0, 0, 0}) return {0, 0, 0, 1};
  rgb /= max(rgb);
  return {rgb.x, rgb.y, rgb.z, 1};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMAGE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Comvert a bump map to a normal map.
void bump_to_normal(
    image_data& normalmap, const image_data& bumpmap, float scale) {
  auto width = bumpmap.width, height = bumpmap.height;
  if (normalmap.width != bumpmap.width || normalmap.height != bumpmap.height) {
    normalmap = make_image(width, height, bumpmap.linear);
  }
  auto dx = 1.0f / width, dy = 1.0f / height;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      auto i1 = (i + 1) % width, j1 = (j + 1) % height;
      auto p00    = bumpmap.pixels[j * bumpmap.width + i],
           p10    = bumpmap.pixels[j * bumpmap.width + i1],
           p01    = bumpmap.pixels[j1 * bumpmap.width + i];
      auto g00    = (p00.x + p00.y + p00.z) / 3;
      auto g01    = (p01.x + p01.y + p01.z) / 3;
      auto g10    = (p10.x + p10.y + p10.z) / 3;
      auto normal = vec3f{
          scale * (g00 - g10) / dx, scale * (g00 - g01) / dy, 1.0f};
      normal.y = -normal.y;  // make green pointing up, even if y axis
                             // points down
      normal = normalize(normal) * 0.5f + vec3f{0.5f, 0.5f, 0.5f};
      set_pixel(normalmap, i, j, {normal.x, normal.y, normal.z, 1});
    }
  }
}
image_data bump_to_normal(const image_data& bumpmap, float scale) {
  auto normalmap = make_image(bumpmap.width, bumpmap.height, bumpmap.linear);
  bump_to_normal(normalmap, bumpmap, scale);
  return normalmap;
}

template <typename Shader>
static image_data make_proc_image(
    int width, int height, bool linear, Shader&& shader) {
  auto image = make_image(width, height, linear);
  auto scale = 1.0f / max(width, height);
  for (auto j : range(height)) {
    for (auto i : range(width)) {
      auto uv                     = vec2f{i * scale, j * scale};
      image.pixels[j * width + i] = shader(uv);
    }
  }
  return image;
}

// Make an image
image_data make_grid(int width, int height, float scale, const vec4f& color0,
    const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 4 * scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto thick = 0.01f / 2;
    auto c     = uv.x <= thick || uv.x >= 1 - thick || uv.y <= thick ||
             uv.y >= 1 - thick ||
             (uv.x >= 0.5f - thick && uv.x <= 0.5f + thick) ||
             (uv.y >= 0.5f - thick && uv.y <= 0.5f + thick);
    return c ? color0 : color1;
  });
}

image_data make_checker(int width, int height, float scale, const vec4f& color0,
    const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 4 * scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto c = uv.x <= 0.5f != uv.y <= 0.5f;
    return c ? color0 : color1;
  });
}

image_data make_bumps(int width, int height, float scale, const vec4f& color0,
    const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 4 * scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto thick  = 0.125f;
    auto center = vec2f{
        uv.x <= 0.5f ? 0.25f : 0.75f,
        uv.y <= 0.5f ? 0.25f : 0.75f,
    };
    auto dist = clamp(length(uv - center), 0.0f, thick) / thick;
    auto val  = uv.x <= 0.5f != uv.y <= 0.5f ? (1 + sqrt(1 - dist)) / 2
                                             : (dist * dist) / 2;
    return lerp(color0, color1, val);
  });
}

image_data make_ramp(int width, int height, float scale, const vec4f& color0,
    const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    return lerp(color0, color1, uv.x);
  });
}

image_data make_gammaramp(int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(width, height, false, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    if (uv.y < 1 / 3.0f) {
      return lerp(color0, color1, pow(uv.x, 2.2f));
    } else if (uv.y < 2 / 3.0f) {
      return lerp(color0, color1, uv.x);
    } else {
      return lerp(color0, color1, pow(uv.x, 1 / 2.2f));
    }
  });
}

image_data make_uvramp(int width, int height, float scale) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    return vec4f{uv.x, uv.y, 0, 1};
  });
}

image_data make_uvgrid(int width, int height, float scale, bool colored) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    uv.y     = 1 - uv.y;
    auto hsv = vec3f{0, 0, 0};
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
    auto sc = suv.x <= st || suv.x >= 1 - st || suv.y <= st || suv.y >= 1 - st;
    if (sc) {
      hsv.y = 0.2f;
      hsv.z = 0.8f;
    } else {
      hsv.y = 0.8f;
    }
    auto rgb = (colored) ? hsv_to_rgb(hsv) : vec3f{hsv.z, hsv.z, hsv.z};
    return vec4f{rgb.x, rgb.y, rgb.z, 1};
  });
}

image_data make_blackbodyramp(
    int width, int height, float scale, float from, float to) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto rgb = blackbody_to_rgb(lerp(from, to, uv.x));
    return vec4f{rgb.x, rgb.y, rgb.z, 1};
  });
}

image_data make_colormapramp(int width, int height, float scale) {
  return make_proc_image(width, height, false, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto rgb = vec3f{0, 0, 0};
    if (uv.y < 0.25) {
      rgb = colormap(uv.x, colormap_type::viridis);
    } else if (uv.y < 0.50) {
      rgb = colormap(uv.x, colormap_type::plasma);
    } else if (uv.y < 0.75) {
      rgb = colormap(uv.x, colormap_type::magma);
    } else {
      rgb = colormap(uv.x, colormap_type::inferno);
    }
    return vec4f{rgb.x, rgb.y, rgb.z, 1};
  });
}

image_data make_noisemap(int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_noise(vec3f{uv.x, uv.y, 0});
    v      = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

image_data make_fbmmap(int width, int height, float scale, const vec4f& noise,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_fbm({uv.x, uv.y, 0}, noise.x, noise.y, (int)noise.z);
    v      = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

image_data make_turbulencemap(int width, int height, float scale,
    const vec4f& noise, const vec4f& color0, const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_turbulence({uv.x, uv.y, 0}, noise.x, noise.y, (int)noise.z);
    v      = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

image_data make_ridgemap(int width, int height, float scale, const vec4f& noise,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(width, height, true, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_ridge(
        {uv.x, uv.y, 0}, noise.x, noise.y, (int)noise.z, noise.w);
    v = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

// Add image border
image_data add_border(
    const image_data& image, float width, const vec4f& color) {
  auto result = image;
  auto scale  = 1.0f / max(image.width, image.height);
  for (auto j : range(image.height)) {
    for (auto i : range(image.width)) {
      auto uv = vec2f{i * scale, j * scale};
      if (uv.x < width || uv.y < width || uv.x > image.width * scale - width ||
          uv.y > image.height * scale - width) {
        set_pixel(result, i, j, color);
      }
    }
  }
  return result;
}

// Implementation of sunsky modified heavily from pbrt
image_data make_sunsky(int width, int height, float theta_sun, float turbidity,
    bool has_sun, float sun_intensity, float sun_radius,
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
  sun_angular_radius = max(sun_angular_radius, 2 * pif / height);

  // sun direction
  auto sun_direction = vec3f{0, cos(theta_sun), sin(theta_sun)};

  auto sun = [has_sun, sun_angular_radius, sun_le](auto theta, auto gamma) {
    return (has_sun && gamma < sun_angular_radius) ? sun_le / 10000
                                                   : vec3f{0, 0, 0};
  };

  // Make the sun sky image
  auto img = make_image(width, height, true);
  for (auto j = 0; j < height / 2; j++) {
    auto theta = pif * ((j + 0.5f) / height);
    theta      = clamp(theta, 0.0f, pif / 2 - flt_eps);
    for (int i = 0; i < width; i++) {
      auto phi = 2 * pif * (float(i + 0.5f) / width);
      auto w = vec3f{cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta)};
      auto gamma   = acos(clamp(dot(w, sun_direction), -1.0f, 1.0f));
      auto sky_col = sky(theta, gamma, theta_sun);
      auto sun_col = sun(theta, gamma);
      auto col     = sky_col + sun_col;
      img.pixels[j * width + i] = {col.x, col.y, col.z, 1};
    }
  }

  if (ground_albedo != vec3f{0, 0, 0}) {
    auto ground = vec3f{0, 0, 0};
    for (auto j = 0; j < height / 2; j++) {
      auto theta = pif * ((j + 0.5f) / height);
      for (int i = 0; i < width; i++) {
        auto pxl   = img.pixels[j * width + i];
        auto le    = vec3f{pxl.x, pxl.y, pxl.z};
        auto angle = sin(theta) * 4 * pif / (width * height);
        ground += le * (ground_albedo / pif) * cos(theta) * angle;
      }
    }
    for (auto j = height / 2; j < height; j++) {
      for (int i = 0; i < width; i++) {
        img.pixels[j * width + i] = {ground.x, ground.y, ground.z, 1};
      }
    }
  } else {
    for (auto j = height / 2; j < height; j++) {
      for (int i = 0; i < width; i++) {
        img.pixels[j * width + i] = {0, 0, 0, 1};
      }
    }
  }

  // done
  return img;
}

// Make an image of multiple lights.
image_data make_lights(int width, int height, const vec3f& le, int nlights,
    float langle, float lwidth, float lheight) {
  auto img = make_image(width, height, true);
  for (auto j = 0; j < height / 2; j++) {
    auto theta = pif * ((j + 0.5f) / height);
    theta      = clamp(theta, 0.0f, pif / 2 - 0.00001f);
    if (fabs(theta - langle) > lheight / 2) continue;
    for (int i = 0; i < width; i++) {
      auto phi     = 2 * pif * (float(i + 0.5f) / width);
      auto inlight = false;
      for (auto l : range(nlights)) {
        auto lphi = 2 * pif * (l + 0.5f) / nlights;
        inlight   = inlight || fabs(phi - lphi) < lwidth / 2;
      }
      img.pixels[j * width + i] = {le.x, le.y, le.z, 1};
    }
  }
  return img;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMAGE SAMPLING
// -----------------------------------------------------------------------------
namespace yocto {

// Lookup an image at coordinates `ij`
static vec4f lookup_image(const vector<vec4f>& img, int width, int height,
    int i, int j, bool as_linear) {
  return img[j * width * i];
}
static vec4f lookup_image(const vector<vec4b>& img, int width, int height,
    int i, int j, bool as_linear) {
  if (as_linear) {
    return srgb_to_rgb(byte_to_float(img[j * width * i]));
  } else {
    return byte_to_float(img[j * width * i]);
  }
}

// Evaluate a texture
template <typename T>
static vec4f eval_image_generic(const vector<T>& img, int width, int height,
    const vec2f& uv, bool as_linear, bool no_interpolation,
    bool clamp_to_edge) {
  if (img.empty()) return vec4f{0, 0, 0, 0};

  // get image width/height
  auto size = vec2i{width, height};

  // get coordinates normalized for tiling
  auto s = 0.0f, t = 0.0f;
  if (clamp_to_edge) {
    s = clamp(uv.x, 0.0f, 1.0f) * size.x;
    t = clamp(uv.y, 0.0f, 1.0f) * size.y;
  } else {
    s = fmod(uv.x, 1.0f) * size.x;
    if (s < 0) s += size.x;
    t = fmod(uv.y, 1.0f) * size.y;
    if (t < 0) t += size.y;
  }

  // get image coordinates and residuals
  auto i = clamp((int)s, 0, size.x - 1), j = clamp((int)t, 0, size.y - 1);
  auto ii = (i + 1) % size.x, jj = (j + 1) % size.y;
  auto u = s - i, v = t - j;

  if (no_interpolation)
    return lookup_image(img, width, height, i, j, as_linear);

  // handle interpolation
  return lookup_image(img, width, height, i, j, as_linear) * (1 - u) * (1 - v) +
         lookup_image(img, width, height, i, jj, as_linear) * (1 - u) * v +
         lookup_image(img, width, height, ii, j, as_linear) * u * (1 - v) +
         lookup_image(img, width, height, ii, jj, as_linear) * u * v;
}

// Evaluates a color image at a point `uv`.
vec4f eval_image(const vector<vec4f>& img, int width, int height,
    const vec2f& uv, bool no_interpolation, bool clamp_to_edge) {
  return eval_image_generic(
      img, width, height, uv, false, no_interpolation, clamp_to_edge);
}
vec4f eval_image(const vector<vec4b>& img, int width, int height,
    const vec2f& uv, bool as_linear, bool no_interpolation,
    bool clamp_to_edge) {
  return eval_image_generic(
      img, width, height, uv, as_linear, no_interpolation, clamp_to_edge);
}

// Conversion from/to floats.
void byte_to_float(vector<vec4f>& fl, const vector<vec4b>& bt) {
  fl.resize(bt.size());
  for (auto i : range(fl.size())) fl[i] = byte_to_float(bt[i]);
}
void float_to_byte(vector<vec4b>& bt, const vector<vec4f>& fl) {
  bt.resize(fl.size());
  for (auto i : range(bt.size())) bt[i] = float_to_byte(fl[i]);
}

// Conversion between linear and gamma-encoded images.
void srgb_to_rgb(vector<vec4f>& rgb, const vector<vec4f>& srgb) {
  rgb.resize(srgb.size());
  for (auto i : range(rgb.size())) rgb[i] = srgb_to_rgb(srgb[i]);
}
void rgb_to_srgb(vector<vec4f>& srgb, const vector<vec4f>& rgb) {
  srgb.resize(rgb.size());
  for (auto i : range(srgb.size())) srgb[i] = rgb_to_srgb(rgb[i]);
}
void srgb_to_rgb(vector<vec4f>& rgb, const vector<vec4b>& srgb) {
  rgb.resize(srgb.size());
  for (auto i : range(rgb.size())) rgb[i] = srgb_to_rgb(byte_to_float(srgb[i]));
}
void rgb_to_srgb(vector<vec4b>& srgb, const vector<vec4f>& rgb) {
  srgb.resize(rgb.size());
  for (auto i : range(srgb.size()))
    srgb[i] = float_to_byte(rgb_to_srgb(rgb[i]));
}

// Apply exposure and filmic tone mapping
void tonemap_image(vector<vec4f>& ldr, const vector<vec4f>& hdr, float exposure,
    bool filmic, bool srgb) {
  ldr.resize(hdr.size());
  for (auto i : range(hdr.size()))
    ldr[i] = tonemap(hdr[i], exposure, filmic, srgb);
}
void tonemap_image(vector<vec4b>& ldr, const vector<vec4f>& hdr, float exposure,
    bool filmic, bool srgb) {
  ldr.resize(hdr.size());
  for (auto i : range(hdr.size()))
    ldr[i] = float_to_byte(tonemap(hdr[i], exposure, filmic, srgb));
}

void tonemap_image_mt(vector<vec4f>& ldr, const vector<vec4f>& hdr,
    float exposure, bool filmic, bool srgb) {
  parallel_for_batch(hdr.size(), (size_t)1024,
      [&](size_t i) { ldr[i] = tonemap(hdr[i], exposure, filmic, srgb); });
}
void tonemap_image_mt(vector<vec4b>& ldr, const vector<vec4f>& hdr,
    float exposure, bool filmic, bool srgb) {
  parallel_for_batch(hdr.size(), (size_t)1024, [&](size_t i) {
    ldr[i] = float_to_byte(tonemap(hdr[i], exposure, filmic, srgb));
  });
}

// Apply exposure and filmic tone mapping
void colorgrade_image(vector<vec4f>& corrected, const vector<vec4f>& img,
    bool linear, const colorgrade_params& params) {
  corrected.resize(img.size());
  for (auto i : range(img.size()))
    corrected[i] = colorgrade(img[i], linear, params);
}

// Apply exposure and filmic tone mapping
void colorgrade_image_mt(vector<vec4f>& corrected, const vector<vec4f>& img,
    bool linear, const colorgrade_params& params) {
  parallel_for_batch(img.size(), (size_t)1024,
      [&](size_t i) { corrected[i] = colorgrade(img[i], linear, params); });
}
void colorgrade_image_mt(vector<vec4b>& corrected, const vector<vec4f>& img,
    bool linear, const colorgrade_params& params) {
  parallel_for_batch(img.size(), (size_t)1024, [&](size_t i) {
    corrected[i] = float_to_byte(colorgrade(img[i], linear, params));
  });
}

// compute white balance
vec3f compute_white_balance(const vector<vec4f>& img) {
  auto rgb = vec3f{0, 0, 0};
  for (auto& p : img) rgb += xyz(p);
  if (rgb == vec3f{0, 0, 0}) return {0, 0, 0};
  return rgb / max(rgb);
}

void resize_image(vector<vec4f>& res, const vector<vec4f>& img, int width,
    int height, int res_width, int res_height) {
  if (res_width == 0 && res_height == 0) {
    throw std::invalid_argument{"bad image size in resize"};
  }
  if (res_height == 0) {
    res_height = (int)round(res_width * (double)height / (double)width);
  } else if (res_width == 0) {
    res_width = (int)round(res_height * (double)width / (double)height);
  }
  res.resize((size_t)res_width * (size_t)res_height);
  stbir_resize_float_generic((float*)img.data(), width, height,
      sizeof(vec4f) * width, (float*)res.data(), res_width, res_height,
      sizeof(vec4f) * res_width, 4, 3, 0, STBIR_EDGE_CLAMP,
      STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
}
void resize_image(vector<vec4b>& res, const vector<vec4b>& img, int width,
    int height, int res_width, int res_height) {
  if (res_width == 0 && res_height == 0) {
    throw std::invalid_argument{"bad image size in resize"};
  }
  if (res_height == 0) {
    res_height = (int)round(res_width * (double)height / (double)width);
  } else if (res_width == 0) {
    res_width = (int)round(res_height * (double)width / (double)height);
  }
  res.resize((size_t)res_width * (size_t)res_height);
  stbir_resize_uint8_generic((byte*)img.data(), width, height,
      sizeof(vec4b) * width, (byte*)res.data(), res_width, res_height,
      sizeof(vec4b) * res_width, 4, 3, 0, STBIR_EDGE_CLAMP,
      STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
}

void image_difference(vector<vec4f>& diff, const vector<vec4f>& a,
    const vector<vec4f>& b, bool display) {
  if (a.size() != b.size())
    throw std::invalid_argument{"image haev different sizes"};
  diff.resize(a.size());
  for (auto i : range(diff.size())) diff[i] = abs(a[i] - b[i]);
  if (display) {
    for (auto i : range(diff.size())) {
      auto d  = max(diff[i]);
      diff[i] = {d, d, d, 1};
    }
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR IMAGE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Comvert a bump map to a normal map.
void bump_to_normal(vector<vec4f>& normalmap, const vector<vec4f>& bumpmap,
    int width, int height, float scale) {
  auto dx = 1.0f / width, dy = 1.0f / height;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      auto i1 = (i + 1) % width, j1 = (j + 1) % height;
      auto p00 = bumpmap[j * width + i], p10 = bumpmap[j * width + i1],
           p01    = bumpmap[j1 * width + i];
      auto g00    = (p00.x + p00.y + p00.z) / 3;
      auto g01    = (p01.x + p01.y + p01.z) / 3;
      auto g10    = (p10.x + p10.y + p10.z) / 3;
      auto normal = vec3f{
          scale * (g00 - g10) / dx, scale * (g00 - g01) / dy, 1.0f};
      normal.y = -normal.y;  // make green pointing up, even if y axis
                             // points down
      normal = normalize(normal) * 0.5f + vec3f{0.5f, 0.5f, 0.5f};
      normalmap[j * width + i] = {normal.x, normal.y, normal.z, 1};
    }
  }
}

template <typename Shader>
static void make_proc_image(
    vector<vec4f>& pixels, int width, int height, Shader&& shader) {
  pixels.resize((size_t)width * (size_t)height);
  auto scale = 1.0f / max(width, height);
  for (auto j : range(height)) {
    for (auto i : range(width)) {
      auto uv               = vec2f{i * scale, j * scale};
      pixels[j * width + i] = shader(uv);
    }
  }
}

// Make an image
void make_grid(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 4 * scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto thick = 0.01f / 2;
    auto c     = uv.x <= thick || uv.x >= 1 - thick || uv.y <= thick ||
             uv.y >= 1 - thick ||
             (uv.x >= 0.5f - thick && uv.x <= 0.5f + thick) ||
             (uv.y >= 0.5f - thick && uv.y <= 0.5f + thick);
    return c ? color0 : color1;
  });
}

void make_checker(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 4 * scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto c = uv.x <= 0.5f != uv.y <= 0.5f;
    return c ? color0 : color1;
  });
}

void make_bumps(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 4 * scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto thick  = 0.125f;
    auto center = vec2f{
        uv.x <= 0.5f ? 0.25f : 0.75f,
        uv.y <= 0.5f ? 0.25f : 0.75f,
    };
    auto dist = clamp(length(uv - center), 0.0f, thick) / thick;
    auto val  = uv.x <= 0.5f != uv.y <= 0.5f ? (1 + sqrt(1 - dist)) / 2
                                             : (dist * dist) / 2;
    return lerp(color0, color1, val);
  });
}

void make_ramp(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    return lerp(color0, color1, uv.x);
  });
}

void make_gammaramp(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    if (uv.y < 1 / 3.0f) {
      return lerp(color0, color1, pow(uv.x, 2.2f));
    } else if (uv.y < 2 / 3.0f) {
      return lerp(color0, color1, uv.x);
    } else {
      return lerp(color0, color1, pow(uv.x, 1 / 2.2f));
    }
  });
}

void make_uvramp(vector<vec4f>& pixels, int width, int height, float scale) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    return vec4f{uv.x, uv.y, 0, 1};
  });
}

void make_uvgrid(
    vector<vec4f>& pixels, int width, int height, float scale, bool colored) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    uv.y     = 1 - uv.y;
    auto hsv = vec3f{0, 0, 0};
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
    auto sc = suv.x <= st || suv.x >= 1 - st || suv.y <= st || suv.y >= 1 - st;
    if (sc) {
      hsv.y = 0.2f;
      hsv.z = 0.8f;
    } else {
      hsv.y = 0.8f;
    }
    auto rgb = (colored) ? hsv_to_rgb(hsv) : vec3f{hsv.z, hsv.z, hsv.z};
    return vec4f{rgb.x, rgb.y, rgb.z, 1};
  });
}

void make_blackbodyramp(vector<vec4f>& pixels, int width, int height,
    float scale, float from, float to) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto rgb = blackbody_to_rgb(lerp(from, to, uv.x));
    return vec4f{rgb.x, rgb.y, rgb.z, 1};
  });
}

void make_colormapramp(
    vector<vec4f>& pixels, int width, int height, float scale) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= scale;
    uv -= vec2f{(float)(int)uv.x, (float)(int)uv.y};
    auto rgb = vec3f{0, 0, 0};
    if (uv.y < 0.25) {
      rgb = colormap(uv.x, colormap_type::viridis);
    } else if (uv.y < 0.50) {
      rgb = colormap(uv.x, colormap_type::plasma);
    } else if (uv.y < 0.75) {
      rgb = colormap(uv.x, colormap_type::magma);
    } else {
      rgb = colormap(uv.x, colormap_type::inferno);
    }
    return vec4f{rgb.x, rgb.y, rgb.z, 1};
  });
}

void make_noisemap(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_noise(vec3f{uv.x, uv.y, 0});
    v      = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

void make_fbmmap(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& noise, const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_fbm({uv.x, uv.y, 0}, noise.x, noise.y, (int)noise.z);
    v      = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

void make_turbulencemap(vector<vec4f>& pixels, int width, int height,
    float scale, const vec4f& noise, const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_turbulence({uv.x, uv.y, 0}, noise.x, noise.y, (int)noise.z);
    v      = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

void make_ridgemap(vector<vec4f>& pixels, int width, int height, float scale,
    const vec4f& noise, const vec4f& color0, const vec4f& color1) {
  return make_proc_image(pixels, width, height, [=](vec2f uv) {
    uv *= 8 * scale;
    auto v = perlin_ridge(
        {uv.x, uv.y, 0}, noise.x, noise.y, (int)noise.z, noise.w);
    v = clamp(v, 0.0f, 1.0f);
    return lerp(color0, color1, v);
  });
}

// Add image border
void add_border(vector<vec4f>& pixels, const vector<vec4f>& source, int width,
    int height, float thickness, const vec4f& color) {
  pixels     = source;
  auto scale = 1.0f / max(width, height);
  for (auto j : range(height)) {
    for (auto i : range(width)) {
      auto uv = vec2f{i * scale, j * scale};
      if (uv.x < thickness || uv.y < thickness ||
          uv.x > width * scale - thickness ||
          uv.y > height * scale - thickness) {
        pixels[j * width + i] = color;
      }
    }
  }
}

// Implementation of sunsky modified heavily from pbrt
void make_sunsky(vector<vec4f>& pixels, int width, int height, float theta_sun,
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
  sun_angular_radius = max(sun_angular_radius, 2 * pif / height);

  // sun direction
  auto sun_direction = vec3f{0, cos(theta_sun), sin(theta_sun)};

  auto sun = [has_sun, sun_angular_radius, sun_le](auto theta, auto gamma) {
    return (has_sun && gamma < sun_angular_radius) ? sun_le / 10000
                                                   : vec3f{0, 0, 0};
  };

  // Make the sun sky image
  pixels.resize(width * height);
  for (auto j = 0; j < height / 2; j++) {
    auto theta = pif * ((j + 0.5f) / height);
    theta      = clamp(theta, 0.0f, pif / 2 - flt_eps);
    for (int i = 0; i < width; i++) {
      auto phi = 2 * pif * (float(i + 0.5f) / width);
      auto w = vec3f{cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta)};
      auto gamma            = acos(clamp(dot(w, sun_direction), -1.0f, 1.0f));
      auto sky_col          = sky(theta, gamma, theta_sun);
      auto sun_col          = sun(theta, gamma);
      auto col              = sky_col + sun_col;
      pixels[j * width + i] = {col.x, col.y, col.z, 1};
    }
  }

  if (ground_albedo != vec3f{0, 0, 0}) {
    auto ground = vec3f{0, 0, 0};
    for (auto j = 0; j < height / 2; j++) {
      auto theta = pif * ((j + 0.5f) / height);
      for (int i = 0; i < width; i++) {
        auto pxl   = pixels[j * width + i];
        auto le    = vec3f{pxl.x, pxl.y, pxl.z};
        auto angle = sin(theta) * 4 * pif / (width * height);
        ground += le * (ground_albedo / pif) * cos(theta) * angle;
      }
    }
    for (auto j = height / 2; j < height; j++) {
      for (int i = 0; i < width; i++) {
        pixels[j * width + i] = {ground.x, ground.y, ground.z, 1};
      }
    }
  } else {
    for (auto j = height / 2; j < height; j++) {
      for (int i = 0; i < width; i++) {
        pixels[j * width + i] = {0, 0, 0, 1};
      }
    }
  }
}

// Make an image of multiple lights.
void make_lights(vector<vec4f>& pixels, int width, int height, const vec3f& le,
    int nlights, float langle, float lwidth, float lheight) {
  pixels.resize(width * height);
  for (auto j = 0; j < height / 2; j++) {
    auto theta = pif * ((j + 0.5f) / height);
    theta      = clamp(theta, 0.0f, pif / 2 - 0.00001f);
    if (fabs(theta - langle) > lheight / 2) continue;
    for (int i = 0; i < width; i++) {
      auto phi     = 2 * pif * (float(i + 0.5f) / width);
      auto inlight = false;
      for (auto l : range(nlights)) {
        auto lphi = 2 * pif * (l + 0.5f) / nlights;
        inlight   = inlight || fabs(phi - lphi) < lwidth / 2;
      }
      pixels[j * width + i] = {le.x, le.y, le.z, 1};
    }
  }
}

}  // namespace yocto

#if 0

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR DEPRECATED CODE
// -----------------------------------------------------------------------------
namespace yocto {

image<vec4f> filter_bilateral(const image<vec4f>& img, float spatial_sigma,
    float range_sigma, const vector<image<vec4f>>& features,
    const vector<float>& features_sigma) {
  auto filtered     = image{img.imsize(), vec4f{0,0,0,0}};
  auto filter_width = (int)ceil(2.57f * spatial_sigma);
  auto sw           = 1 / (2.0f * spatial_sigma * spatial_sigma);
  auto rw           = 1 / (2.0f * range_sigma * range_sigma);
  auto fw           = vector<float>();
  for (auto feature_sigma : features_sigma)
    fw.push_back(1 / (2.0f * feature_sigma * feature_sigma));
  for (auto j = 0; j < img.height(); j++) {
    for (auto i = 0; i < img.width(); i++) {
      auto av = vec4f{0,0,0,0};
      auto aw = 0.0f;
      for (auto fj = -filter_width; fj <= filter_width; fj++) {
        for (auto fi = -filter_width; fi <= filter_width; fi++) {
          auto ii = i + fi, jj = j + fj;
          if (ii < 0 || jj < 0) continue;
          if (ii >= img.width() || jj >= img.height()) continue;
          auto uv  = vec2f{float(i - ii), float(j - jj)};
          auto rgb = img[{i, j}] - img[{i, j}];
          auto w   = (float)exp(-dot(uv, uv) * sw) *
                   (float)exp(-dot(rgb, rgb) * rw);
          for (auto fi = 0; fi < features.size(); fi++) {
            auto feat = features[fi][{i, j}] - features[fi][{i, j}];
            w *= exp(-dot(feat, feat) * fw[fi]);
          }
          av += w * img[{ii, jj}];
          aw += w;
        }
      }
      filtered[{i, j}] = av / aw;
    }
  }
  return filtered;
}

image<vec4f> filter_bilateral(
    const image<vec4f>& img, float spatial_sigma, float range_sigma) {
  auto filtered = image{img.imsize(), vec4f{0,0,0,0}};
  auto fwidth   = (int)ceil(2.57f * spatial_sigma);
  auto sw       = 1 / (2.0f * spatial_sigma * spatial_sigma);
  auto rw       = 1 / (2.0f * range_sigma * range_sigma);
  for (auto j = 0; j < img.height(); j++) {
    for (auto i = 0; i < img.width(); i++) {
      auto av = vec4f{0,0,0,0};
      auto aw = 0.0f;
      for (auto fj = -fwidth; fj <= fwidth; fj++) {
        for (auto fi = -fwidth; fi <= fwidth; fi++) {
          auto ii = i + fi, jj = j + fj;
          if (ii < 0 || jj < 0) continue;
          if (ii >= img.width() || jj >= img.height()) continue;
          auto uv  = vec2f{float(i - ii), float(j - jj)};
          auto rgb = img[{i, j}] - img[{ii, jj}];
          auto w   = exp(-dot(uv, uv) * sw) * exp(-dot(rgb, rgb) * rw);
          av += w * img[{ii, jj}];
          aw += w;
        }
      }
      filtered[{i, j}] = av / aw;
    }
  }
  return filtered;
}

}  // namespace yocto

#endif
