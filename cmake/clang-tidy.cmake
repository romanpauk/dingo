include("${CMAKE_CURRENT_LIST_DIR}/tool.cmake")

set(DINGO_CLANG_TIDY_VERSION "21" CACHE STRING "Required clang-tidy major version")
find_tool(
    DINGO_CLANG_TIDY_EXE
    DINGO_CLANG_TIDY_VERSION_OUTPUT
    DINGO_CLANG_TIDY_ERROR
    NAME clang-tidy
    VERSION ${DINGO_CLANG_TIDY_VERSION}
    VERSION_MATCH "LLVM version ${DINGO_CLANG_TIDY_VERSION}\\."
)
get_property(DINGO_CLANG_TIDY_TARGETS GLOBAL PROPERTY DINGO_EXAMPLE_TARGETS)
get_property(DINGO_CLANG_TIDY_SOURCES GLOBAL PROPERTY DINGO_EXAMPLE_SOURCES)

if(DINGO_CLANG_TIDY_EXE AND DINGO_EXAMPLES_ENABLED)
    add_custom_target(check-tidy
        COMMAND ${CMAKE_COMMAND} -E echo
            "Using ${DINGO_CLANG_TIDY_VERSION_OUTPUT}"
        COMMAND ${DINGO_CLANG_TIDY_EXE}
            -p ${PROJECT_BINARY_DIR}
            --quiet
            ${DINGO_CLANG_TIDY_SOURCES}
        DEPENDS ${DINGO_CLANG_TIDY_TARGETS}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Running clang-tidy on example translation units"
        VERBATIM
    )
elseif(DINGO_CLANG_TIDY_EXE)
    add_custom_target(check-tidy
        COMMAND ${CMAKE_COMMAND} -E echo
            "check-tidy requires DINGO_EXAMPLES_ENABLED=ON so the compile "
            "database includes example translation units"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-tidy requires example translation units"
        VERBATIM
    )
else()
    add_custom_target(check-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "${DINGO_CLANG_TIDY_ERROR}"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-tidy is required for local static analysis"
        VERBATIM
    )
endif()
