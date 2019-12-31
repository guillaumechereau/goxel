/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "img.h"

#include <stdbool.h>

#ifndef HAVE_LIBPNG
#   define HAVE_LIBPNG 0
#endif

// Prevent warnings in stb code.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-function"
#ifdef WIN32
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#endif

#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_GIF
#include "stb_image.h"
#include "stb_image_write.h"

#if HAVE_LIBPNG
#include <png.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static char *read_file(const char *path, int *size)
{
    FILE *file;
    char *ret = NULL;
    long read_size __attribute__((unused));
    int size_default;

    size = size ?: &size_default; // Allow to pass NULL as size;
    *size = 0;
    file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    *size = (int)ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    ret[*size] = '\0';
    fclose(file);
    return ret;
}


uint8_t *img_read_from_mem(const char *data, int size,
                           int *w, int *h, int *bpp)
{
    assert(*bpp >= 0 && *bpp <= 4);
    return stbi_load_from_memory((uint8_t*)data, size, w, h, bpp, *bpp);
}

uint8_t *img_read(const char *path, int *width, int *height, int *bpp)
{
    int size;
    char *data;
    uint8_t *img;

    data = read_file(path, &size);
    if (!data) LOG_E("Cannot open image %s", path);
    assert(data);
    img = img_read_from_mem(data, size, width, height, bpp);
    free(data);
    return img;
}

#if !HAVE_LIBPNG

void img_write(const uint8_t *img, int w, int h, int bpp, const char *path)
{
    stbi_write_png(path, w, h, bpp, img, 0);
}

#else

void img_write(const uint8_t *img, int w, int h, int bpp, const char *path)
{
    int i;
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    fp = fopen(path, "wb");
    if (!fp) {
        LOG_E("Cannot open %s", path);
        return;
    }
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL);
    if (!png_ptr) {
        // With ubuntu 19.04, libpng seems to fail!
        LOG_E("Libpng error: fallback to stb-img");
        fclose(fp);
        stbi_write_png(path, w, h, bpp, img, 0);
        return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
       png_destroy_write_struct(&png_ptr, &info_ptr);
       fclose(fp);
       return;
    }
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, w, h, 8,
                 bpp == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);
    for (i = 0; i < h; i++)
        png_write_row(png_ptr, (png_bytep)(img + i * w * bpp));
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

#endif

uint8_t *img_write_to_mem(const uint8_t *img, int w, int h, int bpp, int *size)
{
    return stbi_write_png_to_mem((void*)img, 0, w, h, bpp, size);
}

void img_downsample(const uint8_t *img, int w, int h, int bpp,
                    uint8_t *out)
{
#define IX(x, y, k) ((((size_t)y) * w + (x)) * bpp + (k))
    int i, j, k;

    assert(w % 2 == 0 && h % 2 == 0);

    for (i = 0; i < h; i += 2)
    for (j = 0; j < w; j += 2)
    for (k = 0; k < bpp; k++) {
        out[(i * w / 4 + j / 2) * bpp + k] = (
                            img[IX(j + 0, i + 0, k)] +
                            img[IX(j + 0, i + 1, k)] +
                            img[IX(j + 1, i + 0, k)] +
                            img[IX(j + 1, i + 1, k)]) / 4;
    }
#undef IX
}
