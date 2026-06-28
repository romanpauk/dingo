option(DINGO_INSTALL "Generate the install target." ${PROJECT_IS_TOP_LEVEL})
option(DINGO_DEVELOPMENT_MODE "Include additional targets for development" OFF)
option(DINGO_CCACHE "Use ccache" OFF)

if (DINGO_DEVELOPMENT_MODE)
    cmake_minimum_required(VERSION 3.25)
    if (DINGO_CCACHE)
        include(cmake/ccache.cmake)
        enable_ccache()
    endif()
else()
    cmake_minimum_required(VERSION 3.21)
endif()

project(dingo VERSION 0.1.2)

add_library(dingo INTERFACE)
add_library(dingo::dingo ALIAS dingo)
target_include_directories(dingo INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if(NOT CMAKE_SKIP_INSTALL_RULES AND DINGO_INSTALL)
    include(GNUInstallDirs)

    install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
    )

    install(
        EXPORT ${PROJECT_NAME}Targets
        NAMESPACE ${PROJECT_NAME}::
        FILE ${PROJECT_NAME}Config.cmake
        DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
    )

    install(
        DIRECTORY "${PROJECT_SOURCE_DIR}/include/dingo"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(DINGO_DEVELOPMENT_MODE)
    enable_testing()
    include(FetchContent)
    include(cmake/clang-format.cmake)
    if(WIN32)
        include(cmake/group-sources.cmake)
    endif()
    include(cmake/md.cmake)
    include(cmake/sanitizers.cmake)

    add_custom_target(check
        DEPENDS check-format md-verify
    )

    set(CMAKE_CXX_STANDARD 17 CACHE STRING "c++ standard")
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if (NOT CMAKE_CXX_STANDARD MATCHES "17|20|23|26")
        message(FATAL_ERROR "unsupported c++ standard version: ${CMAKE_CXX_STANDARD}")
    endif()

    option(DINGO_TESTING_ENABLED "enable testing through googletest" ON)
    option(DINGO_BENCHMARK_ENABLED "enable benchmarking through googlebenchmark" ON)
    option(DINGO_BENCHMARK_FRUIT_ENABLED "enable fetching of google::fruit for benchmarks" OFF)
    option(DINGO_EXAMPLES_ENABLED "enable building of dingo examples" ON)
    option(DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION
        "Run tests with a single representative dynamic container configuration." OFF)
    option(DINGO_TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." ON)

    enable_sanitizers(DINGO)

    # TODO: g++ -Wall -Wextra -Wpedantic -Q --help=warning | grep disabled
    set(DINGO_WARNINGS_GCC
        -Wall
        -Wpedantic
        -Wextra
        -Wshadow
        $<$<BOOL:${DINGO_TREAT_WARNINGS_AS_ERRORS}>:-Werror>
    )

    set(DINGO_WARNINGS_MSVC
        /W4
        /wd4714
        $<$<BOOL:${DINGO_TREAT_WARNINGS_AS_ERRORS}>:/WX>
    )

    set(DINGO_SOURCE_FILES
        core/config.h
        static/graph.h
        static/registry.h
        container.h
        core/auto_constructible.h
        core/binding_model.h
        core/binding_resolution.h
        static/local_resolution.h
        core/binding_resolution_policy.h
        core/binding_selection.h
        core/factory_traits.h
        core/keyed.h
        core/none.h
        static/activation_set.h
        static/container_traits.h
        core/exceptions.h
        factory/callable.h
        factory/constructor_traits.h
        factory/constructor_detection.h
        factory/constructor_typedef.h
        factory/constructor.h
        factory/function.h
        factory/invoke.h
        index/index.h
        index/array.h
        index/map.h
        index/unordered_map.h
        memory/aligned_storage.h
        memory/allocator.h
        memory/arena_allocator.h
        memory/object_lifetime.h
        memory/static_allocator.h
        registration/annotated.h
        registration/collection_traits.h
        registration/constructor.h
        registration/type_registration.h
        runtime_container.h
        runtime/container_traits.h
        runtime/registration_api.h
        runtime/registry.h
        static_container.h
        resolution/conversion_cache.h
        resolution/runtime_binding_interface.h
        resolution/runtime_binding.h
        storage/interface_storage_traits.h
        core/context_base.h
        runtime/context.h
        static/context.h
        resolution/type_cache.h
        rtti/static_provider.h
        rtti/rtti.h
        rtti/typeid_provider.h
        storage/external.h
        storage/materialized_source.h
        storage/resettable.h
        storage/shared_cyclical.h
        storage/shared.h
        storage/storage.h
        storage/type_storage_traits.h
        storage/unique.h
        type/normalized_type.h
        type/rebind_type.h
        type/type_conversion_traits.h
        type/type_descriptor.h
        type/type_list.h
        type/type_map.h
        type/type_name.h
        type/type_traits.h
        resolution/type_conversion.h
    )

    set(DINGO_INTERFACE_SOURCES "")
    foreach(FILE ${DINGO_SOURCE_FILES})
        list(
            APPEND DINGO_INTERFACE_SOURCES
                "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/dingo/${FILE}>"
                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dingo/${FILE}>"
        )
    endforeach()

    target_sources(dingo INTERFACE
        ${DINGO_INTERFACE_SOURCES}
    )

    if(MSVC)
        target_compile_options(dingo INTERFACE
            ${DINGO_WARNINGS_MSVC}
        )
    else()
        target_compile_options(dingo INTERFACE
            ${DINGO_WARNINGS_GCC}
        )
    endif()

    if(DINGO_TESTING_ENABLED)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0)

        if(MSVC)
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        endif()

        FetchContent_MakeAvailable(googletest)
    endif()

    if(DINGO_BENCHMARK_ENABLED)
        FetchContent_Declare(googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.8.3)

        set(BENCHMARK_ENABLE_TESTING off)
        set(BENCHMARK_ENABLE_WERROR OFF)
        FetchContent_MakeAvailable(googlebenchmark)

        if(DINGO_BENCHMARK_FRUIT_ENABLED)
            set(FRUIT_USES_BOOST False)
            FetchContent_Declare(googlefruit
                GIT_REPOSITORY https://github.com/google/fruit.git
                GIT_TAG v3.7.1)

            FetchContent_MakeAvailable(googlefruit)
        endif()
    endif()

    if(DINGO_TESTING_ENABLED)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        find_program(UV_EXECUTABLE NAMES uv REQUIRED)

        set(DINGO_MATRIX_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/matrix")
        set(DINGO_MATRIX_TEST_CMAKE
            "${CMAKE_CURRENT_BINARY_DIR}/generated/matrix-tests.cmake")
        set(DINGO_MATRIX_GENERATOR
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/generate.py")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/axis_config.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/axis_containers.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/axis_exposed_types.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/axis_features.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/axis_resolved_types.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/axis_stored_types.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/data.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/plugins.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/schema.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/templates/cmake.cmake.j2"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/templates/runner.cpp.j2"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/templates/source.cpp.j2"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/allocator_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/container_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/custom_rtti_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/factory_override.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/indexed_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/indexed_registration.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/local_bindings.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/nested_container.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/nested_wrappers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/runtime_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/runtime_regressions.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/scenario.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/static_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/storage_containers.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/values.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/matrix/support/variant_types.h"
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
            test/container/construct_dependency.cpp
            test/container/dingo.cpp
            test/container/incomplete_resolve.cpp
            test/container/incomplete_resolve_fixture.cpp
            test/factory/constructor_detection.cpp
            test/index/index.cpp
            test/registration/binding_graph.cpp
            test/registration/static_bindings_source.cpp
            test/registration/static_parent_container.cpp
            test/registration/type_registration.cpp
            test/storage/shared_type_conversion.cpp
            test/container/construct_common.h
            test/support/assert.h
            test/support/class.h
            test/support/containers.h
            test/support/test.h
            test/type/type_list.cpp
            test/type/type_map.cpp
            test/type/type_name.cpp
            test/type/type_traits.cpp
        )
        add_test(NAME dingo_unit_test COMMAND dingo_unit_test)
        set_tests_properties(dingo_unit_test PROPERTIES LABELS "unit")
        target_link_libraries(dingo_unit_test dingo::dingo gtest_main)
        target_include_directories(dingo_unit_test PRIVATE test)
        target_compile_definitions(dingo_unit_test PRIVATE
            $<$<BOOL:${DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION}>:
                DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION=1>
        )

        target_compile_options(dingo_unit_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

        # Force-include windows.h to each test to make sure library builds with windows.h included
        target_compile_options(dingo_unit_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)

        set(DINGO_MATRIX_TEST_SUPPORT_HEADERS
            test/matrix/features/construct_invoke.h
            test/matrix/features/resolve_concrete.h
            test/matrix/features/resolve_collection.h
            test/matrix/features/resolve_indexed.h
            test/matrix/features/resolve_interface.h
            test/matrix/features/resolve_keyed.h
            test/matrix/features/resolve_unique_wrapper.h
            test/matrix/support/allocator_containers.h
            test/matrix/support/container_containers.h
            test/matrix/support/custom_rtti_containers.h
            test/matrix/support/factory_override.h
            test/matrix/support/indexed_containers.h
            test/matrix/support/indexed_registration.h
            test/matrix/support/local_bindings.h
            test/matrix/support/nested_container.h
            test/matrix/support/nested_wrappers.h
            test/matrix/support/runtime_containers.h
            test/matrix/support/runtime_regressions.h
            test/matrix/support/scenario.h
            test/matrix/support/static_containers.h
            test/matrix/support/storage_containers.h
            test/matrix/support/values.h
            test/matrix/support/variant_types.h
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
                PRIVATE test)
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

            if(UV_EXECUTABLE)
                add_test(NAME dingo_lit_tests
                    COMMAND ${CMAKE_COMMAND} -E env
                        CXX=${CMAKE_CXX_COMPILER}
                        DINGO_CXX_STD=${CMAKE_CXX_STANDARD}
                        UV_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/.uv-cache
                        ${UV_EXECUTABLE} run --locked lit -sv ${CMAKE_CURRENT_SOURCE_DIR}/test/lit)
            elseif(LIT_EXECUTABLE)
                add_test(NAME dingo_lit_tests
                    COMMAND ${CMAKE_COMMAND} -E env
                        CXX=${CMAKE_CXX_COMPILER}
                        DINGO_CXX_STD=${CMAKE_CXX_STANDARD}
                        ${LIT_EXECUTABLE} -sv ${CMAKE_CURRENT_SOURCE_DIR}/test/lit)
            else()
                message(WARNING "Skipping lit tests: neither lit nor uv was found. "
                                "Enable by installing lit (or uv) and setting "
                                "DINGO_LIT_TESTS_ENABLED=ON.")
            endif()
            if(TEST dingo_lit_tests)
                set_tests_properties(dingo_lit_tests PROPERTIES LABELS "compile-only")
            endif()
        endif()
    endif()

    if(DINGO_BENCHMARK_ENABLED)
        add_executable(dingo_benchmark
            benchmark/container/basic.cpp
            benchmark/container/dingo.cpp
            benchmark/index/index.cpp
        )

        target_link_libraries(dingo_benchmark
            dingo::dingo
            benchmark::benchmark
        )
        target_include_directories(dingo_benchmark PRIVATE benchmark)
        target_link_options(dingo_benchmark PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:LINKER:--demangle>)

        if(DINGO_BENCHMARK_FRUIT_ENABLED)
            target_sources(dingo_benchmark PRIVATE benchmark/comparison/fruit.cpp)
            target_link_libraries(dingo_benchmark fruit)
        endif()
    endif()

    if(DINGO_EXAMPLES_ENABLED)
        add_subdirectory(examples)
    endif()

    if(WIN32)
        group_sources(include)
        group_sources(test)
        group_sources(benchmark)
    endif()
endif()
