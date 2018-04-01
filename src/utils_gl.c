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

static int compile_shader(int shader, const char *code, const char *include)
{
    int status, len;
    char *log;
#ifndef GLES2
    const char *pre = "#define highp\n#define mediump\n#define lowp\n";
#else
    const char *pre = "";
#endif
    const char *sources[] = {pre, include, code};
    glShaderSource(shader, 3, (const char**)&sources, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        log = malloc(len + 1);
        LOG_E("Compile shader error:");
        glGetShaderInfoLog(shader, len, &len, log);
        LOG_E("%s", log);
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
    if (compile_shader(vertex_shader, vertex_shader_code, include))
        return 0;
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(fragment_shader);
    if (compile_shader(fragment_shader, fragment_shader_code, include))
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
