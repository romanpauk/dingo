file(GLOB_RECURSE DINGO_FORMAT_SOURCES
    ${PROJECT_SOURCE_DIR}/*.cpp
    ${PROJECT_SOURCE_DIR}/*.h
)
list(FILTER DINGO_FORMAT_SOURCES EXCLUDE REGEX "^${CMAKE_BINARY_DIR}")
list(FILTER DINGO_FORMAT_SOURCES EXCLUDE REGEX "^${PROJECT_SOURCE_DIR}/build[^/]*(/|$)")

set(DINGO_CLANG_FORMAT_VERSION "21" CACHE STRING "Required clang-format major version")
find_program(DINGO_CLANG_FORMAT_EXE
    NAMES clang-format-${DINGO_CLANG_FORMAT_VERSION} clang-format
    DOC "clang-format executable matching DINGO_CLANG_FORMAT_VERSION"
)

if(DINGO_CLANG_FORMAT_EXE)
    execute_process(
        COMMAND ${DINGO_CLANG_FORMAT_EXE} --version
        OUTPUT_VARIABLE DINGO_CLANG_FORMAT_VERSION_OUTPUT
        ERROR_VARIABLE DINGO_CLANG_FORMAT_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE DINGO_CLANG_FORMAT_VERSION_RESULT
    )

    if(NOT DINGO_CLANG_FORMAT_VERSION_RESULT EQUAL 0
            OR NOT DINGO_CLANG_FORMAT_VERSION_OUTPUT MATCHES
                "clang-format version ${DINGO_CLANG_FORMAT_VERSION}\\.")
        set(DINGO_CLANG_FORMAT_ERROR
            "clang-format-${DINGO_CLANG_FORMAT_VERSION} is required; "
            "found '${DINGO_CLANG_FORMAT_VERSION_OUTPUT}' at "
            "'${DINGO_CLANG_FORMAT_EXE}'"
        )
        set(DINGO_CLANG_FORMAT_EXE "")
    endif()
else()
    set(DINGO_CLANG_FORMAT_ERROR
        "clang-format-${DINGO_CLANG_FORMAT_VERSION} is required"
    )
endif()

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
