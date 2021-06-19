project(dingo)

cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
add_link_options("$<$<CXX_COMPILER_ID:MSVC>:LINKER:/DEBUG:FASTLINK>")
enable_testing()

add_library(dingo INTERFACE)
target_include_directories(dingo INTERFACE include)

target_sources(dingo INTERFACE
    include/dingo/ArenaAllocator.h
    include/dingo/Container.h
    include/dingo/Context.h
    include/dingo/Exceptions.h
    include/dingo/IConstructible.h
    include/dingo/IResettable.h
    include/dingo/ITypeInstanceFactory.h
    include/dingo/PerformanceCounter.h
    include/dingo/ScopeGuard.h
    include/dingo/Storage.h
    include/dingo/StorageShared.h
    include/dingo/StorageSharedCyclical.h
    include/dingo/StorageUnique.h
    include/dingo/Tuple.h
    include/dingo/Type.h
    include/dingo/TypeConstructor.h
    include/dingo/TypeDecay.h
    include/dingo/TypeInstance.h
    include/dingo/TypeInstanceFactory.h
    include/dingo/TypeTraits.h
    include/dingo/VirtualPointer.h
)

find_package(Boost)
if(Boost_FOUND)
    add_executable(dingotest 
        src/tests/dingo.cpp
        src/tests/pch.h
    )
    add_test(dingotest COMMAND dingotest)
    target_link_libraries(dingotest dingo Boost::boost)
    target_precompile_headers(dingotest PRIVATE src/tests/pch.h)
endif()