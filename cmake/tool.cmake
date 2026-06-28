function(find_tool out_executable out_version_output out_error)
    cmake_parse_arguments(
        TOOL
        ""
        "NAME"
        ""
        ${ARGN}
    )

    if(NOT TOOL_NAME)
        message(FATAL_ERROR "find_tool requires NAME")
    endif()

    find_program(TOOL_UV_EXE NAMES uv)
    if(NOT TOOL_UV_EXE)
        string(CONCAT TOOL_ERROR
            "${TOOL_NAME} is required; uv was not found"
        )
    elseif(NOT EXISTS "${PROJECT_SOURCE_DIR}/uv.lock")
        string(CONCAT TOOL_ERROR
            "${TOOL_NAME} is required; "
            "${PROJECT_SOURCE_DIR}/uv.lock was not found"
        )
    else()
        execute_process(
            COMMAND ${TOOL_UV_EXE} run --locked python -c
                "import shutil; print(shutil.which('${TOOL_NAME}') or '')"
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE TOOL_UV_OUTPUT
            ERROR_VARIABLE TOOL_UV_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE TOOL_UV_RESULT
        )
        if(TOOL_UV_RESULT EQUAL 0 AND TOOL_UV_OUTPUT)
            set(TOOL_EXECUTABLE "${TOOL_UV_OUTPUT}")
            set(${out_executable} "${TOOL_EXECUTABLE}" CACHE FILEPATH
                "${TOOL_NAME} executable from uv.lock"
                FORCE)
        else()
            string(CONCAT TOOL_ERROR
                "${TOOL_NAME} is required; "
                "uv locked environment did not provide ${TOOL_NAME}"
            )
        endif()
    endif()

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

        if(NOT TOOL_VERSION_RESULT EQUAL 0)
            string(CONCAT TOOL_ERROR
                "${TOOL_NAME} is required; "
                "'${TOOL_NAME} --version' failed at "
                "'${TOOL_EXECUTABLE}'"
            )
            set(TOOL_EXECUTABLE "")
            set(${out_executable} "${out_executable}-NOTFOUND" CACHE FILEPATH
                "${TOOL_NAME} executable from uv.lock"
                FORCE)
        endif()
    endif()

    set(${out_executable} "${TOOL_EXECUTABLE}" PARENT_SCOPE)
    set(${out_version_output} "${TOOL_VERSION_OUTPUT}" PARENT_SCOPE)
    set(${out_error} "${TOOL_ERROR}" PARENT_SCOPE)
endfunction()
