find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(UV_EXECUTABLE NAMES uv REQUIRED)

set(DINGO_MATRIX_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/matrix")
set(DINGO_MATRIX_TEST_CMAKE
    "${PROJECT_BINARY_DIR}/generated/matrix-tests.cmake")
set(DINGO_MATRIX_GENERATOR
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/generate.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axis_config.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axis_containers.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axis_exposed_types.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axis_features.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axis_resolved_types.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axis_stored_types.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/data.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/plugins.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/schema.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/cmake.cmake.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/source.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/allocator_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/container_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/custom_rtti_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/factory_override.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/indexed_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/indexed_registration.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/local_bindings.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/nested_container.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/nested_wrappers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/runtime_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/runtime_regressions.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/scenario.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/static_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/storage_containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/values.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/support/variant_types.h"
    "${DINGO_MATRIX_GENERATOR}")

set(DINGO_MATRIX_GENERATOR_COMMAND
    ${CMAKE_COMMAND} -E env
        UV_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/.uv-cache
        ${UV_EXECUTABLE} run --locked python
        ${DINGO_MATRIX_GENERATOR}
        --out ${DINGO_MATRIX_TEST_DIR}
        --cmake ${DINGO_MATRIX_TEST_CMAKE})

execute_process(
    COMMAND ${DINGO_MATRIX_GENERATOR_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_MATRIX_GENERATOR_RESULT)
if(NOT DINGO_MATRIX_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate matrix tests: "
        "${DINGO_MATRIX_GENERATOR_RESULT}")
endif()
include(${DINGO_MATRIX_TEST_CMAKE})

add_executable(dingo_unit_test
    container/construct_dependency.cpp
    container/dingo.cpp
    container/incomplete_resolve.cpp
    container/incomplete_resolve_fixture.cpp
    factory/constructor_detection.cpp
    index/index.cpp
    registration/binding_graph.cpp
    registration/static_bindings_source.cpp
    registration/static_parent_container.cpp
    registration/type_registration.cpp
    storage/shared_type_conversion.cpp
    container/construct_common.h
    support/assert.h
    support/class.h
    support/containers.h
    support/test.h
    type/type_list.cpp
    type/type_map.cpp
    type/type_name.cpp
    type/type_traits.cpp
)
add_test(NAME dingo_unit_test COMMAND dingo_unit_test)
set_tests_properties(dingo_unit_test PROPERTIES LABELS "unit")
target_link_libraries(dingo_unit_test dingo::dingo gtest_main)
target_include_directories(dingo_unit_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(dingo_unit_test PRIVATE
    $<$<BOOL:${DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION}>:
        DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION=1>
)

target_compile_options(dingo_unit_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

# Force-include windows.h to each test to make sure library builds with windows.h included
target_compile_options(dingo_unit_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)

set(DINGO_MATRIX_TEST_SUPPORT_HEADERS
    matrix/features/construct_invoke.h
    matrix/features/resolve_concrete.h
    matrix/features/resolve_collection.h
    matrix/features/resolve_indexed.h
    matrix/features/resolve_interface.h
    matrix/features/resolve_keyed.h
    matrix/features/resolve_unique_wrapper.h
    matrix/support/allocator_containers.h
    matrix/support/container_containers.h
    matrix/support/custom_rtti_containers.h
    matrix/support/factory_override.h
    matrix/support/indexed_containers.h
    matrix/support/indexed_registration.h
    matrix/support/local_bindings.h
    matrix/support/nested_container.h
    matrix/support/nested_wrappers.h
    matrix/support/runtime_containers.h
    matrix/support/runtime_regressions.h
    matrix/support/scenario.h
    matrix/support/static_containers.h
    matrix/support/storage_containers.h
    matrix/support/values.h
    matrix/support/variant_types.h
)

add_custom_target(dingo_matrix_test
    DEPENDS ${DINGO_MATRIX_TEST_TARGETS})

foreach(DINGO_MATRIX_TEST_TARGET IN LISTS DINGO_MATRIX_TEST_TARGETS)
    add_executable(${DINGO_MATRIX_TEST_TARGET}
        ${${DINGO_MATRIX_TEST_TARGET}_SOURCES}
        ${DINGO_MATRIX_TEST_SUPPORT_HEADERS}
    )
    add_test(NAME ${DINGO_MATRIX_TEST_TARGET}
        COMMAND ${DINGO_MATRIX_TEST_TARGET})
    set_tests_properties(${DINGO_MATRIX_TEST_TARGET}
        PROPERTIES LABELS "runtime")
    target_link_libraries(${DINGO_MATRIX_TEST_TARGET}
        dingo::dingo gtest_main)
    target_include_directories(${DINGO_MATRIX_TEST_TARGET}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${DINGO_MATRIX_TEST_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_compile_options(${DINGO_MATRIX_TEST_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)
endforeach()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    # The unit test binary still carries some template-heavy coverage
    # from the old integration tests. Keep this runner's executable
    # linkable in GCC Release builds.
    target_compile_definitions(dingo_unit_test PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:
            DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION=1>
    )
    target_compile_options(dingo_unit_test PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Os>
    )
endif()

option(DINGO_LIT_TESTS_ENABLED "Enable lit-based tests for static assertions and diagnostics." ON)
if(DINGO_LIT_TESTS_ENABLED)
    find_program(LIT_EXECUTABLE NAMES lit)
    set(DINGO_LIT_EXEC_ROOT "${PROJECT_BINARY_DIR}/test/lit")

    if(UV_EXECUTABLE)
        add_test(NAME dingo_lit_tests
            COMMAND ${CMAKE_COMMAND} -E env
                CXX=${CMAKE_CXX_COMPILER}
                DINGO_CXX_STD=${CMAKE_CXX_STANDARD}
                UV_CACHE_DIR=${PROJECT_BINARY_DIR}/.uv-cache
                ${UV_EXECUTABLE} run --locked lit -sv
                    -D DINGO_LIT_EXEC_ROOT=${DINGO_LIT_EXEC_ROOT}
                    ${CMAKE_CURRENT_SOURCE_DIR}/lit)
    elseif(LIT_EXECUTABLE)
        add_test(NAME dingo_lit_tests
            COMMAND ${CMAKE_COMMAND} -E env
                CXX=${CMAKE_CXX_COMPILER}
                DINGO_CXX_STD=${CMAKE_CXX_STANDARD}
                ${LIT_EXECUTABLE} -sv
                    -D DINGO_LIT_EXEC_ROOT=${DINGO_LIT_EXEC_ROOT}
                    ${CMAKE_CURRENT_SOURCE_DIR}/lit)
    else()
        message(WARNING "Skipping lit tests: neither lit nor uv was found. "
                        "Enable by installing lit (or uv) and setting "
                        "DINGO_LIT_TESTS_ENABLED=ON.")
    endif()
    if(TEST dingo_lit_tests)
        set_tests_properties(dingo_lit_tests PROPERTIES LABELS "compile-only")
    endif()
endif()
