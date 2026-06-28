file(GLOB_RECURSE DINGO_FORMAT_SOURCES
    ${PROJECT_SOURCE_DIR}/*.cpp
    ${PROJECT_SOURCE_DIR}/*.h
)
list(FILTER DINGO_FORMAT_SOURCES EXCLUDE REGEX "^${CMAKE_BINARY_DIR}")
list(FILTER DINGO_FORMAT_SOURCES EXCLUDE REGEX "^${PROJECT_SOURCE_DIR}/build[^/]*(/|$)")
list(FILTER DINGO_FORMAT_SOURCES EXCLUDE REGEX "^${PROJECT_SOURCE_DIR}/\\.venv/")

include("${CMAKE_CURRENT_LIST_DIR}/tool.cmake")

find_tool(
    DINGO_CLANG_FORMAT_EXE
    DINGO_CLANG_FORMAT_VERSION_OUTPUT
    DINGO_CLANG_FORMAT_ERROR
    NAME clang-format
)

if(DINGO_CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo
            "Using ${DINGO_CLANG_FORMAT_VERSION_OUTPUT}"
        COMMAND ${DINGO_CLANG_FORMAT_EXE}
            -i
            --style=file:${PROJECT_SOURCE_DIR}/.clang-format
            ${DINGO_FORMAT_SOURCES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Formatting C++ sources"
        VERBATIM
    )

    add_custom_target(check-format
        COMMAND ${CMAKE_COMMAND} -E echo
            "Using ${DINGO_CLANG_FORMAT_VERSION_OUTPUT}"
        COMMAND ${DINGO_CLANG_FORMAT_EXE}
            --dry-run
            --Werror
            --style=file:${PROJECT_SOURCE_DIR}/.clang-format
            ${DINGO_FORMAT_SOURCES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Checking C++ source formatting"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "${DINGO_CLANG_FORMAT_ERROR}"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-format is required to format C++ sources"
        VERBATIM
    )

    add_custom_target(check-format
        COMMAND ${CMAKE_COMMAND} -E echo "${DINGO_CLANG_FORMAT_ERROR}"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-format is required to check C++ source formatting"
        VERBATIM
    )
endif()

add_custom_target(clang-format-update DEPENDS format)
add_custom_target(clang-format-verify DEPENDS check-format)
