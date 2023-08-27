file(GLOB_RECURSE SOURCE_FILES *.cpp *.h)
list(FILTER SOURCE_FILES EXCLUDE REGEX ${CMAKE_BINARY_DIR})
add_custom_target(clang-format
    COMMAND clang-format -i --style=file:${PROJECT_SOURCE_DIR}/.clang-format --verbose ${SOURCE_FILES}
)

