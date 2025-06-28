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

project(dingo)

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
        aligned_storage.h
        allocator.h
        annotated.h
        arena_allocator.h
        class_instance_factory_i.h
        class_instance_factory_traits.h
        class_instance_factory.h
        class_instance_resolver.h
        class_instance_temporary.h
        class_traits.h
        collection_traits.h
        config.h
        constructor.h
        container.h
        decay.h
        exceptions.h
        factory/callable.h
        factory/constructor_detection.h
        factory/constructor_typedef.h
        factory/constructor.h
        factory/function.h
        factory/invoke.h
        index.h
        index/array.h
        index/map.h
        index/unordered_map.h
        rebind_type.h
        resettable_i.h
        resolving_context.h
        rtti/static_provider.h
        rtti/rtti.h
        rtti/typeid_provider.h
        static_allocator.h
        storage.h
        storage/external.h
        storage/shared_cyclical.h
        storage/shared.h
        storage/unique.h
        type_cache.h
        type_conversion.h
        type_list.h
        type_map.h
        type_registration.h
        type_traits.h
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
            test/allocator.cpp
            test/annotated.cpp
            test/assert.h
            test/class.h
            test/class_factory.cpp
            test/construct.cpp
            test/constructor_detection.cpp
            test/containers.h
            test/dingo.cpp
            test/external.cpp
            test/index.cpp
            test/invoke.cpp
            test/multibindings.cpp
            test/nested_resolution.cpp
            test/nesting.cpp
            test/shared.cpp
            test/shared_cyclical.cpp
            test/test.h
            test/type_registration.cpp
            test/unique.cpp
        )
        add_test(NAME dingo_test COMMAND dingo_test)
        target_link_libraries(dingo_test dingo::dingo gtest_main)

        target_compile_options(dingo_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

        # Force-include windows.h to each test to make sure library builds with windows.h included
        target_compile_options(dingo_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)

        target_sanitize(DINGO dingo_test)
    endif()

    if(DINGO_BENCHMARK_ENABLED)
        add_executable(dingo_benchmark
            benchmark/basic.cpp
            benchmark/dingo.cpp
            benchmark/index.cpp
        )

        target_link_libraries(dingo_benchmark
            dingo::dingo
            benchmark::benchmark
        )
        target_include_directories(dingo_benchmark PRIVATE benchmark)
        target_link_options(dingo_benchmark PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:LINKER:--demangle>)

        if(DINGO_BENCHMARK_FRUIT_ENABLED)
            target_sources(dingo_benchmark PRIVATE benchmark/fruit.cpp)
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
