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


#include "goxel.h"
#include <stdarg.h>

// Global list of all the textures.
static texture_t *g_textures = NULL;

// Return the next power of 2 larger or equal to x.
static int next_pow2(int x)
{
    return pow(2, ceil(log(x) / log(2)));
}

static bool is_pow2(int x) { return x == next_pow2(x); }


static void generate_framebuffer(texture_t *tex)
{
    assert(tex->w > 0 && tex->h > 0);
    const int w = tex->tex_w;
    const int h = tex->tex_h;
    bool stencil = tex->flags & TF_STENCIL;
    bool depth = tex->flags & TF_DEPTH;
    assert(w == next_pow2(w));
    assert(h == next_pow2(h));

    GL(glGenFramebuffers(1, &tex->framebuffer));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
    GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, tex->tex, 0));
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
            GL_FRAMEBUFFER_COMPLETE);

#ifndef GLES2
    if (depth || stencil) {
        GL(glGenRenderbuffers(1, &tex->depth));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, tex->depth));
        GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, w, h));
        GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, tex->depth));
    }
#else
    // With opengl ES2, we need to check if we support packed depth/stencil
    // or not.
    if (has_gl_extension(GL_OES_packed_depth_stencil)) {
        if (depth || stencil) {
            GL(glGenRenderbuffers(1, &tex->depth));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, tex->depth));
            GL(glRenderbufferStorage(GL_RENDERBUFFER,
                                     GL_DEPTH24_STENCIL8_OES, w, h));
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                         GL_RENDERBUFFER, tex->depth));
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                         GL_RENDERBUFFER, tex->depth));
        }
    } else {
        if (depth) {
            GL(glGenRenderbuffers(1, &tex->depth));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, tex->depth));
            GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                     w, h));
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                        GL_RENDERBUFFER, tex->depth));
        }
        if (stencil) {
            GL(glGenRenderbuffers(1, &tex->stencil));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, tex->stencil));
            GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                                     w, h));
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                        GL_RENDERBUFFER, tex->stencil));
        }
    }
#endif
    assert(
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    GL(glBindFramebuffer(GL_FRAMEBUFFER, sys_get_screen_framebuffer()));
}

static void texture_create_empty(texture_t *tex)
{
    assert(tex->flags & TF_HAS_TEX);
    GL(glGenTextures(1, &tex->tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                       GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                       GL_LINEAR));
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

static void texture_set_data(texture_t *tex,
        const uint8_t *data, int w, int h, int bpp)
{
    uint8_t *buff0 = NULL;
    int data_type = GL_UNSIGNED_BYTE;

    if (!is_pow2(w) || !is_pow2(h)) {
        buff0 = calloc(bpp, tex->tex_w * tex->tex_h);
        blit(data, w, h, bpp, buff0, tex->tex_w, tex->tex_h);
        data = buff0;
    }

    GL(glGenTextures(1, &tex->tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            (tex->flags & TF_MIPMAP)? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->tex_w, tex->tex_h,
                0, tex->format, data_type, data));
    free(buff0);

    if (tex->flags & TF_MIPMAP)
        GL(glGenerateMipmap(GL_TEXTURE_2D));
}

texture_t *texture_new_image(const char *path)
{
    texture_t *tex;
    uint8_t *img;
    int w, h, bpp = 0;
    img = img_read(path, &w, &h, &bpp);
    if (!img) {
        LOG_W("Cannot open image '%s'", path);
        return NULL;
    }
    tex = calloc(1, sizeof(*tex));
    tex->path = strdup(path);
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = TF_HAS_TEX;
    tex->format = (int[]){0, 0, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA}[bpp];
    texture_create_empty(tex);
    texture_set_data(tex, img, w, h, bpp);
    free(img);
    tex->ref = 1;
    LL_APPEND(g_textures, tex);
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
    LL_APPEND(g_textures, tex);
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
    texture_create_empty(tex);
    generate_framebuffer(tex);
    tex->ref = 1;
    LL_APPEND(g_textures, tex);
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
    LL_DELETE(g_textures, tex);
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
    int i;
    tmp = calloc(w * h, bpp);
    GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
    GL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp));
    // Flip output y.
    for (i = 0; i < h; i++) {
        memcpy(&buf[i * w * bpp], &tmp[(h - i - 1) * w * bpp], bpp * w);
    }
    free(tmp);
}

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
