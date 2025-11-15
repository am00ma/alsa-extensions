cmake_minimum_required(VERSION 3.12)

# =========================================================================
#                       Project
# =========================================================================
project(
    "alsa-extensions"
    VERSION 0.0.2
    DESCRIPTION "Extensions to alsa-lib for duplex streams on multiple devices"
    LANGUAGES C)

# =========================================================================
#                      Config
# =========================================================================
set(CMAKE_C_STANDARD              23)
set(CMAKE_C_STANDARD_REQUIRED     ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
include(CTest)

# =========================================================================
#                      Documentation
# =========================================================================
find_package(Doxygen REQUIRED)

set(DOXYGEN_QUIET                  YES)
set(DOXYGEN_WARN_IF_UNDOCUMENTED   NO)
set(DOXYGEN_WARN_IF_INCOMPLETE_DOC NO)
set(DOXYGEN_WARN_NO_PARAMDOC       NO)

set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C  YES)
set(DOXYGEN_EXTRACT_STATIC         YES)
set(DOXYGEN_EXTRACT_ALL            YES)
set(DOXYGEN_RECURSIVE              YES)
set(DOXYGEN_CREATE_SUBDIRS         YES)
set(DOXYGEN_FILE_PATTERNS          *.h *.c *.md)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${CMAKE_SOURCE_DIR}/README.md)
set(DOXYGEN_EXAMPLE_PATH           ${CMAKE_SOURCE_DIR}/examples)
set(DOXYGEN_EXAMPLE_PATTERNS       *.c)
set(DOXYGEN_EXAMPLE_RECURSIVE      YES)
set(DOXYGEN_INCLUDE_PATH           ${CMAKE_SOURCE_DIR}/include)
set(DOXYGEN_IMAGE_PATH             ${CMAKE_SOURCE_DIR}/docs/img)

set(DOXYGEN_PLANTUML_JAR_PATH   $(HOME)/.local/bin/plantuml.jar)
set(DOXYGEN_HAVE_DOT            YES)
set(DOXYGEN_HAVE_GRAPHVIZ       YES)
set(DOXYGEN_DOT_IMAGE_FORMAT    "svg")
set(DOXYGEN_CALL_GRAPH          YES)
set(DOXYGEN_CALLER_GRAPH        YES)
set(DOXYGEN_UML_LOOK            YES)
set(DOXYGEN_DOT_GRAPH_MAX_NODES 50)
set(DOXYGEN_INTERACTIVE_SVG     YES)

set(DOXYGEN_HTML_COLORSTYLE   "TOGGLE")
set(DOXYGEN_GENERATE_TREEVIEW YES)
set(DOXYGEN_DISABLE_INDEX     NO)
set(DOXYGEN_FULL_SIDEBAR      NO)
set(DOXYGEN_HTML_EXTRA_STYLESHEET
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen-awesome.css")

# https://cmake.org/cmake/help/latest/module/FindDoxygen.html
doxygen_add_docs(example_docs
    ${CMAKE_SOURCE_DIR}/README.md
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/docs
    ALL)

# =========================================================================
#                     Common
# =========================================================================
set(LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/memops.c
    ${CMAKE_SOURCE_DIR}/src/params.c
    ${CMAKE_SOURCE_DIR}/src/buffer.c
    ${CMAKE_SOURCE_DIR}/src/xrun.c
    ${CMAKE_SOURCE_DIR}/src/timer.c
    ${CMAKE_SOURCE_DIR}/src/duplex.c)
set(LIB_INCLUDES ${CMAKE_SOURCE_DIR}/include)
set(LIB_LIBS     m asound sndfile)
set(LIB_C_FLAGS
    PRIVATE
    -O0
    -g
    -Wall
    -Wextra
    -pedantic
    -fdiagnostics-color=always)

set(LIB_NAME alsax)
add_library(${LIB_NAME} STATIC)
target_link_libraries(${LIB_NAME} PUBLIC ${LIB_LIBS})
target_sources(${LIB_NAME} PRIVATE ${LIB_SOURCES})
target_include_directories(${LIB_NAME} PUBLIC ${LIB_INCLUDES})
target_compile_options(${LIB_NAME} ${LIB_C_FLAGS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

# =========================================================================
#                     Tests
# =========================================================================
add_subdirectory(tests)

# =========================================================================
#                     References
# =========================================================================
# add_subdirectory(refs)

# =========================================================================
#                     Examples
# =========================================================================
add_subdirectory(examples)
