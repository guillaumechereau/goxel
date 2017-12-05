project(goxel)
cmake_minimum_required(VERSION 2.6)

#TODO: extend support to OS X and Windows by using good-quality FindGLFW.cmake and FindGTK3.cmake modules (plenty of those on GitHub) that don't require PkgConfig
if(UNIX AND NOT APPLE)
    set(SYSTEM_SUPPORTED TRUE)
endif()

if(NOT SYSTEM_SUPPORTED)
    message("Build platform not supported, please use scons for non-Unix/Linux systems without PkgConfig.")
endif()

set(FORMAT_SOURCES
        src/formats/dicom.c
        src/formats/gox.c
        src/formats/png.c
        src/formats/png_slices.c
        src/formats/povray.c
        src/formats/qubicle.c
        src/formats/txt.c
        src/formats/vox.c
        src/formats/voxlap.c
        src/formats/vxl.c
        src/formats/wavefront.c
        )
set(TOOLS_SOURCES
        src/tools/brush.c
        src/tools/color_picker.c
        src/tools/extrude.c
        src/tools/laser.c
        src/tools/move.c
        src/tools/plane.c
        src/tools/procedural.c
        src/tools/selection.c
        src/tools/shape.c
        )

set(SOURCES
        src/action.c
        src/assets.c
        src/cache.c
        src/camera.c
        src/color.c
        src/gesture.c
        src/gesture3d.c
        src/goxel.c
        src/gui.cpp
        src/gui_settings.c
        src/image.c
        src/main.c
        src/marchingcube.c
        src/mesh.c
        src/mesh_to_vertices.c
        src/mesh_utils.c
        src/model3d.c
        src/mustache.c
        src/palette.c
        src/procedural.c
        src/quantization.c
        src/render.c
        src/shape.c
        src/sound.c
        src/stack.c
        src/system.c
        src/texture.c
        src/theme.c
        src/tools.c
        src/utils.c
        src/vec.c)

set(HEADERS
        src/assets.inl
        src/block_def.h
        src/box.h
        src/goxel.h
        src/imgui_user.inl
        src/mesh.h
        src/plane.h
        src/procedural.inl
        src/sound_openal.inl
        src/vec.h)


set(UTHASH_HEADERS
        ext_src/uthash/utarray.h
        ext_src/uthash/uthash.h
        ext_src/uthash/utlist.h)

set(NOC_HEADERS
        ext_src/noc/noc_file_dialog.h)

set(STB_HEADERS
        ext_src/stb/stb_image.h
        ext_src/stb/stb_image_write.h
        ext_src/stb/stb_rect_pack.h
        ext_src/stb/stb_textedit.h
        ext_src/stb/stb_truetype.h)

set(IMGUI_SOURCES
        ext_src/imgui/imgui.cpp
        ext_src/imgui/imgui_draw.cpp)
set(IMGUI_HEADERS
        ext_src/imgui/imgui.h
        ext_src/imgui/imgui_internal.h
        ext_src/imgui/imconfig.h)


# Make this a global include, since imgui is using it
include_directories(ext_src/stb)

find_package(PNG)
find_package(GLUT)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
PKG_CHECK_MODULES(GTK REQUIRED gtk+-3.0)
add_subdirectory(ext_src)

#Some headers included as sources in order for CMake-reliant IDEs (such as CLion) to be able to parse them
add_executable(${PROJECT_NAME}
        ${HEADERS}
        ${NOC_HEADERS}
        ${UTHASH_HEADERS}
        ${STB_HEADERS}
        ${IMGUI_HEADERS}
        ${IMGUI_SOURCES}
        ${FORMAT_SOURCES}
        ${TOOLS_SOURCES}
        ${SOURCES})

link_directories(${GTK3_LIBRARY_DIRS})

target_include_directories(${PROJECT_NAME} PUBLIC
        ${GLUT_INCLUDE_DIR}
        ${GLFW_INCLUDE_DIRS}
        ${GTK_INCLUDE_DIRS}
        ${PNG_INCLUDE_DIRS}
        ${GTK_INCLUDE_DIRS}
        ${GLUT_INCLUDE_DIR}
        ext_src/uthash
        ext_src/noc
        ext_src/imgui
        ext_src/inih
        ext_src/stb
        src)

target_link_libraries(${PROJECT_NAME}
        ${PNG_LIBRARIES}
        ${GLUT_LIBRARIES}
        ${GLFW_LIBRARIES}
        ${GLFW_STATIC_LIBRARIES}
        ${GTK_LIBRARIES}
        inih)

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_INCLUDE_IMGUI_USER_INL ${PNG_DEFINITIONS})
if(PNG_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_LIBPNG=1)
endif()
if(GLUT_FOUND)
endif()