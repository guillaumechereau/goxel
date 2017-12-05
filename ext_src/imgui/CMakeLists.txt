project(imgui)
#TODO: see the TODO in ext_src/CMakeLists.txt
set(SOURCES
        imgui.cpp
        imgui_draw.cpp)
set(HEADERS
        imconfig.h
        imgui.h
        imgui_internal.h)

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})
