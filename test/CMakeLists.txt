find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(UV_EXECUTABLE NAMES uv REQUIRED)

set(DINGO_MATRIX_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/matrix")
set(DINGO_MATRIX_TEST_CMAKE
    "${PROJECT_BINARY_DIR}/generated/matrix-tests.cmake")
set(DINGO_MATRIX_GENERATOR
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/generate.py")
file(GLOB DINGO_MATRIX_AXIS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axes/*.py")

set(DINGO_MATRIX_TEST_HEADERS
    matrix/common/assertions.h
    matrix/containers/allocator.h
    matrix/containers/container.h
    matrix/containers/custom_rtti.h
    matrix/containers/indexed.h
    matrix/containers/runtime.h
    matrix/containers/static.h
    matrix/containers/storage.h
    matrix/fixtures/constructor_detection.h
    matrix/fixtures/dependency_shapes.h
    matrix/fixtures/factory.h
    matrix/fixtures/local_bindings.h
    matrix/fixtures/nested_wrappers.h
    matrix/fixtures/shared_cyclical.h
    matrix/fixtures/values.h
    matrix/fixtures/variant_types.h
    matrix/policies/constructor_detection.h
    matrix/policies/policy_aggregate.h
    matrix/policies/policy_collection.h
    matrix/policies/policy_core.h
    matrix/policies/policy_lifetime.h
    matrix/policies/policy_special.h
    matrix/policies/resolution.h
    matrix/scenarios/factory.h
    matrix/scenarios/indexed_registration.h
    matrix/scenarios/invoke.h
    matrix/scenarios/lookup/routes.h
    matrix/scenarios/lookup/runtime_routing.h
    matrix/scenarios/parent/allocators.h
    matrix/scenarios/parent/cache.h
    matrix/scenarios/parent/collections.h
    matrix/scenarios/parent/common.h
    matrix/scenarios/parent/native.h
    matrix/scenarios/parent/origin.h
    matrix/scenarios/parent/ownership.h
    matrix/scenarios/parent/recursion.h
    matrix/scenarios/parent/resolution.h
    matrix/scenarios/parent/search.h
    matrix/scenarios/parent/semantics.h
    matrix/scenarios/parent/transactions.h
    matrix/scenarios/runtime/common.h
    matrix/scenarios/runtime/construct_dependency.h
    matrix/scenarios/runtime/container.h
    matrix/scenarios/runtime/exceptions.h
    matrix/scenarios/runtime/shared_cyclical.h
    matrix/scenarios/runtime/unique_reference.h
    matrix/scenarios/static_indexed.h
    matrix/scenarios/static_parent.h
)
set(DINGO_MATRIX_TEST_HEADER_DEPENDS ${DINGO_MATRIX_TEST_HEADERS})
list(TRANSFORM DINGO_MATRIX_TEST_HEADER_DEPENDS
    PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${DINGO_MATRIX_AXIS_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/data.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/constructor_detection.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/dependency_contract.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/dependency_composition.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/family.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/invoke.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/plugins.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/scenarios.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/shared_cyclical.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/schema.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/cmake.cmake.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/constructor_detection.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/constructor_detection_runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/dependency_composition.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/dependency_composition_runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/scenario.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/shared_cyclical.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/shared_cyclical_runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/source.cpp.j2"
    ${DINGO_MATRIX_TEST_HEADER_DEPENDS}
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

add_test(NAME dingo_matrix_generator_test
    COMMAND ${CMAKE_COMMAND} -E env
        UV_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/.uv-cache
        ${UV_EXECUTABLE} run --locked pytest -q
        ${CMAKE_CURRENT_SOURCE_DIR}/matrix/generator_test.py)
set_tests_properties(dingo_matrix_generator_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/matrix)

add_executable(dingo_unit_test
    container/construct_dependency.cpp
    container/dingo.cpp
    container/incomplete_resolve.cpp
    container/incomplete_resolve_fixture.cpp
    container/parent_container_resolution.cpp
    factory/constructor_detection.cpp
    lookup/associative_backends.cpp
    lookup/index_injection.cpp
    lookup/index_registration.cpp
    lookup/index_runtime.cpp
    lookup/index_typed.cpp
    lookup/index_unkeyed.cpp
    lookup/lookup_index.cpp
    memory/arena_allocator.cpp
    memory/object_store.cpp
    memory/tagged_ptr.cpp
    registration/binding_graph.cpp
    registration/object_sizes.cpp
    registration/registration_traits.cpp
    registration/runtime_registration.cpp
    registration/static_bindings_source.cpp
    registration/static_parent_container.cpp
    registration/type_registration.cpp
    resolution/cache.cpp
    runtime/container_runtime.cpp
    runtime/transaction.cpp
    storage/shared_type_conversion.cpp
    container/construct_common.h
    lookup/index_common.h
    registration/type_registration_common.h
    support/assert.h
    support/class.h
    support/containers.h
    support/test.h
    type/type_list.cpp
    type/type_name.cpp
    type/type_traits.cpp
)
set_source_files_properties(
    lookup/index_injection.cpp
    lookup/index_registration.cpp
    lookup/index_runtime.cpp
    lookup/index_typed.cpp
    lookup/index_unkeyed.cpp
    PROPERTIES COMPILE_OPTIONS
    "$<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:-O0>")
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

add_custom_target(dingo_matrix_test
    DEPENDS ${DINGO_MATRIX_TEST_TARGETS})

foreach(DINGO_MATRIX_TEST_TARGET IN LISTS DINGO_MATRIX_TEST_TARGETS)
    add_executable(${DINGO_MATRIX_TEST_TARGET}
        ${${DINGO_MATRIX_TEST_TARGET}_SOURCES}
        ${DINGO_MATRIX_TEST_HEADERS}
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
