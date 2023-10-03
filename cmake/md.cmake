file(GLOB_RECURSE MARKDOWN_FILES *.md)
list(FILTER MARKDOWN_FILES EXCLUDE REGEX ${CMAKE_BINARY_DIR})

find_package (Python COMPONENTS Interpreter)
if (NOT Python_Interpreter_FOUND)
    message(FATAL_ERROR "python is required to process .md documentation")
endif()

add_custom_target(md-update
    COMMENT "Formatting .md files"
    COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/md.py --mode=update ${MARKDOWN_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

add_custom_target(md-verify
    COMMENT "Verifying .md files are up-to-date"
    COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/md.py --mode=verify ${MARKDOWN_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
