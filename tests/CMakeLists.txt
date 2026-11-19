# SPDX-License-Identifier: GPL-2.0-or-later

# The original command line tool, built from the same unmodified sources.
# It creates the test images and serves as the reference implementation.
add_executable(akaiutil ${AKAIUTIL_LIB_SOURCES} ${AKAIUTIL_DIR}/akaiutil_main.c)
target_include_directories(akaiutil PRIVATE ${AKAIUTIL_DIR})
target_compile_definitions(akaiutil PRIVATE _FILE_OFFSET_BITS=64)
target_compile_options(akaiutil PRIVATE -w)
if(MATH_LIBRARY)
    target_link_libraries(akaiutil PRIVATE ${MATH_LIBRARY})
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(AKX_TESTDATA ${CMAKE_BINARY_DIR}/testdata)
add_custom_command(
    OUTPUT ${AKX_TESTDATA}/.stamp
    COMMAND bash ${CMAKE_SOURCE_DIR}/tools/make_test_images.sh $<TARGET_FILE:akaiutil> ${AKX_TESTDATA}
    COMMAND ${CMAKE_COMMAND} -E touch ${AKX_TESTDATA}/.stamp
    DEPENDS akaiutil ${CMAKE_SOURCE_DIR}/tools/make_test_images.sh ${CMAKE_SOURCE_DIR}/tools/gen_test_wavs.py
    COMMENT "Generating synthetic AKAI test disk images"
    VERBATIM)
add_custom_target(testdata ALL DEPENDS ${AKX_TESTDATA}/.stamp)

add_executable(akx_core_test core_test.cpp)
target_link_libraries(akx_core_test PRIVATE akx_core)
add_dependencies(akx_core_test testdata)

add_test(NAME core_test COMMAND akx_core_test ${AKX_TESTDATA})
add_test(NAME compare_with_akaiutil
         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compare_with_akaiutil.sh
                 $<TARGET_FILE:akaiutil> $<TARGET_FILE:akx-cli> ${AKX_TESTDATA} ${CMAKE_BINARY_DIR}/compare)
