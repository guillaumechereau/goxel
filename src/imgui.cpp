/*
 * Just include the imgui cpp files, so that we don't have to handle them
 * in the Scons file.
 */

// Prevent warnings with gcc.
#ifndef __clang__
#pragma GCC diagnostic push
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

#include "../ext_src/imgui/imgui.cpp"
#include "../ext_src/imgui/imgui_draw.cpp"
#include "../ext_src/imgui/imgui_widgets.cpp"

#ifdef __clang__
#pragma GCC diagnostic pop
#endif
