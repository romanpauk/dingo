include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/uv.cmake)

function(configure_quality_targets out_check_targets)
  set(check_targets)

  uv_get_run_command(layers_run PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}")
  add_custom_target(layers-check
    COMMENT "Verifying header layering"
    COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/tools/layers.py
      ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
  list(APPEND check_targets layers-check)

  # The model probes: probe.py resolves every request of registrations in both
  # containers, detection.py detects and makes the classes generated around
  # them. Each writes the outcome of every cell to model-<size>/, with a
  # summary as .md, and fails on a defect.
  # They read the diagnostics of GCC and clang. A size probes a cover of the
  # dimensions: small every pair of their values, medium every three, large
  # every combination, which takes longest. model-check-small is
  # part of check with those compilers.
  #
  # A check that passed is not made again until what it reads changes: the
  # library, the specification and its generator, the headers of the probes,
  # the Python environment, or the compiler and its flags, which configure
  # writes to a file of its own only when they change.
  set(model_flags
    "-std=c++17 -O0 -I${PROJECT_SOURCE_DIR}/include -I${PROJECT_SOURCE_DIR}/test")
  file(GLOB_RECURSE model_inputs CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/*.h ${PROJECT_SOURCE_DIR}/include/*.hpp
    ${PROJECT_SOURCE_DIR}/spec/*.py ${PROJECT_SOURCE_DIR}/spec/*.j2
    ${PROJECT_SOURCE_DIR}/test/model/*.h)
  list(APPEND model_inputs ${PROJECT_SOURCE_DIR}/pyproject.toml
    ${PROJECT_SOURCE_DIR}/uv.lock)
  foreach(size_cover IN ITEMS small:pairs medium:triples large:all)
    string(REPLACE ":" ";" size_cover "${size_cover}")
    list(GET size_cover 0 size)
    list(GET size_cover 1 cover)
    set(model_dir ${PROJECT_BINARY_DIR}/model-${size})
    set(model_configuration ${model_dir}/configuration.txt)
    file(CONFIGURE OUTPUT ${model_configuration} CONTENT
      "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}\n${cover}\n${model_flags}\n")
    add_custom_command(
      OUTPUT ${model_dir}/checked
      COMMENT "Probing the ${size} cover of the model"
      COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/spec/probe.py
        ${model_dir}/probe.json --work ${model_dir}/probe
        --compiler ${CMAKE_CXX_COMPILER} --cover ${cover}
        "--flags=${model_flags}"
      COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/spec/detection.py
        ${model_dir}/detection.json --work ${model_dir}/detection
        --compiler ${CMAKE_CXX_COMPILER} --cover ${cover}
        "--flags=${model_flags}"
      COMMAND ${CMAKE_COMMAND} -E touch ${model_dir}/checked
      DEPENDS ${model_inputs} ${model_configuration}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      USES_TERMINAL
      VERBATIM
    )
    add_custom_target(model-check-${size} DEPENDS ${model_dir}/checked)
  endforeach()

  # The conversion check: conversion.py asks the conversion layer of every
  # registration what it publishes for every request, as the code compiles,
  # and compares it with the specification. It resolves nothing, so it
  # checks every registration; the model probes check what resolutions do.
  set(conversion_dir ${PROJECT_BINARY_DIR}/conversion)
  set(conversion_configuration ${conversion_dir}/configuration.txt)
  file(CONFIGURE OUTPUT ${conversion_configuration} CONTENT
    "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}\n${model_flags}\n")
  add_custom_command(
    OUTPUT ${conversion_dir}/checked
    COMMENT "Checking the conversion layer of every registration"
    COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/spec/conversion.py
      ${conversion_dir}/conversion.json --work ${conversion_dir}/work
      --compiler ${CMAKE_CXX_COMPILER} "--flags=${model_flags}"
    COMMAND ${CMAKE_COMMAND} -E touch ${conversion_dir}/checked
    DEPENDS ${model_inputs} ${conversion_configuration}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
  )
  add_custom_target(conversion-check DEPENDS ${conversion_dir}/checked)

  # The emptiness check: emptiness.py registers a stored object that holds
  # no leaf, a null handle, an empty optional or an alternative type that
  # holds the other alternative, and resolves every request the
  # specification says is defined when a layer is empty: each has to give
  # what holds no leaf, without failing.
  set(emptiness_dir ${PROJECT_BINARY_DIR}/emptiness)
  set(emptiness_configuration ${emptiness_dir}/configuration.txt)
  file(CONFIGURE OUTPUT ${emptiness_configuration} CONTENT
    "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}\n${model_flags}\n")
  add_custom_command(
    OUTPUT ${emptiness_dir}/checked
    COMMENT "Checking what an empty stored object gives"
    COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/spec/emptiness.py
      ${emptiness_dir}/emptiness.json --work ${emptiness_dir}/work
      --compiler ${CMAKE_CXX_COMPILER} "--flags=${model_flags}"
    COMMAND ${CMAKE_COMMAND} -E touch ${emptiness_dir}/checked
    DEPENDS ${model_inputs} ${emptiness_configuration}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
  )
  add_custom_target(emptiness-check DEPENDS ${emptiness_dir}/checked)

  # The layer checks: probe.py --layer probes what the storage of every
  # registration copies and moves of its leaf, requested as it is stored,
  # and what passing the leaf of a plain registration on does, in every
  # position. With the conversion check, which counts what the conversion of
  # every published request does, they check the copies and the moves of
  # each layer against the specification.
  foreach(layer IN ITEMS storage resolution)
    set(layer_dir ${PROJECT_BINARY_DIR}/${layer})
    set(layer_configuration ${layer_dir}/configuration.txt)
    file(CONFIGURE OUTPUT ${layer_configuration} CONTENT
      "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}\n${model_flags}\n")
    add_custom_command(
      OUTPUT ${layer_dir}/checked
      COMMENT "Checking the copies and the moves of the ${layer}"
      COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/spec/probe.py
        ${layer_dir}/probe.json --work ${layer_dir}/probe
        --compiler ${CMAKE_CXX_COMPILER} --layer ${layer}
        "--flags=${model_flags}"
      COMMAND ${CMAKE_COMMAND} -E touch ${layer_dir}/checked
      DEPENDS ${model_inputs} ${layer_configuration}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      USES_TERMINAL
      VERBATIM
    )
    add_custom_target(${layer}-check DEPENDS ${layer_dir}/checked)
  endforeach()
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND check_targets conversion-check emptiness-check storage-check
      resolution-check model-check-small)
  endif()

  # A report, not a check: it ranks the headers and never fails.
  add_custom_target(complexity-report
    COMMENT "Ranking header complexity"
    COMMAND ${layers_run} ${PROJECT_SOURCE_DIR}/tools/complexity.py
      ${PROJECT_SOURCE_DIR}/include --build-dir ${PROJECT_BINARY_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

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
