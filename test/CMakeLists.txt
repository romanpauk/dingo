include("${PROJECT_SOURCE_DIR}/cmake/uv.cmake")
uv_get_run_command(DINGO_UV_RUN_COMMAND
    PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)


set(DINGO_MODEL_GENERATOR
    "spec.gen.classification.storage")
set(DINGO_SELECTION_MODEL_GENERATOR
    "spec.gen.classification.selection")
set(DINGO_ROUTE_MODEL_GENERATOR
    "spec.gen.resolution.route")
set(DINGO_BEHAVIOR_MODEL_GENERATOR
    "spec.gen.resolution.behavior")
set(DINGO_INTERFACE_MODEL_GENERATOR
    "spec.gen.resolution.interface")
set(DINGO_TYPE_TABLE_GENERATOR
    "spec.gen.resolution.type_table")
set(DINGO_TRAIT_TABLE_GENERATOR
    "spec.gen.resolution.trait_table")
set(DINGO_BINDING_TABLE_GENERATOR
    "spec.gen.registration.binding")
set(DINGO_TYPE_BEHAVIOR_GENERATOR
    "spec.gen.resolution.type_behavior")
set(DINGO_PROPERTY_MODEL_GENERATOR
    "spec.gen.classification.wrapper")
set(DINGO_ANNOTATED_MODEL_GENERATOR
    "spec.gen.resolution.annotated")
set(DINGO_CONST_MODEL_GENERATOR
    "spec.gen.resolution.const")
set(DINGO_KEYED_MODEL_GENERATOR
    "spec.gen.resolution.keyed")
set(DINGO_INTERFACE_RUNTIME_MODEL_GENERATOR
    "spec.gen.resolution.interface_runtime")
set(DINGO_RESOLVE_GENERATOR
    "spec.gen.resolution.resolve")
set(DINGO_CONSUMER_MODEL_GENERATOR
    "spec.gen.resolution.consumers")
set(DINGO_FACTORY_MODEL_GENERATOR
    "spec.gen.registration.factory")
set(DINGO_CONSTRUCT_MODEL_GENERATOR
    "spec.gen.resolution.construct")
set(DINGO_INVOKE_MODEL_GENERATOR
    "spec.gen.resolution.invoke")
set(DINGO_PRODUCER_MODEL_GENERATOR
    "spec.gen.resolution.producer")
set(DINGO_CONFIGURATION_MODEL_GENERATOR
    "spec.gen.resolution.configuration")
set(DINGO_MIXED_MODEL_GENERATOR
    "spec.gen.resolution.mixed")
set(DINGO_CYCLICAL_MODEL_GENERATOR
    "spec.gen.resolution.cyclical")
set(DINGO_LOOKUP_MODEL_GENERATOR
    "spec.gen.resolution.lookup")
set(DINGO_DEPENDENCIES_MODEL_GENERATOR
    "spec.gen.resolution.dependencies")
set(DINGO_CONVERSION_MODEL_GENERATOR
    "spec.gen.resolution.conversion")
set(DINGO_WRAPPER_MODEL_GENERATOR
    "spec.gen.resolution.wrapper")
set(DINGO_COLLECTION_MODEL_GENERATOR
    "spec.gen.resolution.collection")
set(DINGO_INDEXED_MODEL_GENERATOR
    "spec.gen.resolution.indexed")
set(DINGO_CLASSIFICATION_CONSTRUCTOR_GENERATOR
    "spec.gen.classification.constructor")
set(DINGO_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/storage_facts.cpp.j2")
set(DINGO_SCOPES_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/scopes.gen.h.j2")
set(DINGO_ROUTE_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/route_probes.cpp.j2")
set(DINGO_BEHAVIOR_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/behavior_cases.cpp.j2")
set(DINGO_INTERFACE_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/interface_routes.cpp.j2")
set(DINGO_TYPE_TABLE_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/type_table.cpp.j2")
set(DINGO_TRAIT_TABLE_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/trait_table.cpp.j2")
set(DINGO_BINDING_TABLE_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/binding_table.cpp.j2")
set(DINGO_TYPE_BEHAVIOR_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/type_behavior.cpp.j2")
set(DINGO_PROPERTY_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/wrapper_properties.cpp.j2")
set(DINGO_ANNOTATED_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/annotated_cases.cpp.j2")
set(DINGO_CONST_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/const_cases.cpp.j2")
set(DINGO_KEYED_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/keyed_cases.cpp.j2")
set(DINGO_CONSTRUCT_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/construct_shapes.cpp.j2")
set(DINGO_INVOKE_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/invoke_shapes.cpp.j2")
set(DINGO_PRODUCER_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/producer_cases.cpp.j2")
set(DINGO_CONFIGURATION_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/configuration_cases.cpp.j2")
set(DINGO_MIXED_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/mixed_cases.cpp.j2")
set(DINGO_CYCLICAL_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/cyclical_cases.cpp.j2")
set(DINGO_LOOKUP_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/lookup_cases.cpp.j2")
set(DINGO_DEPENDENCIES_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/dependency_cases.cpp.j2")
set(DINGO_CONVERSION_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/conversion_cases.cpp.j2")
set(DINGO_WRAPPER_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/wrapper_cases.cpp.j2")
set(DINGO_CONSUMER_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/consumer_cases.cpp.j2")
set(DINGO_PROBE_FIXTURES_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/_probe_fixtures.h.j2")
set(DINGO_INTERFACE_RUNTIME_MODEL_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/interface_runtime.cpp.j2")
set(DINGO_RESOLVE_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/resolve_cases.cpp.j2")
set(DINGO_CLASSIFICATION_CONSTRUCTOR_TEMPLATE
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/detection_constructor.cpp.j2")
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
    "${PROJECT_SOURCE_DIR}/spec/gen/shared/templates/selection_rules.cpp.j2")
set(DINGO_STORAGE_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/storage-manifest.json")
set(DINGO_SELECTION_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/selection-manifest.json")
set(DINGO_ROUTE_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/route-manifest.json")
set(DINGO_INTERFACE_MODEL_TEST_SOURCE
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/global/interface/interface_routes.cpp")
set(DINGO_TYPE_TABLE_TEST_SOURCE
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/global/type_table.cpp")
set(DINGO_TRAIT_TABLE_TEST_SOURCE
    "${DINGO_RESOLVE_DIR}/resolution/static_asserts/global/trait_table.cpp")
set(DINGO_REGISTRATION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(DINGO_BINDING_TABLE_MANIFEST
    "${DINGO_REGISTRATION_DIR}/registration-manifest.json")
set(DINGO_PROPERTY_MANIFEST
    "${DINGO_CLASSIFICATION_DIR}/property-manifest.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/spec/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/__main__.py"
    "${PROJECT_SOURCE_DIR}/spec/model/domain/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/model/domain/cases.py"
    "${PROJECT_SOURCE_DIR}/spec/model/domain/requests.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/behavior.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/common/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/common/recipe.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/common/runner.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/common/validation.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/annotated.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/behavior.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/const.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/interface.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/property.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/route.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/storage.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/table.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/resolve.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/detection_constructor.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/factory.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/construct.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/collection.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/families/indexed.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/selection.py"
    "${PROJECT_SOURCE_DIR}/spec/gen/traits.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/bindings.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/catalog.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/cpp.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/enumerate.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/expr.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/shapes.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/storage.py"
    "${PROJECT_SOURCE_DIR}/spec/model/grammar/traits.py"
    "${PROJECT_SOURCE_DIR}/spec/report/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/report/baseline.py"
    "${PROJECT_SOURCE_DIR}/spec/report/coverage.py"
    "${PROJECT_SOURCE_DIR}/spec/report/paths.py"
    "${PROJECT_SOURCE_DIR}/spec/report/resolves.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/capabilities.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/storage_forms.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/extended_shapes.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/forms.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/gaps.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/routes/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/routes/derivation.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/routes/ledger.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/routes/rules.py"
    "${PROJECT_SOURCE_DIR}/spec/model/semantics/support.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/__init__.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/annotated_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/architecture_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/behavior_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/conftest.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/const_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/grammar_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/keyed_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/interface_runtime_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/binding_table_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/interface_routes_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/outcome_baseline_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/paths_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/wrapper_properties_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/route_probes_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/route_derivation_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/route_ledger_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/route_rules_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/selection_rules_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/storage_facts_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/type_behavior_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/type_table_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/collection_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/indexed_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/construct_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/detection_constructor_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/factory_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/resolve_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/resolve_report_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/trait_table_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/wrapper_semantics_test.py"
    "${DINGO_MODEL_TEMPLATE}"
    "${DINGO_SCOPES_TEMPLATE}"
    "${DINGO_SELECTION_MODEL_TEMPLATE}"
    "${DINGO_ROUTE_MODEL_TEMPLATE}"
    "${DINGO_BEHAVIOR_MODEL_TEMPLATE}"
    "${DINGO_INTERFACE_MODEL_TEMPLATE}"
    "${DINGO_TYPE_TABLE_TEMPLATE}"
    "${DINGO_TRAIT_TABLE_TEMPLATE}"
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
    "${DINGO_CONVERSION_MODEL_TEMPLATE}"
    "${DINGO_WRAPPER_MODEL_TEMPLATE}"
    "${PROJECT_SOURCE_DIR}/spec/tests/wrapper_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/conversion_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/dependency_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/lookup_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/cyclical_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/mixed_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/configuration_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/producer_cases_test.py"
    "${DINGO_CONSUMER_MODEL_TEMPLATE}"
    "${DINGO_PROBE_FIXTURES_TEMPLATE}"
    "${PROJECT_SOURCE_DIR}/spec/gen/consumers.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/construct_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/consumer_cases_test.py"
    "${PROJECT_SOURCE_DIR}/spec/tests/claim_report_test.py"
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
        "Failed to generate storage coverage: "
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
        ${DINGO_TYPE_TABLE_GENERATOR}
        --out ${DINGO_TYPE_TABLE_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_TYPE_TABLE_GENERATOR_RESULT)
if(NOT DINGO_TYPE_TABLE_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate the type table: "
        "${DINGO_TYPE_TABLE_GENERATOR_RESULT}")
endif()
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TRAIT_TABLE_GENERATOR}
        --out ${DINGO_TRAIT_TABLE_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_TRAIT_TABLE_GENERATOR_RESULT)
if(NOT DINGO_TRAIT_TABLE_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate the leaf trait table: "
        "${DINGO_TRAIT_TABLE_GENERATOR_RESULT}")
endif()

file(MAKE_DIRECTORY ${DINGO_REGISTRATION_DIR})
execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_BINDING_TABLE_GENERATOR}
        --out-dir ${DINGO_REGISTRATION_DIR}
        --manifest ${DINGO_BINDING_TABLE_MANIFEST}
        --include ${PROJECT_SOURCE_DIR}/include
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_BINDING_TABLE_GENERATOR_RESULT)
if(NOT DINGO_BINDING_TABLE_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate the binding table: "
        "${DINGO_BINDING_TABLE_GENERATOR_RESULT}")
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
        ${DINGO_CONVERSION_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_CONVERSION_MODEL_GENERATOR_RESULT)
if(NOT DINGO_CONVERSION_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate conversion probes: "
        "${DINGO_CONVERSION_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_CONVERSION_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/conversion/*.cpp")

execute_process(
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_WRAPPER_MODEL_GENERATOR}
        --out-dir ${DINGO_RESOLVE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE DINGO_WRAPPER_MODEL_GENERATOR_RESULT)
if(NOT DINGO_WRAPPER_MODEL_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate user wrapper probes: "
        "${DINGO_WRAPPER_MODEL_GENERATOR_RESULT}")
endif()
file(GLOB_RECURSE DINGO_WRAPPER_SOURCES CONFIGURE_DEPENDS
    "${DINGO_RESOLVE_DIR}/resolution/runtime/global/wrapper/*.cpp")

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

add_custom_target(generated-check
    COMMENT "Verifying generated coverage"
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
        ${DINGO_TYPE_TABLE_GENERATOR}
        --out ${DINGO_TYPE_TABLE_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_TRAIT_TABLE_GENERATOR}
        --out ${DINGO_TRAIT_TABLE_TEST_SOURCE}
        --include ${PROJECT_SOURCE_DIR}/include
        --check
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        ${DINGO_BINDING_TABLE_GENERATOR}
        --out-dir ${DINGO_REGISTRATION_DIR}
        --manifest ${DINGO_BINDING_TABLE_MANIFEST}
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
# The type table moved to the resolution/static-asserts phase.
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
    ${DINGO_CONVERSION_SOURCES}
    ${DINGO_WRAPPER_SOURCES}
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
    ${DINGO_TYPE_TABLE_TEST_SOURCE}
    ${DINGO_TRAIT_TABLE_TEST_SOURCE}
    ${DINGO_ROUTE_SOURCES})
target_link_libraries(dingo_resolution_static_asserts_test dingo::dingo gtest_main)
target_include_directories(dingo_resolution_static_asserts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(dingo_resolution_static_asserts_test PRIVATE
    DINGO_TYPE_TABLE_REPORT="${CMAKE_CURRENT_BINARY_DIR}/type-table.md"
    DINGO_TYPE_TABLE_JSON="${CMAKE_CURRENT_BINARY_DIR}/type-table.json")
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
        spec.report.claims
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
        spec.report.resolves
        --manifest ${DINGO_RESOLVE_MANIFEST}
        --outcomes-dir ${CMAKE_CURRENT_BINARY_DIR}
        --out ${CMAKE_CURRENT_BINARY_DIR}/resolve-gaps.md
        --json ${CMAKE_CURRENT_BINARY_DIR}/resolve-gaps.json
    DEPENDS dingo_resolve_probe
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_model_baseline_test
    COMMAND ${DINGO_UV_RUN_COMMAND} python -m
        spec.report.baseline check
        --types ${CMAKE_CURRENT_BINARY_DIR}/type-table.json
        ${DINGO_BEHAVIOR_REPORT_ARGS}
        --baseline ${PROJECT_SOURCE_DIR}/test/baselines/model.json)
set_tests_properties(dingo_model_baseline_test PROPERTIES
    LABELS "unit;generator"
    DEPENDS "dingo_model_test;dingo_resolution_static_asserts_test"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_model_generator_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        ${PROJECT_SOURCE_DIR}/spec/tests)
set_tests_properties(dingo_model_generator_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_tools_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        ${PROJECT_SOURCE_DIR}/tools/complexity_test.py
        ${PROJECT_SOURCE_DIR}/tools/deadcode_test.py
        ${PROJECT_SOURCE_DIR}/tools/lsp_refs_test.py
        ${PROJECT_SOURCE_DIR}/tools/release_flow_test.py)
set_tests_properties(dingo_tools_test PROPERTIES
    LABELS "unit"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_test(NAME dingo_cost_tool_test
    COMMAND ${DINGO_UV_RUN_COMMAND} pytest -q
        ${PROJECT_SOURCE_DIR}/tools/cost/measure_test.py
        ${PROJECT_SOURCE_DIR}/tools/cost/tu_report_test.py)
set_tests_properties(dingo_cost_tool_test PROPERTIES
    LABELS "unit;generator"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tools/cost)

add_custom_target(lsp-refs-report
    COMMENT "Counting header declaration references through clangd"
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/lsp_refs.py
        --json ${PROJECT_BINARY_DIR}/lsp-refs.json
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/deadcode.py
        --lsp ${PROJECT_BINARY_DIR}/lsp-refs.json
        --check ${PROJECT_SOURCE_DIR}/tools/deadcode_baseline.json
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(deadcode-report
    COMMENT "Listing header declarations nothing references"
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/deadcode.py
        --all
        --reachable
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_custom_target(complexity-report
    COMMENT "Ranking library headers by accreted-complexity signals"
    COMMAND ${DINGO_UV_RUN_COMMAND} python
        ${PROJECT_SOURCE_DIR}/tools/complexity.py
        --top 20
        --json ${PROJECT_BINARY_DIR}/complexity.json
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

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

add_custom_target(dingo_table_test
    DEPENDS ${DINGO_TABLE_TEST_TARGETS})

function(dingo_configure_table_compile_target DINGO_TABLE_TARGET)
    target_link_libraries(${DINGO_TABLE_TARGET}
        PRIVATE dingo::dingo gtest_main)
    target_include_directories(${DINGO_TABLE_TARGET}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${DINGO_TABLE_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_compile_options(${DINGO_TABLE_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FI windows.h /DNOMINMAX>)
endfunction()

foreach(DINGO_TABLE_TEST_TARGET IN LISTS DINGO_TABLE_TEST_TARGETS)
    add_executable(${DINGO_TABLE_TEST_TARGET}
        ${${DINGO_TABLE_TEST_TARGET}_SOURCES}
        ${DINGO_TABLE_TEST_HEADERS}
    )
    add_test(NAME ${DINGO_TABLE_TEST_TARGET}
        COMMAND ${DINGO_TABLE_TEST_TARGET})
    if(DINGO_TABLE_TEST_AFTER_BUILD)
        add_custom_command(TARGET ${DINGO_TABLE_TEST_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env
                ASAN_OPTIONS=use_sigaltstack=false
                $<TARGET_FILE:${DINGO_TABLE_TEST_TARGET}>
            COMMENT "Running ${DINGO_TABLE_TEST_TARGET}"
            USES_TERMINAL
            VERBATIM)
    endif()
    set_tests_properties(${DINGO_TABLE_TEST_TARGET}
        PROPERTIES LABELS "runtime")
    dingo_configure_table_compile_target(${DINGO_TABLE_TEST_TARGET})

    set(DINGO_TABLE_DIAGNOSTIC_INDEX 0)
    foreach(DINGO_TABLE_ISOLATED_SOURCE IN LISTS
            ${DINGO_TABLE_TEST_TARGET}_ISOLATED_SOURCES)
        math(EXPR DINGO_TABLE_DIAGNOSTIC_INDEX
            "${DINGO_TABLE_DIAGNOSTIC_INDEX} + 1")
        string(REPLACE "dingo_table_test_" "dingo_table_diag_"
            DINGO_TABLE_DIAGNOSTIC_PREFIX ${DINGO_TABLE_TEST_TARGET})
        set(DINGO_TABLE_DIAGNOSTIC_TARGET
            "${DINGO_TABLE_DIAGNOSTIC_PREFIX}_${DINGO_TABLE_DIAGNOSTIC_INDEX}")
        message(STATUS
            "${DINGO_TABLE_DIAGNOSTIC_TARGET}: "
            "${DINGO_TABLE_ISOLATED_SOURCE}")
        add_library(${DINGO_TABLE_DIAGNOSTIC_TARGET} OBJECT
            "${DINGO_TABLE_ISOLATED_SOURCE}")
        dingo_configure_table_compile_target(
            ${DINGO_TABLE_DIAGNOSTIC_TARGET})
        target_sources(${DINGO_TABLE_TEST_TARGET} PRIVATE
            $<TARGET_OBJECTS:${DINGO_TABLE_DIAGNOSTIC_TARGET}>)
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
