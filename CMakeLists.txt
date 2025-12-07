cmake_minimum_required(VERSION 3.12)

# =========================================================================
#                       Project
# =========================================================================
project(
    "alsa-extensions"
    VERSION 0.0.2
    DESCRIPTION "Extensions to alsa-lib for duplex streams on multiple devices"
    LANGUAGES C)

option(SNDX_BUILD_TESTS "Build test programs" ON)

# =========================================================================
#                      Config
# =========================================================================
set(CMAKE_C_STANDARD              23)
set(CMAKE_C_STANDARD_REQUIRED     ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
include(CTest)

# =========================================================================
#                     Common
# =========================================================================
set(LIB_SOURCES
    src/params.c
    src/pcm_lfloat.c
    src/buffer.c
    src/ring.c
    src/timer.c
    src/pollfds.c
    src/xrun.c
    src/duplex.c)
set(LIB_INCLUDES include)
set(LIB_LIBS     m asound sndfile)
set(LIB_C_FLAGS
    PRIVATE
    -O0
    -g
    -Wall
    -Wextra
    # -pedantic # plugin_ops.h
    -fdiagnostics-color=always)

set(LIB_NAME sndx)
add_library(${LIB_NAME} STATIC)
target_link_libraries(${LIB_NAME} PUBLIC ${LIB_LIBS})
target_sources(${LIB_NAME} PRIVATE ${LIB_SOURCES})
target_include_directories(${LIB_NAME} PUBLIC ${LIB_INCLUDES})
target_compile_options(${LIB_NAME} ${LIB_C_FLAGS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

# =========================================================================
#                     Tests
# =========================================================================
if(SNDX_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# =========================================================================
#                     References
# =========================================================================
# add_subdirectory(refs)

# =========================================================================
#                     Examples
# =========================================================================
add_subdirectory(examples)

# =========================================================================
#                     Tools
# =========================================================================
add_subdirectory(tools)

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
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
set(DOXYGEN_EXAMPLE_PATH           examples)
set(DOXYGEN_EXAMPLE_PATTERNS       *.c)
set(DOXYGEN_EXAMPLE_RECURSIVE      YES)
set(DOXYGEN_INCLUDE_PATH           include)
set(DOXYGEN_IMAGE_PATH             docs/img)

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
    "docs/doxygen-awesome.css")

# https://cmake.org/cmake/help/latest/module/FindDoxygen.html
doxygen_add_docs(sndx_docs
    README.md
    src
    include
    docs
    ALL)

