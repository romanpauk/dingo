include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/uv.cmake)

option(DINGO_GENERATE_ON_BUILD
  "Regenerate the generated headers before the library is used in a build" ON
)

# Adds the generate and generate-check targets and the generator unit tests.
function(configure_generate_targets out_check_targets)
  uv_get_run_command(uv_run PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set(generator_run
    ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
    ${uv_run} python ${PROJECT_SOURCE_DIR}/tools/generator/generate.py
  )

  add_custom_target(generate
    COMMENT "Generating files from the spec"
    COMMAND ${generator_run} generate
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
  )
  add_custom_target(generate-check
    COMMENT "Verifying generated files"
    COMMAND ${generator_run} check
      --work-dir ${PROJECT_BINARY_DIR}/generate-check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
  )

  if(DINGO_GENERATE_ON_BUILD)
    # Everything that links dingo is built after generate. Verifying the tree
    # in parallel with regenerating it would race, so generate-check follows
    # generate. Configure with -DDINGO_GENERATE_ON_BUILD=OFF to verify the
    # committed tree as it is, as CI does.
    add_dependencies(dingo generate)
    if(TARGET dingo_next)
      add_dependencies(dingo_next generate)
    endif()
    add_dependencies(generate-check generate)
  endif()

  add_test(NAME dingo_generator_test
    COMMAND ${uv_run} pytest -q -p no:cacheprovider
      ${PROJECT_SOURCE_DIR}/tools/generator
  )
  set_tests_properties(dingo_generator_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    ENVIRONMENT
      "PYTHONDONTWRITEBYTECODE=1;DINGO_GENERATOR_TEST_DIR=${PROJECT_BINARY_DIR}/generator-test"
  )

  set(${out_check_targets} generate-check PARENT_SCOPE)
endfunction()
