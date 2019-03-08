//
// # Yocto/ImageIO: Tiny library for image input and output
//
// Yocto/ImageIO provides loading and saving functionality for images.
// We support PNG, JPG, TGA, HDR, EXR formats.
//
// Error reporting is done through exceptions using the `io_error` exception.
//
// ## Image Loading and Saving
//
// 1. load and save images with `load_image()` and `save_image()`
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

#ifndef _YOCTO_IMAGEIO_H_
#define _YOCTO_IMAGEIO_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_image.h"

// -----------------------------------------------------------------------------
// IMAGE IO
// -----------------------------------------------------------------------------
namespace yocto {

// Check if an image is HDR based on filename.
bool is_hdr_filename(const string& filename);

// Loads/saves a 4 channel float image in linear color space.
void load_image(const string& filename, image4f& img);
void save_image(const string& filename, const image4f& img);
void load_image_from_memory(const byte* data, int data_size, image4f& img);

// Loads/saves a 4 channel byte image in sRGB color space.
void load_image(const string& filename, image4b& img);
void save_image(const string& filename, const image4b& img);
void load_image_from_memory(const byte* data, int data_size, image4b& img);
void load_image_from_memory(const byte* data, int data_size, image4b& img);

// Convenience helper that saves an HDR images as wither a linear HDR file or
// a tonemapped LDR file depending on file name
void save_tonemapped_image(const string& filename, const image4f& hdr,
    float exposure = 0, bool filmic = false, bool srgb = true);

// imageio error
struct imageio_error : runtime_error {
    explicit imageio_error(const char* msg) : runtime_error{msg} {}
    explicit imageio_error(const std::string& msg) : runtime_error{msg} {}
};

}  // namespace yocto

// -----------------------------------------------------------------------------
// VOLUME IMAGE IO
// -----------------------------------------------------------------------------
namespace yocto {

// Loads/saves a 1 channel volume.
void load_volume(const string& filename, volume1f& vol);
void save_volume(const string& filename, const volume1f& vol);

}  // namespace yocto

#endif
