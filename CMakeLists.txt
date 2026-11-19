# AKAI Explorer - file-explorer-like GUI for AKAI S900/S1000/S3000 disk images
# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.21)

project(akai-explorer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(AKX_BUILD_GUI   "Build the Qt 6 Quick application"                     ON)
option(AKX_BUILD_TESTS "Build the original akaiutil CLI and the test-suite"   ON)

# ---------------------------------------------------------------------------
# akaiutil (unmodified upstream sources, GPL-2.0-or-later)
# ---------------------------------------------------------------------------
set(AKAIUTIL_DIR ${CMAKE_SOURCE_DIR}/third_party/akaiutil)
set(AKAIUTIL_LIB_SOURCES
    ${AKAIUTIL_DIR}/akaiutil.c
    ${AKAIUTIL_DIR}/akaiutil_file.c
    ${AKAIUTIL_DIR}/akaiutil_io.c
    ${AKAIUTIL_DIR}/akaiutil_take.c
    ${AKAIUTIL_DIR}/akaiutil_tar.c
    ${AKAIUTIL_DIR}/akaiutil_wav.c
    ${AKAIUTIL_DIR}/commonlib.c)

# Library flavour: console output is redirected into the application through a
# force-included shim header, so the upstream files stay byte-identical.
add_library(akaiutil_core STATIC ${AKAIUTIL_LIB_SOURCES})
target_include_directories(akaiutil_core PUBLIC ${AKAIUTIL_DIR})
target_compile_definitions(akaiutil_core PRIVATE _FILE_OFFSET_BITS=64)
target_compile_options(akaiutil_core PRIVATE -include ${CMAKE_SOURCE_DIR}/src/core/akx_shim.h -w)
set_target_properties(akaiutil_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(akaiutil_core PUBLIC ${MATH_LIBRARY})
endif()

# ---------------------------------------------------------------------------
# akx_core: C++ facade (no Qt)
# ---------------------------------------------------------------------------
add_library(akx_core STATIC
    src/core/AkaiSession.cpp
    src/core/AkaiSession.h
    src/core/Exporter.cpp
    src/core/Exporter.h
    src/core/akx_shim.h)
target_include_directories(akx_core PUBLIC src/core)
target_link_libraries(akx_core PRIVATE akaiutil_core)
target_compile_options(akx_core PRIVATE -Wall -Wextra)

add_executable(akx-cli src/cli/main.cpp)
target_link_libraries(akx-cli PRIVATE akx_core)

# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------
if(AKX_BUILD_GUI)
    add_subdirectory(src/app)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(AKX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
