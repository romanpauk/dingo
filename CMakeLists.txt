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
    include/dingo/ArenaAllocator.h
    include/dingo/class_factory.h
    include/dingo/collection_traits.h
    include/dingo/constructor.h
    include/dingo/container.h
    include/dingo/decay.h
    include/dingo/Exceptions.h
    include/dingo/IConstructible.h
    include/dingo/IResettable.h
    include/dingo/ITypeInstanceFactory.h
    include/dingo/PerformanceCounter.h
    include/dingo/resolving_context.h
    include/dingo/ScopeGuard.h
    include/dingo/Storage.h
    include/dingo/StorageShared.h
    include/dingo/StorageSharedCyclical.h
    include/dingo/StorageUnique.h
    include/dingo/TypeInstance.h
    include/dingo/TypeInstanceFactory.h
    include/dingo/type_list.h
    include/dingo/VirtualPointer.h
)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS unit_test_framework)
      
add_executable(dingotest 
    src/tests/assert.h
    src/tests/class.h
    src/tests/constructor.cpp
    src/tests/dingo.cpp
    src/tests/testmodule.cpp
    src/tests/pch.h
)
add_test(dingotest COMMAND dingotest)
target_link_libraries(dingotest dingo Boost::boost Boost::unit_test_framework)
target_precompile_headers(dingotest PRIVATE src/tests/pch.h)
