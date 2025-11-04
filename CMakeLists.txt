cmake_minimum_required(VERSION 3.5)

# =========================================================================
#                       Project
# =========================================================================
project(
    "alsa-extensions"
    VERSION 0.0.1
    DESCRIPTION "Extensions to alsa-lib for multiple devices"
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
find_package(Doxygen)

if(Doxygen_FOUND)
    set(DOXYGEN_FILE_PATTERNS *.h *.c *.md)
    set(DOXYGEN_HTML_COLORSTYLE "TOGGLE")
    set(DOXYGEN_QUIET YES)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)

    doxygen_add_docs(example_docs ${CMAKE_CURRENT_SOURCE_DIR} ALL)
endif()

# =========================================================================
#                     Common
# =========================================================================
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/memops.c
    ${CMAKE_SOURCE_DIR}/src/params.c
    ${CMAKE_SOURCE_DIR}/src/buffer.c
    ${CMAKE_SOURCE_DIR}/src/xrun.c
    ${CMAKE_SOURCE_DIR}/src/duplex.c)
set(COMMON_INCLUDES ${CMAKE_SOURCE_DIR}/include)
set(COMMON_LIBS     m asound sndfile resample)
set(COMMON_C_FLAGS
    PRIVATE
    -O0
    -g
    -Wall
    -Wextra
    -pedantic
    -fdiagnostics-color=always)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

# =========================================================================
#                     Tests
# =========================================================================
add_subdirectory(tests)

# =========================================================================
#                     Examples
# =========================================================================
add_subdirectory(examples)
