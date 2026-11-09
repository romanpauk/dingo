include("${PROJECT_SOURCE_DIR}/cmake/uv.cmake")
uv_get_run_command(DINGO_UV_RUN_COMMAND
    PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_executable(dingo_unit_test
    container/construct_dependency.cpp
    container/container_move.cpp
    container/container_parameters.cpp
    container/cycles.cpp
    container/dependency_chain.cpp
    container/incomplete_resolve.cpp
    container/incomplete_resolve_fixture.cpp
    container/parent/allocators.cpp
    container/parent/cache.cpp
    container/parent/collections.cpp
    container/parent/common.h
    container/parent/origin.cpp
    container/parent/ownership.cpp
    container/parent/recursion.cpp
    container/parent/resolution.cpp
    container/parent/search.cpp
    container/parent/semantics.cpp
    container/parent/transactions.cpp
    container/parent_container_resolution.cpp
    container/recursion.cpp
    factory/constructor_detection.cpp
    factory/constructor_detection_cases.cpp
    factory/constructor_detection_fixture.h
    factory/factory_kinds.cpp
    lookup/associative_backends.cpp
    lookup/collections.cpp
    lookup/index_injection.cpp
    lookup/index_registration.cpp
    lookup/index_runtime.cpp
    lookup/index_typed.cpp
    lookup/index_unkeyed.cpp
    lookup/indexed_registration.cpp
    lookup/lookup_index.cpp
    lookup/routes.cpp
    lookup/runtime_routing.cpp
    lookup/static_indexed.cpp
    memory/arena_allocator.cpp
    memory/object_store.cpp
    registration/binding_graph.cpp
    registration/object_sizes.cpp
    registration/custom_scope.cpp
    registration/registration_traits.cpp
    registration/replace_registration.cpp
    registration/runtime_registration.cpp
    registration/static_bindings_source.cpp
    registration/static_parent_container.cpp
    registration/static_parent_pairs.cpp
    registration/type_registration.cpp
    resolution/cache.cpp
    resolution/conversions.cpp
    resolution/invoke_callables.cpp
    resolution/request_identity.cpp
    resolution/wrappers.cpp
    runtime/common.h
    runtime/construct_dependency.cpp
    runtime/container.cpp
    runtime/exceptions.cpp
    runtime/memory.cpp
    runtime/shared_cyclical.cpp
    runtime/transaction.cpp
    runtime/unique_reference.cpp
    resolution/resolution_operation.cpp
    container/construct_common.h
    lookup/index_common.h
    registration/type_registration_common.h
    support/assert.h
    support/class.h
    support/containers.h
    support/test.h
    support/values.h
    type/type_identity.cpp
    type/type_identity_fixture.cpp
    type/type_identity_shared.h
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

# The owning handle families are one list for a program, which this test
# extends with handles of its own.
add_executable(dingo_owning_handles_test registration/owning_handles.cpp)
add_test(NAME dingo_owning_handles_test COMMAND dingo_owning_handles_test)
set_tests_properties(dingo_owning_handles_test PROPERTIES LABELS "unit")
target_link_libraries(dingo_owning_handles_test dingo::dingo gtest_main)
target_include_directories(dingo_owning_handles_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

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
