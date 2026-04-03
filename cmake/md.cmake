set(MARKDOWN_FILES README.md)
file(GLOB_RECURSE DOC_MARKDOWN_FILES docs/*.md)
file(GLOB_RECURSE CONTAINER_MARKDOWN_FILES docker/*.md)
list(APPEND MARKDOWN_FILES ${DOC_MARKDOWN_FILES})
list(APPEND MARKDOWN_FILES ${CONTAINER_MARKDOWN_FILES})
list(SORT MARKDOWN_FILES)

find_program(UV_EXECUTABLE "uv")

if (NOT UV_EXECUTABLE)
    message(FATAL_ERROR "uv is required to process .md documentation")
endif()

set(MD_TOOL_COMMAND
    ${UV_EXECUTABLE}
    run
    --locked
    --project
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/tools/md.py
)

add_custom_target(md-update
    COMMENT "Formatting .md files"
    COMMAND ${MD_TOOL_COMMAND} --mode=update ${MARKDOWN_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

add_custom_target(md-verify
    COMMENT "Verifying .md files are up-to-date"
    COMMAND ${MD_TOOL_COMMAND} --mode=verify ${MARKDOWN_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
