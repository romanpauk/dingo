cmake_minimum_required(VERSION 3.19)
project(dingo)
enable_testing()
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) 
endif(CCACHE_FOUND)

option(DINGO_ENABLE_TESTING "enable testing through googletest" ON)
option(DINGO_ENABLE_BENCHMARK "enable benchmarking through googlebenchmark" ON)

if(DINGO_ENABLE_TESTING)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main)

    if(MSVC)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()

    FetchContent_MakeAvailable(googletest)
endif()

if(DINGO_ENABLE_BENCHMARK)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG main) 
       
    set(BENCHMARK_ENABLE_TESTING off)
    FetchContent_MakeAvailable(benchmark)
endif()

add_library(dingo INTERFACE)
target_include_directories(dingo INTERFACE include)

target_sources(dingo INTERFACE
    include/dingo/annotated.h
    include/dingo/class_constructor.h
    include/dingo/class_factory.h
    include/dingo/class_instance_factory.h
    include/dingo/class_instance_factory_i.h
    include/dingo/class_instance_resolver.h
    include/dingo/class_instance_traits.h
    include/dingo/collection_traits.h
    include/dingo/constructible_i.h
    include/dingo/container.h
    include/dingo/decay.h
    include/dingo/exceptions.h
    include/dingo/memory/arena_allocator.h
    include/dingo/rebind_type.h
    include/dingo/resettable_i.h
    include/dingo/resolving_context.h
    include/dingo/rtti.h
    include/dingo/storage.h
    include/dingo/storage/external.h
    include/dingo/storage/shared.h
    include/dingo/storage/shared_cyclical.h
    include/dingo/storage/unique.h
    include/dingo/type_list.h
    include/dingo/type_map.h
    include/dingo/type_traits.h
    README.md
)
    
if(DINGO_ENABLE_TESTING)
    add_executable(dingo_test 
        test/annotated.cpp
        test/assert.h
        test/class.h
        test/constructor.cpp
        test/containers.h
        test/cyclical.cpp
        test/dingo.cpp
        test/external.cpp
        test/multibindings.cpp
        test/shared.cpp
        test/unique.cpp
    )
    add_test(NAME dingo_test COMMAND dingo_test)
    target_link_libraries(dingo_test dingo gtest_main)
endif()

if(DINGO_ENABLE_BENCHMARK)
    add_executable(dingo_benchmark
        benchmark/main.cpp
    )

    target_link_libraries(dingo_benchmark 
        dingo
        benchmark::benchmark
    )
    target_include_directories(dingo_benchmark PRIVATE benchmark)
endif()

