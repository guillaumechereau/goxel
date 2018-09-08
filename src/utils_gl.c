/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

static const char* get_gl_error_text(int code) {
    switch (code) {
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "undefined error";
    }
}

int gl_check_errors(const char *file, int line)
{
    int errors = 0;
    while (true)
    {
        GLenum x = glGetError();
        if (x == GL_NO_ERROR)
            return errors;
        LOG_E("%s:%d: OpenGL error: %d (%s)\n",
            file, line, x, get_gl_error_text(x));
        errors++;
    }
}

static int compile_shader(int shader, const char *code,
                          const char *include1,
                          const char *include2)
{
    int status, len;
    char *log;
    // Common header we add to all the shaders.
#ifndef GLES2
    const char *pre = "#define highp\n#define mediump\n#define lowp\n";
#else
    const char *pre = "";
#endif
    const char *sources[] = {pre, include1, include2, code};
    assert(code);
    glShaderSource(shader, 4, (const char**)&sources, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        log = malloc(len + 1);
        LOG_E("Compile shader error:");
        glGetShaderInfoLog(shader, len, &len, log);
        LOG_E("%s", log);
        LOG_E("%s", code);
        free(log);
        assert(false);
    }
    return 0;
}

int gl_create_prog(const char *vertex_shader_code,
                   const char *fragment_shader_code, const char *include)
{
    int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    include = include ? : "";
    assert(vertex_shader);
    if (compile_shader(vertex_shader, vertex_shader_code,
                       "#define VERTEX_SHADER\n", include))
        return 0;
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(fragment_shader);
    if (compile_shader(fragment_shader, fragment_shader_code,
                       "#define FRAGMENT_SHADER\n", include))
        return 0;
    int prog = glCreateProgram();
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);
    glLinkProgram(prog);
    int status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOG_E("Link Error");
        int len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        char log[len];
        glGetProgramInfoLog(prog, len, &len, log);
        LOG_E("%s", log);
        return 0;
    }
    return prog;
}

void gl_delete_prog(int prog)
{
    int i;
    GLuint shaders[2];
    GLint count;
    if (DEBUG) {
        GL(glGetProgramiv(prog, GL_ATTACHED_SHADERS, &count));
        assert(count == 2);
    }
    GL(glGetAttachedShaders(prog, 2, NULL, shaders));
    for (i = 0; i < 2; i++)
        GL(glDeleteShader(shaders[i]));
    GL(glDeleteProgram(prog));
}

/*
 * Function: gl_gen_fbo
 * Helper function to generate an OpenGL framebuffer object with an
 * associated texture.
 *
 * Parameters:
 *   w          - Width of the fbo.
 *   h          - Height of the fbo.
 *   format     - GL_RGBA or GL_DEPTH_COMPONENT.
 *   out_fbo    - The created fbo.
 *   out_tex    - The created texture.
 */
int gl_gen_fbo(int w, int h, GLenum format, int msaa,
               GLuint *out, GLuint *out_tex)
{
    assert(format == GL_RGBA || format == GL_DEPTH_COMPONENT);
    assert(msaa == 1); // For the moment.
    GLuint fbo, color, tex = 0, depth, internal_format;

    internal_format = (format != GL_DEPTH_COMPONENT) ? GL_UNSIGNED_BYTE
                                                     : GL_UNSIGNED_INT;
    GL(glGenFramebuffers(1, &fbo));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    GL(glGenTextures(1, &tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, format, w, h,
                    0, format, internal_format, NULL));

#ifndef GLES2
    if (format != GL_DEPTH_COMPONENT) {
        // Create color render buffer.
        GL(glGenRenderbuffers(1, &color));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, color));
        glRenderbufferStorage(GL_RENDERBUFFER, format, w, h);
        GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_RENDERBUFFER, color));
        // Create depth/stencil render buffer
        GL(glGenRenderbuffers(1, &depth));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, depth));
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, w, h);
        GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth));
        // Attach texture.
        if (tex) GL(glFramebufferTexture2D(
                    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D, tex, 0));
    } else {
        GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_TEXTURE_2D, tex, 0));
    }

#else
    if (format != GL_DEPTH_COMPONENT) {
        if (gl_has_extension("GL_OES_packed_depth_stencil")) {
            // Create depth/stencil buffer.
            GL(glGenRenderbuffers(1, &depth));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, depth));
            GL(glRenderbufferStorage(GL_RENDERBUFFER,
                                     GL_DEPTH24_STENCIL8_OES, w, h));
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                         GL_RENDERBUFFER, depth));
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                        GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth));
        } else {
            assert(false); // Not implemented yet!
        }
        if (tex) GL(glFramebufferTexture2D(
                    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D, tex, 0));
    } else {
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_EXT,
                           GL_COMPARE_REF_TO_TEXTURE_EXT));
        GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_TEXTURE_2D, tex, 0));
    }

#endif

    assert(
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    *out = fbo;
    if (tex) *out_tex = tex;
    return 0;
}

bool gl_has_extension(const char *ext)
{
    const char *str;
    GL(str = (const char*)glGetString(GL_EXTENSIONS));
    return strstr(str, ext);
}
