/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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


#include "texture.h"

#include "utils/gl.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Return the next power of 2 larger or equal to x.
static int next_pow2(int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

static bool is_pow2(int x) { return x == next_pow2(x); }

static void texture_create_empty(texture_t *tex)
{
    assert(tex->flags & TF_HAS_TEX);
    GL(glGenTextures(1, &tex->tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));
    if (!(tex->flags & TF_NEAREST)) {
        GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            (tex->flags & TF_MIPMAP)? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR));
    } else {
        GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    }
    GL(glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->tex_w, tex->tex_h,
            0, tex->format, GL_UNSIGNED_BYTE, NULL));
}

static void blit(const uint8_t *src, int src_w, int src_h, int bpp,
                 uint8_t *dst, int dst_w, int dst_h)
{
    int i, j;
    for (i = 0; i < src_h; i++) for (j = 0; j < src_w; j++) {
        memcpy(&dst[(i * dst_w + j) * bpp], &src[(i * src_w + j) * bpp], bpp);
    }
}

void texture_set_data(texture_t *tex,
                      const uint8_t *data, int w, int h, int bpp)
{
    uint8_t *buf = NULL;
    assert(tex->tex);
    if (!is_pow2(w) || !is_pow2(h)) {
        buf = calloc(bpp, tex->tex_w * tex->tex_h);
        blit(data, w, h, bpp, buf, tex->tex_w, tex->tex_h);
        data = buf;
    }
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->tex_w, tex->tex_h,
                0, tex->format, GL_UNSIGNED_BYTE, data));
    free(buf);
    if (tex->flags & TF_MIPMAP)
        GL(glGenerateMipmap(GL_TEXTURE_2D));
}

texture_t *texture_new_from_buf(const uint8_t *data,
                                int w, int h, int bpp, int flags)
{
    texture_t *tex;
    tex = calloc(1, sizeof(*tex));
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = TF_HAS_TEX | flags;
    tex->format = (int[]){0, 0, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA}[bpp];
    texture_create_empty(tex);
    texture_set_data(tex, data, w, h, bpp);
    tex->ref = 1;
    return tex;
}

texture_t *texture_new_surface(int w, int h, int flags)
{
    texture_t *tex;
    tex = calloc(1, sizeof(*tex));
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = flags | TF_HAS_TEX;
    tex->format = (flags & TF_RGB) ? GL_RGB : GL_RGBA;
    texture_create_empty(tex);
    tex->ref = 1;
    return tex;
}

texture_t *texture_new_buffer(int w, int h, int flags)
{
    texture_t *tex;
    tex = calloc(1, sizeof(*tex));
    tex->format = (flags & TF_RGB) ? GL_RGB : GL_RGBA;
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = flags | TF_HAS_FB | TF_HAS_TEX;
    gl_gen_fbo(tex->tex_w, tex->tex_h, tex->format, 1,
               &tex->framebuffer, &tex->tex);
    tex->ref = 1;
    return tex;
}

void texture_delete(texture_t *tex)
{
    if (!tex) return;
    tex->ref--;
    if (tex->ref > 0) return;

    free(tex->path);
    if (tex->framebuffer) {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
        if (tex->depth)
            GL(glDeleteRenderbuffers(1, &tex->depth));
        if (tex->stencil)
            GL(glDeleteRenderbuffers(1, &tex->stencil));
        GL(glDeleteFramebuffers(1, &tex->framebuffer));
    }
    if (tex->tex)
        GL(glDeleteTextures(1, &tex->tex));
    free(tex);
}

texture_t *texture_copy(texture_t *tex)
{
    // XXX: since texture are immutable, we just increase the ref counter.
    if (tex) tex->ref++;
    return tex;
}

void texture_get_data(const texture_t *tex, int w, int h, int bpp,
                      uint8_t *buf)
{
    uint8_t *tmp;
    size_t i, j;
    tmp = calloc(w * h, 4);
    GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
    GL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp));
    // Flip output y and remove alpha if needed.
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            memcpy(&buf[(i * w + j) * bpp],
                   &tmp[((h - i - 1) * w  + j) * 4],
                   bpp);
        }
    }
    free(tmp);
}

/*
void texture_save_to_file(const texture_t *tex, const char *path)
{
    uint8_t *data;
    int w, h;
    LOG_I("save texture to %s", path);
    w = tex->tex_w;
    h = tex->tex_h;
    data = calloc(w * h, 4);
    texture_get_data(tex, w, h, 4, data);
    img_write(data, w, h, 4, path);
    free(data);
}
*/
