include_guard(GLOBAL)

include(CMakeParseArguments)

# Initializes one aggregate include-checking target.
function(clang_includes_init)
  cmake_parse_arguments(
    arg
    ""
    "CHECK_TARGET;EXECUTABLE"
    "ENVIRONMENT;OPTIONS"
    ${ARGN}
  )
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "clang_includes_init received unknown arguments: ${arg_UNPARSED_ARGUMENTS}"
    )
  endif()
  get_property(initialized GLOBAL PROPERTY CLANG_INCLUDES_MODULE_INITIALIZED)
  if(initialized)
    message(FATAL_ERROR "clang_includes_init may only be called once")
  endif()
  if(NOT arg_EXECUTABLE)
    message(FATAL_ERROR "clang_includes_init requires EXECUTABLE")
  endif()
  if(NOT arg_CHECK_TARGET)
    message(FATAL_ERROR "clang_includes_init requires CHECK_TARGET")
  endif()
  if(TARGET ${arg_CHECK_TARGET})
    message(FATAL_ERROR
      "clang-includes aggregate target already exists: ${arg_CHECK_TARGET}"
    )
  endif()

  if(arg_ENVIRONMENT)
    set(
      includes_command
      "${CMAKE_COMMAND}"
      -E
      env
      ${arg_ENVIRONMENT}
      "${arg_EXECUTABLE}"
    )
  else()
    set(includes_command "${arg_EXECUTABLE}")
  endif()
  list(APPEND includes_command ${arg_OPTIONS})

  add_custom_target(${arg_CHECK_TARGET})
  set_property(GLOBAL PROPERTY CLANG_INCLUDES_MODULE_INITIALIZED TRUE)
  set_property(GLOBAL PROPERTY CLANG_INCLUDES_MODULE_COMMAND "${includes_command}")
  set_property(GLOBAL PROPERTY CLANG_INCLUDES_MODULE_CHECK_TARGET "${arg_CHECK_TARGET}")
endfunction()

# Enables include checking for one compilation target and creates a scoped target.
function(clang_includes_enable)
  cmake_parse_arguments(arg "" "TARGET" "" ${ARGN})
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "clang_includes_enable received unknown arguments: ${arg_UNPARSED_ARGUMENTS}"
    )
  endif()
  get_property(initialized GLOBAL PROPERTY CLANG_INCLUDES_MODULE_INITIALIZED)
  if(NOT initialized)
    message(FATAL_ERROR "clang_includes_enable requires clang_includes_init")
  endif()
  if(NOT arg_TARGET OR NOT TARGET ${arg_TARGET})
    message(FATAL_ERROR "clang_includes_enable requires an existing TARGET")
  endif()
  get_target_property(aliased_target ${arg_TARGET} ALIASED_TARGET)
  if(aliased_target)
    message(FATAL_ERROR
      "clang_includes_enable cannot modify alias target: ${arg_TARGET}"
    )
  endif()
  get_target_property(imported_target ${arg_TARGET} IMPORTED)
  if(imported_target)
    message(FATAL_ERROR
      "clang_includes_enable cannot modify imported target: ${arg_TARGET}"
    )
  endif()
  get_target_property(target_type ${arg_TARGET} TYPE)
  if(NOT target_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
    message(FATAL_ERROR
      "clang_includes_enable requires a compilation target: ${arg_TARGET}"
    )
  endif()

  get_property(aggregate_check GLOBAL PROPERTY CLANG_INCLUDES_MODULE_CHECK_TARGET)
  set(check_target "${arg_TARGET}-${aggregate_check}")
  if(TARGET ${check_target})
    message(FATAL_ERROR
      "clang-includes target already exists for ${arg_TARGET}"
    )
  endif()

  get_property(includes_command GLOBAL PROPERTY CLANG_INCLUDES_MODULE_COMMAND)
  set_property(
    TARGET ${arg_TARGET}
    PROPERTY CXX_INCLUDE_WHAT_YOU_USE "${includes_command}"
  )
  add_custom_target(${check_target} DEPENDS ${arg_TARGET})
  add_dependencies(${aggregate_check} ${check_target})
endfunction()
