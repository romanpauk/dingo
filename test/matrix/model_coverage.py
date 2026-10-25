#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Matrix rows whose behavior the model-generated suites now own.

A restriction keeps the feature in the catalog so its policy sources and
dependency provisionings still validate, but stops generating the
scope/resolved-type rows that a behavior class already executes. The covered
cells are checked against the model's behavior product by the generator test,
so a restriction cannot outlive the test that justifies it.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class FeatureRestriction:
    feature: str
    # (scope, resolved type) cells that the model now executes.
    drop: frozenset[tuple[str, str]]
    covered_cells: frozenset[tuple[str, str]]
    reason: str
    # Interface behavior cases that justify dropping interface rows.
    interface_cases: frozenset[str] = frozenset()


FEATURE_RESTRICTIONS = (
    FeatureRestriction(
        feature="lifetime_counts",
        drop=frozenset(
            {
                ("shared", "value_ref_ptr"),
                ("shared", "value_shared_ptr"),
                ("shared", "value_optional_ref"),
                ("unique", "value_unique_ptr"),
                ("unique", "value_optional"),
                ("unique", "value_rvalue"),
            }
        ),
        covered_cells=frozenset(
            {
                ("shared", "plain"),
                ("unique", "plain"),
                ("shared", "optional"),
                ("unique", "optional"),
                ("shared", "shared_handle"),
                ("unique", "shared_handle"),
                ("unique", "unique_handle"),
            }
        ),
        reason=(
            "behavior classes assert construction, copy, move, destruction and "
            "the lifetime balance for these cells, including the unique rvalue "
            "route"
        ),
    ),
    FeatureRestriction(
        feature="variant",
        drop=frozenset(
            {
                ("shared", "variant_value"),
                ("unique", "variant_value"),
            }
        ),
        covered_cells=frozenset(
            {
                ("shared", "alternative"),
                ("unique", "alternative"),
            }
        ),
        reason=(
            "behavior classes resolve both the variant wrapper value and the "
            "alternative object; the alternative reference and pointer routes "
            "keep their matrix rows"
        ),
    ),
    FeatureRestriction(
        feature="resolve_interface",
        drop=frozenset(
            {
                ("shared", "interface_ref"),
                ("shared", "interface_ptr"),
                ("shared", "interface_ref_ptr_shared_ptr"),
                ("shared", "multiple_interface_shared_ptr_ref"),
                ("shared", "interface_value"),
                ("shared", "second_interface_ref"),
            }
        ),
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {
                "shared_ref",
                "shared_ptr",
                "shared_handle",
                "shared_plain_ref",
                "shared_second_handle",
                "shared_second_ref",
                "shared_copyable_value",
            }
        ),
        reason=(
            "behavior and interface route cases cover the shared reference, "
            "pointer, shared-handle, plain-value, copyable-value, and "
            "multiple-interface shapes"
        ),
    ),
    FeatureRestriction(
        feature="resolve_unique_wrapper",
        drop=frozenset({("unique", "unique_interface_unique_ptr")}),
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {
                "unique_handle",
                "unique_shared_handle",
            }
        ),
        reason=(
            "behavior interface cases cover the unique interface unique_ptr and "
            "shared_ptr owning handles"
        ),
    ),
    FeatureRestriction(
        feature="annotated",
        drop=frozenset(
            {
                ("shared", "annotated_value_ref"),
                ("external", "annotated_value_ref"),
                ("shared", "annotated_interface_ref"),
                ("shared", "annotated_interface_ptr"),
                ("shared", "annotated_interface_shared_ptr"),
                ("shared", "annotated_interface_shared_ptr_ref"),
            }
        ),
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {
                "shared_annotated_ref",
                "shared_annotated_value",
                "shared_annotated_shared_ptr_ref",
                "shared_annotated_optional_ref",
                "shared_annotated_pointer_ref",
                "shared_annotated_interface_ref",
                "shared_annotated_interface_ptr",
                "shared_annotated_interface_handle",
                "shared_annotated_interface_handle_ref",
                "external_annotated_value",
                "external_annotated_ref",
                "external_annotated_ptr",
                "external_annotated_shared_ptr",
            }
        ),
        reason=(
            "annotation recipes cover shared annotated concrete values, "
            "annotated interfaces, and external annotated registrations"
        ),
    ),
)

# (scope, resolved type) provisioning cells whose expected matrix rows are
# model-covered. Other scopes of the same resolved type are unaffected.
MODEL_COVERED_PROVISIONING_CELLS = frozenset(
    {
        ("shared", "annotated_value_ref"),
        ("external", "annotated_value_ref"),
        ("shared", "annotated_interface_ref"),
        ("shared", "annotated_interface_ptr"),
        ("shared", "annotated_interface_shared_ptr"),
        ("shared", "annotated_interface_shared_ptr_ref"),
    }
)

RESTRICTIONS_BY_FEATURE = {
    restriction.feature: restriction for restriction in FEATURE_RESTRICTIONS
}

# Exposed-type axis members whose matrix rows are entirely model-covered. The
# axis stays declared so the catalog keeps its identity, but the coverage check
# no longer requires a matrix row.
MODEL_COVERED_EXPOSED_TYPES = frozenset(
    {
        "annotated_concrete",
        "annotated_interface",
        "copyable_interface_type",
        "multiple_interfaces",
    }
)

# Features whose supported rows are now entirely owned by the model, so the
# matrix no longer generates a case for them.
FULLY_MODEL_COVERED_FEATURES = frozenset(
    {
        "annotated",
        "lifetime_counts",
        "resolve_interface",
        "resolve_unique_wrapper",
    }
)


@dataclass(frozen=True, slots=True)
class ResolvedTypeCoverage:
    resolved_type: str
    covered_cells: frozenset[tuple[str, str]]
    reason: str
    interface_cases: frozenset[str] = frozenset()


# A resolved type that only the restricted rows used would otherwise fail the
# "every axis member is used" check. These are covered by behavior classes
# instead of a matrix row.
MODEL_COVERED_RESOLVED_TYPES = (
    ResolvedTypeCoverage(
        resolved_type="variant_value",
        covered_cells=frozenset(
            {
                ("shared", "alternative"),
                ("unique", "alternative"),
            }
        ),
        reason="behavior classes resolve the variant wrapper value directly",
    ),
    ResolvedTypeCoverage(
        resolved_type="value_rvalue",
        covered_cells=frozenset({("unique", "plain")}),
        reason="the unique plain behavior class resolves and moves the rvalue route",
    ),
    ResolvedTypeCoverage(
        resolved_type="interface_ref",
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {"shared_ref", "shared_ptr", "shared_plain_ref"}
        ),
        reason="the interface behavior cases resolve interface references",
    ),
    ResolvedTypeCoverage(
        resolved_type="interface_ptr",
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {"shared_ref", "shared_ptr", "shared_plain_ref"}
        ),
        reason="the interface behavior cases resolve interface pointers",
    ),
    ResolvedTypeCoverage(
        resolved_type="interface_ref_ptr_shared_ptr",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_handle"}),
        reason="the shared interface handle behavior case covers the shared_ptr route",
    ),
    ResolvedTypeCoverage(
        resolved_type="multiple_interface_shared_ptr_ref",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_second_handle"}),
        reason="the multiple-interface shared handle behavior case covers the wrapper",
    ),
    ResolvedTypeCoverage(
        resolved_type="interface_value",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_copyable_value"}),
        reason="the copyable interface value behavior case covers the sliced value",
    ),
    ResolvedTypeCoverage(
        resolved_type="second_interface_ref",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_second_ref"}),
        reason="the multiple-interface behavior case resolves the second interface",
    ),
    ResolvedTypeCoverage(
        resolved_type="annotated_value_ref",
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {
                "shared_annotated_ref",
                "shared_annotated_value",
                "shared_annotated_shared_ptr_ref",
                "shared_annotated_optional_ref",
                "shared_annotated_pointer_ref",
                "external_annotated_value",
                "external_annotated_ref",
                "external_annotated_ptr",
                "external_annotated_shared_ptr",
            }
        ),
        reason="annotation recipes cover the annotated concrete reference requests",
    ),
    ResolvedTypeCoverage(
        resolved_type="annotated_interface_ref",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_annotated_interface_ref"}),
        reason="the annotated interface behavior case resolves the reference",
    ),
    ResolvedTypeCoverage(
        resolved_type="annotated_interface_ptr",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_annotated_interface_ptr"}),
        reason="the annotated interface behavior case resolves the pointer",
    ),
    ResolvedTypeCoverage(
        resolved_type="annotated_interface_shared_ptr",
        covered_cells=frozenset(),
        interface_cases=frozenset({"shared_annotated_interface_handle"}),
        reason="the annotated interface behavior case resolves the shared handle",
    ),
    ResolvedTypeCoverage(
        resolved_type="annotated_interface_shared_ptr_ref",
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {"shared_annotated_interface_handle_ref"}
        ),
        reason="the annotated interface behavior case resolves the handle reference",
    ),
    ResolvedTypeCoverage(
        resolved_type="unique_interface_unique_ptr",
        covered_cells=frozenset(),
        interface_cases=frozenset(
            {"unique_handle", "unique_shared_handle"}
        ),
        reason="the unique interface handle behavior cases cover the owning handles",
    ),
)

MODEL_COVERED_RESOLVED_TYPE_NAMES = frozenset(
    coverage.resolved_type for coverage in MODEL_COVERED_RESOLVED_TYPES
)

# Policy sources that only the removed rows selected. They stay declared so the
# catalogs keep their identity, but the coverage check must not require a row.
MODEL_COVERED_POLICY_SOURCES = frozenset(
    {
        (
            "lifetime",
            "shared",
            "value_optional",
            "concrete",
            "value_optional_ref",
        ),
        (
            "lifetime",
            "shared",
            "value_shared_ptr",
            "concrete",
            "value_shared_ptr",
        ),
        ("lifetime", "shared", "value_type", "concrete", "value_ref_ptr"),
        ("lifetime", "unique", "value_optional", "concrete", "value_optional"),
        (
            "lifetime",
            "unique",
            "value_unique_ptr",
            "concrete",
            "value_unique_ptr",
        ),
        ("lifetime", "unique", "unique_value_type", "concrete", "value_rvalue"),
        ("resolved_type", "variant_value", "variant"),
        (
            "resolved_type",
            "annotated_value_ref",
            "annotated",
        ),
        (
            "resolved_type",
            "annotated_interface_ref",
            "annotated",
        ),
        (
            "resolved_type",
            "annotated_interface_ptr",
            "annotated",
        ),
        (
            "resolved_type",
            "annotated_interface_shared_ptr",
            "annotated",
        ),
        (
            "resolved_type",
            "annotated_interface_shared_ptr_ref",
            "annotated",
        ),
        ("resolved_type", "interface_ref", "resolve_interface"),
        ("resolved_type", "interface_ptr", "resolve_interface"),
        (
            "resolved_type",
            "interface_ref_ptr_shared_ptr",
            "resolve_interface",
        ),
        (
            "resolved_type",
            "unique_interface_unique_ptr",
            "resolve_unique_wrapper",
        ),
        (
            "resolved_type",
            "multiple_interface_shared_ptr_ref",
            "resolve_interface",
        ),
        ("resolved_type", "interface_value", "resolve_interface"),
        ("resolved_type", "second_interface_ref", "resolve_interface"),
    }
)
