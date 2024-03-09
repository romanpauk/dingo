file(GLOB_RECURSE MARKDOWN_FILES *.md)
list(FILTER MARKDOWN_FILES EXCLUDE REGEX ${CMAKE_BINARY_DIR})

# TODO: need to figure out how to force cmake to find the same python version
# as is in github actions path.

#find_package (Python COMPONENTS Interpreter)
#if (NOT Python_Interpreter_FOUND)
#    message(FATAL_ERROR "python is required to process .md documentation")
#endif()

find_program(PYTHON "python3")
if (NOT PYTHON)
    message(FATAL_ERROR "python3 is required to process .md documentation")
endif()

add_custom_target(md-update
    COMMENT "Formatting .md files"
    COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/tools/md.py --mode=update ${MARKDOWN_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

add_custom_target(md-verify
    COMMENT "Verifying .md files are up-to-date"
    COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/tools/md.py --mode=verify ${MARKDOWN_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
