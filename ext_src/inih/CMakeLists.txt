project(inih)

set(SOURCES
        ini.c)
set(HEADERS
        ini.h)

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_compile_definitions(${PROJECT_NAME} PRIVATE INI_HANDLER_LINENO=1)
