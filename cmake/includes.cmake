file(GLOB_RECURSE DINGO_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/dingo/*.h"
)
list(SORT DINGO_PUBLIC_HEADERS)

set(DINGO_INCLUDES_DIRECTORY
    "${PROJECT_BINARY_DIR}/includes"
)
set(DINGO_INCLUDES_SOURCES "")

foreach(HEADER IN LISTS DINGO_PUBLIC_HEADERS)
    string(REGEX REPLACE "\\.h$" ".cpp" INCLUDE_SOURCE "${HEADER}")
    set(INCLUDE_SOURCE "${DINGO_INCLUDES_DIRECTORY}/${INCLUDE_SOURCE}")
    get_filename_component(INCLUDE_SOURCE_DIRECTORY "${INCLUDE_SOURCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${INCLUDE_SOURCE_DIRECTORY}")
    file(GENERATE
        OUTPUT "${INCLUDE_SOURCE}"
        CONTENT "#include <${HEADER}> // IWYU pragma: keep\n"
    )
    list(APPEND DINGO_INCLUDES_SOURCES "${INCLUDE_SOURCE}")
endforeach()

add_library(dingo_includes_compile OBJECT EXCLUDE_FROM_ALL
    ${DINGO_INCLUDES_SOURCES}
)
target_link_libraries(dingo_includes_compile PRIVATE dingo)

add_custom_target(includes-compile
    DEPENDS dingo_includes_compile
)
