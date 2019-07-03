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

/*
 * This file is just there to compile all the needed yocto-gl sources, so
 * that we keeps a clean build system.
 */

#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#include <cstddef>
#include <cstdio>
#include <vector>

// Attempt to fix a bug with clang.
#ifdef __clang__
template<class T>
static int size(const std::vector<T> &x) {
    return x.size();
}
#endif


#define YOCTO_EMBREE 0
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC

// #include "../ext_src/yocto/ext/ArHosekSkyModel.cpp"
#include "../ext_src/yocto/yocto_bvh.cpp"
#include "../ext_src/yocto/yocto_image.cpp"
#include "../ext_src/yocto/yocto_scene.cpp"
#include "../ext_src/yocto/yocto_shape.cpp"
#include "../ext_src/yocto/yocto_trace.cpp"

#define file_holder yocto_obj_file_holder
#define open_input_file yocto_obj_open_input_file
#include "../ext_src/yocto/yocto_obj.cpp"

#ifndef __clang__
#pragma GCC diagnostic pop
#endif
