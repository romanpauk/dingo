#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from collections import Counter
from dataclasses import replace
from pathlib import Path

import pytest
from jinja2 import Environment, FileSystemLoader, StrictUndefined, Template

from data import CATALOG, CONTAINERS, STORED_TYPES, validate_catalog
from axes.constructor_detection import (
    CONSTRUCTOR_ARGUMENT_CATEGORIES,
    CONSTRUCTOR_ARGUMENT_STORAGES,
    CONSTRUCTOR_DETECTION_BACKENDS,
    CONSTRUCTOR_DETECTION_MODES,
    CONSTRUCTOR_SHAPES,
    DEPENDENCY_CONSTRUCTOR_SHAPES,
    WRAPPED_DEPENDENCY_CONSTRUCTOR_SHAPES,
)
from axes.dependency_compositions import (
    DEPENDENCY_COMPOSITION_LEAVES,
    DEPENDENCY_COMPOSITION_OPERATORS,
    DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES,
    DEPENDENCY_COMPOSITIONS,
    MAX_DEPENDENCY_COMPOSITION_DEPTH,
    compose_dependency,
    dependency_composition_depth,
    render_dependency_composition,
    validate_dependency_compositions,
)
from axes.dependency_forms import (
    DEPENDENCY_CARRIERS,
    DEPENDENCY_DECORATIONS,
    DEPENDENCY_FORMS,
    DEPENDENCY_PROVISIONINGS,
    DEPENDENCY_SHAPES,
    build_dependency_forms,
)
from axes.invoke import (
    ANNOTATED_DEPENDENCY_CALLABLES,
    INVOKE_CALLABLES,
    INVOKE_CONTAINERS,
    INVOKE_MODES,
    INVOKE_PROVISIONINGS,
    PLAIN_DEPENDENCY_CALLABLES,
    SELECTED_DEPENDENCY_CALLABLES,
)
from axes.scenarios import (
    PARENT_SCENARIOS,
    REGRESSION_SCENARIOS,
    SCENARIO_CONTAINERS,
)
from axes.shared_cyclical import (
    SHARED_CYCLICAL_CONTAINERS,
    SHARED_CYCLICAL_DEPENDENCY_EDGES,
    SHARED_CYCLICAL_DEPENDENCY_EDGE_SHAPES,
    SHARED_CYCLICAL_STORAGE_REPRESENTATIONS,
    SHARED_CYCLICAL_STORAGE_SHAPES,
)
from constructor_detection import (
    generate_constructor_argument_conversion_rows,
    generate_constructor_detection_rows,
)
from dependency_contract import validate_dependency_contract
from dependency_composition import (
    DEPENDENCY_COMPOSITION_COVERAGE_REPORT,
    DEPENDENCY_COMPOSITION_CONTAINERS,
    DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION,
    DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT,
    DEPENDENCY_COMPOSITION_OPERATIONS,
    DEPENDENCY_COMPOSITION_PROFILES,
    DEPENDENCY_COMPOSITION_SCOPES,
    DEPENDENCY_COMPOSITION_SCOPE_RULES,
    DependencyCompositionRow,
    build_dependency_composition_coverage,
    disable_dependency_composition_projected_cases,
    generate_dependency_composition_executables,
    generate_dependency_composition_rows,
    project_dependency_composition_rows,
    render_dependency_composition_coverage,
)
from family import (
    SourceShard,
    render_case_family_executables,
    render_family_executables,
)
from generate import (
    MatrixRow,
    RejectionReason,
    assert_dependency_provisioning_coverage,
    assert_axis_used,
    assert_filter_coverage,
    generate,
    generate_rows,
    implemented,
    materialize_row,
    merge_executables,
)
from invoke import InvokeRow, generate_invoke_rows
from plugins import (
    RegistrationRecipe,
    build_registration_plan,
    build_registration_plan_from_recipe,
    register_type,
    render_registration_plan,
)
from scenarios import ScenarioRow, generate_scenario_rows
from shared_cyclical import SharedCyclicalRow, generate_shared_cyclical_rows
from schema import (
    ConstructorDetectionLimitation,
    DependencyComposition,
    DependencyCompositionResolutionLimitation,
    ExposedType,
    GeneratedExecutable,
    LimitationDisposition,
    MixedRegistrationPlacement,
    RegistrationSpec,
)


@pytest.fixture(scope="module")
def rows() -> tuple[MatrixRow, ...]:
    return generate_rows()


@pytest.fixture(scope="module")
def scenario_rows() -> tuple[ScenarioRow, ...]:
    return generate_scenario_rows()


@pytest.fixture(scope="module")
def invoke_rows() -> tuple[InvokeRow, ...]:
    return generate_invoke_rows()


@pytest.fixture(scope="module")
def composition_rows() -> tuple[DependencyCompositionRow, ...]:
    return generate_dependency_composition_rows()


@pytest.fixture(scope="module")
def shared_cyclical_rows() -> tuple[SharedCyclicalRow, ...]:
    return generate_shared_cyclical_rows()


def test_matrix_cardinality_remains_bounded(rows: tuple[MatrixRow, ...]) -> None:
    assert len(rows) == 1917
    assert len({(row.feature.name, row.name) for row in rows}) == len(rows)


def test_shared_cyclical_is_an_independent_full_product(
    shared_cyclical_rows: tuple[SharedCyclicalRow, ...],
) -> None:
    assert len(SHARED_CYCLICAL_STORAGE_SHAPES) == (
        len(SHARED_CYCLICAL_STORAGE_REPRESENTATIONS) ** 2
    )
    assert len(SHARED_CYCLICAL_DEPENDENCY_EDGE_SHAPES) == (
        len(SHARED_CYCLICAL_DEPENDENCY_EDGES) ** 2
    )
    assert len(shared_cyclical_rows) == (
        len(SHARED_CYCLICAL_CONTAINERS)
        * 3
        * len(SHARED_CYCLICAL_STORAGE_SHAPES)
        * len(SHARED_CYCLICAL_DEPENDENCY_EDGE_SHAPES)
    )
    assert len(shared_cyclical_rows) == 900
    assert len(
        {
            (
                row.container.name,
                row.mode.name,
                row.storage.name,
                row.edges.name,
            )
            for row in shared_cyclical_rows
        }
    ) == len(shared_cyclical_rows)


def test_shared_cyclical_limitations_are_explicit(
    shared_cyclical_rows: tuple[SharedCyclicalRow, ...],
) -> None:
    assert Counter(row.supported for row in shared_cyclical_rows) == {
        True: 245,
        False: 655,
    }
    assert all(
        row.supported == (row.unsupported_reason is None)
        for row in shared_cyclical_rows
    )
    assert Counter(
        (row.storage.name, row.supported) for row in shared_cyclical_rows
    ) == {
        ("value_value", True): 20,
        ("value_value", False): 205,
        ("value_shared_pointer", True): 50,
        ("value_shared_pointer", False): 175,
        ("shared_pointer_value", True): 50,
        ("shared_pointer_value", False): 175,
        ("shared_pointer_shared_pointer", True): 125,
        ("shared_pointer_shared_pointer", False): 100,
    }
    assert {
        (row.container.name, row.mode.name)
        for row in shared_cyclical_rows
        if row.supported
    } == {
        ("runtime_container", "runtime"),
        ("static_container", "static"),
        ("container", "runtime"),
        ("container", "static"),
        ("container", "mixed"),
    }


def test_shared_cyclical_rejects_duplicate_axis_members() -> None:
    container = SHARED_CYCLICAL_CONTAINERS[0]
    with pytest.raises(ValueError, match="container.*duplicate"):
        generate_shared_cyclical_rows(containers=(container, container))


def test_behavioral_scenarios_use_independent_axes(
    scenario_rows: tuple[ScenarioRow, ...],
) -> None:
    assert len(scenario_rows) == 42
    assert sum(
        row.scenario in REGRESSION_SCENARIOS for row in scenario_rows
    ) == 27
    assert sum(row.scenario in PARENT_SCENARIOS for row in scenario_rows) == 15
    assert {
        (row.scenario.suite, row.scenario.name) for row in scenario_rows
    } == {
        (scenario.suite, scenario.name)
        for scenario in (*REGRESSION_SCENARIOS, *PARENT_SCENARIOS)
    }


def test_behavioral_scenarios_preserve_legacy_test_names(
    scenario_rows: tuple[ScenarioRow, ...],
) -> None:
    static_parent_suffixes = {
        "container_child_container_parent": "container_static",
        "container_child_static_parent": "container_static",
        "static_child_static_parent": "static_container",
        "mixed_runtime_static_pairs": "container_static",
    }
    for row in scenario_rows:
        if row.scenario.suite == "nested_container":
            expected = (
                "runtime_shared_value_type_concrete_value_ref_ptr_"
                f"{row.scenario.name}_runtime_container"
            )
        elif row.scenario.suite == "static_parent_container":
            expected = (
                "static_shared_value_type_concrete_value_ref_ptr_"
                f"{row.scenario.name}_"
                f"{static_parent_suffixes[row.scenario.name]}"
            )
        elif row.scenario.suite == "static_indexed_regressions":
            expected = (
                "static_shared_value_type_concrete_value_ref_ptr_"
                "static_container"
            )
        else:
            scenario_name = (
                "" if row.scenario.name == "default" else f"{row.scenario.name}_"
            )
            expected = (
                "runtime_shared_scenario_scenario_scenario_"
                f"{scenario_name}{row.container.name}"
            )
        assert row.name == expected


def test_scenario_axis_rejects_duplicate_members() -> None:
    container = SCENARIO_CONTAINERS[0]
    with pytest.raises(ValueError, match="container.*duplicate"):
        generate_scenario_rows(containers=(container, container))


def test_scenario_axis_rejects_unused_members() -> None:
    unused = replace(SCENARIO_CONTAINERS[0], name="unused")
    with pytest.raises(RuntimeError, match="container.*unused"):
        generate_scenario_rows(containers=(*SCENARIO_CONTAINERS, unused))


def test_constructor_detection_is_an_independent_full_product() -> None:
    rows = generate_constructor_detection_rows()
    assert len(rows) == (
        len(CONSTRUCTOR_DETECTION_BACKENDS)
        * len(CONSTRUCTOR_DETECTION_MODES)
        * len(CONSTRUCTOR_SHAPES)
    )

    expected_shapes = {shape.name for shape in CONSTRUCTOR_SHAPES}
    for backend in CONSTRUCTOR_DETECTION_BACKENDS:
        for mode in CONSTRUCTOR_DETECTION_MODES:
            assert {
                row.constructor_shape.name
                for row in rows
                if row.backend == backend and row.mode == mode
            } == expected_shapes


def test_constructor_detection_limitations_are_explicit() -> None:
    rows = generate_constructor_detection_rows()
    unsupported = {
        (row.backend.name, row.mode.name, row.constructor_shape.name)
        for row in rows
        if not row.supported
    }
    unsupported_signature_shapes = {
        "dependency_optional",
        "dependency_variant",
        "mixed_wrappers",
        "nested_forwarding_wrapper",
    } | {composition.name for composition in DEPENDENCY_COMPOSITIONS}
    composition_limitations = {
        (backend.name, limitation.mode, composition.name)
        for composition in DEPENDENCY_COMPOSITIONS
        for limitation in composition.constructor_detection_limitations
        for backend in CONSTRUCTOR_DETECTION_BACKENDS
        if limitation.backend is None or limitation.backend == backend.name
    }
    assert unsupported == (
        {
            (backend.name, "signature", shape)
            for backend in CONSTRUCTOR_DETECTION_BACKENDS
            for shape in unsupported_signature_shapes
        }
        | {
            (backend.name, mode.name, "unconstrained_forwarding_wrapper")
            for backend in CONSTRUCTOR_DETECTION_BACKENDS
            for mode in CONSTRUCTOR_DETECTION_MODES
        }
        | {
            (backend.name, "shape", shape)
            for backend in CONSTRUCTOR_DETECTION_BACKENDS
            for shape in {
                "dependency_optional",
                "mixed_wrappers",
                "nested_forwarding_wrapper",
            }
        }
        | composition_limitations
    )
    assert all(row.unsupported_reason for row in rows if not row.supported)
    shape_limited_compositions = {
        composition.name
        for composition in DEPENDENCY_COMPOSITIONS
        if any(
            limitation.mode == "shape"
            for limitation in composition.constructor_detection_limitations
        )
    }
    assert {
        row.constructor_shape.name
        for row in rows
        if row.mode.name == "shape" and not row.supported
    } == shape_limited_compositions | {
        "dependency_optional",
        "mixed_wrappers",
        "nested_forwarding_wrapper",
        "unconstrained_forwarding_wrapper",
    }


def test_dependency_shapes_own_constructor_detection_limitations() -> None:
    limitations = {
        shape.name: tuple(shape.constructor_detection_limitations)
        for shape in DEPENDENCY_SHAPES
        if shape.constructor_detection_limitations
    }
    assert set(limitations) == {"optional", "variant"}
    for capabilities in limitations.values():
        capability = capabilities[0]
        assert capability.carriers == frozenset({"value"})
        assert capability.decorations == frozenset({"plain"})
        assert capability.limitation.backend is None
        assert capability.limitation.mode == "signature"
    assert len(limitations["variant"]) == 1
    assert len(limitations["optional"]) == 2
    optional_shape_limitation = limitations["optional"][1]
    assert optional_shape_limitation.carriers == frozenset({"value"})
    assert optional_shape_limitation.decorations == frozenset({"plain"})
    assert optional_shape_limitation.limitation.backend is None
    assert optional_shape_limitation.limitation.mode == "shape"
    assert optional_shape_limitation.limitation.guard == (
        "defined(__clang__) || defined(_MSC_VER)"
    )

    constructor_limitations = {
        next(iter(shape.dependency_forms)): shape.constructor_detection_limitations
        for shape in DEPENDENCY_CONSTRUCTOR_SHAPES
        if shape.constructor_detection_limitations
    }
    assert set(constructor_limitations) == {"optional", "variant"}
    assert {
        form: tuple(capability.limitation for capability in limitations[form])
        for form in constructor_limitations
    } == constructor_limitations


def test_dependency_compositions_are_bounded_and_rendered_structurally() -> None:
    validate_dependency_compositions()
    assert MAX_DEPENDENCY_COMPOSITION_DEPTH == 2
    assert {operator.name for operator in DEPENDENCY_COMPOSITION_OPERATORS} == {
        "array",
        "const_pointer",
        "optional",
        "pointer",
        "shared_pointer",
        "unique_pointer",
        "variant",
    }
    assert {
        operator.name for operator in DEPENDENCY_COMPOSITION_OPERATORS
    } == {shape.name for shape in DEPENDENCY_SHAPES} - {"identity"}
    assert {leaf.name for leaf in DEPENDENCY_COMPOSITION_LEAVES} == {
        "regular",
        "move_only",
        "copy_only",
    }
    rendered = {
        composition.name: render_dependency_composition(composition)
        for composition in DEPENDENCY_COMPOSITIONS
    }
    assert len(rendered) == 420
    assert Counter(
        dependency_composition_depth(composition)
        for composition in DEPENDENCY_COMPOSITIONS
    ) == {1: 21, 2: 399}
    limited = {
        composition.name: composition.constructor_detection_limitations
        for composition in DEPENDENCY_COMPOSITIONS
        if composition.constructor_detection_limitations
    }
    optional_pointer_compositions = {
        composition.name
        for composition in DEPENDENCY_COMPOSITIONS
        if composition.operator is not None
        and composition.operator.name == "optional"
        and composition.operands[0].operator is not None
        and composition.operands[0].operator.name
        in {"pointer", "const_pointer"}
    }
    assert len(optional_pointer_compositions) == 6
    unconditional_shape_limitations = optional_pointer_compositions | {
        "optional_array_copy_only",
        "optional_variant_regular_copy_only",
    }
    non_gnu_ambiguous_compositions = {
        composition.name
        for composition in DEPENDENCY_COMPOSITIONS
        if composition.operator is not None
        and (
            composition.operator.name == "optional"
            or (
                composition.operator.name == "variant"
                and any(
                    operand.operator is not None
                    and operand.operator.name == "optional"
                    for operand in composition.operands
                )
            )
        )
    }
    assert len(non_gnu_ambiguous_compositions) == 90
    assert set(limited) == non_gnu_ambiguous_compositions
    for name, limitations in limited.items():
        assert len(limitations) == 1
        limitation = limitations[0]
        assert limitation.mode == "shape"
        if name in unconditional_shape_limitations:
            assert limitation.backend is None
            assert limitation.guard is None
        else:
            assert limitation.backend is None
            assert limitation.guard == (
                "defined(__clang__) || defined(_MSC_VER)"
            )
    assert {
        name: rendered[name]
        for name in {
            "nested_optional_shared_pointer",
            "nested_variant_optional",
            "optional_copy_only",
            "optional_move_only",
            "optional_unique_pointer",
            "variant_move_copy_only",
            "variant_shared_unique_pointers",
        }
    } == {
        "nested_optional_shared_pointer": (
            "std::optional<std::shared_ptr<dependency_regular>>"
        ),
        "nested_variant_optional": (
            "std::variant<std::optional<dependency_regular>, "
            "dependency_move_only>"
        ),
        "optional_copy_only": "std::optional<dependency_copy_only>",
        "optional_move_only": "std::optional<dependency_move_only>",
        "optional_unique_pointer": (
            "std::optional<std::unique_ptr<dependency_regular>>"
        ),
        "variant_move_copy_only": (
            "std::variant<dependency_move_only, dependency_copy_only>"
        ),
        "variant_shared_unique_pointers": (
            "std::variant<std::shared_ptr<dependency_regular>, "
            "std::unique_ptr<dependency_regular>>"
        ),
    }


def test_constructor_detection_consumes_every_dependency_composition() -> None:
    compositions = {
        composition.name: render_dependency_composition(composition)
        for composition in DEPENDENCY_COMPOSITIONS
    }
    shapes = {
        shape.name: shape
        for shape in WRAPPED_DEPENDENCY_CONSTRUCTOR_SHAPES
    }
    assert shapes.keys() == compositions.keys()
    for name, type_name in compositions.items():
        assert shapes[name].target_type == f"constructor_dependency<{type_name}>"
        assert shapes[name].signature_arguments == (
            f"dingo::type_list<{type_name}>"
        )


def test_dependency_composition_resolution_constraints_are_declarative(
) -> None:
    assert {
        strategy.name for strategy in DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES
    } == {"stable", "value", "rvalue"}
    limitations = {
        operator.name: operator.resolution_limitations
        for operator in DEPENDENCY_COMPOSITION_OPERATORS
    }
    limitation_positions = {
        operator: {limitation.position for limitation in operator_limitations}
        for operator, operator_limitations in limitations.items()
    }
    assert limitation_positions["pointer"] == {"request_composed_operand"}
    assert limitation_positions["const_pointer"] == {
        "request_composed_operand",
        "nested",
    }
    assert limitation_positions["shared_pointer"] == {
        "request_composed_operand"
    }
    assert limitation_positions["unique_pointer"] == {
        "request_composed_operand"
    }
    assert limitation_positions["array"] == {"request_composed_operand"}
    assert not limitation_positions["optional"]
    assert not limitations["variant"]
    assert all(
        limitation.reason
        and limitation.disposition is LimitationDisposition.KNOWN_GAP
        for operator_limitations in limitations.values()
        for limitation in operator_limitations
    )
    assert all(
        operator.unsupported_request_disposition
        is LimitationDisposition.INTENTIONAL_CONSTRAINT
        for operator in DEPENDENCY_COMPOSITION_OPERATORS
        if operator.supported_request_strategies != {"stable", "value", "rvalue"}
    )
    assert limitations["array"][0].request_strategies == {"value", "rvalue"}
    assert limitations["array"][0].operand_operators == {"optional"}

    scope_rules = {
        rule.scope: rule for rule in DEPENDENCY_COMPOSITION_SCOPE_RULES
    }
    movable_requirements = {
        scope: rule.non_movable_reason
        for scope, rule in scope_rules.items()
        if rule.requires_movable
    }
    assert all(movable_requirements.values())
    assert movable_requirements.keys() == {"shared", "unique"}
    assert all(
        rule.non_movable_disposition
        is LimitationDisposition.INTENTIONAL_CONSTRAINT
        for rule in scope_rules.values()
        if rule.requires_movable
    )
    assert scope_rules["external"].requires_runtime_registration
    assert scope_rules["external"].static_registration_reason
    assert (
        scope_rules["external"].static_registration_disposition
        is LimitationDisposition.INTENTIONAL_CONSTRAINT
    )
    assert {
        container.name
        for container in DEPENDENCY_COMPOSITION_CONTAINERS
    } == {
        "container_mixed",
        "container_runtime",
        "container_static",
        "runtime_container",
        "static_container",
    }
    assert {scope.name for scope in DEPENDENCY_COMPOSITION_SCOPES} == {
        "external",
        "shared",
        "unique",
    }


@pytest.mark.parametrize(
    "limitation",
    (
        DependencyCompositionResolutionLimitation(
            position="unknown",
            reason="invalid test limitation",
            disposition=LimitationDisposition.KNOWN_GAP,
        ),
        DependencyCompositionResolutionLimitation(
            position="nested",
            reason="invalid test limitation",
            disposition=LimitationDisposition.KNOWN_GAP,
            request_strategies=frozenset({"unknown"}),
        ),
        DependencyCompositionResolutionLimitation(
            position="nested",
            reason="invalid test limitation",
            disposition=LimitationDisposition.KNOWN_GAP,
            operand_operators=frozenset({"unknown"}),
        ),
    ),
)
def test_dependency_compositions_reject_invalid_resolution_constraints(
    limitation: DependencyCompositionResolutionLimitation,
) -> None:
    pointer = DEPENDENCY_COMPOSITION_OPERATORS[0]
    invalid_pointer = replace(
        pointer,
        resolution_limitations=(limitation,),
    )
    with pytest.raises(ValueError, match="invalid resolution limitation"):
        validate_dependency_compositions(
            operators=(
                invalid_pointer,
                *DEPENDENCY_COMPOSITION_OPERATORS[1:],
            )
        )


def test_dependency_compositions_reject_unclassified_limitations() -> None:
    pointer = DEPENDENCY_COMPOSITION_OPERATORS[0]
    with pytest.raises(
        ValueError,
        match="incomplete unsupported request disposition",
    ):
        validate_dependency_compositions(
            operators=(
                replace(pointer, unsupported_request_disposition=None),
                *DEPENDENCY_COMPOSITION_OPERATORS[1:],
            )
        )

    shared_rule = DEPENDENCY_COMPOSITION_SCOPE_RULES[0]
    with pytest.raises(
        ValueError,
        match="incomplete movable requirement",
    ):
        generate_dependency_composition_rows(
            scope_rules=(
                replace(shared_rule, non_movable_disposition=None),
                *DEPENDENCY_COMPOSITION_SCOPE_RULES[1:],
            )
        )


def test_dependency_compositions_allow_independently_filtered_limitations(
) -> None:
    pointer = DEPENDENCY_COMPOSITION_OPERATORS[0]
    filtered_pointer = replace(
        pointer,
        resolution_limitations=(
            DependencyCompositionResolutionLimitation(
                position="request_composed_operand",
                reason="value limitation",
                disposition=LimitationDisposition.KNOWN_GAP,
                request_strategies=frozenset({"value"}),
            ),
            DependencyCompositionResolutionLimitation(
                position="request_composed_operand",
                reason="rvalue limitation",
                disposition=LimitationDisposition.KNOWN_GAP,
                request_strategies=frozenset({"rvalue"}),
            ),
        ),
    )
    operators = (
        filtered_pointer,
        *DEPENDENCY_COMPOSITION_OPERATORS[1:],
    )
    operators_by_name = {operator.name: operator for operator in operators}

    def replace_operators(
        composition: DependencyComposition,
    ) -> DependencyComposition:
        if composition.operator is None:
            return composition
        return replace(
            composition,
            operator=operators_by_name[composition.operator.name],
            operands=tuple(
                replace_operators(operand)
                for operand in composition.operands
            ),
        )

    validate_dependency_compositions(
        compositions=tuple(
            replace_operators(composition)
            for composition in DEPENDENCY_COMPOSITIONS
        ),
        operators=operators,
    )


def test_resolution_and_invoke_consume_every_dependency_composition(
    composition_rows: tuple[DependencyCompositionRow, ...],
) -> None:
    assert len(composition_rows) == (
        len(DEPENDENCY_COMPOSITIONS)
        * len(DEPENDENCY_COMPOSITION_OPERATIONS)
        * len(DEPENDENCY_COMPOSITION_CONTAINERS)
        * sum(
            len(rule.request_strategies)
            for rule in DEPENDENCY_COMPOSITION_SCOPE_RULES
        )
    )
    assert len(composition_rows) == 16800
    assert len(
        {
            (
                row.operation.name,
                row.container.name,
                row.scope.name,
                row.request_strategy.name,
                row.composition.name,
            )
            for row in composition_rows
        }
    ) == len(composition_rows)
    scope_rules = {
        rule.scope: rule for rule in DEPENDENCY_COMPOSITION_SCOPE_RULES
    }
    for operation in DEPENDENCY_COMPOSITION_OPERATIONS:
        for container in DEPENDENCY_COMPOSITION_CONTAINERS:
            for scope in DEPENDENCY_COMPOSITION_SCOPES:
                for request_strategy in scope_rules[
                    scope.name
                ].request_strategies:
                    projected = {
                        row.composition.name: row.type_name
                        for row in composition_rows
                        if row.operation == operation
                        and row.container == container
                        and row.scope == scope
                        and row.request_strategy.name == request_strategy
                    }
                    assert projected == {
                        composition.name: render_dependency_composition(
                            composition
                        )
                        for composition in DEPENDENCY_COMPOSITIONS
                    }


def test_dependency_composition_limitations_are_explicit_and_operation_neutral(
    composition_rows: tuple[DependencyCompositionRow, ...],
) -> None:
    assert all(
        row.supported
        == (row.unsupported_reason is None)
        == (row.unsupported_disposition is None)
        for row in composition_rows
    )
    assert Counter(
        (row.operation.name, row.supported) for row in composition_rows
    ) == {
        ("resolve", True): 3172,
        ("resolve", False): 5228,
        ("invoke", True): 3172,
        ("invoke", False): 5228,
    }
    assert Counter(
        row.unsupported_reason
        for row in composition_rows
        if not row.supported
    ) == {
        (
            "shared storage cannot materialize a composition that is not "
            "movable"
        ): 1150,
        (
            "container storage does not publish nested raw-pointer "
            "compositions as exact wrapper requests"
        ): 672,
        (
            "nested smart-pointer storage exposes inner conversion "
            "capabilities that cannot materialize the exact composition"
        ): 1512,
        (
            "wrapper storage normalizes a nested pointer-to-const and cannot "
            "publish the exact composed type"
        ): 2142,
        "pointer compositions do not support rvalue requests": 240,
        "const_pointer compositions do not support rvalue requests": 240,
        (
            "unique storage cannot materialize a composition that is not "
            "movable"
        ): 2300,
        "pointer compositions do not support value requests": 240,
        "const_pointer compositions do not support value requests": 240,
        (
            "owning array requests use constructor-shape materialization and "
            "cannot construct an optional element"
        ): 40,
        (
            "external storage requires runtime registration and cannot be "
            "provided by a static-only container"
        ): 1680,
    }

    classifications = {
        (
            row.container.name,
            row.scope.name,
            row.request_strategy.name,
            row.composition.name,
        ): (
            row.supported,
            row.unsupported_disposition,
            row.unsupported_reason,
        )
        for row in composition_rows
        if row.operation.name == "resolve"
    }
    assert classifications == {
        (
            row.container.name,
            row.scope.name,
            row.request_strategy.name,
            row.composition.name,
        ): (
            row.supported,
            row.unsupported_disposition,
            row.unsupported_reason,
        )
        for row in composition_rows
        if row.operation.name == "invoke"
    }

    supported_cells = {
        composition_name: {
            (
                row.container.name,
                row.scope.name,
                row.request_strategy.name,
            )
            for row in composition_rows
            if row.operation.name == "resolve"
            and row.composition.name == composition_name
            and row.supported
        }
        for composition_name in {
            "nested_optional_shared_pointer",
            "nested_variant_optional",
            "optional_copy_only",
            "optional_move_only",
            "optional_unique_pointer",
            "variant_move_copy_only",
            "variant_shared_unique_pointers",
        }
    }
    stable_cells = {
        (container.name, "shared", "stable")
        for container in DEPENDENCY_COMPOSITION_CONTAINERS
    }
    external_cells = {
        (container, "external", "stable")
        for container in {
            "container_mixed",
            "container_runtime",
            "runtime_container",
        }
    }
    unique_cells = {
        (container.name, "unique", request_strategy)
        for container in DEPENDENCY_COMPOSITION_CONTAINERS
        for request_strategy in {"value", "rvalue"}
    }
    stable_cells |= external_cells
    all_cells = stable_cells | unique_cells
    assert supported_cells == {
        "nested_optional_shared_pointer": all_cells,
        "nested_variant_optional": all_cells,
        "optional_copy_only": external_cells,
        "optional_move_only": all_cells,
        "optional_unique_pointer": all_cells,
        "variant_move_copy_only": external_cells,
        "variant_shared_unique_pointers": all_cells,
    }


def test_dependency_composition_coverage_aggregates_every_shared_axis(
    composition_rows: tuple[DependencyCompositionRow, ...],
) -> None:
    coverage = build_dependency_composition_coverage(composition_rows)

    assert (
        coverage.overall.supported,
        coverage.overall.functionality_gaps,
        coverage.overall.intentional_constraints,
    ) == (6344, 4366, 6090)
    assert {
        axis.name: {
            cell.name: (
                cell.count.supported,
                cell.count.functionality_gaps,
                cell.count.intentional_constraints,
            )
            for cell in axis.cells
        }
        for axis in coverage.axes
    } == {
        "operation": {
            "invoke": (3172, 2183, 3045),
            "resolve": (3172, 2183, 3045),
        },
        "operator": {
            "array": (566, 148, 246),
            "const_pointer": (48, 336, 576),
            "optional": (606, 108, 246),
            "pointer": (48, 336, 576),
            "shared_pointer": (108, 756, 96),
            "unique_pointer": (108, 756, 96),
            "variant": (4860, 1926, 4254),
        },
        "container": {
            "container_mixed": (1480, 998, 882),
            "container_runtime": (1480, 998, 882),
            "container_static": (952, 686, 1722),
            "runtime_container": (1480, 998, 882),
            "static_container": (952, 686, 1722),
        },
        "scope": {
            "external": (1584, 936, 1680),
            "shared": (1640, 1410, 1150),
            "unique": (3120, 2020, 3260),
        },
        "request strategy": {
            "rvalue": (1560, 1010, 1630),
            "stable": (3224, 2346, 2830),
            "value": (1560, 1010, 1630),
        },
        "copyability": {
            "copyable": (2428, 2816, 3716),
            "non_copyable": (3916, 1550, 2374),
        },
        "movability": {
            "movable": (5744, 4276, 2180),
            "non_movable": (600, 90, 3910),
        },
        "depth": {
            "1": (516, 0, 324),
            "2": (5828, 4366, 5766),
        },
    }
    for axis in coverage.axes:
        assert sum(cell.count.supported for cell in axis.cells) == 6344
        assert sum(cell.count.functionality_gaps for cell in axis.cells) == 4366
        assert sum(
            cell.count.intentional_constraints for cell in axis.cells
        ) == 6090
    assert {
        disposition: sum(
            count
            for item_disposition, _, count
            in coverage.limitations
            if item_disposition is disposition
        )
        for disposition in LimitationDisposition
    } == {
        LimitationDisposition.KNOWN_GAP: 4366,
        LimitationDisposition.INTENTIONAL_CONSTRAINT: 6090,
    }

    report = render_dependency_composition_coverage(coverage)
    assert "| 6344 | 4366 | 6090 |" in report
    assert "| `variant` | 4860 | 1926 | 4254 |" in report
    assert "## Functionality Gaps" in report
    assert "## Intentional Constraints" in report


def _composition_projection_structure(
    row: DependencyCompositionRow,
) -> tuple[object, ...]:
    assert row.composition.operator is not None
    return (
        row.operation.name,
        row.composition.operator.name,
        *(
            operand.operator.name if operand.operator else "leaf"
            for operand in row.composition.operands
        ),
        dependency_composition_depth(row.composition),
        row.composition.copyable,
        row.composition.movable,
    )


def _composition_projection_environment(
    row: DependencyCompositionRow,
) -> tuple[object, ...]:
    assert row.composition.operator is not None
    return (
        row.operation.name,
        row.composition.operator.name,
        row.container.name,
        row.scope.name,
        row.request_strategy.name,
    )


def _composition_projection_mobility_scope(
    row: DependencyCompositionRow,
) -> tuple[object, ...]:
    return (
        row.operation.name,
        row.composition.copyable,
        row.composition.movable,
        row.scope.name,
        row.request_strategy.name,
    )


@pytest.mark.parametrize(
    ("profile", "expected_count"), (("full", 608), ("portable", 322))
)
def test_dependency_composition_projection_covers_its_execution_contract(
    composition_rows: tuple[DependencyCompositionRow, ...],
    profile: str,
    expected_count: int,
) -> None:
    supported_rows = tuple(row for row in composition_rows if row.supported)
    projected = project_dependency_composition_rows(
        composition_rows, profile
    )

    assert len(projected) == expected_count
    assert len(projected) == len(set(projected))
    assert all(
        row.supported and row.unsupported_reason is None for row in projected
    )
    assert {
        _composition_projection_environment(row) for row in projected
    } == {
        _composition_projection_environment(row) for row in supported_rows
    }
    assert {
        _composition_projection_mobility_scope(row) for row in projected
    } == {
        _composition_projection_mobility_scope(row) for row in supported_rows
    }
    if profile == "full":
        assert {
            (row.operation.name, row.composition.name) for row in projected
        } == {
            (row.operation.name, row.composition.name)
            for row in supported_rows
        }
    else:
        assert {
            _composition_projection_structure(row) for row in projected
        } == {
            _composition_projection_structure(row) for row in supported_rows
        }


def test_dependency_composition_projection_rejects_unknown_profiles(
    composition_rows: tuple[DependencyCompositionRow, ...],
) -> None:
    assert DEPENDENCY_COMPOSITION_PROFILES == {"full", "portable"}
    with pytest.raises(ValueError, match="unknown.*profile"):
        project_dependency_composition_rows(composition_rows, "missing")


@pytest.mark.parametrize("profile", ("full", "portable"))
@pytest.mark.parametrize(
    "implementation_case_limit",
    (None, DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT),
)
def test_dependency_composition_executables_are_bounded_and_balanced(
    tmp_path: Path,
    profile: str,
    implementation_case_limit: int | None,
) -> None:
    script_dir = Path(__file__).resolve().parent
    environment = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )
    executables = generate_dependency_composition_executables(
        tmp_path,
        environment.get_template("dependency_composition.cpp.j2"),
        environment.get_template("dependency_composition_runner.cpp.j2"),
        profile=profile,
        implementation_case_limit=implementation_case_limit,
    )

    assert len(executables) == (
        len(DEPENDENCY_COMPOSITION_OPERATIONS)
        * DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION
    )
    for operation in DEPENDENCY_COMPOSITION_OPERATIONS:
        operation_executables = tuple(
            executable
            for executable in executables
            if executable.name.startswith(
                f"dingo_matrix_test_dependency_composition_{operation.name}_"
            )
        )
        assert len(operation_executables) == (
            DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION
        )
        projected_case_counts: list[int] = []
        for executable in operation_executables:
            runner_sources = tuple(
                source
                for source in executable.sources
                if source.name.startswith("matrix_runner_")
            )
            implementation_sources = tuple(
                source
                for source in executable.sources
                if not source.name.startswith("matrix_runner_")
            )
            assert len(runner_sources) == 1
            projected_case_count = sum(
                source.read_text(encoding="utf-8").count("\nTEST(")
                for source in runner_sources
            )
            projected_case_counts.append(projected_case_count)
            expected_implementation_count = 1
            if implementation_case_limit is not None:
                expected_implementation_count = (
                    projected_case_count + implementation_case_limit - 1
                ) // implementation_case_limit
            assert len(implementation_sources) == expected_implementation_count
            implementation_case_counts = tuple(
                source.read_text(encoding="utf-8").count("\nstruct ")
                for source in implementation_sources
            )
            assert sum(implementation_case_counts) == projected_case_count
            if implementation_case_limit is not None:
                assert max(implementation_case_counts) <= (
                    implementation_case_limit
                )
            assert max(implementation_case_counts) - min(
                implementation_case_counts
            ) <= 1
        assert max(projected_case_counts) - min(projected_case_counts) <= 1


def test_dependency_composition_selected_executables_are_isolated_by_case(
    tmp_path: Path,
) -> None:
    script_dir = Path(__file__).resolve().parent
    environment = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )
    isolated_executables = frozenset({("invoke", 4), ("resolve", 4)})
    executables = generate_dependency_composition_executables(
        tmp_path,
        environment.get_template("dependency_composition.cpp.j2"),
        environment.get_template("dependency_composition_runner.cpp.j2"),
        implementation_case_limit=(
            DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT
        ),
        isolated_implementation_executables=isolated_executables,
    )

    for operation in ("invoke", "resolve"):
        executable = next(
            executable
            for executable in executables
            if executable.name
            == f"dingo_matrix_test_dependency_composition_{operation}_4"
        )
        implementation_sources = tuple(
            source
            for source in executable.sources
            if not source.name.startswith("matrix_runner_")
        )
        assert len(implementation_sources) == 76
        assert executable.isolated_sources == implementation_sources
        assert all(
            source.read_text(encoding="utf-8").count("\nstruct ") == 1
            for source in executable.isolated_sources
        )
        assert any(
            source.name.endswith(
                "container_mixed_external_stable_array_array_copy_only.cpp"
            )
            for source in executable.isolated_sources
        )
        runner_sources = tuple(
            source
            for source in executable.sources
            if source.name.startswith("matrix_runner_")
        )
        assert len(runner_sources) == 1
        assert runner_sources[0].read_text(encoding="utf-8").count(
            "\nTEST("
        ) == 76
    assert all(
        not executable.isolated_sources
        for executable in executables
        if executable.name
        not in {
            "dingo_matrix_test_dependency_composition_invoke_4",
            "dingo_matrix_test_dependency_composition_resolve_4",
        }
    )


def test_dependency_composition_disabled_projected_cases_are_omitted(
    tmp_path: Path,
) -> None:
    script_dir = Path(__file__).resolve().parent
    environment = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )
    row_name = (
        "runtime_container_unique_value_array_shared_pointer_copy_only"
    )
    disabled_projected_cases = frozenset(
        (operation, row_name) for operation in ("invoke", "resolve")
    )
    executables = generate_dependency_composition_executables(
        tmp_path,
        environment.get_template("dependency_composition.cpp.j2"),
        environment.get_template("dependency_composition_runner.cpp.j2"),
        implementation_case_limit=(
            DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT
        ),
        disabled_projected_cases=disabled_projected_cases,
    )

    generated = "\n".join(
        source.read_text(encoding="utf-8")
        for executable in executables
        for source in executable.sources
    )
    assert row_name not in generated
    assert sum(
        source.read_text(encoding="utf-8").count("\nTEST(")
        for executable in executables
        for source in executable.sources
        if source.name.startswith("matrix_runner_")
    ) == 606


def test_dependency_composition_rejects_unmatched_disabled_projected_cases() -> None:
    projected = project_dependency_composition_rows(
        generate_dependency_composition_rows(), "full"
    )
    with pytest.raises(ValueError, match="did not match.*missing"):
        disable_dependency_composition_projected_cases(
            projected,
            frozenset({("resolve", "missing")}),
        )


def test_dependency_composition_disabled_full_case_is_optional_in_portable(
) -> None:
    rows = generate_dependency_composition_rows()
    portable = project_dependency_composition_rows(rows, "portable")
    assert disable_dependency_composition_projected_cases(
        portable,
        frozenset(
            {
                (
                    "resolve",
                    "runtime_container_unique_value_"
                    "array_shared_pointer_copy_only",
                )
            }
        ),
        rows,
    ) == portable


def test_dependency_composition_rejects_unmatched_isolated_executables(
    tmp_path: Path,
) -> None:
    script_dir = Path(__file__).resolve().parent
    environment = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )
    with pytest.raises(ValueError, match="did not match.*resolve.*5"):
        generate_dependency_composition_executables(
            tmp_path,
            environment.get_template("dependency_composition.cpp.j2"),
            environment.get_template(
                "dependency_composition_runner.cpp.j2"
            ),
            implementation_case_limit=(
                DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT
            ),
            isolated_implementation_executables=frozenset({("resolve", 5)}),
        )


def test_dependency_compositions_reject_excessive_depth() -> None:
    too_deep = compose_dependency(
        "too_deep",
        "optional",
        next(
            composition
            for composition in DEPENDENCY_COMPOSITIONS
            if dependency_composition_depth(composition) == 2
        ),
    )
    with pytest.raises(ValueError, match="has depth 3"):
        validate_dependency_compositions(compositions=(too_deep,))


def test_dependency_forms_are_the_compatible_structural_product() -> None:
    assert DEPENDENCY_FORMS == build_dependency_forms()
    assert {
        (form.shape, form.carrier, form.decoration)
        for form in DEPENDENCY_FORMS
    } == {
        (shape.name, carrier.name, decoration.name)
        for shape in DEPENDENCY_SHAPES
        for carrier in DEPENDENCY_CARRIERS
        for decoration in DEPENDENCY_DECORATIONS
        if (shape.name, carrier.name)
        in decoration.supported_shape_carriers
    }
    assert {shape.name for shape in DEPENDENCY_SHAPES} == {
        "identity",
        "pointer",
        "const_pointer",
        "shared_pointer",
        "unique_pointer",
        "optional",
        "array",
        "variant",
    }
    assert {form.name for form in DEPENDENCY_FORMS} == {
        "value",
        "lvalue_reference",
        "const_lvalue_reference",
        "rvalue_reference",
        "pointer",
        "const_pointer",
        "shared_pointer",
        "shared_pointer_lvalue_reference",
        "unique_pointer",
        "optional",
        "optional_lvalue_reference",
        "array_lvalue_reference",
        "variant",
        "selected_value",
        "selected_lvalue_reference",
        "annotated_lvalue_reference",
        "annotated_pointer",
        "annotated_shared_pointer",
        "annotated_shared_pointer_lvalue_reference",
    }


def test_dependency_product_drives_each_applicable_family() -> None:
    expected_detection = {
        form.name
        for form in DEPENDENCY_FORMS
        if "constructor_detection" in form.required_families
    }
    detection_coverage = Counter(
        form
        for shape in DEPENDENCY_CONSTRUCTOR_SHAPES
        for form in shape.dependency_forms
    )
    assert detection_coverage == Counter(
        {form: 1 for form in expected_detection}
    )

    expected_invoke = {
        form.name
        for form in DEPENDENCY_FORMS
        if "invoke" in form.required_families
    }
    invoke_coverage = Counter(
        form
        for callable_spec in (
            *PLAIN_DEPENDENCY_CALLABLES,
            *SELECTED_DEPENDENCY_CALLABLES,
            *ANNOTATED_DEPENDENCY_CALLABLES,
        )
        for form in callable_spec.dependency_forms
    )
    assert invoke_coverage == Counter({form: 1 for form in expected_invoke})

    expected_resolution = {
        form.name
        for form in DEPENDENCY_FORMS
        if "resolution" in form.required_families
    }
    provisioned = frozenset().union(
        *(provisioning.supported_forms for provisioning in DEPENDENCY_PROVISIONINGS)
    )
    assert expected_resolution <= provisioned


def test_dependency_shape_family_support_is_explicit() -> None:
    assert {
        shape.name: shape.required_families for shape in DEPENDENCY_SHAPES
    } == {
        "identity": frozenset(
            {
                "constructor_argument_conversion",
                "constructor_detection",
                "resolution",
                "invoke",
            }
        ),
        "pointer": frozenset(
            {
                "constructor_argument_conversion",
                "constructor_detection",
                "resolution",
                "invoke",
            }
        ),
        "const_pointer": frozenset(
            {
                "constructor_argument_conversion",
                "constructor_detection",
                "resolution",
                "invoke",
            }
        ),
        "shared_pointer": frozenset(
            {"constructor_detection", "resolution", "invoke"}
        ),
        "unique_pointer": frozenset(
            {"constructor_detection", "resolution", "invoke"}
        ),
        "optional": frozenset(
            {"constructor_detection", "resolution", "invoke"}
        ),
        "array": frozenset(
            {"constructor_detection", "resolution", "invoke"}
        ),
        "variant": frozenset(
            {"constructor_detection", "resolution", "invoke"}
        ),
    }
    assert {
        form.shape
        for form in DEPENDENCY_FORMS
        if "constructor_detection" in form.required_families
    } == {
        "identity",
        "pointer",
        "const_pointer",
        "shared_pointer",
        "unique_pointer",
        "optional",
        "array",
        "variant",
    }


def test_only_array_invocation_requires_an_explicit_callable_signature() -> None:
    policies = {
        next(iter(callable_spec.dependency_forms)): callable_spec.policy
        for callable_spec in PLAIN_DEPENDENCY_CALLABLES
    }
    assert "dependency_invoke_explicit" in policies["array_lvalue_reference"]
    assert all(
        "dependency_invoke_explicit" not in policy
        for form, policy in policies.items()
        if form != "array_lvalue_reference"
    )


def test_constructor_detection_modes_select_expected_argument_metadata() -> None:
    rows = generate_constructor_detection_rows()
    two_values = {
        row.mode.name: row.expected_arguments
        for row in rows
        if row.backend.name == "portable"
        and row.constructor_shape.name == "two_values"
    }
    assert two_values == {
        "shape": "void",
        "signature": "dingo::type_list<int, float>",
    }


def test_constructor_detection_rejects_duplicate_local_axis_members() -> None:
    backend = CONSTRUCTOR_DETECTION_BACKENDS[0]
    with pytest.raises(ValueError, match="backend.*duplicate"):
        generate_constructor_detection_rows(backends=(backend, backend))


def test_constructor_detection_rejects_invalid_limitation_mode() -> None:
    shape = replace(
        CONSTRUCTOR_SHAPES[0],
        constructor_detection_limitations=(
            ConstructorDetectionLimitation(None, "missing", "not supported"),
        ),
    )
    with pytest.raises(ValueError, match="unknown limitation modes"):
        generate_constructor_detection_rows(constructor_shapes=(shape,))


def test_constructor_detection_rejects_invalid_limitation_backend() -> None:
    shape = replace(
        CONSTRUCTOR_SHAPES[0],
        constructor_detection_limitations=(
            ConstructorDetectionLimitation(
                "missing", "signature", "not supported"
            ),
        ),
    )
    with pytest.raises(ValueError, match="unknown limitation backends"):
        generate_constructor_detection_rows(constructor_shapes=(shape,))


def test_constructor_detection_requires_a_limitation_reason() -> None:
    shape = replace(
        CONSTRUCTOR_SHAPES[0],
        constructor_detection_limitations=(
            ConstructorDetectionLimitation(None, "signature", ""),
        ),
    )
    with pytest.raises(ValueError, match="without a reason"):
        generate_constructor_detection_rows(constructor_shapes=(shape,))


def test_constructor_detection_requires_a_nonempty_limitation_guard() -> None:
    shape = replace(
        CONSTRUCTOR_SHAPES[0],
        constructor_detection_limitations=(
            ConstructorDetectionLimitation(
                None, "signature", "not supported", guard=""
            ),
        ),
    )
    with pytest.raises(ValueError, match="empty guard"):
        generate_constructor_detection_rows(constructor_shapes=(shape,))


def test_constructor_detection_rejects_overlapping_limitations() -> None:
    shape = replace(
        CONSTRUCTOR_SHAPES[0],
        constructor_detection_limitations=(
            ConstructorDetectionLimitation(None, "signature", "all"),
            ConstructorDetectionLimitation(
                "portable", "signature", "portable"
            ),
        ),
    )
    with pytest.raises(ValueError, match="overlapping limitations"):
        generate_constructor_detection_rows(constructor_shapes=(shape,))


def test_constructor_argument_conversion_uses_independent_axes() -> None:
    rows = generate_constructor_argument_conversion_rows()
    assert len(rows) == 10
    assert {row.storage for row in rows} == set(CONSTRUCTOR_ARGUMENT_STORAGES)
    assert {row.category for row in rows} == set(CONSTRUCTOR_ARGUMENT_CATEGORIES)
    assert {
        category.dependency_form for category in CONSTRUCTOR_ARGUMENT_CATEGORIES
    } == {
        "value",
        "lvalue_reference",
        "const_lvalue_reference",
        "rvalue_reference",
        "pointer",
        "const_pointer",
    }
    assert {row.name for row in rows} == {
        "unique_arg_value",
        "unique_arg_lvalue_reference",
        "unique_arg_const_lvalue_reference",
        "unique_arg_rvalue_reference",
        "unique_arg_pointer",
        "unique_arg_const_pointer",
        "shared_arg_value",
        "shared_arg_lvalue_reference",
        "shared_arg_const_lvalue_reference",
        "shared_arg_rvalue_reference",
    }


def test_invoke_uses_independent_callable_and_provisioning_axes(
    invoke_rows: tuple[InvokeRow, ...],
) -> None:
    expected_count = sum(
        1
        for callable_spec in INVOKE_CALLABLES
        for provisioning in INVOKE_PROVISIONINGS
        if provisioning.name in callable_spec.supported_provisionings
        for mode in INVOKE_MODES
        if mode.name in provisioning.supported_modes
        for container in INVOKE_CONTAINERS
        if mode.name in container.modes
        and (
            callable_spec.supported_containers is None
            or container.name in callable_spec.supported_containers
        )
    )
    assert len(invoke_rows) == expected_count
    assert len({row.name for row in invoke_rows}) == len(invoke_rows)
    assert {row.callable for row in invoke_rows} == set(INVOKE_CALLABLES)
    assert {row.container for row in invoke_rows} == set(INVOKE_CONTAINERS)

    for callable_spec in INVOKE_CALLABLES:
        assert {
            row.provisioning.name
            for row in invoke_rows
            if row.callable == callable_spec
        } == callable_spec.supported_provisionings

    for callable_spec in ANNOTATED_DEPENDENCY_CALLABLES:
        assert {
            row.container.name
            for row in invoke_rows
            if row.callable == callable_spec
        } == callable_spec.supported_containers


def test_invoke_axis_rejects_duplicate_members() -> None:
    provisioning = INVOKE_PROVISIONINGS[0]
    with pytest.raises(ValueError, match="provisioning.*duplicate"):
        generate_invoke_rows(provisionings=(provisioning, provisioning))


def test_invoke_axis_rejects_unknown_callable_container() -> None:
    invalid = replace(
        INVOKE_CALLABLES[0],
        supported_containers=frozenset({"missing_container"}),
    )
    with pytest.raises(ValueError, match="unknown containers"):
        generate_invoke_rows(callables=(invalid,))


@pytest.mark.parametrize(
    ("axis", "kwargs"),
    (
        (
            "provisioning",
            {
                "provisionings": (
                    *INVOKE_PROVISIONINGS,
                    replace(
                        INVOKE_PROVISIONINGS[0],
                        name="unused_provisioning",
                        supported_modes=frozenset(),
                    ),
                )
            },
        ),
        (
            "mode",
            {
                "modes": (
                    *INVOKE_MODES,
                    replace(INVOKE_MODES[0], name="unused_mode"),
                )
            },
        ),
        (
            "container",
            {
                "containers": (
                    *INVOKE_CONTAINERS,
                    replace(
                        INVOKE_CONTAINERS[0],
                        name="unused_container",
                        modes=frozenset(),
                    ),
                )
            },
        ),
    ),
)
def test_invoke_rejects_unused_axis_members(
    axis: str, kwargs: dict[str, tuple]
) -> None:
    with pytest.raises(RuntimeError, match=rf"{axis}.*unused"):
        generate_invoke_rows(**kwargs)


def test_matrix_families_preserve_total_registration_behavior(
    rows: tuple[MatrixRow, ...],
    scenario_rows: tuple[ScenarioRow, ...],
    invoke_rows: tuple[InvokeRow, ...],
    shared_cyclical_rows: tuple[SharedCyclicalRow, ...],
) -> None:
    assert (
        len(rows)
        + len(scenario_rows)
        + len(invoke_rows)
        + len(shared_cyclical_rows)
        == 4876
    )


def test_dependency_contract_covers_required_families() -> None:
    validate_dependency_contract()

    detector_only = {
        shape.name for shape in CONSTRUCTOR_SHAPES if shape.detector_only
    }
    assert detector_only == {
        "ambiguous",
        "defaulted",
        "generic",
        "initializer_list",
        "nested_forwarding_wrapper",
        "same_arity_overload",
        "unconstrained_forwarding_wrapper",
    } | {composition.name for composition in DEPENDENCY_COMPOSITIONS}


def test_dependency_contract_rejects_missing_family_coverage() -> None:
    missing_form = "annotated_pointer"
    incomplete_shapes = tuple(
        shape
        for shape in CONSTRUCTOR_SHAPES
        if missing_form not in shape.dependency_forms
    )
    with pytest.raises(
        RuntimeError,
        match="annotated_pointer / constructor_detection",
    ):
        validate_dependency_contract(constructor_shapes=incomplete_shapes)


def test_dependency_contract_rejects_missing_product_cell() -> None:
    with pytest.raises(
        ValueError, match="shape x carrier x decoration product"
    ):
        validate_dependency_contract(forms=DEPENDENCY_FORMS[:-1])


def test_dependency_contract_rejects_duplicate_product_cell() -> None:
    duplicate = replace(DEPENDENCY_FORMS[0], name="duplicate_value")
    with pytest.raises(
        ValueError, match="duplicate shape/carrier/decoration"
    ):
        validate_dependency_contract(forms=(*DEPENDENCY_FORMS, duplicate))


def test_dependency_contract_rejects_incompatible_invoke_provisioning() -> None:
    incompatible_callable = replace(
        INVOKE_CALLABLES[0],
        supported_provisionings=frozenset({"unique_value"}),
    )
    with pytest.raises(ValueError, match="unsupported forms.*unique_value"):
        validate_dependency_contract(
            invoke_callables=(incompatible_callable, *INVOKE_CALLABLES[1:])
        )


def test_dependency_contract_rejects_unknown_behavior_feature() -> None:
    provisioning = DEPENDENCY_PROVISIONINGS[0]
    form, resolved_type, _ = provisioning.resolution_cases[0]
    invalid_provisioning = replace(
        provisioning,
        resolution_cases=(
            (form, resolved_type, "missing_feature"),
            *provisioning.resolution_cases[1:],
        ),
    )
    with pytest.raises(ValueError, match="unknown behavior feature"):
        validate_dependency_contract(
            provisionings=(
                invalid_provisioning,
                *DEPENDENCY_PROVISIONINGS[1:],
            )
        )


def test_dependency_contract_rejects_mismatched_resolution_case() -> None:
    provisioning = DEPENDENCY_PROVISIONINGS[0]
    invalid_provisioning = replace(
        provisioning,
        resolution_cases=(
            provisioning.resolution_cases[0],
            ("pointer", "value", "resolve_concrete"),
            *provisioning.resolution_cases[2:],
        ),
    )
    with pytest.raises(ValueError, match="does not declare that form"):
        validate_dependency_contract(
            provisionings=(
                invalid_provisioning,
                *DEPENDENCY_PROVISIONINGS[1:],
            )
        )


def test_dependency_provisionings_are_resolvable_registration_rows(
    rows: tuple[MatrixRow, ...],
) -> None:
    assert_dependency_provisioning_coverage(rows)
    assert {
        (
            feature,
            provisioning.scope,
            provisioning.stored_type,
            provisioning.exposed_type,
            resolved_type,
            form,
            mode,
            container.name,
        )
        for provisioning in DEPENDENCY_PROVISIONINGS
        for form, resolved_type, feature in provisioning.resolution_cases
        for mode in provisioning.supported_modes
        for container in implemented(CONTAINERS)
        if mode in container.modes
    } <= {
        (
            row.feature.name,
            row.scope.name,
            row.stored_type.id,
            row.exposed_type.name,
            row.resolved_type.name,
            form,
            row.mode.name,
            row.container.name,
        )
        for row in rows
        for form in row.resolved_type.dependency_forms
    }


def test_dependency_provisioning_rejects_missing_form_mode_pair(
    rows: tuple[MatrixRow, ...],
) -> None:
    incomplete_rows = tuple(
        row
        for row in rows
        if not (
            row.scope.name == "shared"
            and row.stored_type.id == "value_type"
            and row.exposed_type.name == "concrete"
            and row.mode.name == "static"
            and "const_lvalue_reference" in row.resolved_type.dependency_forms
        )
    )
    with pytest.raises(
        RuntimeError, match="form/resolution/mode/container"
    ):
        assert_dependency_provisioning_coverage(incomplete_rows)


def test_dependency_provisioning_rejects_rows_from_unrelated_features(
    rows: tuple[MatrixRow, ...],
) -> None:
    incomplete_rows = tuple(
        row
        for row in rows
        if not (
            row.feature.name == "resolve_concrete"
            and row.scope.name == "external"
        )
    )
    with pytest.raises(RuntimeError, match="dependency provisionings"):
        assert_dependency_provisioning_coverage(incomplete_rows)


@pytest.mark.parametrize(
    ("scope", "exposed_type"),
    (
        ("external", "annotated_concrete"),
        ("external", "keyed_concrete"),
    ),
)
def test_dependency_provisioning_rejects_missing_external_shapes(
    rows: tuple[MatrixRow, ...], scope: str, exposed_type: str
) -> None:
    incomplete_rows = tuple(
        row
        for row in rows
        if not (
            row.scope.name == scope
            and row.exposed_type.name == exposed_type
        )
    )
    with pytest.raises(RuntimeError, match="dependency provisionings"):
        assert_dependency_provisioning_coverage(incomplete_rows)


def test_dependency_provisioning_rejects_missing_container_cell(
    rows: tuple[MatrixRow, ...],
) -> None:
    incomplete_rows = tuple(
        row
        for row in rows
        if not (
            row.feature.name == "resolve_concrete"
            and row.scope.name == "shared"
            and row.stored_type.id == "value_type"
            and row.exposed_type.name == "concrete"
            and row.resolved_type.name == "value_ref_ptr"
            and row.mode.name == "static"
            and row.container.name == "static_container"
        )
    )
    with pytest.raises(RuntimeError, match="dependency provisionings"):
        assert_dependency_provisioning_coverage(incomplete_rows)


def test_family_renderer_assembles_source_and_runner_pairs(
    tmp_path: Path,
) -> None:
    executables = render_family_executables(
        tmp_path,
        Template("source {{ value }}"),
        Template("runner {{ value }}"),
        (
            SourceShard(
                executable="second",
                name="beta",
                source_context={"value": 2},
                runner_context={"value": 2},
            ),
            SourceShard(
                executable="first",
                name="alpha",
                source_context={"value": 1},
                runner_context={"value": 1},
            ),
        ),
    )

    assert tuple(executable.name for executable in executables) == (
        "first",
        "second",
    )
    assert (tmp_path / "alpha.cpp").read_text(encoding="utf-8") == "source 1"
    assert (
        tmp_path / "matrix_runner_alpha.cpp"
    ).read_text(encoding="utf-8") == "runner 1"


def test_case_family_renderer_batches_runners_by_executable(
    tmp_path: Path,
) -> None:
    executables = render_case_family_executables(
        tmp_path,
        Template("source {{ value }}"),
        Template("runner {{ cases | join(',') }}"),
        (
            SourceShard("test", "beta", {"value": 2}, {"cases": (3, 4)}),
            SourceShard("test", "alpha", {"value": 1}, {"cases": (1, 2)}),
        ),
        runner_case_limit=3,
    )

    assert len(executables) == 1
    assert tuple(source.name for source in executables[0].sources) == (
        "beta.cpp",
        "alpha.cpp",
        "matrix_runner_alpha_batch_1.cpp",
        "matrix_runner_alpha_batch_2.cpp",
    )
    assert (
        tmp_path / "matrix_runner_alpha_batch_1.cpp"
    ).read_text(encoding="utf-8") == "runner 1,2,3"
    assert (
        tmp_path / "matrix_runner_alpha_batch_2.cpp"
    ).read_text(encoding="utf-8") == "runner 4"


def test_family_renderer_rejects_duplicate_source_shards(tmp_path: Path) -> None:
    shard = SourceShard(
        executable="test",
        name="duplicate",
        source_context={},
        runner_context={},
    )
    with pytest.raises(ValueError, match="source shard.*duplicate"):
        render_family_executables(
            tmp_path,
            Template("source"),
            Template("runner"),
            (shard, shard),
        )


def test_family_renderer_rejects_sources_claimed_by_another_family(
    tmp_path: Path,
) -> None:
    claimed_sources: set[Path] = set()
    first = SourceShard("first", "shared", {}, {})
    second = SourceShard("second", "shared", {}, {})
    render_family_executables(
        tmp_path,
        Template("first source"),
        Template("first runner"),
        (first,),
        claimed_sources,
    )

    with pytest.raises(RuntimeError, match="duplicate sources.*shared.cpp"):
        render_family_executables(
            tmp_path,
            Template("second source"),
            Template("second runner"),
            (second,),
            claimed_sources,
        )
    assert (tmp_path / "shared.cpp").read_text(encoding="utf-8") == "first source"


def test_executable_merge_rejects_cross_family_source_collisions(
    tmp_path: Path,
) -> None:
    shared_source = tmp_path / "shared.cpp"
    with pytest.raises(RuntimeError, match="duplicate sources.*shared.cpp"):
        merge_executables(
            (GeneratedExecutable("first", (shared_source,)),),
            (GeneratedExecutable("second", (shared_source,)),),
        )


def test_executable_merge_preserves_isolated_sources(tmp_path: Path) -> None:
    regular_source = tmp_path / "regular.cpp"
    isolated_source = tmp_path / "isolated.cpp"

    assert merge_executables(
        (GeneratedExecutable("test", (regular_source,)),),
        (
            GeneratedExecutable(
                "test", (isolated_source,), (isolated_source,)
            ),
        ),
    ) == (
        GeneratedExecutable(
            "test",
            (isolated_source, regular_source),
            (isolated_source,),
        ),
    )


def test_executable_merge_rejects_unknown_isolated_sources(
    tmp_path: Path,
) -> None:
    with pytest.raises(ValueError, match="isolates unknown sources"):
        merge_executables(
            (
                GeneratedExecutable(
                    "test",
                    (tmp_path / "regular.cpp",),
                    (tmp_path / "unknown.cpp",),
                ),
            )
        )


def test_every_policy_source_kind_is_selected(rows: tuple[MatrixRow, ...]) -> None:
    assert {row.lines.policy_source[0] for row in rows} == {
        "feature",
        "feature_case",
        "lifetime",
        "resolved_type",
    }


def test_filter_coverage_rejects_undeclared_reasons() -> None:
    rejected = Counter({reason: 1 for reason in RejectionReason})
    rejected["undeclared"] = 1

    with pytest.raises(RuntimeError, match="undeclared"):
        assert_filter_coverage(rejected)


def test_stored_type_coverage_uses_unique_ids(rows: tuple[MatrixRow, ...]) -> None:
    generated_ids = {row.stored_type.id for row in rows}
    expected_ids = {stored_type.id for stored_type in implemented(STORED_TYPES)}
    assert generated_ids == expected_ids

    value_row = next(row for row in rows if row.stored_type.id == "value_type")
    with pytest.raises(RuntimeError, match="unique_value_type"):
        assert_axis_used(
            (value_row,),
            "stored_type",
            {"value_type", "unique_value_type"},
            identity="id",
        )


def test_catalog_rejects_dangling_axis_references() -> None:
    invalid_feature_case = replace(
        CATALOG.feature_cases[0], feature="missing_feature"
    )
    invalid_catalog = replace(
        CATALOG,
        feature_cases=(invalid_feature_case, *CATALOG.feature_cases[1:]),
    )

    with pytest.raises(ValueError, match="unknown features.*missing_feature"):
        validate_catalog(invalid_catalog)


def test_catalog_rejects_duplicate_axis_identities() -> None:
    duplicate_stored_type = replace(
        CATALOG.stored_types[1], id=CATALOG.stored_types[0].id
    )
    invalid_catalog = replace(
        CATALOG,
        stored_types=(CATALOG.stored_types[0], duplicate_stored_type),
    )

    with pytest.raises(ValueError, match="duplicate identities.*value_type"):
        validate_catalog(invalid_catalog)


def test_registration_placement_is_explicit(rows: tuple[MatrixRow, ...]) -> None:
    row = rows[0]
    exposed_type = ExposedType(
        name="placement_test",
        kind=row.exposed_type.kind,
        supported_stored_kinds=frozenset({row.stored_type.kind}),
        provides=frozenset(),
        registrations=(
            RegistrationSpec(),
            RegistrationSpec(
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
            ),
            RegistrationSpec(
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
                include_in_static=False,
            ),
            RegistrationSpec(
                include_in_runtime=False,
                include_in_mixed=False,
            ),
        ),
    )

    plan = build_registration_plan(row.scope, row.stored_type, exposed_type)

    assert len(plan.static_bindings) == 3
    assert len(plan.runtime_setup) == 3
    assert plan.mixed_static_bindings is not None
    assert len(plan.mixed_static_bindings) == 1
    assert len(plan.mixed_runtime_setup) == 2


def test_registration_recipe_renders_every_mode(
    rows: tuple[MatrixRow, ...],
) -> None:
    scope = rows[0].scope
    plan = build_registration_plan_from_recipe(
        RegistrationRecipe(
            scope=scope,
            storage="arbitrary_storage",
            factory="arbitrary_factory",
            registrations=(
                RegistrationSpec(
                    include_in_runtime=False,
                    include_in_mixed=False,
                ),
                RegistrationSpec(
                    mixed_placement=MixedRegistrationPlacement.RUNTIME,
                    include_in_static=False,
                ),
                RegistrationSpec(
                    scope="anchor_scope",
                    storage="anchor_storage",
                    include_in_static=False,
                    include_in_runtime=False,
                ),
            ),
        )
    )

    static = render_registration_plan("static", plan, context="test recipe")
    runtime = render_registration_plan("runtime", plan, context="test recipe")
    mixed = render_registration_plan("mixed", plan, context="test recipe")

    assert static.static_bindings == (
        "dingo::bindings<dingo::bind<"
        f"{scope.type_name}, arbitrary_storage, arbitrary_factory>>"
    )
    assert static.setup_lines == ()
    runtime_registration = (
        "container.template register_type<"
        f"{scope.type_name}, arbitrary_storage, arbitrary_factory>();"
    )
    assert runtime.static_bindings is None
    assert runtime.setup_lines == (runtime_registration,)
    assert mixed.static_bindings == (
        "dingo::bindings<dingo::bind<anchor_scope, anchor_storage>>"
    )
    assert mixed.setup_lines == (runtime_registration,)


def test_registration_recipe_renders_external_runtime_storage(
    rows: tuple[MatrixRow, ...],
) -> None:
    scope = next(row.scope for row in rows if row.scope.name == "external")
    plan = build_registration_plan_from_recipe(
        RegistrationRecipe(
            scope=scope,
            storage="dingo::storage<arbitrary_type>",
            registrations=(
                RegistrationSpec(
                    storage="dingo::storage<arbitrary_type &>",
                    interfaces="dingo::interfaces<arbitrary_type>",
                    runtime_setup=("arbitrary_type external_value{};",),
                    runtime_argument="external_value",
                    include_in_static=False,
                    include_in_mixed=False,
                ),
            ),
        )
    )

    runtime = render_registration_plan("runtime", plan, context="external recipe")

    assert runtime.static_bindings is None
    assert runtime.setup_lines == (
        "arbitrary_type external_value{};",
        "container.template register_type<dingo::scope<dingo::external>, "
        "dingo::storage<arbitrary_type &>, "
        "dingo::interfaces<arbitrary_type>>(external_value);",
    )


@pytest.mark.parametrize(
    ("argument", "expected"),
    (
        (None, "container.template register_type<scope, storage>();"),
        ("value", "container.template register_type<scope, storage>(value);"),
    ),
)
def test_registration_arguments_are_rendered_without_string_slicing(
    argument: str | None, expected: str
) -> None:
    assert register_type("scope", "storage", argument=argument) == expected


def test_invalid_registration_plan_is_an_error(
    rows: tuple[MatrixRow, ...],
) -> None:
    static_row = next(row for row in rows if row.mode.name == "static")
    invalid_candidate = replace(
        static_row,
        exposed_type=replace(static_row.exposed_type, registrations=()),
    )

    with pytest.raises(RuntimeError, match="has no bindings"):
        materialize_row(invalid_candidate, {}, Counter[RejectionReason]())


@pytest.mark.parametrize(
    (
        "dependency_composition_case_limit",
        "dependency_composition_isolated_executables",
        "dependency_composition_disabled_projected_cases",
        "expected_implementation_sources",
        "expected_isolated_sources",
        "expected_compiled_rows",
    ),
    (
        (None, frozenset(), frozenset(), 113, 0, 608),
        (
            DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT,
            frozenset(),
            frozenset(),
            161,
            0,
            608,
        ),
        (
            DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT,
            frozenset({("invoke", 4), ("resolve", 4)}),
            frozenset(),
            299,
            152,
            608,
        ),
        (
            DEPENDENCY_COMPOSITION_IMPLEMENTATION_CASE_LIMIT,
            frozenset(),
            frozenset(
                (operation, row_name)
                for operation in ("invoke", "resolve")
                for row_name in (
                    "runtime_container_unique_value_"
                    "array_shared_pointer_copy_only",
                )
            ),
            161,
            0,
            606,
        ),
    ),
)
def test_generation_removes_stale_outputs_and_preserves_unchanged_files(
    tmp_path: Path,
    composition_rows: tuple[DependencyCompositionRow, ...],
    dependency_composition_case_limit: int | None,
    dependency_composition_isolated_executables: frozenset[tuple[str, int]],
    dependency_composition_disabled_projected_cases: frozenset[tuple[str, str]],
    expected_implementation_sources: int,
    expected_isolated_sources: int,
    expected_compiled_rows: int,
) -> None:
    out_dir = tmp_path / "matrix"
    out_dir.mkdir()
    stale_source = out_dir / "stale.cpp"
    stale_source.write_text("stale", encoding="utf-8")
    stale_report = out_dir / "stale-coverage.md"
    stale_report.write_text("stale", encoding="utf-8")
    cmake_file = tmp_path / "matrix-tests.cmake"

    generate(
        out_dir,
        cmake_file,
        dependency_composition_case_limit=dependency_composition_case_limit,
        dependency_composition_isolated_executables=(
            dependency_composition_isolated_executables
        ),
        dependency_composition_disabled_projected_cases=(
            dependency_composition_disabled_projected_cases
        ),
    )

    assert not stale_source.exists()
    assert not stale_report.exists()
    implementation_sources = tuple(
        source
        for source in out_dir.glob("*.cpp")
        if not source.name.startswith("matrix_runner_")
    )
    runner_sources = tuple(out_dir.glob("matrix_runner_*.cpp"))
    assert len(implementation_sources) == expected_implementation_sources
    assert len(runner_sources) == 55
    coverage_report = out_dir / DEPENDENCY_COMPOSITION_COVERAGE_REPORT
    assert coverage_report.read_text(encoding="utf-8") == (
        render_dependency_composition_coverage(
            build_dependency_composition_coverage(composition_rows),
            profile="full",
            compiled_rows=expected_compiled_rows,
        )
    )
    dependency_runners = tuple(
        out_dir.glob("matrix_runner_dependency_composition_*.cpp")
    )
    assert len(dependency_runners) == 8
    assert sum(
        source.read_text(encoding="utf-8").count("\nTEST(")
        for source in dependency_runners
    ) == expected_compiled_rows
    for backend in ("portable", "msvc"):
        shape_source = (
            out_dir / f"constructor_detection_{backend}_shape.cpp"
        ).read_text(encoding="utf-8")
        shape_runner = (
            out_dir
            / f"matrix_runner_constructor_detection_{backend}_shape.cpp"
        ).read_text(encoding="utf-8")
        assert "#if !(defined(__clang__) || defined(_MSC_VER))" in (
            shape_source
        )
        assert "#if defined(__clang__) || defined(_MSC_VER)" in shape_runner
    assert cmake_file.is_file()
    cmake_content = cmake_file.read_text(encoding="utf-8")
    isolated_source_count = 0
    in_isolated_sources = False
    for line in cmake_content.splitlines():
        if line.startswith("set(") and line.endswith("_ISOLATED_SOURCES"):
            in_isolated_sources = True
        elif in_isolated_sources and line == ")":
            in_isolated_sources = False
        elif in_isolated_sources and line.strip().endswith('.cpp"'):
            isolated_source_count += 1
    assert isolated_source_count == expected_isolated_sources
    for operation in ("invoke", "resolve"):
        sources_marker = (
            "set(dingo_matrix_test_dependency_composition_"
            f"{operation}_4_SOURCES\n"
        )
        sources_start = cmake_content.index(sources_marker)
        sources_end = cmake_content.index("\n)\n", sources_start)
        sources_block = cmake_content[sources_start:sources_end]
        if (operation, 4) in dependency_composition_isolated_executables:
            expected_source_count = 1
        elif dependency_composition_case_limit is None:
            expected_source_count = 2
        else:
            expected_source_count = 8
        assert sources_block.count('.cpp"') == expected_source_count
    timestamps = {
        path: path.stat().st_mtime_ns
        for path in (*out_dir.glob("*.cpp"), coverage_report, cmake_file)
    }

    generate(
        out_dir,
        cmake_file,
        dependency_composition_case_limit=dependency_composition_case_limit,
        dependency_composition_isolated_executables=(
            dependency_composition_isolated_executables
        ),
        dependency_composition_disabled_projected_cases=(
            dependency_composition_disabled_projected_cases
        ),
    )

    assert {path: path.stat().st_mtime_ns for path in timestamps} == timestamps
