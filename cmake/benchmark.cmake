include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/uv.cmake)

# The benchmarks: nbench, of the submodule tools/nbench, runs the suites of
# benchmark/nbench_conf.py on top of the locked uv environment. nbench runs
# on Linux; the submodule is needed only here.
function(configure_benchmark_targets)
  set(nbench_directory ${PROJECT_SOURCE_DIR}/tools/nbench)
  if(NOT EXISTS ${nbench_directory}/pyproject.toml)
    message(FATAL_ERROR
      "The benchmarks need the submodule tools/nbench: run "
      "`git submodule update --init tools/nbench`, or configure with "
      "-DDINGO_BENCHMARK_ENABLED=OFF")
  endif()
  set(DINGO_BENCHMARK_ARGUMENTS "-v;this=." CACHE STRING
    "The arguments of nbench run: the variants, the suites, the limits")
  uv_get_run_command(uv_run)
  add_custom_target(benchmark
    COMMENT "Running the benchmarks"
    COMMAND ${uv_run} --with-editable ${nbench_directory}
      nbench run -C ${PROJECT_SOURCE_DIR}/benchmark/nbench_conf.py
      ${DINGO_BENCHMARK_ARGUMENTS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
  )
endfunction()
