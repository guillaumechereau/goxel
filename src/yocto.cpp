/*
 * This file is just there to compile all the needed yocto-gl sources, so
 * that we keeps a clean build system.
 */

#ifndef YOCTO
#   define YOCTO 1
#endif

#if YOCTO

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wcomma"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#include <cstddef>
#include <cstdio>
#include <vector>

#define YOCTO_EMBREE 0
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC

// Fix compilation on Windows.
#ifdef NOMINMAX
#undef NOMINMAX
#endif


#include "../lib/yocto/yocto_bvh.cpp"
#include "../lib/yocto/yocto_image.cpp"
#include "../lib/yocto/yocto_scene.cpp"
#include "../lib/yocto/yocto_shape.cpp"
#include "../lib/yocto/yocto_trace.cpp"

#pragma GCC diagnostic pop

#endif // YOCTO
