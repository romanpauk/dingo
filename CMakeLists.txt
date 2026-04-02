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

    set(CMAKE_CXX_STANDARD 17 CACHE STRING "c++ standard")
    set(DINGO_REQUESTED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
    if(MSVC AND DINGO_REQUESTED_CXX_STANDARD STREQUAL "26")
        # CMake 3.31 still maps MSVC up to C++23, so request the latest draft mode
        # explicitly while keeping the project-level requested standard at C++26.
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if (NOT DINGO_REQUESTED_CXX_STANDARD MATCHES "17|20|23|26")
        message(FATAL_ERROR "unsupported c++ standard version: ${DINGO_REQUESTED_CXX_STANDARD}")
    endif()

    if(MSVC AND DINGO_REQUESTED_CXX_STANDARD STREQUAL "26")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>)
    endif()

    option(DINGO_TESTING_ENABLED "enable testing through googletest" ON)
    option(DINGO_BENCHMARK_ENABLED "enable benchmarking through googlebenchmark" ON)
    option(DINGO_BENCHMARK_FRUIT_ENABLED "enable fetching of google::fruit for benchmarks" OFF)
    option(DINGO_EXAMPLES_ENABLED "enable building of dingo examples" ON)
    option(DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION
        "Run tests with a single representative dynamic container configuration." OFF)
    option(DINGO_TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." ON)

    sanitizers_init(DINGO)

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
        $<$<BOOL:${DINGO_TREAT_WARNINGS_AS_ERRORS}>:/WX>
    )

    set(DINGO_SOURCE_FILES
        config.h
        container.h
        exceptions.h
        factory/callable.h
        factory/class_traits.h
        factory/constructor_detection.h
        factory/constructor_typedef.h
        factory/constructor.h
        factory/function.h
        factory/invoke.h
        index.h
        index/array.h
        index/map.h
        index/unordered_map.h
        memory/aligned_storage.h
        memory/allocator.h
        memory/arena_allocator.h
        memory/static_allocator.h
        registration/annotated.h
        registration/collection_traits.h
        registration/constructor.h
        registration/type_registration.h
        resolution/conversion_cache.h
        resolution/instance_factory_interface.h
        resolution/instance_factory.h
        resolution/interface_storage_traits.h
        resolution/resolving_context.h
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
        add_executable(dingo_test
            test/container/allocator.cpp
            test/container/construct.cpp
            test/container/dingo.cpp
            test/container/invoke.cpp
            test/container/nested_resolution.cpp
            test/container/nesting.cpp
            test/factory/class_factory.cpp
            test/factory/constructor_detection.cpp
            test/index/index.cpp
            test/index/multibindings.cpp
            test/registration/annotated.cpp
            test/registration/type_registration.cpp
            test/storage/array.cpp
            test/storage/external.cpp
            test/storage/shared.cpp
            test/storage/shared_cyclical_regressions.cpp
            test/storage/shared_cyclical.cpp
            test/storage/shared_type_conversion.cpp
            test/storage/unique.cpp
            test/support/assert.h
            test/support/class.h
            test/support/containers.h
            test/support/test.h
            test/type/type_list.cpp
            test/type/type_name.cpp
            test/type/type_traits.cpp
        )
        add_test(NAME dingo_test COMMAND dingo_test)
        target_link_libraries(dingo_test dingo::dingo gtest_main)
        target_include_directories(dingo_test PRIVATE test)
        target_compile_definitions(dingo_test PRIVATE
            $<$<BOOL:${DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION}>:
                DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION=1>
        )

        target_compile_options(dingo_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

        # Force-include windows.h to each test to make sure library builds with windows.h included
        target_compile_options(dingo_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)

        target_sanitize(DINGO dingo_test)
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
