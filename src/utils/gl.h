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

#ifndef GL_H
#define GL_H

#include <stdbool.h>

// Set the DEBUG macro if needed.
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif

// Include OpenGL properly.
#define GL_GLEXT_PROTOTYPES
#ifdef WIN32
#    include <windows.h>
#    include "GL/glew.h"
#endif
#ifdef __APPLE__
#   include "TargetConditionals.h"
#   if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#       define GLES2 1
#       include <OPenGLES/ES2/gl.h>
#       include <OpenGLES/ES2/glext.h>
#   else
#       include <OpenGL/gl.h>
#   endif
#else
#   ifdef GLES2
#       include <GLES2/gl2.h>
#       include <GLES2/gl2ext.h>
#   else
#       include <GL/gl.h>
#   endif
#endif

// GL macro that automatically checks for errors in debug mode.
#if DEBUG
#  define GL(line) ({                                                   \
       line;                                                            \
       if (gl_check_errors(__FILE__, __LINE__)) assert(false);          \
   })
#else
#  define GL(line) line
#endif

typedef struct gl_uniform {
    char        name[64];
    GLint       size;
    GLenum      type;
    GLint       loc;
} gl_uniform_t;

typedef struct gl_shader {
    GLint           prog;
    gl_uniform_t    uniforms[32];
} gl_shader_t;


int gl_check_errors(const char *file, int line);

/*
 * Function: gl_has_extension
 * Check whether an OpenGL extension is available.
 */
bool gl_has_extension(const char *extension);

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
               GLuint *out_fbo, GLuint *out_tex);


/*
 * Function: gl_shader_create
 * Helper function that compiles an opengl shader.
 *
 * Parameters:
 *   vert       - The vertex shader code.
 *   frag       - The fragment shader code.
 *   include    - Extra includes added to both shaders.
 *   attr_names - NULL terminated list of attribute names that will be binded.
 *
 * Return:
 *   A new gl_shader_t instance.
 */
gl_shader_t *gl_shader_create(const char *vert, const char *frag,
                              const char *include, const char **attr_names);

void gl_shader_delete(gl_shader_t *shader);

bool gl_has_uniform(gl_shader_t *shader, const char *name);
void gl_update_uniform(gl_shader_t *shader, const char *name, ...);

#endif // GL_H
