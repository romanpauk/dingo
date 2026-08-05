include_guard(GLOBAL)

include(CMakeParseArguments)

# Initializes one pair of aggregate targets using the caller-provided command.
function(clang_format_init)
  cmake_parse_arguments(
    arg
    ""
    "FORMAT_TARGET;CHECK_TARGET;STYLE_FILE;STAMP_DIRECTORY;EXECUTABLE"
    "DEPENDENCIES"
    ${ARGN}
  )
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "clang_format_init received unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  get_property(initialized GLOBAL PROPERTY CLANG_FORMAT_MODULE_INITIALIZED)
  if(initialized)
    message(FATAL_ERROR "clang_format_init may only be called once")
  endif()
  if(NOT arg_EXECUTABLE)
    message(FATAL_ERROR "clang_format_init requires EXECUTABLE")
  endif()

  if(NOT arg_FORMAT_TARGET OR NOT arg_CHECK_TARGET)
    message(FATAL_ERROR
      "clang_format_init requires FORMAT_TARGET and CHECK_TARGET"
    )
  endif()
  if(NOT arg_STAMP_DIRECTORY)
    set(arg_STAMP_DIRECTORY "${CMAKE_BINARY_DIR}/clang-format")
  endif()
  if(TARGET ${arg_FORMAT_TARGET} OR TARGET ${arg_CHECK_TARGET})
    message(FATAL_ERROR "clang-format aggregate target already exists")
  endif()

  set(style_file)
  if(arg_STYLE_FILE)
    get_filename_component(
      style_file
      "${arg_STYLE_FILE}"
      ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(NOT EXISTS "${style_file}")
      message(FATAL_ERROR "clang-format style file does not exist: ${style_file}")
    endif()
  endif()

  add_custom_target(${arg_FORMAT_TARGET})
  add_custom_target(${arg_CHECK_TARGET})

  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_INITIALIZED TRUE)
  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_EXECUTABLE "${arg_EXECUTABLE}")
  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_FORMAT_TARGET "${arg_FORMAT_TARGET}")
  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_CHECK_TARGET "${arg_CHECK_TARGET}")
  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_STYLE_FILE "${style_file}")
  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_STAMP_DIRECTORY "${arg_STAMP_DIRECTORY}")
  set_property(GLOBAL PROPERTY CLANG_FORMAT_MODULE_DEPENDENCIES "${arg_DEPENDENCIES}")
endfunction()

# Adds target-scoped format and check targets to the initialized aggregates.
# Explicit SOURCES replace target source discovery and are useful for headers
# which are intentionally not registered with target_sources().
function(clang_format_enable)
  cmake_parse_arguments(
    arg
    ""
    "TARGET"
    "SOURCES"
    ${ARGN}
  )
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "clang_format_enable received unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  get_property(initialized GLOBAL PROPERTY CLANG_FORMAT_MODULE_INITIALIZED)
  if(NOT initialized)
    message(FATAL_ERROR "clang_format_enable requires clang_format_init")
  endif()
  if(NOT arg_TARGET OR NOT TARGET ${arg_TARGET})
    message(FATAL_ERROR "clang_format_enable requires an existing TARGET")
  endif()
  get_target_property(aliased_target ${arg_TARGET} ALIASED_TARGET)
  if(aliased_target)
    message(FATAL_ERROR "clang_format_enable cannot modify alias target: ${arg_TARGET}")
  endif()
  get_target_property(imported_target ${arg_TARGET} IMPORTED)
  if(imported_target)
    message(FATAL_ERROR "clang_format_enable cannot modify imported target: ${arg_TARGET}")
  endif()

  get_property(aggregate_format GLOBAL PROPERTY CLANG_FORMAT_MODULE_FORMAT_TARGET)
  get_property(aggregate_check GLOBAL PROPERTY CLANG_FORMAT_MODULE_CHECK_TARGET)
  set(format_target "${arg_TARGET}-${aggregate_format}")
  set(check_target "${arg_TARGET}-${aggregate_check}")
  if(TARGET ${format_target} OR TARGET ${check_target})
    message(FATAL_ERROR "clang-format target already exists for ${arg_TARGET}")
  endif()
  get_target_property(base_directory ${arg_TARGET} SOURCE_DIR)

  set(format_sources ${arg_SOURCES})
  if(NOT format_sources)
    get_target_property(target_sources ${arg_TARGET} SOURCES)
    get_target_property(interface_sources ${arg_TARGET} INTERFACE_SOURCES)
    set(format_sources)
    if(target_sources AND NOT target_sources MATCHES "-NOTFOUND$")
      list(APPEND format_sources ${target_sources})
    endif()
    if(interface_sources AND NOT interface_sources MATCHES "-NOTFOUND$")
      list(APPEND format_sources ${interface_sources})
    endif()
  endif()
  set(absolute_sources)
  foreach(source IN LISTS format_sources)
    if(source MATCHES "^\\$<")
      continue()
    endif()
    get_filename_component(extension "${source}" LAST_EXT)
    string(TOLOWER "${extension}" extension)
    if(NOT extension MATCHES "^[.](c|cc|cpp|cppm|cxx|cxxm|h|hh|hpp|hxx|ixx)$")
      continue()
    endif()
    get_filename_component(
      absolute_source
      "${source}"
      ABSOLUTE
      BASE_DIR "${base_directory}"
    )
    if(NOT EXISTS "${absolute_source}")
      message(FATAL_ERROR "clang-format source does not exist: ${absolute_source}")
    endif()
    list(APPEND absolute_sources "${absolute_source}")
  endforeach()
  list(REMOVE_DUPLICATES absolute_sources)
  if(NOT absolute_sources)
    message(FATAL_ERROR "clang_format_enable found no C/C++ sources for ${arg_TARGET}")
  endif()

  get_property(format_executable GLOBAL PROPERTY CLANG_FORMAT_MODULE_EXECUTABLE)
  get_property(style_file GLOBAL PROPERTY CLANG_FORMAT_MODULE_STYLE_FILE)
  get_property(stamp_directory GLOBAL PROPERTY CLANG_FORMAT_MODULE_STAMP_DIRECTORY)
  get_property(module_dependencies GLOBAL PROPERTY CLANG_FORMAT_MODULE_DEPENDENCIES)

  set(format_stamps)
  set(check_stamps)
  foreach(source IN LISTS absolute_sources)
    string(SHA256 source_hash "${source}")
    string(SUBSTRING "${source_hash}" 0 20 source_key)
    set(format_stamp "${stamp_directory}/format/${source_key}.stamp")
    set(check_stamp "${stamp_directory}/check/${source_key}.stamp")
    get_property(registered GLOBAL PROPERTY "CLANG_FORMAT_SOURCE_${source_key}")
    if(NOT registered)
      set(dependencies "${source}")
      if(style_file)
        list(APPEND dependencies "${style_file}")
      endif()
      if(EXISTS "${format_executable}")
        list(APPEND dependencies "${format_executable}")
      endif()
      list(APPEND dependencies ${module_dependencies})
      add_custom_command(
        OUTPUT "${format_stamp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stamp_directory}/format"
        COMMAND "${format_executable}" -i "${source}"
        COMMAND ${CMAKE_COMMAND} -E touch "${format_stamp}"
        DEPENDS ${dependencies}
        COMMENT "Formatting ${source}"
        VERBATIM
      )
      add_custom_command(
        OUTPUT "${check_stamp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stamp_directory}/check"
        COMMAND "${format_executable}" --dry-run --Werror "${source}"
        COMMAND ${CMAKE_COMMAND} -E touch "${check_stamp}"
        DEPENDS ${dependencies}
        COMMENT "Checking ${source}"
        VERBATIM
      )
      set_property(GLOBAL PROPERTY "CLANG_FORMAT_SOURCE_${source_key}" TRUE)
    endif()
    list(APPEND format_stamps "${format_stamp}")
    list(APPEND check_stamps "${check_stamp}")
  endforeach()

  add_custom_target(${format_target} DEPENDS ${format_stamps})
  add_custom_target(${check_target} DEPENDS ${check_stamps})
  add_dependencies(${aggregate_format} ${format_target})
  add_dependencies(${aggregate_check} ${check_target})
endfunction()
