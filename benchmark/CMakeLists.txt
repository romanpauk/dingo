add_executable(dingo_benchmark
    container/basic.cpp
    container/dingo.cpp
    index/index.cpp
)

target_link_libraries(dingo_benchmark
    dingo::dingo
    benchmark::benchmark
)
target_include_directories(dingo_benchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_options(dingo_benchmark PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:LINKER:--demangle>)

if(DINGO_BENCHMARK_FRUIT_ENABLED)
    target_sources(dingo_benchmark PRIVATE comparison/fruit.cpp)
    target_link_libraries(dingo_benchmark fruit)
endif()

# The resolution benchmarks of the public API, built against the current and
# the next implementation, to compare their runtime cost.
add_executable(dingo_resolve_benchmark resolve/resolve.cpp)
target_link_libraries(dingo_resolve_benchmark dingo::dingo benchmark::benchmark)

if(TARGET dingo::next)
    add_executable(dingo_next_resolve_benchmark resolve/resolve.cpp)
    target_link_libraries(dingo_next_resolve_benchmark
        dingo::next benchmark::benchmark)
endif()
