//
// # Yocto/Image : Image utilities
//
// Yocto/Image is a collection of image utilities useful when writing rendering
// algorithms. These include a simple image data structure, color conversion
// utilities and tone mapping, loading and saving functionality, and image
// resizing.
// Yocto/Image is implemented in `yocto_image.h` and `yocto_image.cpp`, and
// depends on `stb_image_resize.h` for image resizing.
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

#ifndef _YOCTO_IMAGE_H_
#define _YOCTO_IMAGE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <string>
#include <utility>
#include <vector>

#include "yocto_color.h"
#include "yocto_math.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::string;
using std::vector;

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMAGE DATA AND UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Image data as array of float or byte pixels. Images can be stored in linear
// or non linear color space.
struct image_data {
  // image data
  int           width  = 0;
  int           height = 0;
  bool          linear = false;
  vector<vec4f> pixels = {};

  // pixel access
  vec4f&       operator[](vec2i ij);
  const vec4f& operator[](vec2i ij) const;
};

// image creation
image_data make_image(int width, int height, bool linear);

// equality
bool operator==(const image_data& a, const image_data& b);
bool operator!=(const image_data& a, const image_data& b);

// swap
void swap(image_data& a, image_data& b);

// pixel access
inline vec4f get_pixel(const image_data& image, int i, int j);
inline void  set_pixel(image_data& image, int i, int j, const vec4f& pixel);

// conversions
image_data convert_image(const image_data& image, bool linear);
void       convert_image(image_data& result, const image_data& image);

// Evaluates an image at a point `uv`.
vec4f eval_image(const image_data& image, const vec2f& uv,
    bool as_linear = false, bool no_interpolation = false,
    bool clamp_to_edge = false);

// Apply tone mapping returning a float or byte image.
image_data tonemap_image(
    const image_data& image, float exposure, bool filmic = false);

// Apply tone mapping. If the input image is an ldr, does nothing.
void tonemap_image(image_data& ldr, const image_data& image, float exposure,
    bool filmic = false);
// Apply tone mapping using multithreading for speed.
void tonemap_image_mt(image_data& ldr, const image_data& image, float exposure,
    bool filmic = false);

// Resize an image.
image_data resize_image(const image_data& image, int width, int height);

// set/get region
void set_region(image_data& image, const image_data& region, int x, int y);
void get_region(image_data& region, const image_data& image, int x, int y,
    int width, int height);

// Compute the difference between two images.
image_data image_difference(
    const image_data& image_a, const image_data& image_b, bool display_diff);

// Composite two images together.
image_data composite_image(
    const image_data& image_a, const image_data& image_b);

// Composite two images together.
void composite_image(
    image_data& result, const image_data& image_a, const image_data& image_b);

// Composite two images together.
void composite_image(image_data& result, const vector<image_data>& images);

// Color grade an hsr or ldr image to an ldr image.
image_data colorgrade_image(
    const image_data& image, const colorgrade_params& params);

// Color grade an hsr or ldr image to an ldr image.
// Uses multithreading for speed.
void colorgrade_image(image_data& result, const image_data& image,
    const colorgrade_params& params);

// Color grade an hsr or ldr image to an ldr image.
// Uses multithreading for speed.
void colorgrade_image_mt(image_data& result, const image_data& image,
    const colorgrade_params& params);

// determine white balance colors
vec4f compute_white_balance(const image_data& image);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EXAMPLE IMAGES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a grid image.
image_data make_grid(int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0.2f, 0.2f, 0.2f, 1.0f},
    const vec4f& color1 = vec4f{0.5f, 0.5f, 0.5f, 1.0f});
// Make a checker image.
image_data make_checker(int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0.2f, 0.2f, 0.2f, 1.0f},
    const vec4f& color1 = vec4f{0.5f, 0.5f, 0.5f, 1.0f});
// Make a bump map.
image_data make_bumps(int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0, 0, 0, 1},
    const vec4f& color1 = vec4f{1, 1, 1, 1});
// Make a ramp
image_data make_ramp(int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0, 0, 0, 1},
    const vec4f& color1 = vec4f{1, 1, 1, 1});
// Make a gamma ramp.
image_data make_gammaramp(int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0, 0, 0, 1},
    const vec4f& color1 = vec4f{1, 1, 1, 1});
// Make a uv ramp
image_data make_uvramp(int width, int height, float scale = 1);
// Make a uv grid
image_data make_uvgrid(
    int width, int height, float scale = 1, bool colored = true);
// Make blackbody ramp.
image_data make_blackbodyramp(int width, int height, float scale = 1,
    float from = 1000, float to = 12000);
// Make color map ramp.
image_data make_colormapramp(int width, int height, float scale = 1);
// Make a noise image. Noise parameters: lacunarity, gain, octaves, offset.
image_data make_noisemap(int width, int height, float scale = 1,
    const vec4f& color0 = {0, 0, 0, 1}, const vec4f& color1 = {1, 1, 1, 1});
image_data make_fbmmap(int width, int height, float scale = 1,
    const vec4f& noise = {2, 0.5, 8, 1}, const vec4f& color0 = {0, 0, 0, 1},
    const vec4f& color1 = {1, 1, 1, 1});
image_data make_turbulencemap(int width, int height, float scale = 1,
    const vec4f& noise = {2, 0.5, 8, 1}, const vec4f& color0 = {0, 0, 0, 1},
    const vec4f& color1 = {1, 1, 1, 1});
image_data make_ridgemap(int width, int height, float scale = 1,
    const vec4f& noise = {2, 0.5, 8, 1}, const vec4f& color0 = {0, 0, 0, 1},
    const vec4f& color1 = {1, 1, 1, 1});

// Make a sunsky HDR model with sun at sun_angle elevation in [0,pif/2],
// turbidity in [1.7,10] with or without sun. The sun can be enabled or
// disabled with has_sun. The sun parameters can be slightly modified by
// changing the sun intensity and temperature. Has a convention, a temperature
// of 0 sets the eath sun defaults (ignoring intensity too).
image_data make_sunsky(int width, int height, float sun_angle,
    float turbidity = 3, bool has_sun = false, float sun_intensity = 1,
    float sun_radius = 1, const vec3f& ground_albedo = {0.2f, 0.2f, 0.2f});
// Make an image of multiple lights.
image_data make_lights(int width, int height, const vec3f& le = {1, 1, 1},
    int nlights = 4, float langle = pif / 4, float lwidth = pif / 16,
    float lheight = pif / 16);

// Comvert a bump map to a normal map. All linear color spaces.
image_data bump_to_normal(const image_data& image, float scale = 1);

// Add a border to an image
image_data add_border(
    const image_data& img, float width, const vec4f& color = {0, 0, 0, 1});

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMAGE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Conversion from/to floats.
void byte_to_float(vector<vec4f>& fl, const vector<vec4b>& bt);
void float_to_byte(vector<vec4b>& bt, const vector<vec4f>& fl);

// Conversion between linear and gamma-encoded images.
void srgb_to_rgb(vector<vec4f>& rgb, const vector<vec4f>& srgb);
void rgb_to_srgb(vector<vec4f>& srgb, const vector<vec4f>& rgb);
void srgb_to_rgb(vector<vec4f>& rgb, const vector<vec4b>& srgb);
void rgb_to_srgb(vector<vec4b>& srgb, const vector<vec4f>& rgb);

// Apply tone mapping
void tonemap_image(vector<vec4f>& ldr, const vector<vec4f>& hdr, float exposure,
    bool filmic = false, bool srgb = true);
void tonemap_image(vector<vec4b>& ldr, const vector<vec4f>& hdr, float exposure,
    bool filmic = false, bool srgb = true);

// Apply tone mapping using multithreading for speed
void tonemap_image_mt(vector<vec4f>& ldr, const vector<vec4f>& hdr,
    float exposure, bool filmic = false, bool srgb = true);
void tonemap_image_mt(vector<vec4b>& ldr, const vector<vec4f>& hdr,
    float exposure, bool filmic = false, bool srgb = true);

// Color grade a linear or srgb image to an srgb image.
// Uses multithreading for speed.
void colorgrade_image_mt(vector<vec4f>& corrected, const vector<vec4f>& img,
    bool linear, const colorgrade_params& params);
void colorgrade_image_mt(vector<vec4b>& corrected, const vector<vec4f>& img,
    bool linear, const colorgrade_params& params);

// determine white balance colors
vec3f compute_white_balance(const vector<vec4f>& img);

// Resize an image.
void resize_image(vector<vec4f>& res, const vector<vec4f>& img, int width,
    int height, int res_width, int res_height);
void resize_image(vector<vec4b>& res, const vector<vec4b>& img, int width,
    int height, int res_width, int res_height);

// Compute the difference between two images
void image_difference(vector<vec4f>& diff, const vector<vec4f>& a,
    const vector<vec4f>& b, bool disply_diff);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EXAMPLE IMAGES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a grid image.
void make_grid(vector<vec4f>& pixels, int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0.2f, 0.2f, 0.2f, 1.0f},
    const vec4f& color1 = vec4f{0.5f, 0.5f, 0.5f, 1.0f});
// Make a checker image.
void make_checker(vector<vec4f>& pixels, int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0.2f, 0.2f, 0.2f, 1.0f},
    const vec4f& color1 = vec4f{0.5f, 0.5f, 0.5f, 1.0f});
// Make a bump map.
void make_bumps(vector<vec4f>& pixels, int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0, 0, 0, 1},
    const vec4f& color1 = vec4f{1, 1, 1, 1});
// Make a ramp
void make_ramp(vector<vec4f>& pixels, int width, int height, float scale = 1,
    const vec4f& color0 = vec4f{0, 0, 0, 1},
    const vec4f& color1 = vec4f{1, 1, 1, 1});
// Make a gamma ramp.
void make_gammaramp(vector<vec4f>& pixels, int width, int height,
    float scale = 1, const vec4f& color0 = vec4f{0, 0, 0, 1},
    const vec4f& color1 = vec4f{1, 1, 1, 1});
// Make a uv ramp
void make_uvramp(vector<vec4f>& pixels, int width, int height, float scale = 1);
// Make a uv grid
void make_uvgrid(vector<vec4f>& pixels, int width, int height, float scale = 1,
    bool colored = true);
// Make blackbody ramp.
void make_blackbodyramp(vector<vec4f>& pixels, int width, int height,
    float scale = 1, float from = 1000, float to = 12000);
// Make color map ramp.
void make_colormapramp(
    vector<vec4f>& pixels, int width, int height, float scale = 1);
// Make a noise image. Noise parameters: lacunarity, gain, octaves, offset.
void make_noisemap(vector<vec4f>& pixels, int width, int height,
    float scale = 1, const vec4f& color0 = {0, 0, 0, 1},
    const vec4f& color1 = {1, 1, 1, 1});
void make_fbmmap(vector<vec4f>& pixels, int width, int height, float scale = 1,
    const vec4f& noise = {2, 0.5, 8, 1}, const vec4f& color0 = {0, 0, 0, 1},
    const vec4f& color1 = {1, 1, 1, 1});
void make_turbulencemap(vector<vec4f>& pixels, int width, int height,
    float scale = 1, const vec4f& noise = {2, 0.5, 8, 1},
    const vec4f& color0 = {0, 0, 0, 1}, const vec4f& color1 = {1, 1, 1, 1});
void make_ridgemap(vector<vec4f>& pixels, int width, int height,
    float scale = 1, const vec4f& noise = {2, 0.5, 8, 1},
    const vec4f& color0 = {0, 0, 0, 1}, const vec4f& color1 = {1, 1, 1, 1});

// Make a random image.
void make_randpoints(vector<vec4f>& pixels, int width, int height,
    float scale = 1, const vec4f& color0 = vec4f{0.2f, 0.2f, 0.2f, 1.0f},
    const vec4f& color1 = vec4f{0.5f, 0.5f, 0.5f, 1.0f});
void make_randlines(vector<vec4f>& pixels, int width, int height,
    float scale = 1, const vec4f& color0 = vec4f{0.2f, 0.2f, 0.2f, 1.0f},
    const vec4f& color1 = vec4f{0.5f, 0.5f, 0.5f, 1.0f});

// Make a sunsky HDR model with sun at sun_angle elevation in [0,pif/2],
// turbidity in [1.7,10] with or without sun. The sun can be enabled or
// disabled with has_sun. The sun parameters can be slightly modified by
// changing the sun intensity and temperature. Has a convention, a temperature
// of 0 sets the eath sun defaults (ignoring intensity too).
void make_sunsky(vector<vec4f>& pixels, int width, int height, float sun_angle,
    float turbidity = 3, bool has_sun = false, float sun_intensity = 1,
    float sun_radius = 1, const vec3f& ground_albedo = {0.2f, 0.2f, 0.2f});
// Make an image of multiple lights.
void make_lights(vector<vec4f>& pixels, int width, int height,
    const vec3f& le = {1, 1, 1}, int nlights = 4, float langle = pif / 4,
    float lwidth = pif / 16, float lheight = pif / 16);

// Comvert a bump map to a normal map. All linear color spaces.
void bump_to_normal(vector<vec4f>& normal, const vector<vec4f>& bump, int width,
    int height, float scale = 1);

// Add a border to an image
void add_border(vector<vec4f>& pixels, const vector<vec4f>& source, int width,
    int height, float thickness, const vec4f& color = {0, 0, 0, 1});

}  // namespace yocto

// -----------------------------------------------------------------------------
// BACKWARDS COMPATIBILITY
// -----------------------------------------------------------------------------
namespace yocto {

using color_image = image_data;

}  // namespace yocto

// -----------------------------------------------------------------------------
//
//
// IMPLEMENTATION
//
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF IMAGE FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// pixel access
inline vec4f& image_data::operator[](vec2i ij) {
  return pixels[ij.y * width + ij.x];
}
inline const vec4f& image_data::operator[](vec2i ij) const {
  return pixels[ij.y * width + ij.x];
}

// pixel access
inline vec4f get_pixel(const image_data& image, int i, int j) {
  return image.pixels[j * image.width + i];
}
inline void set_pixel(image_data& image, int i, int j, const vec4f& pixel) {
  image.pixels[j * image.width + i] = pixel;
}

}  // namespace yocto

#endif
