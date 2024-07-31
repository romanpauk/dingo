cmake_minimum_required(VERSION 3.19)
project(dingo_fetchcontent_test)

add_executable(dingo_fetchcontent_test test.cpp)

####
include(FetchContent)
FetchContent_Declare(dingo
                     GIT_REPOSITORY https://github.com/romanpauk/dingo.git
                     GIT_TAG        master)
FetchContent_MakeAvailable(dingo)
target_link_libraries(dingo_fetchcontent_test PRIVATE dingo::dingo)
####



