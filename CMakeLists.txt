option(DINGO_INSTALL "Generate the install target." ${PROJECT_IS_TOP_LEVEL})
option(DINGO_DEVELOPMENT_MODE "Include additional targets for development" OFF)
option(DINGO_CCACHE "Use ccache" OFF)
option(DINGO_CLANG_TOOLS_ENABLED "Enable clang-format and clang-tidy targets" ON)

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
    if(DINGO_CLANG_TOOLS_ENABLED)
        include(cmake/clang-format.cmake)
    endif()
    if(WIN32)
        include(cmake/group-sources.cmake)
    endif()
    include(cmake/md.cmake)
    include(cmake/sanitizers.cmake)

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
        memory/aligned_storage.h
        memory/allocator.h
        memory/arena_allocator.h
        memory/object_lifetime.h
        registration/annotated.h
        registration/collection_traits.h
        registration/constructor.h
        registration/type_registration.h
        runtime_container.h
        runtime/container_traits.h
        runtime/registration_api.h
        runtime/lookup_index.h
        runtime/registry.h
        static_container.h
        lookup/array.h
        lookup/collection.h
        lookup/lookup.h
        lookup/ordered.h
        lookup/storage.h
        lookup/tags.h
        lookup/unordered.h
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
        add_subdirectory(test)
    endif()

    if(DINGO_BENCHMARK_ENABLED)
        add_subdirectory(benchmark)
    endif()

    if(DINGO_EXAMPLES_ENABLED)
        add_subdirectory(examples)
    endif()

    set(DINGO_CHECK_TARGETS md-verify)

    if(DINGO_CLANG_TOOLS_ENABLED)
        include(cmake/clang-tidy.cmake)
        list(APPEND DINGO_CHECK_TARGETS check-format)
        if(NOT WIN32)
            list(APPEND DINGO_CHECK_TARGETS check-tidy)
        endif()
    endif()

    add_custom_target(check
        DEPENDS ${DINGO_CHECK_TARGETS}
    )

    if(WIN32)
        group_sources(include)
        group_sources(test)
        group_sources(benchmark)
    endif()
endif()
