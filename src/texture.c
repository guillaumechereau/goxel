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


// #define LOG_LEVEL LOG_VERBOSE

#include "goxel.h"
#include <stdarg.h>

#ifdef DEBUG
    static const char *debug_tag;
#   define LOG_TEX(tex, msg, ...) do { \
        LOG_V("tex %-16s %-16s (%4dx%-4d %4dx%-4d): " msg, \
              tex->debug.tag, \
              tex->key, tex->w, tex->h, tex->tex_w, tex->tex_h, \
              ##__VA_ARGS__); \
    } while (0)
#else
#   define LOG_TEX(tex, msg, ...)
#endif

// Fix to use glDiscardFramebuffer on android.
#ifdef ANDROID
#   include <dlfcn.h>
    static __typeof__(glDiscardFramebufferEXT) *p_glDiscardFramebufferEXT;
#   define glDiscardFramebufferEXT (*p_glDiscardFramebufferEXT)
#endif

static texture_t *textures = NULL;

// Return the next power of 2 larger or equal to x.
static int next_pow2(int x)
{
    return pow(2, ceil(log(x) / log(2)));
}

static bool is_pow2(int x) { return x == next_pow2(x); }

static void blit(const uint8_t *src, int src_w, int src_h, int bpp,
                 uint8_t *dst, int dst_w, int dst_h)
{
    int i, j;
    for (i = 0; i < src_h; i++) for (j = 0; j < src_w; j++) {
        memcpy(&dst[(i * dst_w + j) * bpp], &src[(i * src_w + j) * bpp], bpp);
    }
}

// XXX: I could optimize it to only compress the used part of the data.
static void rgb888_to_565(const uint8_t *data, int size, int bpp,
                          uint8_t *out)
{
    uint8_t r, g, b;
    int i;
    for (i = 0; i < size; i++) {
        r = data[i * bpp + 0];
        g = data[i * bpp + 1];
        b = data[i * bpp + 2];
        ((uint16_t*)out)[i] = ((r >> 3) << 11) |
                              ((g >> 2) << 5)  |
                              ((b >> 3) << 0);
    }
}

static void clear_buffer(GLuint buffer)
{
    // Clear in two times to prevent bugs on some hardware, like the kindle
    // Fire.
    GL(glBindFramebuffer(GL_FRAMEBUFFER, buffer));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glDisable(GL_STENCIL_TEST));
    GL(glClearColor(0, 0, 0, 0));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    GL(glStencilMask(0xFF));
    GL(glClearStencil(0));
    GL(glClear(GL_COLOR_BUFFER_BIT |
               GL_DEPTH_BUFFER_BIT |
               GL_STENCIL_BUFFER_BIT));
}

static texture_t *tex_new()
{
    texture_t *tex = calloc(1, sizeof(*tex));
    LL_APPEND(textures, tex);
#ifdef DEBUG
    tex->debug.tag = debug_tag;
#endif
    return tex;
}


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

    clear_buffer(tex->framebuffer);
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

static void texture_set_data(texture_t *tex,
        const uint8_t *data, int w, int h, int bpp)
{
    uint8_t *buff0 = NULL, *buff1 = NULL;
    int data_type = GL_UNSIGNED_BYTE;

    if (!is_pow2(w) || !is_pow2(h)) {
        buff0 = calloc(bpp, tex->tex_w * tex->tex_h);
        blit(data, w, h, bpp, buff0, tex->tex_w, tex->tex_h);
        data = buff0;
    }

    if (tex->flags & TF_RGB_565) {  // Compress to RGB_565
        buff1 = calloc(2, tex->tex_w * tex->tex_h);
        rgb888_to_565(data, tex->tex_w * tex->tex_h, bpp, buff1);
        data = buff1;
        data_type = GL_UNSIGNED_SHORT_5_6_5;
        tex->format = GL_RGB;
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
    free(buff1);

    if (tex->flags & TF_MIPMAP)
        GL(glGenerateMipmap(GL_TEXTURE_2D));
}

static void regenerate_from_image(texture_t *tex)
{
    const char *path = tex->data;
    uint8_t *img;
    int w, h, bpp = 0;
    img = img_read(path, &w, &h, &bpp);
    texture_set_data(tex, img, w, h, bpp);
    free(img);
}

texture_t *texture_create_from_image(const char *path)
{
    texture_t *tex;
    uint8_t *img;
    int w, h, bpp = 0;
    img = img_read(path, &w, &h, &bpp);
    tex = tex_new();
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = TF_HAS_TEX;
    tex->format = (int[]){0, 0, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA}[bpp];
    texture_create_empty(tex);
    texture_set_data(tex, img, w, h, bpp);
    free(img);
    tex->data = strdup(path);
    tex->regenerate_func = regenerate_from_image;
    LOG_TEX(tex, "Create from image");
    return tex;
}

texture_t *texture_create_sub(texture_t *parent, int x, int y, int w, int h)
{
    texture_t *tex;
    texture_inc_ref(parent);
    tex = tex_new();
    tex->parent = parent;
    tex->tex = parent->tex;
    tex->format = parent->format;
    tex->flags = parent->flags;
    tex->framebuffer = parent->framebuffer;
    tex->tex_w = parent->tex_w;
    tex->tex_h = parent->tex_h;
    tex->x = parent->x + x;
    tex->y = parent->y + y;
    tex->h = h;
    tex->w = w;
    return tex;
}

typedef struct {
    int height;
    char msg[];
} text_regenerate_data_t;

texture_t *texture_create_surface(int w, int h, int flags)
{
    texture_t *tex;
    tex = tex_new();
    tex->format = (flags & TF_RGB) ? GL_RGB : GL_RGBA;
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = flags | TF_HAS_TEX;
    texture_create_empty(tex);
    LOG_TEX(tex, "Create surface");
    return tex;
}

texture_t *texture_create_buffer(int w, int h, int flags)
{
    texture_t *tex;
    tex = tex_new();
    tex->format = (flags & TF_RGB) ? GL_RGB : GL_RGBA;
    tex->tex_w = next_pow2(w);
    tex->tex_h = next_pow2(h);
    tex->w = w;
    tex->h = h;
    tex->flags = flags | TF_HAS_FB | TF_HAS_TEX;
    texture_create_empty(tex);
    generate_framebuffer(tex);
    LOG_TEX(tex, "Create buffer");
    return tex;
}

texture_t *texture_default_framebuffer(int w, int h)
{
    texture_t *tex;
    tex = tex_new();
    tex->flags = TF_IS_DEFAULT_FB;
    tex->w = tex->tex_w = w;
    tex->h = tex->tex_h = h;
    return tex;
}

void texture_clear(texture_t *tex)
{
    GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glDisable(GL_STENCIL_TEST));
    GL(glClearColor(0, 0, 0, 0));
    GL(glStencilMask(0xFF));
    GL(glClear(GL_COLOR_BUFFER_BIT |
               GL_DEPTH_BUFFER_BIT |
               GL_STENCIL_BUFFER_BIT));
}

void texture_discard(texture_t *tex)
{
    return;
#ifdef GLES2
    if (!has_gl_extension(GL_EXT_discard_framebuffer))
        return;

#ifdef ANDROID
    if (!p_glDiscardFramebufferEXT) {
        p_glDiscardFramebufferEXT =
            dlsym(RTLD_DEFAULT, "glDiscardFramebufferEXT");
        assert(p_glDiscardFramebufferEXT);
    }
#endif
    GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
    GLenum attachments[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT};
    GL(glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, attachments));
#endif
}

static void texture_release(texture_t *tex)
{
    if (tex->parent) return;
    if (tex->flags & TF_IS_DEFAULT_FB) return;
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
    tex->tex = 0;
    tex->framebuffer = 0;
    tex->depth = tex->stencil = 0;
}

static void texture_delete(texture_t *tex)
{
    if (tex->parent)
        goto end;

    LOG_TEX(tex, "destructor");
    texture_release(tex);
end:
    if (tex->parent)
        texture_dec_ref(tex->parent);
    LL_DELETE(textures, tex);
    free(tex->data);
    free(tex->key);
    free(tex);
}

static void texture_regenerate(texture_t *tex)
{
    if (tex->parent) return;
    if (tex->flags & TF_IS_DEFAULT_FB) return;
    if (tex->flags & TF_HAS_TEX)
        texture_create_empty(tex);
    if (tex->flags & TF_HAS_FB)
        generate_framebuffer(tex);
    if (tex->regenerate_func) {
        tex->regenerate_func(tex);
    }
}

void texture_get_quad(const texture_t *tex, vec2_t quad[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        quad[i]= vec2((tex->x + (i % 2) * tex->w) / (float)tex->tex_w,
                      (tex->y + (i / 2) * tex->h) / (float)tex->tex_h
        );
    }
}

void texture_dec_ref(texture_t *tex)
{
    if (!tex) return;
    assert(tex->ref);
    tex->ref--;
}

void texture_inc_ref(texture_t *tex)
{
    if (!tex) return;
    tex->ref++;
}

void texture_set_timer(texture_t *tex, float timer)
{
    tex->life = timer;
}

void textures_vacuum()
{
    texture_t *tex, *tmp;
    LL_FOREACH_SAFE(textures, tex, tmp) {
        if (tex->ref == 0) {
            texture_delete(tex);
        }
    }
}

void textures_full_vacuum()
{
    textures_vacuum();
}

int textures_count()
{
    int ret;
    texture_t *tmp;
    LL_COUNT(textures, tmp, ret);
    return ret;
}

void textures_iter(float dt)
{
    textures_vacuum();
}

void texture_save_to_file(const texture_t *tex, const char *path, int flags)
{
    LOG_I("save texture to %s", path);
    int bpp;
    uint8_t *data, *tmp;
    float *fdata = NULL;
    int i, w, h;

    w = tex->tex_w;
    h = tex->tex_h;
    GL(glBindFramebuffer(GL_FRAMEBUFFER, tex->framebuffer));
    if (flags & TF_DEPTH) {
        bpp = 1;
        fdata = calloc(w * h, sizeof(*fdata));
        data = calloc(w * h, 1);
        GL(glReadPixels(0, 0, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, fdata));
        for (i = 0; i < w * h; i++) {
            data[i] = fdata[i] * 255;
        }
    } else {
        bpp = 4;
        data = calloc(w * h, bpp);
        GL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data));
    }
    // Flip output y.
    tmp = data;
    data = calloc(w * h, bpp);
    for (i = 0; i < h; i++) {
        memcpy(&data[i * w * bpp], &tmp[(h - i - 1) * w * bpp], bpp * w);
    }
    free(tmp);

    img_write(data, w, h, bpp, path);
    free(data);
    free(fdata);
}

void texture_set_key(texture_t *tex, const char *key)
{
    free(tex->key);
    tex->key = strdup(key);
    assert(texture_search(key));
}

texture_t *texture_search(const char *key)
{
    texture_t *tex;
    LL_FOREACH(textures, tex) {
        if (tex->key && strcmp(tex->key, key) == 0)
            return tex;
    }
    return NULL;
}

#ifdef DEBUG
void texture_debug_set_tag(const char *tag)
{
    debug_tag = tag;
}
#endif

void textures_release_all()
{
    texture_t *tex;
    LL_FOREACH(textures, tex) {
        texture_release(tex);
    }
}

void textures_regenerate_all()
{
    texture_t *tex;
    LL_FOREACH(textures, tex) {
        texture_regenerate(tex);
    }

    LL_FOREACH(textures, tex) {
        if (tex->parent) {
            tex->tex = tex->parent->tex;
            tex->framebuffer = tex->parent->framebuffer;
            tex->depth = tex->parent->depth;
            tex->stencil = tex->parent->stencil;
        }
    }
}
