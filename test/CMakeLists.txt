include("${PROJECT_SOURCE_DIR}/cmake/uv.cmake")
uv_get_run_command(DINGO_UV_RUN_COMMAND
    PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

set(DINGO_MATRIX_PROFILE "full" CACHE STRING
    "Generated matrix execution profile (full or portable)")
set_property(CACHE DINGO_MATRIX_PROFILE PROPERTY STRINGS full portable)
if(NOT DINGO_MATRIX_PROFILE MATCHES "^(full|portable)$")
    message(FATAL_ERROR
        "DINGO_MATRIX_PROFILE must be either full or portable")
endif()
option(DINGO_MATRIX_RECORD
    "Record matrix assertion outcomes for differential testing" OFF)
option(DINGO_MATRIX_TEST_AFTER_BUILD
    "Run each matrix test executable immediately after linking" OFF)

set(DINGO_MATRIX_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/matrix")
set(DINGO_MATRIX_TEST_CMAKE
    "${PROJECT_BINARY_DIR}/generated/matrix-tests.cmake")
set(DINGO_MATRIX_GENERATOR
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/generate.py")
set(DINGO_MATRIX_COMPILER_ARCHITECTURE "${CMAKE_GENERATOR_PLATFORM}")
if(NOT DINGO_MATRIX_COMPILER_ARCHITECTURE)
    set(DINGO_MATRIX_COMPILER_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(MSVC)
    set(DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT 12)

    # To identify any future crashing row, uncomment these lines after its
    # failing executable is known.
    # list(APPEND DINGO_MATRIX_DEPENDENCY_COMPOSITION_DIAGNOSTIC_ARGS
    #     --dependency-composition-isolate-executable invoke:4
    #     --dependency-composition-isolate-executable resolve:4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    # Bound peak compiler memory as the full composition matrix expands.
    set(DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT 64)
else()
    set(DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT 0)
endif()
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
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/exclusions.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/family.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/invoke.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/plugins.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/report.py"
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
    ${DINGO_UV_RUN_COMMAND} python
        ${DINGO_MATRIX_GENERATOR}
        --out ${DINGO_MATRIX_TEST_DIR}
        --cmake ${DINGO_MATRIX_TEST_CMAKE}
        --profile ${DINGO_MATRIX_PROFILE}
        --compiler-id=${CMAKE_CXX_COMPILER_ID}
        --compiler-version=${CMAKE_CXX_COMPILER_VERSION}
        --compiler-architecture=${DINGO_MATRIX_COMPILER_ARCHITECTURE}
        --dependency-composition-case-limit
            ${DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT}
        ${DINGO_MATRIX_DEPENDENCY_COMPOSITION_DIAGNOSTIC_ARGS})

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
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        --basetemp ${CMAKE_CURRENT_BINARY_DIR}/matrix-generator-test
        ${CMAKE_CURRENT_SOURCE_DIR}/matrix/generator_test.py)
set_tests_properties(dingo_matrix_generator_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/matrix)

add_test(NAME dingo_spec_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q -p no:cacheprovider
        ${PROJECT_SOURCE_DIR}/spec/tests)
set_tests_properties(dingo_spec_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    ENVIRONMENT
        "PYTHONDONTWRITEBYTECODE=1;DINGO_SPEC_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/spec-test")

add_test(NAME dingo_routes_conform_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q -p no:cacheprovider
        ${CMAKE_CURRENT_SOURCE_DIR}/routes/conform_test.py
        ${CMAKE_CURRENT_SOURCE_DIR}/routes/messages_test.py)
set_tests_properties(dingo_routes_conform_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# The witnesses of the algebra, compiled against the library.
add_test(NAME dingo_routes_witness_test
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${CMAKE_CURRENT_SOURCE_DIR}/routes/witness.py
        --work-dir ${CMAKE_CURRENT_BINARY_DIR}/routes-witness
        --cxx-command "${CMAKE_CXX_COMPILER} -std=c++20 -I${PROJECT_SOURCE_DIR}/include")
set_tests_properties(dingo_routes_witness_test PROPERTIES
    LABELS "next;routes"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_test(NAME dingo_selection_conform_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q -p no:cacheprovider
        ${CMAKE_CURRENT_SOURCE_DIR}/selection/selection_test.py)
set_tests_properties(dingo_selection_conform_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_test(NAME dingo_differential_compare_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q -p no:cacheprovider
        ${CMAKE_CURRENT_SOURCE_DIR}/differential/compare_test.py)
set_tests_properties(dingo_differential_compare_test PROPERTIES
    LABELS "unit;generator"
    ENVIRONMENT
        "PYTHONDONTWRITEBYTECODE=1;DINGO_DIFFERENTIAL_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/differential-test")

# The tests of the next implementation of the spec-driven design, the library.
# The engine cases are generated from the reference implementation of the spec.
set(DINGO_NEXT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/next")
file(GLOB_RECURSE DINGO_NEXT_SPEC_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/spec/*.py")
add_custom_command(
    OUTPUT "${DINGO_NEXT_GENERATED_DIR}/conversion_cases.cpp"
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        ${DINGO_UV_RUN_COMMAND} python
        "${CMAKE_CURRENT_SOURCE_DIR}/next/conversion_cases.py"
        --output "${DINGO_NEXT_GENERATED_DIR}/conversion_cases.cpp"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/next/conversion_cases.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/next/templates/conversion_cases.cpp.j2"
        ${DINGO_NEXT_SPEC_SOURCES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating the engine cases of the next implementation"
    VERBATIM)

add_executable(dingo_next_test
    next/containers/allocator.cpp
    next/containers/annotated.cpp
    next/containers/array_handles.cpp
    next/containers/collections.cpp
    next/containers/construct.cpp
    next/containers/construction.cpp
    next/containers/container.cpp
    next/containers/container_move.cpp
    next/containers/dependency_chain.cpp
    next/containers/diagnostics.cpp
    next/containers/factories.cpp
    next/containers/incomplete_resolve.cpp
    next/containers/incomplete_resolve_fixture.cpp
    next/containers/keyed_diagnostics.cpp
    next/containers/keys.cpp
    next/containers/local.cpp
    next/containers/lookups.cpp
    next/containers/mixed.cpp
    next/containers/nested_requests.cpp
    next/containers/object_routes.cpp
    next/containers/parents.cpp
    next/containers/recursion.cpp
    next/containers/runtime_registration.cpp
    next/containers/scope_headers.cpp
    next/containers/scopes.cpp
    next/containers/shared_cyclical.cpp
    next/containers/static_bindings.cpp
    next/containers/static_parents.cpp
    next/containers/stored_type_trees.cpp
    next/containers/transactions.cpp
    next/containers/unique.cpp
    next/containers/variants.cpp
    next/backend/index.cpp
    next/backend/local.cpp
    next/backend/memo.cpp
    next/backend/object_sizes.cpp
    next/backend/slot.cpp
    next/backend/static_graph.cpp
    next/base/mechanics.cpp
    next/base/type_identity.cpp
    next/base/type_identity_fixture.cpp
    next/base/type_list.cpp
    next/conversion/conversion_conformance.cpp
    next/conversion/execute.cpp
    next/conversion/registration.cpp
    next/conversion/routes.cpp
    next/memory/allocation.cpp
    next/constructor/constructor.cpp
    next/diagnostics/diagnostic_message.cpp
    next/memory/retained.cpp
    next/memory/transaction_log.cpp
    next/diagnostics/type_name.cpp
    next/conversion/request_types.cpp
    next/type/type_tree.cpp
    "${DINGO_NEXT_GENERATED_DIR}/conversion_cases.cpp"
)
target_include_directories(dingo_next_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/next")
target_link_libraries(dingo_next_test PRIVATE dingo::dingo gtest_main)
add_test(NAME dingo_next_test COMMAND dingo_next_test)
set_tests_properties(dingo_next_test PROPERTIES LABELS "unit;next")

add_custom_target(dingo_matrix_test
    DEPENDS ${DINGO_MATRIX_TEST_TARGETS})

function(dingo_configure_matrix_compile_target DINGO_MATRIX_TARGET)
    target_link_libraries(${DINGO_MATRIX_TARGET}
        PRIVATE dingo::dingo gtest_main)
    target_include_directories(${DINGO_MATRIX_TARGET}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${DINGO_MATRIX_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_compile_options(${DINGO_MATRIX_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)
    if(DINGO_MATRIX_RECORD)
        target_compile_definitions(${DINGO_MATRIX_TARGET} PRIVATE
            DINGO_MATRIX_RECORD=1)
    endif()
    # A differential build of the baseline asserts what the baseline does where
    # a behavior change of the spec declares a difference.
    if(DINGO_DIFFERENTIAL_INCLUDE_DIR)
        target_compile_definitions(${DINGO_MATRIX_TARGET} PRIVATE
            DINGO_MATRIX_BASELINE=1)
    endif()
endfunction()

foreach(DINGO_MATRIX_TEST_TARGET IN LISTS DINGO_MATRIX_TEST_TARGETS)
    add_executable(${DINGO_MATRIX_TEST_TARGET}
        ${${DINGO_MATRIX_TEST_TARGET}_SOURCES}
        ${DINGO_MATRIX_TEST_HEADERS}
    )
    add_test(NAME ${DINGO_MATRIX_TEST_TARGET}
        COMMAND ${DINGO_MATRIX_TEST_TARGET})
    if(DINGO_MATRIX_TEST_AFTER_BUILD)
        add_custom_command(TARGET ${DINGO_MATRIX_TEST_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env
                ASAN_OPTIONS=use_sigaltstack=false
                $<TARGET_FILE:${DINGO_MATRIX_TEST_TARGET}>
            COMMENT "Running ${DINGO_MATRIX_TEST_TARGET}"
            USES_TERMINAL
            VERBATIM)
    endif()
    set_tests_properties(${DINGO_MATRIX_TEST_TARGET}
        PROPERTIES LABELS "runtime")
    dingo_configure_matrix_compile_target(${DINGO_MATRIX_TEST_TARGET})
    if(DINGO_MATRIX_RECORD)
        target_sources(${DINGO_MATRIX_TEST_TARGET} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/differential/recorder.cpp")
    endif()

    set(DINGO_MATRIX_DIAGNOSTIC_INDEX 0)
    foreach(DINGO_MATRIX_ISOLATED_SOURCE IN LISTS
            ${DINGO_MATRIX_TEST_TARGET}_ISOLATED_SOURCES)
        math(EXPR DINGO_MATRIX_DIAGNOSTIC_INDEX
            "${DINGO_MATRIX_DIAGNOSTIC_INDEX} + 1")
        string(REPLACE "dingo_matrix_test_" "dingo_matrix_diag_"
            DINGO_MATRIX_DIAGNOSTIC_PREFIX ${DINGO_MATRIX_TEST_TARGET})
        set(DINGO_MATRIX_DIAGNOSTIC_TARGET
            "${DINGO_MATRIX_DIAGNOSTIC_PREFIX}_${DINGO_MATRIX_DIAGNOSTIC_INDEX}")
        message(STATUS
            "${DINGO_MATRIX_DIAGNOSTIC_TARGET}: "
            "${DINGO_MATRIX_ISOLATED_SOURCE}")
        add_library(${DINGO_MATRIX_DIAGNOSTIC_TARGET} OBJECT
            "${DINGO_MATRIX_ISOLATED_SOURCE}")
        dingo_configure_matrix_compile_target(
            ${DINGO_MATRIX_DIAGNOSTIC_TARGET})
        target_sources(${DINGO_MATRIX_TEST_TARGET} PRIVATE
            $<TARGET_OBJECTS:${DINGO_MATRIX_DIAGNOSTIC_TARGET}>)
    endforeach()
endforeach()

option(DINGO_LIT_TESTS_ENABLED "Enable lit-based tests for static assertions and diagnostics." ON)
if(DINGO_LIT_TESTS_ENABLED)
    set(DINGO_LIT_EXEC_ROOT "${PROJECT_BINARY_DIR}/test/lit")

    add_test(NAME dingo_lit_tests
        COMMAND ${CMAKE_COMMAND} -E env
            CXX=${CMAKE_CXX_COMPILER}
            DINGO_CXX_STD=${CMAKE_CXX_STANDARD}
            ${DINGO_UV_RUN_COMMAND} lit -sv
                -D DINGO_LIT_EXEC_ROOT=${DINGO_LIT_EXEC_ROOT}
                ${CMAKE_CURRENT_SOURCE_DIR}/lit)
    set_tests_properties(dingo_lit_tests PROPERTIES LABELS "compile-only")
endif()
