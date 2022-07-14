/* This file is only here to be included by cycles ! */

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

#ifndef GLEW_VERSION_1_5
#   define GLEW_VERSION_1_5 0
#endif
