/*
 * File: img.h
 * A few 2d image manipulation helper functions.
 */

#ifndef IMG_H
#define IMG_H

#include <stdint.h>

/*
 * Function: img_read
 * Read an image from a file.
 */
uint8_t *img_read(const char *path, int *width, int *height, int *bpp);

/*
 * Function: img_read_from_mem
 * Read an image from memory.
 */
uint8_t *img_read_from_mem(const char *data, int size,
                           int *w, int *h, int *bpp);

/*
 * Function: img_write
 * Write an image to a file.
 */
void img_write(const uint8_t *img, int w, int h, int bpp, const char *path);

/*
 * Function: img_write_to_mem
 * Write an image to memory.
 */
uint8_t *img_write_to_mem(const uint8_t *img, int w, int h, int bpp,
                          int *size);

/*
 * Function: img_downsample
 * Downsample an image by half, using interpolation.
 */
void img_downsample(const uint8_t *img, int w, int h, int bpp,
                    uint8_t *out);

#endif // IMG_H
