include("${PROJECT_SOURCE_DIR}/cmake/uv.cmake")
uv_get_run_command(DINGO_UV_RUN_COMMAND
    PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

set(DINGO_MATRIX_PROFILE "full" CACHE STRING
    "Generated matrix execution profile (full or portable)")
set_property(CACHE DINGO_MATRIX_PROFILE PROPERTY STRINGS full portable)
if(NOT DINGO_MATRIX_PROFILE MATCHES "^(full|portable)$")
    message(FATAL_ERROR
        "DINGO_MATRIX_PROFILE must be either full or portable")
endif()
option(DINGO_MATRIX_TEST_AFTER_BUILD
    "Run each matrix test executable immediately after linking" OFF)

set(DINGO_MATRIX_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/matrix")
set(DINGO_MATRIX_TEST_CMAKE
    "${PROJECT_BINARY_DIR}/generated/matrix-tests.cmake")
set(DINGO_MATRIX_GENERATOR
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/generate.py")
set(DINGO_MATRIX_COMPILER_ARCHITECTURE "${CMAKE_GENERATOR_PLATFORM}")
if(NOT DINGO_MATRIX_COMPILER_ARCHITECTURE)
    set(DINGO_MATRIX_COMPILER_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(MSVC)
    set(DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT 12)

    # To identify any future crashing row, uncomment these lines after its
    # failing executable is known.
    # list(APPEND DINGO_MATRIX_DEPENDENCY_COMPOSITION_DIAGNOSTIC_ARGS
    #     --dependency-composition-isolate-executable invoke:4
    #     --dependency-composition-isolate-executable resolve:4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    # Bound peak compiler memory as the full composition matrix expands.
    set(DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT 64)
else()
    set(DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT 0)
endif()
file(GLOB DINGO_MATRIX_AXIS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/axes/*.py")

set(DINGO_MATRIX_TEST_HEADERS
    matrix/common/assertions.h
    matrix/containers/allocator.h
    matrix/containers/container.h
    matrix/containers/custom_rtti.h
    matrix/containers/indexed.h
    matrix/containers/runtime.h
    matrix/containers/static.h
    matrix/containers/storage.h
    matrix/fixtures/constructor_detection.h
    matrix/fixtures/dependency_shapes.h
    matrix/fixtures/factory.h
    matrix/fixtures/local_bindings.h
    matrix/fixtures/nested_wrappers.h
    matrix/fixtures/shared_cyclical.h
    matrix/fixtures/values.h
    matrix/fixtures/variant_types.h
    matrix/policies/constructor_detection.h
    matrix/policies/policy_aggregate.h
    matrix/policies/policy_collection.h
    matrix/policies/policy_core.h
    matrix/policies/policy_lifetime.h
    matrix/policies/policy_special.h
    matrix/policies/resolution.h
    matrix/scenarios/factory.h
    matrix/scenarios/indexed_registration.h
    matrix/scenarios/invoke.h
    matrix/scenarios/lookup/routes.h
    matrix/scenarios/lookup/runtime_routing.h
    matrix/scenarios/parent/allocators.h
    matrix/scenarios/parent/cache.h
    matrix/scenarios/parent/collections.h
    matrix/scenarios/parent/common.h
    matrix/scenarios/parent/native.h
    matrix/scenarios/parent/origin.h
    matrix/scenarios/parent/ownership.h
    matrix/scenarios/parent/recursion.h
    matrix/scenarios/parent/resolution.h
    matrix/scenarios/parent/search.h
    matrix/scenarios/parent/semantics.h
    matrix/scenarios/parent/transactions.h
    matrix/scenarios/runtime/common.h
    matrix/scenarios/runtime/construct_dependency.h
    matrix/scenarios/runtime/container.h
    matrix/scenarios/runtime/exceptions.h
    matrix/scenarios/runtime/shared_cyclical.h
    matrix/scenarios/runtime/unique_reference.h
    matrix/scenarios/static_indexed.h
    matrix/scenarios/static_parent.h
)
set(DINGO_MATRIX_TEST_HEADER_DEPENDS ${DINGO_MATRIX_TEST_HEADERS})
list(TRANSFORM DINGO_MATRIX_TEST_HEADER_DEPENDS
    PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${DINGO_MATRIX_AXIS_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/data.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/constructor_detection.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/dependency_contract.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/dependency_composition.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/exclusions.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/family.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/invoke.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/plugins.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/report.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/scenarios.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/shared_cyclical.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/schema.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/cmake.cmake.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/constructor_detection.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/constructor_detection_runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/dependency_composition.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/dependency_composition_runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/scenario.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/shared_cyclical.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/shared_cyclical_runner.cpp.j2"
    "${CMAKE_CURRENT_SOURCE_DIR}/matrix/templates/source.cpp.j2"
    ${DINGO_MATRIX_TEST_HEADER_DEPENDS}
    "${DINGO_MATRIX_GENERATOR}")

set(DINGO_MATRIX_GENERATOR_COMMAND
    ${DINGO_UV_RUN_COMMAND} python
        ${DINGO_MATRIX_GENERATOR}
        --out ${DINGO_MATRIX_TEST_DIR}
        --cmake ${DINGO_MATRIX_TEST_CMAKE}
        --profile ${DINGO_MATRIX_PROFILE}
        --compiler-id=${CMAKE_CXX_COMPILER_ID}
        --compiler-version=${CMAKE_CXX_COMPILER_VERSION}
        --compiler-architecture=${DINGO_MATRIX_COMPILER_ARCHITECTURE}
        --dependency-composition-case-limit
            ${DINGO_MATRIX_DEPENDENCY_COMPOSITION_CASE_LIMIT}
        ${DINGO_MATRIX_DEPENDENCY_COMPOSITION_DIAGNOSTIC_ARGS})

execute_process(
    COMMAND ${DINGO_MATRIX_GENERATOR_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_MATRIX_GENERATOR_RESULT)
if(NOT DINGO_MATRIX_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate matrix tests: "
        "${DINGO_MATRIX_GENERATOR_RESULT}")
endif()
include(${DINGO_MATRIX_TEST_CMAKE})

set(DINGO_MODEL_GENERATOR
    "tools.model.emit.families.storage")
set(DINGO_SELECTION_MODEL_GENERATOR
    "tools.model.emit.selection")
set(DINGO_ROUTE_MODEL_GENERATOR
    "tools.model.emit.families.route")
set(DINGO_BEHAVIOR_MODEL_GENERATOR
    "tools.model.emit.families.behavior")
set(DINGO_INTERFACE_MODEL_GENERATOR
    "tools.model.emit.families.interface")
set(DINGO_TYPE_MATRIX_GENERATOR
    "tools.model.emit.matrix")
set(DINGO_TRAIT_MATRIX_GENERATOR
    "tools.model.emit.traits")
set(DINGO_BINDING_MATRIX_GENERATOR
    "tools.model.emit.binding_matrix")
set(DINGO_TYPE_BEHAVIOR_GENERATOR
    "tools.model.emit.behavior")
set(DINGO_PROPERTY_MODEL_GENERATOR
    "tools.model.emit.families.property")
set(DINGO_ANNOTATED_MODEL_GENERATOR
    "tools.model.emit.families.annotated")
set(DINGO_CONST_MODEL_GENERATOR
    "tools.model.emit.families.const")
set(DINGO_KEYED_MODEL_GENERATOR
    "tools.model.emit.families.keyed")
set(DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR
    "tools.model.emit.families.interface_runtime")
set(DINGO_RESOLVE_GENERATOR
    "tools.model.emit.resolve")
set(DINGO_CONSUMER_MODEL_GENERATOR
    "tools.model.emit.consumers")
set(DINGO_FACTORY_MODEL_GENERATOR
    "tools.model.emit.families.factory")
set(DINGO_CONSTRUCT_MODEL_GENERATOR
    "tools.model.emit.families.construct")
set(DINGO_INVOKE_MODEL_GENERATOR
    "tools.model.emit.families.invoke")
set(DINGO_PRODUCER_MODEL_GENERATOR
    "tools.model.emit.families.producer")
set(DINGO_CONFIGURATION_MODEL_GENERATOR
    "tools.model.emit.families.configuration")
set(DINGO_MIXED_MODEL_GENERATOR
    "tools.model.emit.families.mixed")
set(DINGO_CYCLICAL_MODEL_GENERATOR
    "tools.model.emit.families.cyclical")
set(DINGO_LOOKUP_MODEL_GENERATOR
    "tools.model.emit.families.lookup")
set(DINGO_DEPENDENCIES_MODEL_GENERATOR
    "tools.model.emit.families.dependencies")
set(DINGO_COLLECTION_MODEL_GENERATOR
    "tools.model.emit.families.collection")
set(DINGO_INDEXED_MODEL_GENERATOR
    "tools.model.emit.families.indexed")
set(DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR
    "tools.model.emit.detection_constructor")
set(DINGO_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/storage_cells.cpp.j2")
set(DINGO_SCOPES_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/scopes.gen.h.j2")
set(DINGO_ROUTE_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/route_cells.cpp.j2")
set(DINGO_BEHAVIOR_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/behavior_cells.cpp.j2")
set(DINGO_INTERFACE_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/interface_cells.cpp.j2")
set(DINGO_TYPE_MATRIX_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/type_matrix.cpp.j2")
set(DINGO_TRAIT_MATRIX_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/trait_matrix.cpp.j2")
set(DINGO_BINDING_MATRIX_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/binding_matrix.cpp.j2")
set(DINGO_TYPE_BEHAVIOR_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/type_behavior.cpp.j2")
set(DINGO_PROPERTY_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/property_cells.cpp.j2")
set(DINGO_ANNOTATED_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/annotated_cells.cpp.j2")
set(DINGO_CONST_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/const_cells.cpp.j2")
set(DINGO_KEYED_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/keyed_cells.cpp.j2")
set(DINGO_CONSTRUCT_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/construct_shapes.cpp.j2")
set(DINGO_INVOKE_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/invoke_shapes.cpp.j2")
set(DINGO_PRODUCER_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/producer_cells.cpp.j2")
set(DINGO_CONFIGURATION_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/configuration_cells.cpp.j2")
set(DINGO_MIXED_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/mixed_cells.cpp.j2")
set(DINGO_CYCLICAL_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/cyclical_cells.cpp.j2")
set(DINGO_LOOKUP_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/lookup_cells.cpp.j2")
set(DINGO_DEPENDENCIES_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/dependency_cells.cpp.j2")
set(DINGO_CONSUMER_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/consumer_cells.cpp.j2")
set(DINGO_PROBE_FIXTURES_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/_probe_fixtures.h.j2")
set(DINGO_INTERFACE_RUNTIME_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/interface_runtime.cpp.j2")
set(DINGO_RESOLVE_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/resolve_cells.cpp.j2")
set(DINGO_CLASSIFICATION_CONSTRUCTOR_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/detection_constructor.cpp.j2")
set(DINGO_CLASSIFICATION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(DINGO_CLASSIFICATION_CONSTRUCTOR_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/classification-manifest.json")
set(DINGO_RESOLVE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(DINGO_RESOLVE_MANIFEST
    "${DINGO_RESOLVE_DIR}/resolve-manifest.json")
set(DINGO_SCOPES_HEADER
    "${PROJECT_SOURCE_DIR}/include/dingo/storage/scopes.gen.h")
set(DINGO_SELECTION_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/tools/model/emit/templates/selection_rules.cpp.j2")
set(DINGO_STORAGE_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/storage-manifest.json")
set(DINGO_SELECTION_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/selection-manifest.json")
set(DINGO_ROUTE_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/route-manifest.json")
set(DINGO_INTERFACE_MODEL_TEST_SOURCE
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/global/interface/interface_cells.cpp")
set(DINGO_TYPE_MATRIX_TEST_SOURCE
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/global/matrix/type_matrix.cpp")
set(DINGO_TRAIT_MATRIX_TEST_SOURCE
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/global/matrix/trait_matrix.cpp")
set(DINGO_REGISTRATION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(DINGO_BINDING_MATRIX_MANIFEST
    "${DINGO_REGISTRATION_DIR}/registration-manifest.json")
set(DINGO_PROPERTY_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/property-manifest.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/tools/model/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/__main__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/domain/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/domain/cases.py"
    "${PROJECT_SOURCE_DIR}/tools/model/domain/requests.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/behavior.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/common/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/common/recipe.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/common/runner.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/common/validation.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/annotated.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/behavior.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/const.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/interface.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/property.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/route.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/storage.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/matrix.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/resolve.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/detection_constructor.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/factory.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/construct.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/collection.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/families/indexed.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/selection.py"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/traits.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/bindings.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/catalog.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/cpp.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/enumerate.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/expr.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/shapes.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/storage.py"
    "${PROJECT_SOURCE_DIR}/tools/model/grammar/traits.py"
    "${PROJECT_SOURCE_DIR}/tools/model/legacy/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/legacy/binding.py"
    "${PROJECT_SOURCE_DIR}/tools/model/legacy/request.py"
    "${PROJECT_SOURCE_DIR}/tools/model/report/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/report/baseline.py"
    "${PROJECT_SOURCE_DIR}/tools/model/report/coverage.py"
    "${PROJECT_SOURCE_DIR}/tools/model/report/paths.py"
    "${PROJECT_SOURCE_DIR}/tools/model/report/resolves.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/capabilities.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/cells.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/extended_shapes.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/forms.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/gaps.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/routes/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/routes/derivation.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/routes/ledger.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/routes/rules.py"
    "${PROJECT_SOURCE_DIR}/tools/model/semantics/support.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/__init__.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/annotated_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/architecture_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/behavior_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/conftest.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/const_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/grammar_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/keyed_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/interface_runtime_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/binding_matrix_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/interface_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/outcome_baseline_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/paths_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/property_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/route_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/route_derivation_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/route_ledger_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/route_rules_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/selection_rules_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/storage_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/type_behavior_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/type_matrix_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/collection_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/indexed_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/construct_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/detection_constructor_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/factory_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/resolve_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/resolve_report_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/trait_matrix_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/wrapper_semantics_test.py"
    "${DINGO_MODEL_TEMPLATE}"
    "${DINGO_SCOPES_TEMPLATE}"
    "${DINGO_SELECTION_MODEL_TEMPLATE}"
    "${DINGO_ROUTE_MODEL_TEMPLATE}"
    "${DINGO_BEHAVIOR_MODEL_TEMPLATE}"
    "${DINGO_INTERFACE_MODEL_TEMPLATE}"
    "${DINGO_TYPE_MATRIX_TEMPLATE}"
    "${DINGO_TRAIT_MATRIX_TEMPLATE}"
    "${DINGO_RESOLVE_TEMPLATE}"
    "${DINGO_CLASSIFICATION_CONSTRUCTOR_TEMPLATE}"
    "${DINGO_TYPE_BEHAVIOR_TEMPLATE}"
    "${DINGO_PROPERTY_MODEL_TEMPLATE}"
    "${DINGO_ANNOTATED_MODEL_TEMPLATE}"
    "${DINGO_CONSTRUCT_MODEL_TEMPLATE}"
    "${DINGO_INVOKE_MODEL_TEMPLATE}"
    "${DINGO_PRODUCER_MODEL_TEMPLATE}"
    "${DINGO_CONFIGURATION_MODEL_TEMPLATE}"
    "${DINGO_MIXED_MODEL_TEMPLATE}"
    "${DINGO_CYCLICAL_MODEL_TEMPLATE}"
    "${DINGO_LOOKUP_MODEL_TEMPLATE}"
    "${DINGO_DEPENDENCIES_MODEL_TEMPLATE}"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/dependency_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/lookup_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/cyclical_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/mixed_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/configuration_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/producer_cells_test.py"
    "${DINGO_CONSUMER_MODEL_TEMPLATE}"
    "${DINGO_PROBE_FIXTURES_TEMPLATE}"
    "${PROJECT_SOURCE_DIR}/tools/model/emit/consumers.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/construct_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/consumer_cells_test.py"
    "${PROJECT_SOURCE_DIR}/tools/model/tests/claim_report_test.py"
    "${DINGO_CONST_MODEL_TEMPLATE}"
    "${DINGO_SCOPES_HEADER}"
    "${PROJECT_SOURCE_DIR}/tools/cost/measure_test.py")
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_STORAGE_MANIFEST}
        --header ${DINGO_SCOPES_HEADER}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_MODEL_GENERATOR_RESULT)
if(NOT DINGO_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate storage-cell coverage: "
        "${DINGO_MODEL_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_SELECTION_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_SELECTION_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_SELECTION_MODEL_GENERATOR_RESULT)
if(NOT DINGO_SELECTION_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate selection-rule coverage: "
        "${DINGO_SELECTION_MODEL_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_ROUTE_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_ROUTE_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_ROUTE_MODEL_GENERATOR_RESULT)
if(NOT DINGO_ROUTE_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate route coverage: "
        "${DINGO_ROUTE_MODEL_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_BEHAVIOR_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_BEHAVIOR_MODEL_GENERATOR_RESULT)
if(NOT DINGO_BEHAVIOR_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate behavior coverage: "
        "${DINGO_BEHAVIOR_MODEL_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_INTERFACE_MODEL_GENERATOR}
        --out ${DINGO_INTERFACE_MODEL_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_INTERFACE_MODEL_GENERATOR_RESULT)
if(NOT DINGO_INTERFACE_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate interface coverage: "
        "${DINGO_INTERFACE_MODEL_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TYPE_MATRIX_GENERATOR}
        --out ${DINGO_TYPE_MATRIX_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_TYPE_MATRIX_GENERATOR_RESULT)
if(NOT DINGO_TYPE_MATRIX_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate the type matrix: "
        "${DINGO_TYPE_MATRIX_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TRAIT_MATRIX_GENERATOR}
        --out ${DINGO_TRAIT_MATRIX_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_TRAIT_MATRIX_GENERATOR_RESULT)
if(NOT DINGO_TRAIT_MATRIX_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate the leaf trait matrix: "
        "${DINGO_TRAIT_MATRIX_GENERATOR_RESULT}")
endif()

file(MAKE_DIRECTORY ${DINGO_REGISTRATION_DIR})
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_BINDING_MATRIX_GENERATOR}
        --out-dir ${DINGO_REGISTRATION_DIR}
        --manifest ${DINGO_BINDING_MATRIX_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_BINDING_MATRIX_GENERATOR_RESULT)
if(NOT DINGO_BINDING_MATRIX_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate the binding matrix: "
        "${DINGO_BINDING_MATRIX_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_REGISTRATION_SOURCES CONFIGURE_DEPENDS
    "${DINGO_REGISTRATION_DIR}/registration/static_asserts/*/*/*.cpp")
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TYPE_BEHAVIOR_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_TYPE_BEHAVIOR_GENERATOR_RESULT)
if(NOT DINGO_TYPE_BEHAVIOR_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate type behavior coverage: "
        "${DINGO_TYPE_BEHAVIOR_GENERATOR_RESULT}")
endif()

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_PROPERTY_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_PROPERTY_MANIFEST}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_PROPERTY_MODEL_GENERATOR_RESULT)
if(NOT DINGO_PROPERTY_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate wrapper property coverage: "
        "${DINGO_PROPERTY_MODEL_GENERATOR_RESULT}")
endif()

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_FACTORY_MODEL_GENERATOR}
        --out-dir ${DINGO_REGISTRATION_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_FACTORY_MODEL_GENERATOR_RESULT)
if(NOT DINGO_FACTORY_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate factory registration probes: "
        "${DINGO_FACTORY_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_REGISTRATION_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${DINGO_REGISTRATION_DIR}/registration/runtime/*/*/factory/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CONSTRUCT_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_CONSTRUCT_MODEL_GENERATOR_RESULT)
if(NOT DINGO_CONSTRUCT_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate construction probes: "
        "${DINGO_CONSTRUCT_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_CONSTRUCT_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/construct/*/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_INVOKE_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_INVOKE_MODEL_GENERATOR_RESULT)
if(NOT DINGO_INVOKE_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate invoke shape probes: "
        "${DINGO_INVOKE_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_INVOKE_SHAPE_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/invoke/*/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_PRODUCER_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_PRODUCER_MODEL_GENERATOR_RESULT)
if(NOT DINGO_PRODUCER_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate producer probes: "
        "${DINGO_PRODUCER_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_PRODUCER_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/producer/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CONFIGURATION_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_CONFIGURATION_MODEL_GENERATOR_RESULT)
if(NOT DINGO_CONFIGURATION_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate configuration probes: "
        "${DINGO_CONFIGURATION_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_CONFIGURATION_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/allocator/global/*.cpp"
    "${DINGO_RESOLVE_DIR}/resolution/runtime/rtti/global/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_MIXED_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_MIXED_MODEL_GENERATOR_RESULT)
if(NOT DINGO_MIXED_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate mixed co-residence probes: "
        "${DINGO_MIXED_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_MIXED_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/mixed/global/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CYCLICAL_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_CYCLICAL_MODEL_GENERATOR_RESULT)
if(NOT DINGO_CYCLICAL_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate cyclical publication probes: "
        "${DINGO_CYCLICAL_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_CYCLICAL_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/cyclical.cpp"
    "${DINGO_RESOLVE_DIR}/resolution/mixed/global/cyclical.cpp")
file(GLOB_RECURSE DINGO_STATIC_CYCLICAL_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/static/global/cyclical.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_LOOKUP_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_LOOKUP_MODEL_GENERATOR_RESULT)
if(NOT DINGO_LOOKUP_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate lookup scope probes: "
        "${DINGO_LOOKUP_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_LOOKUP_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/lookup/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_DEPENDENCIES_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_DEPENDENCIES_MODEL_GENERATOR_RESULT)
if(NOT DINGO_DEPENDENCIES_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate declared dependency probes: "
        "${DINGO_DEPENDENCIES_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_DEPENDENCIES_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/dependencies/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_COLLECTION_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_COLLECTION_MODEL_GENERATOR_RESULT)
if(NOT DINGO_COLLECTION_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate collection probes: "
        "${DINGO_COLLECTION_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_COLLECTION_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/static/global/collection.cpp")
file(GLOB_RECURSE DINGO_MIXED_COLLECTION_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/mixed/global/collection.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_INDEXED_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_INDEXED_MODEL_GENERATOR_RESULT)
if(NOT DINGO_INDEXED_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate indexed lookup probes: "
        "${DINGO_INDEXED_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_INDEXED_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/indexed/global/*/resolve.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_ANNOTATED_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_ANNOTATED_MODEL_GENERATOR_RESULT)
if(NOT DINGO_ANNOTATED_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate annotated coverage: "
        "${DINGO_ANNOTATED_MODEL_GENERATOR_RESULT}")
endif()

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CONST_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_CONST_MODEL_GENERATOR_RESULT)
if(NOT DINGO_CONST_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate const coverage: "
        "${DINGO_CONST_MODEL_GENERATOR_RESULT}")
endif()

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_KEYED_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_KEYED_MODEL_GENERATOR_RESULT)
if(NOT DINGO_KEYED_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate keyed coverage: "
        "${DINGO_KEYED_MODEL_GENERATOR_RESULT}")
endif()

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR_RESULT)
if(NOT DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate interface runtime coverage: "
        "${DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR_RESULT}")
endif()

file(MAKE_DIRECTORY ${DINGO_RESOLVE_DIR})
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_RESOLVE_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_RESOLVE_GENERATOR_RESULT)
if(NOT DINGO_RESOLVE_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate per-storage resolve probes: "
        "${DINGO_RESOLVE_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_RESOLVE_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/resolve.cpp")
foreach(consumer construct invoke)
    execute_process(
        COMMAND ${DINGO_UV_RUN_COMMAND} python -m
            ${DINGO_CONSUMER_MODEL_GENERATOR}
            --out-dir ${DINGO_RESOLVE_DIR}
            --manifest ${DINGO_RESOLVE_MANIFEST}
            --consumer ${consumer}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        RESULT_VARIABLE DINGO_CONSUMER_MODEL_GENERATOR_RESULT)
    if(NOT DINGO_CONSUMER_MODEL_GENERATOR_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to generate ${consumer} claim files: "
            "${DINGO_CONSUMER_MODEL_GENERATOR_RESULT}")
    endif()
endforeach()
file(GLOB_RECURSE DINGO_CONSUMER_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/construct.cpp"
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/invoke.cpp")
foreach(claim_source ${DINGO_CONSUMER_SOURCES})
    file(RELATIVE_PATH claim_relative
        "${DINGO_RESOLVE_DIR}" "${claim_source}")
    string(REPLACE "/" "_" claim_name "${claim_relative}")
    string(REPLACE ".cpp" "" claim_name "${claim_name}")
    set_source_files_properties(${claim_source} PROPERTIES
        COMPILE_DEFINITIONS
        "DINGO_CLAIM_REPORT=\"${CMAKE_CURRENT_BINARY_DIR}/claim-${claim_name}.jsonl\"")
endforeach()
file(GLOB_RECURSE DINGO_RUNTIME_REQUEST_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/annotated/*.cpp"
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/keyed/*.cpp"
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/const/*.cpp"
    "${DINGO_RESOLVE_DIR}/resolution/runtime/*/*/interface/*.cpp")

file(MAKE_DIRECTORY ${DINGO_CLASSIFICATION_DIR})
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_CLASSIFICATION_CONSTRUCTOR_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR_RESULT)
if(NOT DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate constructor-detection probes: "
        "${DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_ROUTE_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/*/*/route.cpp")
file(GLOB_RECURSE DINGO_CLASSIFICATION_SOURCES CONFIGURE_DEPENDS
    "${DINGO_CLASSIFICATION_DIR}/classification/constructor/*/*/*.cpp"
    "${DINGO_CLASSIFICATION_DIR}/classification/*/*/storage.cpp"
    "${DINGO_CLASSIFICATION_DIR}/classification/global/wrapper/*.cpp"
    "${DINGO_CLASSIFICATION_DIR}/classification/global/selection/*.cpp")

add_custom_target(cells-check
    COMMENT "Verifying generated cell coverage"
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_STORAGE_MANIFEST}
        --header ${DINGO_SCOPES_HEADER}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_SELECTION_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_SELECTION_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_ROUTE_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_ROUTE_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_BEHAVIOR_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_INTERFACE_MODEL_GENERATOR}
        --out ${DINGO_INTERFACE_MODEL_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TYPE_MATRIX_GENERATOR}
        --out ${DINGO_TYPE_MATRIX_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TRAIT_MATRIX_GENERATOR}
        --out ${DINGO_TRAIT_MATRIX_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_BINDING_MATRIX_GENERATOR}
        --out-dir ${DINGO_REGISTRATION_DIR}
        --manifest ${DINGO_BINDING_MATRIX_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TYPE_BEHAVIOR_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_PROPERTY_MODEL_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_PROPERTY_MANIFEST}
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_ANNOTATED_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CONST_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_KEYED_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_RESOLVE_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CONSUMER_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --consumer construct
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CONSUMER_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --consumer invoke
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR}
        --out-dir ${DINGO_CLASSIFICATION_DIR}
        --manifest ${DINGO_CLASSIFICATION_CONSTRUCTOR_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

file(GLOB_RECURSE DINGO_TYPE_BEHAVIOR_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/*/*/*/type_behavior/*.cpp")
file(GLOB_RECURSE DINGO_BEHAVIOR_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/*/*/*/behavior/*.cpp")
add_executable(dingo_model_test
    ${DINGO_BEHAVIOR_SOURCES}
    ${DINGO_TYPE_BEHAVIOR_SOURCES}
)
target_link_libraries(dingo_model_test dingo::dingo gtest_main)
target_include_directories(dingo_model_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# The type matrix moved to the resolution/static-asserts phase.
set(DINGO_BEHAVIOR_REPORT_ARGS "")
foreach(behavior_source ${DINGO_BEHAVIOR_SOURCES})
    file(RELATIVE_PATH behavior_relative
        "${DINGO_RESOLVE_DIR}" "${behavior_source}")
    string(REPLACE "/" "_" behavior_name "${behavior_relative}")
    string(REPLACE ".cpp" "" behavior_name "${behavior_name}")
    set(behavior_report
        "${CMAKE_CURRENT_BINARY_DIR}/behavior-outcomes-${behavior_name}.jsonl")
    set_source_files_properties(${behavior_source} PROPERTIES
        COMPILE_DEFINITIONS
        "DINGO_BEHAVIOR_REPORT=\"${behavior_report}\"")
    list(APPEND DINGO_BEHAVIOR_REPORT_ARGS --behavior ${behavior_report})
endforeach()
add_test(NAME dingo_model_test COMMAND dingo_model_test)
set_tests_properties(dingo_model_test PROPERTIES LABELS "unit;generator")

add_executable(dingo_resolution_runtime_test
    ${DINGO_MIXED_COLLECTION_SOURCES}
    ${DINGO_RUNTIME_REQUEST_SOURCES}
    ${DINGO_CONSTRUCT_SOURCES}
    ${DINGO_INVOKE_SHAPE_SOURCES}
    ${DINGO_PRODUCER_SOURCES}
    ${DINGO_CONFIGURATION_SOURCES}
    ${DINGO_MIXED_SOURCES}
    ${DINGO_CYCLICAL_SOURCES}
    ${DINGO_LOOKUP_SOURCES}
    ${DINGO_DEPENDENCIES_SOURCES}
    ${DINGO_CONSUMER_SOURCES}
    ${DINGO_INDEXED_SOURCES})
target_link_libraries(dingo_resolution_runtime_test dingo::dingo gtest_main)
target_include_directories(dingo_resolution_runtime_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME dingo_resolution_runtime_test
    COMMAND dingo_resolution_runtime_test)
set_tests_properties(dingo_resolution_runtime_test PROPERTIES
    LABELS "unit;generator")

add_executable(dingo_resolution_static_test
    ${DINGO_COLLECTION_SOURCES}
    ${DINGO_STATIC_CYCLICAL_SOURCES})
target_link_libraries(dingo_resolution_static_test dingo::dingo gtest_main)
add_test(NAME dingo_resolution_static_test
    COMMAND dingo_resolution_static_test)
set_tests_properties(dingo_resolution_static_test PROPERTIES
    LABELS "unit;generator")

add_executable(dingo_registration_runtime_test
    ${DINGO_REGISTRATION_RUNTIME_SOURCES})
target_link_libraries(dingo_registration_runtime_test dingo::dingo gtest_main)
add_test(NAME dingo_registration_runtime_test
    COMMAND dingo_registration_runtime_test)
set_tests_properties(dingo_registration_runtime_test PROPERTIES
    LABELS "unit;generator")

add_executable(dingo_registration_test ${DINGO_REGISTRATION_SOURCES})
target_link_libraries(dingo_registration_test dingo::dingo gtest_main)
add_test(NAME dingo_registration_test COMMAND dingo_registration_test)
set_tests_properties(dingo_registration_test PROPERTIES
    LABELS "unit;generator")

add_executable(dingo_classification_test ${DINGO_CLASSIFICATION_SOURCES})
target_link_libraries(dingo_classification_test dingo::dingo gtest_main)
target_include_directories(dingo_classification_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME dingo_classification_test COMMAND dingo_classification_test)
set_tests_properties(dingo_classification_test PROPERTIES
    LABELS "unit;generator")

add_executable(dingo_resolution_static_asserts_test
    ${DINGO_INTERFACE_MODEL_TEST_SOURCE}
    ${DINGO_TYPE_MATRIX_TEST_SOURCE}
    ${DINGO_TRAIT_MATRIX_TEST_SOURCE}
    ${DINGO_ROUTE_SOURCES})
target_link_libraries(dingo_resolution_static_asserts_test dingo::dingo gtest_main)
target_include_directories(dingo_resolution_static_asserts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(dingo_resolution_static_asserts_test PRIVATE
    DINGO_TYPE_MATRIX_REPORT="${CMAKE_CURRENT_BINARY_DIR}/type-matrix.md"
    DINGO_TYPE_MATRIX_JSON="${CMAKE_CURRENT_BINARY_DIR}/type-matrix.json")
add_test(NAME dingo_resolution_static_asserts_test
    COMMAND dingo_resolution_static_asserts_test)
set_tests_properties(dingo_resolution_static_asserts_test PROPERTIES
    LABELS "unit;generator")

add_executable(dingo_resolve_probe EXCLUDE_FROM_ALL ${DINGO_RESOLVE_SOURCES})
target_link_libraries(dingo_resolve_probe PRIVATE dingo::dingo gtest_main)
target_include_directories(dingo_resolve_probe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(resolve_source ${DINGO_RESOLVE_SOURCES})
    file(RELATIVE_PATH resolve_relative
        "${DINGO_RESOLVE_DIR}" "${resolve_source}")
    string(REPLACE "/" "_" resolve_name "${resolve_relative}")
    string(REPLACE ".cpp" "" resolve_name "${resolve_name}")
    set_source_files_properties(${resolve_source} PROPERTIES
        COMPILE_DEFINITIONS
        "DINGO_RESOLVE_REPORT=\"${CMAKE_CURRENT_BINARY_DIR}/resolve-${resolve_name}.jsonl\"")
endforeach()
add_custom_target(claim-check
    COMMENT "Running the consumer claim probes and writing the claim report"
    COMMAND $<TARGET_FILE:dingo_resolve_probe>
    COMMAND $<TARGET_FILE:dingo_resolution_runtime_test>
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        tools.model.report.claims
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --outcomes-dir ${CMAKE_CURRENT_BINARY_DIR}
        --out ${CMAKE_CURRENT_BINARY_DIR}/claim-gaps.md
        --json ${CMAKE_CURRENT_BINARY_DIR}/claim-gaps.json
        --check-pins
        --check-runtime-pins
        --compiler ${CMAKE_CXX_COMPILER}
        --include ${PROJECT_SOURCE_DIR}/include
        --include ${CMAKE_CURRENT_SOURCE_DIR}
        --include ${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include
        --library ${CMAKE_BINARY_DIR}/lib/libgtest_main.a
        --library ${CMAKE_BINARY_DIR}/lib/libgtest.a
        --jobs 8
    DEPENDS dingo_resolve_probe dingo_resolution_runtime_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(resolve-check
    COMMENT "Running the per-storage resolve probes and writing the gap report"
    COMMAND $<TARGET_FILE:dingo_resolve_probe>
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        tools.model.report.resolves
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --outcomes-dir ${CMAKE_CURRENT_BINARY_DIR}
        --out ${CMAKE_CURRENT_BINARY_DIR}/resolve-gaps.md
        --json ${CMAKE_CURRENT_BINARY_DIR}/resolve-gaps.json
    DEPENDS dingo_resolve_probe
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_model_baseline_test
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        tools.model.report.baseline check
        --matrix ${CMAKE_CURRENT_BINARY_DIR}/type-matrix.json
        ${DINGO_BEHAVIOR_REPORT_ARGS}
        --baseline ${PROJECT_SOURCE_DIR}/test/baselines/model.json)
set_tests_properties(dingo_model_baseline_test PROPERTIES
    LABELS "unit;generator"
    DEPENDS "dingo_model_test;dingo_resolution_static_asserts_test"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_model_generator_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        ${PROJECT_SOURCE_DIR}/tools/model/tests)
set_tests_properties(dingo_model_generator_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_cost_tool_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        ${PROJECT_SOURCE_DIR}/tools/cost/measure_test.py
        ${PROJECT_SOURCE_DIR}/tools/cost/tu_report_test.py)
set_tests_properties(dingo_cost_tool_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tools/cost)

add_custom_target(test-cost
    COMMENT "Reporting the compile cost of the generated test translation units"
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/cost/tu_report.py
        --compile-commands ${PROJECT_BINARY_DIR}/compile_commands.json
        --filter ${PROJECT_BINARY_DIR}/test/generated
        --out ${PROJECT_BINARY_DIR}/cost/test-tus.json
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(cost-record
    COMMENT "Recording the current code-size and compile-cost probe"
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/cost/measure.py
        record
        --include ${PROJECT_SOURCE_DIR}/include
        --source ${PROJECT_SOURCE_DIR}/test/lit/codegen-static-probes.cpp
        --out ${PROJECT_BINARY_DIR}/cost/current.json
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(cost-record-runtime
    COMMENT "Recording the runtime-only code-size and compile-cost probe"
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/cost/measure.py
        record
        --include ${PROJECT_SOURCE_DIR}/include
        --source ${PROJECT_SOURCE_DIR}/tools/cost/probes/runtime.cpp
        --out ${PROJECT_BINARY_DIR}/cost/current-runtime.json
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_matrix_generator_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        ${CMAKE_CURRENT_SOURCE_DIR}/matrix/generator_test.py)
set_tests_properties(dingo_matrix_generator_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/matrix)

add_executable(dingo_unit_test
    container/construct_dependency.cpp
    container/container_move.cpp
    container/dependency_chain.cpp
    container/dingo.cpp
    container/incomplete_resolve.cpp
    container/incomplete_resolve_fixture.cpp
    container/parent_container_resolution.cpp
    container/recursion.cpp
    factory/constructor_detection.cpp
    lookup/associative_backends.cpp
    lookup/index_injection.cpp
    lookup/index_registration.cpp
    lookup/index_runtime.cpp
    lookup/index_typed.cpp
    lookup/index_unkeyed.cpp
    lookup/lookup_index.cpp
    memory/arena_allocator.cpp
    memory/object_store.cpp
    registration/binding_graph.cpp
    registration/object_routes.cpp
    registration/object_sizes.cpp
    registration/registration_traits.cpp
    registration/request_lift.cpp
    registration/runtime_registration.cpp
    registration/static_bindings_source.cpp
    registration/static_parent_container.cpp
    registration/type_registration.cpp
    resolution/cache.cpp
    runtime/storage.cpp
    runtime/transaction.cpp
    resolution/resolution_operation.cpp
    container/construct_common.h
    lookup/index_common.h
    registration/type_registration_common.h
    support/assert.h
    support/class.h
    support/containers.h
    support/test.h
    type/type_identity.cpp
    type/type_identity_fixture.cpp
    type/type_identity_shared.h
    type/type_list.cpp
    type/type_list_portable.cpp
    type/scope_info.cpp
    type/type_name.cpp
    type/type_traits.cpp
)
set_source_files_properties(
    lookup/index_injection.cpp
    lookup/index_registration.cpp
    lookup/index_runtime.cpp
    lookup/index_typed.cpp
    lookup/index_unkeyed.cpp
    PROPERTIES COMPILE_OPTIONS
    "$<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:-O0>")
add_test(NAME dingo_unit_test COMMAND dingo_unit_test)
set_tests_properties(dingo_unit_test PROPERTIES LABELS "unit")
target_link_libraries(dingo_unit_test dingo::dingo gtest_main)
target_include_directories(dingo_unit_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(dingo_unit_test PRIVATE
    $<$<BOOL:${DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION}>:
        DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION=1>
)

target_compile_options(dingo_unit_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

# Force-include windows.h to each test to make sure library builds with windows.h included
target_compile_options(dingo_unit_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)

add_custom_target(dingo_matrix_test
    DEPENDS ${DINGO_MATRIX_TEST_TARGETS})

function(dingo_configure_matrix_compile_target DINGO_MATRIX_TARGET)
    target_link_libraries(${DINGO_MATRIX_TARGET}
        PRIVATE dingo::dingo gtest_main)
    target_include_directories(${DINGO_MATRIX_TARGET}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${DINGO_MATRIX_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_compile_options(${DINGO_MATRIX_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)
endfunction()

foreach(DINGO_MATRIX_TEST_TARGET IN LISTS DINGO_MATRIX_TEST_TARGETS)
    add_executable(${DINGO_MATRIX_TEST_TARGET}
        ${${DINGO_MATRIX_TEST_TARGET}_SOURCES}
        ${DINGO_MATRIX_TEST_HEADERS}
    )
    add_test(NAME ${DINGO_MATRIX_TEST_TARGET}
        COMMAND ${DINGO_MATRIX_TEST_TARGET})
    if(DINGO_MATRIX_TEST_AFTER_BUILD)
        add_custom_command(TARGET ${DINGO_MATRIX_TEST_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env
                ASAN_OPTIONS=use_sigaltstack=false
                $<TARGET_FILE:${DINGO_MATRIX_TEST_TARGET}>
            COMMENT "Running ${DINGO_MATRIX_TEST_TARGET}"
            USES_TERMINAL
            VERBATIM)
    endif()
    set_tests_properties(${DINGO_MATRIX_TEST_TARGET}
        PROPERTIES LABELS "runtime")
    dingo_configure_matrix_compile_target(${DINGO_MATRIX_TEST_TARGET})

    set(DINGO_MATRIX_DIAGNOSTIC_INDEX 0)
    foreach(DINGO_MATRIX_ISOLATED_SOURCE IN LISTS
            ${DINGO_MATRIX_TEST_TARGET}_ISOLATED_SOURCES)
        math(EXPR DINGO_MATRIX_DIAGNOSTIC_INDEX
            "${DINGO_MATRIX_DIAGNOSTIC_INDEX} + 1")
        string(REPLACE "dingo_matrix_test_" "dingo_matrix_diag_"
            DINGO_MATRIX_DIAGNOSTIC_PREFIX ${DINGO_MATRIX_TEST_TARGET})
        set(DINGO_MATRIX_DIAGNOSTIC_TARGET
            "${DINGO_MATRIX_DIAGNOSTIC_PREFIX}_${DINGO_MATRIX_DIAGNOSTIC_INDEX}")
        message(STATUS
            "${DINGO_MATRIX_DIAGNOSTIC_TARGET}: "
            "${DINGO_MATRIX_ISOLATED_SOURCE}")
        add_library(${DINGO_MATRIX_DIAGNOSTIC_TARGET} OBJECT
            "${DINGO_MATRIX_ISOLATED_SOURCE}")
        dingo_configure_matrix_compile_target(
            ${DINGO_MATRIX_DIAGNOSTIC_TARGET})
        target_sources(${DINGO_MATRIX_TEST_TARGET} PRIVATE
            $<TARGET_OBJECTS:${DINGO_MATRIX_DIAGNOSTIC_TARGET}>)
    endforeach()
endforeach()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    # The unit test binary still carries some template-heavy coverage
    # from the old integration tests. Keep this runner's executable
    # linkable in GCC Release builds.
    target_compile_definitions(dingo_unit_test PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:
            DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION=1>
    )
    target_compile_options(dingo_unit_test PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Os>
    )
endif()

option(DINGO_LIT_TESTS_ENABLED "Enable lit-based tests for static assertions and diagnostics." ON)
if(DINGO_LIT_TESTS_ENABLED)
    set(DINGO_LIT_EXEC_ROOT "${PROJECT_BINARY_DIR}/test/lit")

    add_test(NAME dingo_lit_tests
        COMMAND ${CMAKE_COMMAND} -E env
            CXX=${CMAKE_CXX_COMPILER}
            DINGO_CXX_STD=${CMAKE_CXX_STANDARD}
            ${DINGO_UV_RUN_COMMAND} lit -sv
                -D DINGO_LIT_EXEC_ROOT=${DINGO_LIT_EXEC_ROOT}
                ${CMAKE_CURRENT_SOURCE_DIR}/lit)
    set_tests_properties(dingo_lit_tests PROPERTIES LABELS "compile-only")
endif()
