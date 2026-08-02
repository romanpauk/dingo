file(GLOB_RECURSE DINGO_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/dingo/*.h"
)
list(SORT DINGO_PUBLIC_HEADERS)

set(DINGO_HEADER_CHECK_DIRECTORY
    "${PROJECT_BINARY_DIR}/header-self-compile"
)
set(DINGO_HEADER_CHECK_SOURCES "")

foreach(HEADER IN LISTS DINGO_PUBLIC_HEADERS)
    string(REGEX REPLACE "\\.h$" ".cpp" HEADER_SOURCE "${HEADER}")
    set(HEADER_SOURCE "${DINGO_HEADER_CHECK_DIRECTORY}/${HEADER_SOURCE}")
    get_filename_component(HEADER_SOURCE_DIRECTORY "${HEADER_SOURCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${HEADER_SOURCE_DIRECTORY}")
    file(GENERATE
        OUTPUT "${HEADER_SOURCE}"
        CONTENT "#include <${HEADER}> // IWYU pragma: keep\n"
    )
    list(APPEND DINGO_HEADER_CHECK_SOURCES "${HEADER_SOURCE}")
endforeach()

add_library(dingo_header_self_compile OBJECT EXCLUDE_FROM_ALL
    ${DINGO_HEADER_CHECK_SOURCES}
)
target_link_libraries(dingo_header_self_compile PRIVATE dingo)

add_custom_target(check-headers
    DEPENDS dingo_header_self_compile
)
