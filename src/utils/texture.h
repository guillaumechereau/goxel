#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>
#include <stdbool.h>

enum {
    TF_DEPTH    = 1 << 0,
    TF_STENCIL  = 1 << 1,
    TF_MIPMAP   = 1 << 2,
    TF_KEEP     = 1 << 3,
    TF_RGB      = 1 << 4,
    TF_RGB_565  = 1 << 5,
    TF_HAS_TEX  = 1 << 6,
    TF_HAS_FB   = 1 << 7,
    TF_NEAREST  = 1 << 8,
};

// Type: texture_t
// Reresent a 2d texture.
typedef struct texture texture_t;
struct texture {
    int         ref;        // For reference copy.
    char        *path;      // Only for image textures.

    int         format;
    uint32_t    tex;
    int tex_w, tex_h;       // The actual OpenGL texture size.
    int x, y, w, h;         // Position of the sub texture.
    int flags;
    // This is only used for buffer textures.
    uint32_t framebuffer, depth, stencil;
};

texture_t *texture_new_from_buf(const uint8_t *data,
                                int w, int h, int bpp, int flags);
texture_t *texture_new_surface(int w, int h, int flags);
texture_t *texture_new_buffer(int w, int h, int flags);
void texture_get_data(const texture_t *tex, int w, int h, int bpp,
                      uint8_t *buf);

texture_t *texture_copy(texture_t *tex);
void texture_delete(texture_t *tex);

void texture_set_data(texture_t *tex,
                      const uint8_t *data, int w, int h, int bpp);

#endif // TEXTURE_H
