/*
 * This file gets included before any compiled file.  So we can use it
 * to set configuration macros that affect external libraries.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#pragma GCC diagnostic ignored "-Wpragmas"

// Define the LOG macros, so that they get available in the utils files.
#include "log.h"

// Disable yocto with older version of gcc, since it doesn't compile then.
#ifndef YOCTO
#   if !defined(__clang__) && __GNUC__ < 6
#       define YOCTO 0
#   endif
#endif

// Disable OpenGL deprecation warnings on Mac.
#define GL_SILENCE_DEPRECATION 1

#ifdef __cplusplus
}
#endif
