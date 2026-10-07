find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(DINGO_COMPILE_TIME_CLANG_MAJOR 22)
set(DINGO_COMPILE_TIME_OUTPUT_DIR
    "${PROJECT_BINARY_DIR}/compile-time")
set(DINGO_COMPILE_TIME_OBSERVED
    "${DINGO_COMPILE_TIME_OUTPUT_DIR}/observed.json")
set(DINGO_COMPILE_TIME_BUDGET
    "${PROJECT_SOURCE_DIR}/test/compile_time/budgets/clang-${DINGO_COMPILE_TIME_CLANG_MAJOR}.json")

find_program(DINGO_COMPILE_TIME_COMPILER
    NAMES clang++-${DINGO_COMPILE_TIME_CLANG_MAJOR} clang++
    HINTS /usr/bin /usr/local/bin
    NO_CACHE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "${DINGO_COMPILE_TIME_CLANG_MAJOR}.0" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS "23.0")
    add_custom_target(check-compile-time
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/compile_time_check_test.py"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/compile_time_check.py"
            check
            --compiler "${DINGO_COMPILE_TIME_COMPILER}"
            --include-dir "${PROJECT_SOURCE_DIR}/include"
            --output "${DINGO_COMPILE_TIME_OBSERVED}"
            --project-root "${PROJECT_SOURCE_DIR}"
            --work-dir "${DINGO_COMPILE_TIME_OUTPUT_DIR}/work"
            --budget "${DINGO_COMPILE_TIME_BUDGET}"
        BYPRODUCTS "${DINGO_COMPILE_TIME_OBSERVED}"
        COMMENT "Checking deterministic compile-time budgets"
        USES_TERMINAL
        VERBATIM
    )
else()
    add_custom_target(check-compile-time
        COMMAND "${CMAKE_COMMAND}" -E echo
            "check-compile-time requires Clang ${DINGO_COMPILE_TIME_CLANG_MAJOR}.x; configured compiler is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()
