project(dingo)
cmake_minimum_required(VERSION 3.19)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/EHa>")
add_link_options("$<$<CXX_COMPILER_ID:MSVC>:LINKER:/DEBUG:FASTLINK>")

add_library(dingo INTERFACE)
target_include_directories(dingo INTERFACE include)

target_sources(dingo INTERFACE
    include/dingo/annotated.h
    include/dingo/class_constructor.h
    include/dingo/class_factory.h
    include/dingo/class_instance.h
    include/dingo/class_instance_cache.h
    include/dingo/class_instance_destructor.h
    include/dingo/class_instance_factory.h   
    include/dingo/class_instance_factory_i.h
    include/dingo/class_instance_i.h
    include/dingo/class_instance_traits.h
    include/dingo/class_recursion_guard.h
    include/dingo/collection_traits.h
    include/dingo/constructible_i.h
    include/dingo/container.h
    include/dingo/decay.h
    include/dingo/exceptions.h
    include/dingo/memory/arena_allocator.h
    include/dingo/memory/virtual_pointer.h
    include/dingo/performance_counter.h
    include/dingo/rebind_type.h
    include/dingo/resettable_i.h
    include/dingo/resolving_context.h
    include/dingo/scope_guard.h
    include/dingo/storage.h
    include/dingo/storage/shared.h
    include/dingo/storage/shared_cyclical.h
    include/dingo/storage/shared_cyclical_protected.h
    include/dingo/storage/unique.h
    include/dingo/type_list.h
    include/dingo/type_traits.h
)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS unit_test_framework)
      
add_executable(dingotest 
    src/tests/annotated.cpp
    src/tests/assert.h
    src/tests/class.h
    src/tests/constructor.cpp
    src/tests/cyclical.cpp
    src/tests/dingo.cpp
    src/tests/multibindings.cpp
    src/tests/performance.cpp
    src/tests/shared.cpp
    src/tests/unique.cpp
    src/tests/testmodule.cpp
    src/tests/pch.h
)
add_test(dingotest COMMAND dingotest)
target_link_libraries(dingotest dingo Boost::boost Boost::unit_test_framework)
target_precompile_headers(dingotest PRIVATE src/tests/pch.h)
