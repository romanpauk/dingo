include_guard(GLOBAL)

include(CMakeParseArguments)

# Returns a command prefix for running tools from a locked uv project.
function(uv_get_run_command out_command)
  cmake_parse_arguments(arg "" "PROJECT_DIRECTORY" "" ${ARGN})
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "uv_get_run_command received unknown arguments: ${arg_UNPARSED_ARGUMENTS}"
    )
  endif()
  if(NOT arg_PROJECT_DIRECTORY)
    set(arg_PROJECT_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endif()
  get_filename_component(project_directory "${arg_PROJECT_DIRECTORY}" ABSOLUTE)
  if(NOT EXISTS "${project_directory}/uv.lock")
    message(FATAL_ERROR "uv lock file does not exist: ${project_directory}/uv.lock")
  endif()

  find_program(UV_EXECUTABLE NAMES uv REQUIRED)
  set(UV_CACHE_DIRECTORY "${CMAKE_BINARY_DIR}/uv-cache" CACHE PATH
    "Cache directory for locked uv commands"
  )
  get_filename_component(cache_directory "${UV_CACHE_DIRECTORY}" ABSOLUTE
    BASE_DIR "${CMAKE_BINARY_DIR}"
  )
  set(UV_CACHE_DIRECTORY "${cache_directory}" CACHE PATH
    "Cache directory for locked uv commands" FORCE
  )
  mark_as_advanced(UV_EXECUTABLE UV_CACHE_DIRECTORY)

  set(${out_command}
    "${CMAKE_COMMAND}"
    -E
    env
    "UV_CACHE_DIR=${cache_directory}"
    "${UV_EXECUTABLE}"
    run
    --locked
    --project
    "${project_directory}"
    PARENT_SCOPE
  )
endfunction()

# Finds an executable installed by a locked uv project.
function(uv_find_tool out_executable)
  cmake_parse_arguments(arg "" "NAME;PROJECT_DIRECTORY" "" ${ARGN})
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "uv_find_tool received unknown arguments: ${arg_UNPARSED_ARGUMENTS}"
    )
  endif()
  if(NOT arg_NAME)
    message(FATAL_ERROR "uv_find_tool requires NAME")
  endif()

  uv_get_run_command(uv_run PROJECT_DIRECTORY "${arg_PROJECT_DIRECTORY}")
  execute_process(
    COMMAND ${uv_run} python -c
      "import shutil, sys; print(shutil.which(sys.argv[1]) or '')"
      "${arg_NAME}"
    OUTPUT_VARIABLE tool_executable
    ERROR_VARIABLE tool_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE tool_result
  )
  if(NOT tool_result EQUAL 0)
    message(FATAL_ERROR
      "${arg_NAME} lookup in the locked uv environment failed: ${tool_error}"
    )
  endif()
  if(NOT tool_executable)
    message(FATAL_ERROR
      "${arg_NAME} was not found in the locked uv environment"
    )
  endif()
  set(${out_executable} "${tool_executable}" PARENT_SCOPE)
endfunction()
