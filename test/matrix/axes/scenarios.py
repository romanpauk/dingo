#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Independent axes for complete behavioral and regression scenarios."""

from __future__ import annotations

from axes.containers import CONTAINERS
from schema import ScenarioContainer, ScenarioSpec


def _container(name: str) -> ScenarioContainer:
    container = next(container for container in CONTAINERS if container.name == name)
    return ScenarioContainer(
        name=container.name,
        type_name=container.container_type,
        headers=container.headers,
    )


STANDALONE = "standalone"
RUNTIME_CONTAINERS = frozenset({"runtime_container", "container_runtime"})
INDEXED_CONTAINERS = frozenset(
    {
        "runtime_container_indexed",
        "container_indexed",
        "runtime_container_indexed_int",
        "container_indexed_int",
        "runtime_container_indexed_string",
        "container_indexed_string",
        "runtime_container_indexed_dsl",
        "container_indexed_dsl",
    }
)

SCENARIO_CONTAINERS = (
    ScenarioContainer(name=STANDALONE, type_name=None),
    *(
        _container(name)
        for name in (
            "runtime_container",
            "container_runtime",
            "runtime_container_indexed",
            "container_indexed",
            "runtime_container_indexed_int",
            "container_indexed_int",
            "runtime_container_indexed_string",
            "container_indexed_string",
            "runtime_container_indexed_dsl",
            "container_indexed_dsl",
        )
    ),
)

REGRESSION_SCENARIOS = (
    ScenarioSpec(
        suite="indexed_registration_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="indexed_registration_scenario::run",
        supported_containers=INDEXED_CONTAINERS,
        headers=("matrix/scenarios/indexed_registration.h",),
    ),
    ScenarioSpec(
        suite="static_indexed_regressions",
        name="default",
        test_name="static_shared_value_type_concrete_value_ref_ptr_static_container",
        function="exercise_static_indexed_regressions",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/static_indexed.h",),
    ),
    ScenarioSpec(
        suite="runtime_container_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="runtime_container_regression_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/runtime/container.h",),
    ),
    ScenarioSpec(
        suite="runtime_lookup_routing_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="runtime_lookup_routing_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/lookup/runtime_routing.h",),
    ),
    ScenarioSpec(
        suite="lookup_route_regressions",
        name="unkeyed",
        test_name=(
            "runtime_shared_scenario_scenario_scenario_unkeyed_{container}"
        ),
        function="unkeyed_lookup_route_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/lookup/routes.h",),
    ),
    ScenarioSpec(
        suite="lookup_route_regressions",
        name="typed",
        test_name="runtime_shared_scenario_scenario_scenario_typed_{container}",
        function="typed_lookup_route_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/lookup/routes.h",),
    ),
    ScenarioSpec(
        suite="lookup_route_regressions",
        name="key_value",
        test_name=(
            "runtime_shared_scenario_scenario_scenario_key_value_{container}"
        ),
        function="key_value_lookup_route_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/lookup/routes.h",),
    ),
    ScenarioSpec(
        suite="runtime_construct_dependency_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="runtime_construct_dependency_regression_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/runtime/construct_dependency.h",),
    ),
    ScenarioSpec(
        suite="runtime_exception_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="runtime_exception_regression_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        system_headers=("dingo/type/type_name.h",),
        headers=("matrix/scenarios/runtime/exceptions.h",),
    ),
    ScenarioSpec(
        suite="runtime_unique_reference_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="runtime_unique_reference_regression_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/runtime/unique_reference.h",),
    ),
    ScenarioSpec(
        suite="shared_cyclical_regressions",
        name="default",
        test_name="runtime_shared_scenario_scenario_scenario_{container}",
        function="shared_cyclical_regression_scenario::run",
        supported_containers=RUNTIME_CONTAINERS,
        headers=("matrix/scenarios/runtime/shared_cyclical.h",),
    ),
)

PARENT_SCENARIOS = (
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_resolution",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_resolution_runtime_container"
        ),
        function="exercise_parent_resolution_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/resolution.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_ownership",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_ownership_runtime_container"
        ),
        function="exercise_parent_ownership_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/ownership.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_origin",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_origin_runtime_container"
        ),
        function="exercise_parent_origin_chains<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/origin.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_cache",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_cache_runtime_container"
        ),
        function="exercise_parent_cache_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/cache.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_transactions",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_transactions_runtime_container"
        ),
        function="exercise_parent_transaction_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/transactions.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_recursion",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_recursion_runtime_container"
        ),
        function="exercise_parent_recursion_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/recursion.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_semantics",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_semantics_runtime_container"
        ),
        function="exercise_parent_semantics",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/semantics.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_registration_transactions",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_registration_transactions_runtime_container"
        ),
        function="exercise_parent_registration_transactions",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/semantics.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_allocators",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_allocators_runtime_container"
        ),
        function="exercise_parent_allocator_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/allocators.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_collections",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_collections_runtime_container"
        ),
        function="exercise_parent_collection_pairs<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/collections.h",),
    ),
    ScenarioSpec(
        suite="nested_container",
        name="cross_parent_intermediate_ambiguity",
        test_name=(
            "runtime_shared_value_type_concrete_value_ref_ptr_"
            "cross_parent_intermediate_ambiguity_runtime_container"
        ),
        function="exercise_intermediate_ambiguity_chains<>",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/parent/search.h",),
    ),
    ScenarioSpec(
        suite="static_parent_container",
        name="container_child_container_parent",
        test_name=(
            "static_shared_value_type_concrete_value_ref_ptr_"
            "container_child_container_parent_container_static"
        ),
        function=(
            "exercise_static_parent_container_pair<container_parent_shape, "
            "container_child_shape>"
        ),
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/static_parent.h",),
    ),
    ScenarioSpec(
        suite="static_parent_container",
        name="container_child_static_parent",
        test_name=(
            "static_shared_value_type_concrete_value_ref_ptr_"
            "container_child_static_parent_container_static"
        ),
        function=(
            "exercise_static_parent_container_pair<static_parent_shape, "
            "container_child_shape>"
        ),
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/static_parent.h",),
    ),
    ScenarioSpec(
        suite="static_parent_container",
        name="static_child_static_parent",
        test_name=(
            "static_shared_value_type_concrete_value_ref_ptr_"
            "static_child_static_parent_static_container"
        ),
        function=(
            "exercise_static_parent_container_pair<static_parent_shape, "
            "static_child_shape>"
        ),
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/static_parent.h",),
    ),
    ScenarioSpec(
        suite="static_parent_container",
        name="mixed_runtime_static_pairs",
        test_name=(
            "static_shared_value_type_concrete_value_ref_ptr_"
            "mixed_runtime_static_pairs_container_static"
        ),
        function="exercise_mixed_runtime_static_parent_pairs",
        supported_containers=frozenset({STANDALONE}),
        headers=("matrix/scenarios/static_parent.h",),
    ),
)

SCENARIOS = (*REGRESSION_SCENARIOS, *PARENT_SCENARIOS)

__all__ = (
    "PARENT_SCENARIOS",
    "REGRESSION_SCENARIOS",
    "SCENARIO_CONTAINERS",
    "SCENARIOS",
)
