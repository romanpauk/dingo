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

from data import CATALOG, STORED_TYPES, validate_catalog
from generate import (
    MatrixRow,
    RejectionReason,
    assert_axis_used,
    assert_filter_coverage,
    generate,
    generate_rows,
    implemented,
    materialize_row,
)
from plugins import build_registration_plan, register_type
from schema import ExposedType, MixedRegistrationPlacement, RegistrationSpec


@pytest.fixture(scope="module")
def rows() -> tuple[MatrixRow, ...]:
    return generate_rows()


def test_matrix_cardinality_remains_bounded(rows: tuple[MatrixRow, ...]) -> None:
    assert len(rows) == 3297
    assert len({(row.feature.name, row.name) for row in rows}) == len(rows)


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
    assert len(tuple(out_dir.glob("*.cpp"))) == 148
    assert cmake_file.is_file()
    timestamps = {
        path: path.stat().st_mtime_ns
        for path in (*out_dir.glob("*.cpp"), cmake_file)
    }

    generate(out_dir, cmake_file)

    assert {path: path.stat().st_mtime_ns for path in timestamps} == timestamps
