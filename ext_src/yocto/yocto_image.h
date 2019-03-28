//
// # Yocto/Image: Tiny imaging Library mostly for rendering and color support
//
//
// Yocto/Image is a collection of image utilities useful when writing rendering
// algorithms. These include a simple image data structure, color conversion
// utilities and tone mapping.
//
//
// ## Image Utilities
//
// Yocto/Image supports a very small set of color and image utilities including
// color utilities, example image creation, tone mapping, image resizing, and
// sunsky procedural images. Yocto/Image is written to support the need of a
// global illumination renderer, rather than the need of generic image editing.
// We support 4-channels float images (assumed to be in linear color) and
// 4-channels byte images (assumed to be in sRGB).
//
//
// 1. store images using the image<T> structure
// 2. resize images with `resize_image()`
// 3. tonemap images with `tonemap_image()` that convert from linear HDR to
//    sRGB LDR with exposure and an optional filmic curve
// 5. make various image examples with the `make_XXX_image()` functions
// 6. create procedural sun-sky images with `make_sunsky_image()`
// 7. load and save images with Yocto/ImageIO
// 8. many color conversion functions are available in the code below
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
//
//  LICENSE for blackbody code
//
// Copyright (c) 2015 Neil Bartlett
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//

#ifndef _YOCTO_IMAGE_H_
#define _YOCTO_IMAGE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_math.h"
#include "yocto_random.h"
#include "yocto_utils.h"

// -----------------------------------------------------------------------------
// IMAGE DATA AND UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Image container.
template <typename T>
struct image {
    // constructors
    image() : _size{0, 0}, _pixels{} {}
    image(const vec2i& size, const T& value = {})
        : _size{size}, _pixels{(size_t)size.x * (size_t)size.y, value} {}
    image(const vec2i& size, const T* value)
        : _size{size}
        , _pixels{value, value + (size_t)size.x * (size_t)size.y} {}

    // size
    bool  empty() const { return _pixels.empty(); }
    vec2i size() const { return _size; }
    void  resize(const vec2i& size) {
        if (size == _size) return;
        _size = size;
        _pixels.resize((size_t)size.x * (size_t)size.y);
    }

    // element access
    T& operator[](const vec2i& ij) { return _pixels[ij.y * _size.x + ij.x]; }
    const T& operator[](const vec2i& ij) const {
        return _pixels[ij.y * _size.x + ij.x];
    }

    // data access
    T*       data() { return _pixels.data(); }
    const T* data() const { return _pixels.data(); }

    // iteration
    T*       begin() { return _pixels.data(); }
    T*       end() { return _pixels.data() + _pixels.size(); }
    const T* begin() const { return _pixels.data(); }
    const T* end() const { return _pixels.data() + _pixels.size(); }

    // data
    vec2i     _size   = zero2i;
    vector<T> _pixels = {};
};

// Typedefs
using image4f = image<vec4f>;
using image4b = image<vec4b>;

// equality
template <typename T>
inline bool operator==(const image<T>& a, const image<T>& b) {
    return a.size() == b.size() && a._pixels == b._pixels;
}
template <typename T>
inline bool operator!=(const image<T>& a, const image<T>& b) {
    return a.size() != b.size() || a._pixels != b._pixels;
}

// Image region
struct image_region {
    vec2i min = zero2i;
    vec2i max = zero2i;
    vec2i size() const { return max - min; }
};

// Splits an image into an array of regions
inline void make_image_regions(vector<image_region>& regions, const vec2i& size,
    int region_size = 32, bool shuffled = false);

// Gets pixels in an image region
template <typename T>
inline void get_image_region(
    image<T>& clipped, const image<T>& img, const image_region& region);

// Conversion from/to floats.
void byte_to_float(image4f& fl, const image4b& bt);
void float_to_byte(image4b& bt, const image4f& fl);

// Conversion between linear and gamma-encoded images.
void srgb_to_linear(image4f& lin, const image4f& srgb);
void linear_to_srgb(image4f& srgb, const image4f& lin);
void srgb_to_linear(image4f& lin, const image4b& srgb);
void linear_to_srgb(image4b& srgb, const image4f& lin);

// Conversion between linear and gamma-encoded images.
void gamma_to_linear(image4f& lin, const image4f& srgb, float gamma);
void linear_to_gamma(image4f& srgb, const image4f& lin, float gamma);

// Apply exposure and filmic tone mapping
void tonemap_image(
    image4f& ldr, const image4f& hdr, float exposure, bool filmic, bool srgb);
void tonemap_image(
    image4b& ldr, const image4f& hdr, float exposure, bool filmic, bool srgb);
void tonemap_image_region(image4f& ldr, const image_region& region,
    const image4f& hdr, float exposure, bool filmic, bool srgb);

// Resize an image.
void resize_image(image4f& res, const image4f& img, const vec2i& size);
void resize_image(image4f& res, const image4f& img);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EXAMPLE IMAGES
// -----------------------------------------------------------------------------
namespace yocto {

// Make example images in linear color space. Takes as input images allocated
// to the desired size and fill the pixel with expected values.
void make_grid_image(image4f& img, int tile = 8,
    const vec4f& c0 = {0.2f, 0.2f, 0.2f, 1},
    const vec4f& c1 = {0.5f, 0.5f, 0.5f, 1});
void make_checker_image(image4f& img, int tile = 8,
    const vec4f& c0 = {0.2f, 0.2f, 0.2f, 1},
    const vec4f& c1 = {0.5f, 0.5f, 0.5f, 1});
void make_bumpdimple_image(image4f& img, int tile = 8);
void make_ramp_image(
    image4f& img, const vec4f& c0, const vec4f& c1, float srgb = false);
void make_gammaramp_image(image4f& img);
void make_uvramp_image(image4f& img);
void make_uvgrid_image(image4f& img, int tile = 8, bool colored = true);
void make_blackbodyramp_image(image4f& img, float start_temperature = 1000,
    float end_temperature = 12000);

// Comvert a bump map to a normal map. All linear color spaces.
void bump_to_normal_map(image4f& norm, const image4f& img, float scale = 1);

// Make a sunsky HDR model with sun at sun_angle elevation in [0,pif/2],
// turbidity in [1.7,10] with or without sun. The sun can be enabled or
// disabled with has_sun. The sun parameters can be slightly modified by
// changing the sun intensity and temperature. Has a convention, a temperature
// of 0 sets the eath sun defaults (ignoring intensity too).
void make_sunsky_image(image4f& img, float sun_angle, float turbidity = 3,
    bool has_sun = false, float sun_intensity = 1.0f, float sun_temperature = 0,
    const vec3f& ground_albedo = {0.2f, 0.2f, 0.2f});
// Make an image of multiple lights.
void make_lights_image(image4f& img, const vec3f& le = {1, 1, 1},
    int nlights = 4, float langle = pif / 4, float lwidth = pif / 16,
    float lheight = pif / 16);

// Make a noise image. Wrap works only if both resx and resy are powers of two.
void make_noise_image(image4f& img, float scale = 1, bool wrap = true);
void make_fbm_image(image4f& img, float scale = 1, float lacunarity = 2,
    float gain = 0.5f, int octaves = 6, bool wrap = true);
void make_ridge_image(image4f& img, float scale = 1, float lacunarity = 2,
    float gain = 0.5f, float offset = 1.0f, int octaves = 6, bool wrap = true);
void make_turbulence_image(image4f& img, float scale = 1, float lacunarity = 2,
    float gain = 0.5f, int octaves = 6, bool wrap = true);

// Add a border to an image
void add_image_border(image4f& img, int border_width = 2,
    const vec4f& border_color = {0, 0, 0, 1});

}  // namespace yocto

// -----------------------------------------------------------------------------
// VOLUME TYPE AND UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Volume container.
template <typename T>
struct volume {
    // constructors
    volume() : _size{0, 0, 0}, _voxels{} {}
    volume(const vec3i& size, const T& value = {})
        : _size{size}
        , _voxels((size_t)size.x * (size_t)size.y * (size_t)size.z, value) {}
    volume(const vec3i& size, const T* value)
        : _size{size}
        , _voxels(value,
              value + (size_t)size.x * (size_t)size.y * (size_t)size.z) {}

    // size
    bool  empty() const { return _voxels.empty(); }
    vec3i size() const { return _size; }
    void  resize(const vec3i& size) {
        if (size == _size) return;
        _size = size;
        _voxels.resize((size_t)size.x * (size_t)size.y * (size_t)size.z);
    }

    // element access
    T& operator[](const vec3i& ijk) {
        return _voxels[ijk.z * _size.x * _size.y + ijk.y * _size.x + ijk.x];
    }
    const T& operator[](const vec3i& ijk) const {
        return _voxels[ijk.z * _size.x * _size.y + ijk.y * _size.x + ijk.x];
    }

    // data access
    T*       data() { return _voxels.data(); }
    const T* data() const { return _voxels.data(); }

    // iteration
    T*       begin() { return _voxels.data(); }
    T*       end() { return _voxels.data() + _voxels.size(); }
    const T* begin() const { return _voxels.data(); }
    const T* end() const { return _voxels.data() + _voxels.size(); }

    // data
    vec3i         _size   = zero3i;
    vector<float> _voxels = {};
};

// Typedefs
using volume1f = volume<float>;

// equality
template <typename T>
inline bool operator==(const volume<T>& a, const volume<T>& b) {
    return a.size() == b.size() && a._voxels == b._voxels;
}
template <typename T>
inline bool operator!=(const volume<T>& a, const volume<T>& b) {
    return a.size() != b.size() && a._voxels != b._voxels;
}

// make a simple example volume
void make_test_volume(volume1f& vol, float scale = 10, float exponent = 6);

}  // namespace yocto

// -----------------------------------------------------------------------------
// COLOR CONVERSION UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Element-wise float to byte conversion.
inline vec4b float_to_byte(const vec4f& a) {
    return {
        (byte)clamp(int(a.x * 256), 0, 255),
        (byte)clamp(int(a.y * 256), 0, 255),
        (byte)clamp(int(a.z * 256), 0, 255),
        (byte)clamp(int(a.w * 256), 0, 255),
    };
}
inline vec4f byte_to_float(const vec4b& a) {
    return {a.x / 255.0f, a.y / 255.0f, a.z / 255.0f, a.w / 255.0f};
}

// Conversion between linear and gamma-encoded colors.
inline vec3f gamma_to_linear(const vec3f& srgb, float gamma) {
    return {pow(srgb.x, gamma), pow(srgb.y, gamma), pow(srgb.z, gamma)};
}
inline vec3f linear_to_gamma(const vec3f& lin, float gamma) {
    return {
        pow(lin.x, 1 / gamma), pow(lin.y, 1 / gamma), pow(lin.z, 1 / gamma)};
}
inline vec4f gamma_to_linear(const vec4f& srgb, float gamma) {
    return {pow(srgb.x, gamma), pow(srgb.y, gamma), pow(srgb.z, gamma), srgb.w};
}
inline vec4f linear_to_gamma(const vec4f& lin, float gamma) {
    return {pow(lin.x, 1 / gamma), pow(lin.y, 1 / gamma), pow(lin.z, 1 / gamma),
        lin.w};
}

// sRGB non-linear curve
inline float srgb_to_linear(float srgb) {
    if (srgb <= 0.04045) {
        return srgb / 12.92f;
    } else {
        return pow((srgb + 0.055f) / (1.0f + 0.055f), 2.4f);
    }
}
inline float linear_to_srgb(float lin) {
    if (lin <= 0.0031308f) {
        return 12.92f * lin;
    } else {
        return (1 + 0.055f) * pow(lin, 1 / 2.4f) - 0.055f;
    }
}

// Conversion between linear and srgb colors.
inline vec3f srgb_to_linear(const vec3f& srgb) {
    return {
        srgb_to_linear(srgb.x), srgb_to_linear(srgb.y), srgb_to_linear(srgb.z)};
}
inline vec3f linear_to_srgb(const vec3f& lin) {
    return {
        linear_to_srgb(lin.x), linear_to_srgb(lin.y), linear_to_srgb(lin.z)};
}
inline vec4f srgb_to_linear(const vec4f& srgb) {
    return {srgb_to_linear(srgb.x), srgb_to_linear(srgb.y),
        srgb_to_linear(srgb.z), srgb.w};
}
inline vec4f linear_to_srgb(const vec4f& lin) {
    return {linear_to_srgb(lin.x), linear_to_srgb(lin.y), linear_to_srgb(lin.z),
        lin.w};
}

// Approximate luminance estimate for sRGB primaries (better relative luminance)
inline float luminance(const vec3f& a) {
    return (0.2126f * a.x + 0.7152f * a.y + 0.0722 * a.z);
}
inline float luminance(const vec4f& a) {
    return (0.2126f * a.x + 0.7152f * a.y + 0.0722 * a.z);
}

// Fitted ACES tonemapping curve.
inline float tonemap_filmic(float hdr) {
    // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    // hdr *= 0.6; // brings it back to ACES range
    return (hdr * hdr * 2.51f + hdr * 0.03f) /
           (hdr * hdr * 2.43f + hdr * 0.59f + 0.14f);
}
// Apply ACES fitted curve.
inline vec4f tonemap_filmic(const vec4f& hdr) {
    return {tonemap_filmic(hdr.x), tonemap_filmic(hdr.y), tonemap_filmic(hdr.z),
        hdr.w};
}

// Tonemap a color value according to an exposure-gamma tone mapper, with
// an optional filmic curve.
inline vec4f tonemap_filmic(
    const vec4f& hdr, float exposure, bool filmic, bool srgb) {
    auto scale = pow(2.0f, exposure);
    auto ldr   = vec4f{hdr.x * scale, hdr.y * scale, hdr.z * scale, hdr.w};
    if (filmic) ldr = tonemap_filmic(ldr);
    if (srgb) ldr = linear_to_srgb(ldr);
    return ldr;
}

// Convert between CIE XYZ and xyY
inline vec3f xyz_to_xyY(const vec3f& xyz) {
    if (xyz == zero3f) return zero3f;
    return {xyz.x / (xyz.x + xyz.y + xyz.z), xyz.y / (xyz.x + xyz.y + xyz.z),
        xyz.y};
}
// Convert between CIE XYZ and xyY
inline vec3f xyY_to_xyz(const vec3f& xyY) {
    if (xyY.y == 0) return zero3f;
    return {xyY.x * xyY.z / xyY.y, xyY.z, (1 - xyY.x - xyY.y) * xyY.z / xyY.y};
}
// Convert between CIE XYZ and RGB
inline vec3f xyz_to_rgb(const vec3f& xyz) {
    // from http://www.brucelindbloom.com/index.html?Eqn_RGB_to_XYZ.html
    return {
        +3.2404542f * xyz.x - 1.5371385f * xyz.y - 0.4985314f * xyz.z,
        -0.9692660f * xyz.x + 1.8760108f * xyz.y + 0.0415560f * xyz.z,
        +0.0556434f * xyz.x - 0.2040259f * xyz.y + 1.0572252f * xyz.z,
    };
}
// Convert between CIE XYZ and RGB
inline vec3f rgb_to_xyz(const vec3f& rgb) {
    // from http://www.brucelindbloom.com/index.html?Eqn_RGB_to_XYZ.html
    return {
        0.4124564f * rgb.x + 0.3575761f * rgb.y + 0.1804375f * rgb.z,
        0.2126729f * rgb.x + 0.7151522f * rgb.y + 0.0721750f * rgb.z,
        0.0193339f * rgb.x + 0.1191920f * rgb.y + 0.9503041f * rgb.z,
    };
}

// Approximate color of blackbody radiation from wavelength in nm.
inline vec3f blackbody_to_rgb(float temperature);

// Converts HSV to RGB.
inline vec3f hsv_to_rgb(const vec3f& hsv);
inline vec3f rgb_to_hsv(const vec3f& rgb);

}  // namespace yocto

// ---------------------------------------------------------------------------//
//                                                                            //
//                             IMPLEMENTATION                                 //
//                                                                            //
// ---------------------------------------------------------------------------//

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF COLOR CONVERSION UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Approximate color of blackbody radiation from wavelength in nm.
inline vec3f blackbody_to_rgb(float temperature) {
    // https://github.com/neilbartlett/color-temperature
    auto rgb = zero3f;
    if ((temperature / 100) < 66.0) {
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

    if ((temperature / 100) < 66.0) {
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
            rgb.z = -254.76935184120902 + 0.8274096064007395 * rgb.z +
                    115.67994401066147 * log(rgb.z);
            if (rgb.z < 0) rgb.z = 0;
            if (rgb.z > 255) rgb.z = 255;
        }
    }

    return srgb_to_linear(rgb / 255);
}

// Convert HSV to RGB
inline vec3f hsv_to_rgb(const vec3f& hsv) {
    // from Imgui.cpp
    auto h = hsv.x, s = hsv.y, v = hsv.z;
    if (hsv.y == 0.0f) return {v, v, v};

    h       = fmodf(h, 1.0f) / (60.0f / 360.0f);
    int   i = (int)h;
    float f = h - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

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
inline vec3f rgb_to_hsv(const vec3f& rgb) {
    // from Imgui.cpp
    auto  r = rgb.x, g = rgb.y, b = rgb.z;
    float K = 0.f;
    if (g < b) {
        swap(g, b);
        K = -1.f;
    }
    if (r < g) {
        swap(r, g);
        K = -2.f / 6.f - K;
    }

    float chroma = r - (g < b ? g : b);
    return {
        fabsf(K + (g - b) / (6.f * chroma + 1e-20f)), chroma / (r + 1e-20f), r};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR IMAGE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Gets pixels in an image region
template <typename T>
inline void get_image_region(
    image<T>& clipped, const image<T>& img, const image_region& region) {
    clipped.resize(region.size());
    for (auto j = 0; j < region.size().y; j++) {
        for (auto i = 0; i < region.size().x; i++) {
            clipped[{i, j}] = img[{i + region.min.x, j + region.min.y}];
        }
    }
}

// Splits an image into an array of regions
inline void make_image_regions(vector<image_region>& regions, const vec2i& size,
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
        random_shuffle(regions, rng);
    }
}

}  // namespace yocto

#endif
