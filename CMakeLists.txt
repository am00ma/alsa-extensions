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
#                     Common
# =========================================================================
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/params.c
    ${CMAKE_SOURCE_DIR}/src/duplex.c)
set(COMMON_INCLUDES ${CMAKE_SOURCE_DIR}/include)
set(COMMON_LIBS     asound sndfile resample)
set(COMMON_C_FLAGS
    PRIVATE
    -O0
    -g
    -Wall
    -Wextra
    -pedantic
    -fdiagnostics-color=always)

# =========================================================================
#                     Tests
# =========================================================================
add_subdirectory(tests)
