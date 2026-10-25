#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Matrix rows whose behavior the model-generated suites now own.

A restriction keeps the feature in the catalog so its policy sources and
dependency provisionings still validate, but stops generating the resolved-type
rows that a behavior class already executes. The covered cells are checked
against the model's behavior product by the generator test, so a restriction
cannot outlive the test that justifies it.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class FeatureRestriction:
    feature: str
    keep_resolved_types: frozenset[str]
    covered_cells: frozenset[tuple[str, str]]
    reason: str


FEATURE_RESTRICTIONS = (
    FeatureRestriction(
        feature="lifetime_counts",
        keep_resolved_types=frozenset({"value_rvalue"}),
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
            "the lifetime balance for these cells; only the unique rvalue route "
            "has no behavior case"
        ),
    ),
    FeatureRestriction(
        feature="variant",
        keep_resolved_types=frozenset(
            {
                "variant_alternative_value",
                "variant_alternative_ref",
                "variant_alternative_ptr",
            }
        ),
        covered_cells=frozenset(
            {
                ("shared", "alternative"),
                ("unique", "alternative"),
            }
        ),
        reason=(
            "behavior classes resolve the variant value and the alternative "
            "object; the alternative reference and pointer routes keep their "
            "matrix rows"
        ),
    ),
)

RESTRICTIONS_BY_FEATURE = {
    restriction.feature: restriction for restriction in FEATURE_RESTRICTIONS
}


@dataclass(frozen=True, slots=True)
class ResolvedTypeCoverage:
    resolved_type: str
    covered_cells: frozenset[tuple[str, str]]
    reason: str


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
        reason="behavior classes resolve the variant value directly",
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
        ("resolved_type", "variant_value", "variant"),
    }
)
