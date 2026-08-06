include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/uv.cmake)

function(configure_quality_targets out_check_targets)
  set(check_targets)

  if(NOT DINGO_CLANG_TOOLS_ENABLED)
    add_custom_target(format DEPENDS markdown-format)
    add_custom_target(format-check DEPENDS markdown-format-check)
    list(APPEND check_targets format-check)
    set(${out_check_targets} "${check_targets}" PARENT_SCOPE)
    return()
  endif()

  include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clang-format.cmake)
  if(NOT CLANG_FORMAT_EXECUTABLE)
    uv_find_tool(
      CLANG_FORMAT_EXECUTABLE
      NAME clang-format
      PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
  endif()

  file(GLOB_RECURSE DINGO_FORMAT_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/benchmark/*.cpp
    ${PROJECT_SOURCE_DIR}/benchmark/*.h
    ${PROJECT_SOURCE_DIR}/benchmark/*.hpp
    ${PROJECT_SOURCE_DIR}/examples/*.cpp
    ${PROJECT_SOURCE_DIR}/examples/*.h
    ${PROJECT_SOURCE_DIR}/examples/*.hpp
    ${PROJECT_SOURCE_DIR}/include/*.cpp
    ${PROJECT_SOURCE_DIR}/include/*.h
    ${PROJECT_SOURCE_DIR}/include/*.hpp
    ${PROJECT_SOURCE_DIR}/test/*.cpp
    ${PROJECT_SOURCE_DIR}/test/*.h
    ${PROJECT_SOURCE_DIR}/test/*.hpp
  )
  clang_format_init(
    FORMAT_TARGET sources-format
    CHECK_TARGET sources-format-check
    EXECUTABLE "${CLANG_FORMAT_EXECUTABLE}"
    STYLE_FILE ${PROJECT_SOURCE_DIR}/.clang-format
    DEPENDENCIES ${PROJECT_SOURCE_DIR}/uv.lock
  )
  clang_format_enable(TARGET dingo SOURCES ${DINGO_FORMAT_SOURCES})
  add_custom_target(format DEPENDS sources-format markdown-format)
  add_custom_target(format-check
    DEPENDS sources-format-check markdown-format-check
  )
  list(APPEND check_targets format-check)

  if(NOT WIN32)
    include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clang-tidy.cmake)
    if(NOT CLANG_TIDY_EXECUTABLE)
      uv_find_tool(
        CLANG_TIDY_EXECUTABLE
        NAME clang-tidy
        PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
      )
    endif()
    add_library(dingo_includes_tidy OBJECT EXCLUDE_FROM_ALL
      ${DINGO_INCLUDES_SOURCES}
    )
    target_link_libraries(dingo_includes_tidy PRIVATE dingo)
    clang_tidy_init(
      CHECK_TARGET includes-tidy
      EXECUTABLE "${CLANG_TIDY_EXECUTABLE}"
      QUIET
    )
    clang_tidy_enable(TARGET dingo_includes_tidy)

    include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clang-includes.cmake)
    set(includes_download_directory
      "${PROJECT_BINARY_DIR}/clang-includes-toolchain"
    )
    set(includes_environment
      "CLANG_TOOL_CHAIN_DOWNLOAD_PATH=${includes_download_directory}"
    )
    if(NOT CLANG_INCLUDES_EXECUTABLE)
      uv_find_tool(
        CLANG_INCLUDES_EXECUTABLE
        NAME clang-tool-chain-iwyu
        PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
      )
    endif()

    execute_process(
      COMMAND ${CMAKE_COMMAND} -E env
        ${includes_environment}
        "${CLANG_INCLUDES_EXECUTABLE}" --version
      OUTPUT_QUIET
      ERROR_VARIABLE includes_version_error
      ERROR_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE includes_version_result
    )
    if(NOT includes_version_result EQUAL 0)
      message(FATAL_ERROR
        "clang-includes initialization failed: ${includes_version_error}"
      )
    endif()

    uv_get_run_command(uv_run PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}")
    execute_process(
      COMMAND ${uv_run} python -c
        "import clang_tidy, pathlib; root = pathlib.Path(clang_tidy.__file__).parent / 'data/lib/clang'; print(sorted(root.iterdir())[0] / 'include')"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_VARIABLE includes_clang_include_directory
      ERROR_VARIABLE includes_resource_error
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE includes_resource_result
    )
    if(NOT includes_resource_result EQUAL 0 OR
       NOT EXISTS "${includes_clang_include_directory}")
      message(FATAL_ERROR
        "clang-includes requires the Clang resource headers from the locked clang-tidy package: ${includes_resource_error}"
      )
    endif()

    add_library(dingo_includes_check OBJECT EXCLUDE_FROM_ALL
      ${DINGO_INCLUDES_SOURCES}
    )
    target_link_libraries(dingo_includes_check PRIVATE dingo)
    clang_includes_init(
      CHECK_TARGET includes-check
      EXECUTABLE "${CLANG_INCLUDES_EXECUTABLE}"
      ENVIRONMENT ${includes_environment}
      OPTIONS -isystem ${includes_clang_include_directory}
    )
    clang_includes_enable(TARGET dingo_includes_check)
    list(APPEND check_targets includes-tidy includes-check)
  endif()

  set(${out_check_targets} "${check_targets}" PARENT_SCOPE)
endfunction()
