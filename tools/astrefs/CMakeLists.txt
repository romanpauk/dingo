#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

# Exact reference edges, built only when the Clang development libraries are
# available. It is a development tool, never part of the library build.

find_package(Clang QUIET)

if(Clang_FOUND)
    add_executable(dingo_astrefs main.cpp)
    target_link_libraries(dingo_astrefs PRIVATE clang-cpp LLVM)
    target_compile_options(dingo_astrefs PRIVATE -fno-rtti)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(dingo_astrefs PRIVATE -Wno-maybe-uninitialized)
    endif()

    add_custom_target(astrefs-report
        COMMENT "Collecting exact reference edges for the model tests"
        COMMAND ${DINGO_UV_RUN_COMMAND} python
            ${PROJECT_SOURCE_DIR}/tools/astrefs/collect.py
            --astrefs $<TARGET_FILE:dingo_astrefs>
            --compile-commands ${PROJECT_BINARY_DIR}
            --out ${PROJECT_BINARY_DIR}/astrefs.jsonl
        DEPENDS dingo_astrefs
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
else()
    message(STATUS "Clang development libraries not found: astrefs disabled")
endif()
