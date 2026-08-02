include("${CMAKE_CURRENT_LIST_DIR}/tool.cmake")

set(DINGO_IWYU_DOWNLOAD_DIRECTORY
    "${PROJECT_BINARY_DIR}/iwyu-toolchain"
)
set(DINGO_IWYU_ENVIRONMENT
    "CLANG_TOOL_CHAIN_DOWNLOAD_PATH=${DINGO_IWYU_DOWNLOAD_DIRECTORY}"
)

find_tool(
    DINGO_IWYU_EXE
    DINGO_IWYU_VERSION_OUTPUT
    DINGO_IWYU_ERROR
    NAME clang-tool-chain-iwyu
    ENVIRONMENT ${DINGO_IWYU_ENVIRONMENT}
)
if(DINGO_IWYU_EXE)
    string(REGEX MATCH "include-what-you-use[^\r\n]*"
        DINGO_IWYU_VERSION_OUTPUT
        "${DINGO_IWYU_VERSION_OUTPUT}"
    )
endif()

if(DINGO_IWYU_EXE)
    find_program(DINGO_IWYU_UV_EXE NAMES uv)
    execute_process(
        COMMAND ${DINGO_IWYU_UV_EXE} run --locked python -c
            "import clang_tidy, pathlib; root = pathlib.Path(clang_tidy.__file__).parent / 'data/lib/clang'; print(sorted(root.iterdir())[0] / 'include')"
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE DINGO_IWYU_CLANG_INCLUDE_DIRECTORY
        ERROR_VARIABLE DINGO_IWYU_RESOURCE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE DINGO_IWYU_RESOURCE_RESULT
    )
    if(NOT DINGO_IWYU_RESOURCE_RESULT EQUAL 0 OR
       NOT EXISTS "${DINGO_IWYU_CLANG_INCLUDE_DIRECTORY}")
        set(DINGO_IWYU_ERROR
            "IWYU requires the Clang resource headers from the locked clang-tidy package: ${DINGO_IWYU_RESOURCE_ERROR}"
        )
        set(DINGO_IWYU_EXE "")
    endif()
endif()

if(DINGO_IWYU_EXE)
    add_library(dingo_header_iwyu OBJECT EXCLUDE_FROM_ALL
        ${DINGO_HEADER_CHECK_SOURCES}
    )
    target_link_libraries(dingo_header_iwyu PRIVATE dingo)
    set_property(TARGET dingo_header_iwyu PROPERTY CXX_INCLUDE_WHAT_YOU_USE
        ${CMAKE_COMMAND}
        -E
        env
        ${DINGO_IWYU_ENVIRONMENT}
        ${DINGO_IWYU_EXE}
        -isystem
        ${DINGO_IWYU_CLANG_INCLUDE_DIRECTORY}
    )

    add_custom_target(check-iwyu
        COMMAND ${CMAKE_COMMAND} -E echo
            "Using ${DINGO_IWYU_VERSION_OUTPUT}"
        DEPENDS dingo_header_iwyu
        COMMENT "Auditing direct includes in public headers"
        VERBATIM
    )
else()
    add_custom_target(check-iwyu
        COMMAND ${CMAKE_COMMAND} -E echo "${DINGO_IWYU_ERROR}"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "IWYU is required to check public headers"
        VERBATIM
    )
endif()
