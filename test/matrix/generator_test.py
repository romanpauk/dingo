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
from jinja2 import Template

from data import CATALOG, CONTAINERS, STORED_TYPES, validate_catalog
from axes.constructor_detection import (
    CONSTRUCTOR_ARGUMENT_CATEGORIES,
    CONSTRUCTOR_ARGUMENT_STORAGES,
    CONSTRUCTOR_DETECTION_BACKENDS,
    CONSTRUCTOR_DETECTION_MODES,
    CONSTRUCTOR_SHAPES,
    DEPENDENCY_CONSTRUCTOR_SHAPES,
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
from constructor_detection import (
    generate_constructor_argument_conversion_rows,
    generate_constructor_detection_rows,
)
from dependency_contract import validate_dependency_contract
from family import SourceShard, render_family_executables
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
from plugins import build_registration_plan, register_type
from scenarios import ScenarioRow, generate_scenario_rows
from schema import (
    ConstructorDetectionLimitation,
    ExposedType,
    GeneratedExecutable,
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


def test_matrix_cardinality_remains_bounded(rows: tuple[MatrixRow, ...]) -> None:
    assert len(rows) == 1932
    assert len({(row.feature.name, row.name) for row in rows}) == len(rows)


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


def test_constructor_signature_recovery_limitations_are_explicit() -> None:
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
        "nested_optional_shared_pointer",
        "nested_variant_optional",
        "optional_copy_only",
        "optional_move_only",
        "optional_unique_pointer",
        "variant_move_copy_only",
        "variant_shared_unique_pointers",
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
    )
    assert all(row.unsupported_reason for row in rows if not row.supported)
    assert {
        row.constructor_shape.name
        for row in rows
        if row.mode.name == "shape" and not row.supported
    } == {"unconstrained_forwarding_wrapper"}


def test_dependency_shapes_own_constructor_detection_limitations() -> None:
    limitations = {
        shape.name: tuple(shape.constructor_detection_limitations)
        for shape in DEPENDENCY_SHAPES
        if shape.constructor_detection_limitations
    }
    assert set(limitations) == {"optional", "variant"}
    for capabilities in limitations.values():
        assert len(capabilities) == 1
        capability = capabilities[0]
        assert capability.carriers == frozenset({"value"})
        assert capability.decorations == frozenset({"plain"})
        assert capability.limitation.backend is None
        assert capability.limitation.mode == "signature"

    constructor_limitations = {
        next(iter(shape.dependency_forms)): shape.constructor_detection_limitations
        for shape in DEPENDENCY_CONSTRUCTOR_SHAPES
        if shape.constructor_detection_limitations
    }
    assert set(constructor_limitations) == {"optional", "variant"}
    assert {
        form: limitations[form][0].limitation
        for form in constructor_limitations
    } == {
        form: constructor_limitations[form][0]
        for form in constructor_limitations
    }


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
) -> None:
    assert len(rows) + len(scenario_rows) + len(invoke_rows) == 3991


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
        "nested_optional_shared_pointer",
        "nested_variant_optional",
        "optional_copy_only",
        "optional_move_only",
        "optional_unique_pointer",
        "same_arity_overload",
        "unconstrained_forwarding_wrapper",
        "variant_move_copy_only",
        "variant_shared_unique_pointers",
    }


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
        ),
    )

    plan = build_registration_plan(row.scope, row.stored_type, exposed_type)

    assert len(plan.static_bindings) == 2
    assert plan.mixed_static_bindings is not None
    assert len(plan.mixed_static_bindings) == 1
    assert len(plan.mixed_runtime_setup) == 2


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


def test_generation_removes_stale_sources_and_preserves_unchanged_files(
    tmp_path: Path,
) -> None:
    out_dir = tmp_path / "matrix"
    out_dir.mkdir()
    stale_source = out_dir / "stale.cpp"
    stale_source.write_text("stale", encoding="utf-8")
    cmake_file = tmp_path / "matrix-tests.cmake"

    generate(out_dir, cmake_file)

    assert not stale_source.exists()
    assert len(tuple(out_dir.glob("*.cpp"))) == 192
    assert cmake_file.is_file()
    timestamps = {
        path: path.stat().st_mtime_ns
        for path in (*out_dir.glob("*.cpp"), cmake_file)
    }

    generate(out_dir, cmake_file)

    assert {path: path.stat().st_mtime_ns for path in timestamps} == timestamps
