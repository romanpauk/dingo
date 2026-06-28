set(DINGO_CLANG_TIDY_VERSION "21" CACHE STRING "Required clang-tidy major version")
find_program(DINGO_CLANG_TIDY_EXE
    NAMES clang-tidy-${DINGO_CLANG_TIDY_VERSION} clang-tidy
    DOC "clang-tidy executable matching DINGO_CLANG_TIDY_VERSION"
)

if(DINGO_CLANG_TIDY_EXE)
    execute_process(
        COMMAND ${DINGO_CLANG_TIDY_EXE} --version
        OUTPUT_VARIABLE DINGO_CLANG_TIDY_VERSION_OUTPUT
        ERROR_VARIABLE DINGO_CLANG_TIDY_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE DINGO_CLANG_TIDY_VERSION_RESULT
    )
    string(REPLACE "\n" " " DINGO_CLANG_TIDY_VERSION_OUTPUT
        "${DINGO_CLANG_TIDY_VERSION_OUTPUT}"
    )

    if(NOT DINGO_CLANG_TIDY_VERSION_RESULT EQUAL 0
            OR NOT DINGO_CLANG_TIDY_VERSION_OUTPUT MATCHES
                "LLVM version ${DINGO_CLANG_TIDY_VERSION}\\.")
        set(DINGO_CLANG_TIDY_ERROR
            "clang-tidy-${DINGO_CLANG_TIDY_VERSION} is required; "
            "found '${DINGO_CLANG_TIDY_VERSION_OUTPUT}' at "
            "'${DINGO_CLANG_TIDY_EXE}'"
        )
        set(DINGO_CLANG_TIDY_EXE "")
    endif()
else()
    set(DINGO_CLANG_TIDY_ERROR
        "clang-tidy-${DINGO_CLANG_TIDY_VERSION} is required"
    )
endif()

file(GLOB_RECURSE DINGO_CLANG_TIDY_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/examples/*.cpp"
)
get_property(DINGO_CLANG_TIDY_TARGETS GLOBAL PROPERTY DINGO_EXAMPLE_TARGETS)

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
