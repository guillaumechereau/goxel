/*
  Native File Dialog Extended
  Repository: https://github.com/btzy/nativefiledialog-extended
  License: Zlib
  Authors: Bernard Teo

  This header contains a function to convert a GLFW window handle to a native window handle for
  passing to NFDe.
 */

#ifndef _NFD_GLFW3_H
#define _NFD_GLFW3_H

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <nfd.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#define NFD_INLINE inline
#else
#define NFD_INLINE static inline
#endif  // __cplusplus

/**
 *  Converts a GLFW window handle to a native window handle that can be passed to NFDe.
 *  @param sdlWindow The GLFW window handle.
 *  @param[out] nativeWindow The output native window handle, populated if and only if this function
 *  returns true.
 *  @return Either true to indicate success, or false to indicate failure.  It is intended that
 * users ignore the error and simply pass a value-initialized nfdwindowhandle_t to NFDe if this
 * function fails. */
NFD_INLINE bool NFD_GetNativeWindowFromGLFWWindow(GLFWwindow* glfwWindow,
                                                  nfdwindowhandle_t* nativeWindow) {
    GLFWerrorfun oldCallback = glfwSetErrorCallback(NULL);
    bool success = false;
#if defined(GLFW_EXPOSE_NATIVE_WIN32)
    if (!success) {
        const HWND hwnd = glfwGetWin32Window(glfwWindow);
        if (hwnd) {
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
            nativeWindow->handle = (void*)hwnd;
            success = true;
        }
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_COCOA)
    if (!success) {
        const id cocoa_window = glfwGetCocoaWindow(glfwWindow);
        if (cocoa_window) {
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_COCOA;
            nativeWindow->handle = (void*)cocoa_window;
            success = true;
        }
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (!success) {
        const Window x11_window = glfwGetX11Window(glfwWindow);
        if (x11_window != None) {
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_X11;
            nativeWindow->handle = (void*)x11_window;
            success = true;
        }
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    // For now we don't support Wayland, but we intend to support it eventually.
    // Silence the warnings.
    {
        (void)glfwWindow;
        (void)nativeWindow;
    }
#endif
    glfwSetErrorCallback(oldCallback);
    return success;
}

#undef NFD_INLINE
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // _NFD_GLFW3_H
