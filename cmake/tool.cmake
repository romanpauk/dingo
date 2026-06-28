function(find_tool out_executable out_version_output out_error)
    cmake_parse_arguments(
        TOOL
        ""
        "NAME;VERSION;VERSION_MATCH"
        "NAMES"
        ${ARGN}
    )

    if(NOT TOOL_NAME)
        message(FATAL_ERROR "find_tool requires NAME")
    endif()
    if(NOT TOOL_VERSION)
        message(FATAL_ERROR "find_tool requires VERSION")
    endif()
    if(NOT TOOL_VERSION_MATCH)
        message(FATAL_ERROR "find_tool requires VERSION_MATCH")
    endif()
    if(NOT TOOL_NAMES)
        set(TOOL_NAMES
            ${TOOL_NAME}-${TOOL_VERSION}
            ${TOOL_NAME}
        )
    endif()

    find_program(${out_executable}
        NAMES ${TOOL_NAMES}
        DOC "${TOOL_NAME} executable matching version ${TOOL_VERSION}"
    )
    set(TOOL_EXECUTABLE "${${out_executable}}")

    if(TOOL_EXECUTABLE)
        execute_process(
            COMMAND ${TOOL_EXECUTABLE} --version
            OUTPUT_VARIABLE TOOL_VERSION_OUTPUT
            ERROR_VARIABLE TOOL_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE TOOL_VERSION_RESULT
        )
        string(REPLACE "\n" " " TOOL_VERSION_OUTPUT
            "${TOOL_VERSION_OUTPUT}"
        )

        if(NOT TOOL_VERSION_RESULT EQUAL 0
                OR NOT TOOL_VERSION_OUTPUT MATCHES "${TOOL_VERSION_MATCH}")
            string(CONCAT TOOL_ERROR
                "${TOOL_NAME}-${TOOL_VERSION} is required; "
                "found '${TOOL_VERSION_OUTPUT}' at "
                "'${TOOL_EXECUTABLE}'"
            )
            set(TOOL_EXECUTABLE "")
            set(${out_executable} "${out_executable}-NOTFOUND" CACHE FILEPATH
                "${TOOL_NAME} executable matching version ${TOOL_VERSION}"
                FORCE)
        endif()
    else()
        string(CONCAT TOOL_ERROR
            "${TOOL_NAME}-${TOOL_VERSION} is required"
        )
    endif()

    set(${out_executable} "${TOOL_EXECUTABLE}" PARENT_SCOPE)
    set(${out_version_output} "${TOOL_VERSION_OUTPUT}" PARENT_SCOPE)
    set(${out_error} "${TOOL_ERROR}" PARENT_SCOPE)
endfunction()
