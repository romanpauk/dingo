cmake_minimum_required(VERSION 3.19)
project(dingo)
enable_testing()
include(FetchContent)
include(cmake/clang-format.cmake)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) 
endif(CCACHE_FOUND)

option(DINGO_TESTING_ENABLED "enable testing through googletest" ON)
option(DINGO_BENCHMARK_ENABLED "enable benchmarking through googlebenchmark" ON)
option(DINGO_BENCHMARK_FRUIT_ENABLED "enable fetching of google::fruit for benchmarks" OFF)

if(DINGO_TESTING_ENABLED)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main)

    if(MSVC)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()

    FetchContent_MakeAvailable(googletest)
endif()

if(DINGO_BENCHMARK_ENABLED)
    FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG main) 
       
    set(BENCHMARK_ENABLE_TESTING off)
    FetchContent_MakeAvailable(googlebenchmark)

    if(DINGO_BENCHMARK_FRUIT_ENABLED)
        set(FRUIT_USES_BOOST False)
        FetchContent_Declare(googlefruit
            GIT_REPOSITORY https://github.com/google/fruit.git
            GIT_TAG master)

        FetchContent_MakeAvailable(googlefruit)
    endif()
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
    include/dingo/rebind_type.h
    include/dingo/resettable_i.h
    include/dingo/resolving_context.h
    include/dingo/rtti.h
    include/dingo/storage.h
    include/dingo/storage/external.h
    include/dingo/storage/shared.h
    include/dingo/storage/shared_cyclical.h
    include/dingo/storage/unique.h
    include/dingo/type_conversion.h
    include/dingo/type_list.h
    include/dingo/type_map.h
    include/dingo/type_traits.h
    README.md
)
    
if(DINGO_TESTING_ENABLED)
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

if(DINGO_BENCHMARK_ENABLED)
    add_executable(dingo_benchmark
        benchmark/dingo.cpp
    )

    target_link_libraries(dingo_benchmark 
        dingo
        benchmark::benchmark
    )
    target_include_directories(dingo_benchmark PRIVATE benchmark)
    target_link_options(dingo_benchmark PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:LINKER:--demangle>)

    if(DINGO_BENCHMARK_FRUIT_ENABLED)
        target_sources(dingo_benchmark PRIVATE benchmark/fruit.cpp)
        target_link_libraries(dingo_benchmark fruit)
    endif()
endif()
