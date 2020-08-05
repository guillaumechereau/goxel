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
