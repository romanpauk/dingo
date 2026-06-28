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
