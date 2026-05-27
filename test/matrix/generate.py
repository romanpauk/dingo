#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from model import (
    CONTAINERS,
    EXPOSED_TYPES,
    FILTER_RULES,
    REGISTRATION_MODES,
    RESOLVED_TYPES,
    SCOPES,
    STORED_TYPES,
    ContainerSpec,
    ExposedType,
    Feature,
    RegistrationMode,
    ResolvedType,
    ScopeSpec,
    StoredType,
)
from plugins import (
    FEATURE_CASE_PLUGINS,
    REGISTRATION_PLUGIN,
    RegistrationPlan,
    bindings_type,
    check_lines_for,
)


@dataclass(frozen=True)
class MatrixRow:
    feature: Feature
    mode: RegistrationMode
    scope: ScopeSpec
    stored_type: StoredType
    exposed_type: ExposedType
    resolved_type: ResolvedType
    container: ContainerSpec
    plan: RegistrationPlan


@dataclass(frozen=True)
class GeneratedCase:
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]
    check_lines: tuple[str, ...]


@dataclass(frozen=True)
class GeneratedTest:
    suite: str
    name: str
    case_name: str


def implemented(axis: tuple) -> tuple:
    return tuple(member for member in axis if member.implemented)


def row_tags(
    mode: RegistrationMode,
    scope: ScopeSpec,
    stored_type: StoredType,
    exposed_type: ExposedType,
    resolved_type: ResolvedType,
    container: ContainerSpec,
) -> frozenset[str]:
    return frozenset({mode.name}) | (
        scope.provides
        | stored_type.provides
        | exposed_type.provides
        | resolved_type.provides
        | container.provides
    )


def reject_reason(
    feature: Feature,
    mode: RegistrationMode,
    scope: ScopeSpec,
    stored_type: StoredType,
    exposed_type: ExposedType,
    resolved_type: ResolvedType,
    container: ContainerSpec,
) -> str | None:
    if mode.name not in container.modes:
        return "container_mode"
    if mode.name not in feature.modes:
        return "feature_mode"
    if mode.name not in stored_type.supported_modes:
        return "stored_type_supports_mode"
    if scope.name not in stored_type.supported_scopes:
        return "scope_supports_stored_type"
    if stored_type.kind not in exposed_type.supported_stored_kinds:
        return "exposure_supports_stored_type"
    if exposed_type.name not in resolved_type.supported_exposed_types:
        return "resolved_type_supports_exposure"
    if mode.name not in resolved_type.supported_modes:
        return "resolved_type_supports_mode"

    tags = row_tags(mode, scope, stored_type, exposed_type, resolved_type, container)
    if not feature.requires <= tags:
        return "feature_requires_tags"
    if not resolved_type.requires <= tags:
        return "resolved_type_requires_tags"
    if not feature.container_requires <= container.provides:
        return "container_requires"

    return None


def generate_rows() -> tuple[MatrixRow, ...]:
    rows: list[MatrixRow] = []
    rejected: dict[str, int] = defaultdict(int)

    for feature_plugin in FEATURE_CASE_PLUGINS:
        feature = feature_plugin.feature
        for mode in REGISTRATION_MODES:
            for scope in implemented(SCOPES):
                for stored_type in implemented(STORED_TYPES):
                    for exposed_type in implemented(EXPOSED_TYPES):
                        for resolved_type in implemented(RESOLVED_TYPES):
                            for container in implemented(CONTAINERS):
                                reason = reject_reason(
                                    feature,
                                    mode,
                                    scope,
                                    stored_type,
                                    exposed_type,
                                    resolved_type,
                                    container,
                                )
                                if reason is not None:
                                    rejected[reason] += 1
                                    continue

                                plan = REGISTRATION_PLUGIN.build(
                                    scope, stored_type, exposed_type
                                )
                                if mode.name == "static" and not plan.static_bindings:
                                    continue
                                if (
                                    mode.name == "mixed"
                                    and plan.mixed_static_bindings is None
                                ):
                                    continue

                                row = MatrixRow(
                                    feature=feature,
                                    mode=mode,
                                    scope=scope,
                                    stored_type=stored_type,
                                    exposed_type=exposed_type,
                                    resolved_type=resolved_type,
                                    container=container,
                                    plan=plan,
                                )
                                if feature_plugin.emit(row) is None:
                                    rejected["feature_has_check"] += 1
                                    continue

                                rows.append(row)

    assert_coverage(rows, rejected)
    return tuple(rows)


def assert_axis_used(
    rows: tuple[MatrixRow, ...], axis: str, expected: set[str]
) -> None:
    used = {getattr(row, axis).name for row in rows}
    missing = sorted(expected - used)
    if missing:
        raise RuntimeError(f"matrix axis {axis} did not generate rows for: {missing}")


def assert_coverage(rows: list[MatrixRow], rejected: dict[str, int]) -> None:
    if not rows:
        raise RuntimeError("matrix did not generate any rows")

    row_tuple = tuple(rows)
    assert_axis_used(
        row_tuple, "feature", {plugin.feature.name for plugin in FEATURE_CASE_PLUGINS}
    )
    assert_axis_used(row_tuple, "mode", {mode.name for mode in REGISTRATION_MODES})
    assert_axis_used(
        row_tuple, "scope", {scope.name for scope in implemented(SCOPES)}
    )
    assert_axis_used(
        row_tuple,
        "stored_type",
        {stored_type.name for stored_type in implemented(STORED_TYPES)},
    )
    assert_axis_used(
        row_tuple,
        "exposed_type",
        {exposed_type.name for exposed_type in implemented(EXPOSED_TYPES)},
    )
    assert_axis_used(
        row_tuple,
        "resolved_type",
        {resolved_type.name for resolved_type in implemented(RESOLVED_TYPES)},
    )
    assert_axis_used(
        row_tuple,
        "container",
        {container.name for container in implemented(CONTAINERS)},
    )

    missing_filters = sorted(rule for rule in FILTER_RULES if rejected[rule] == 0)
    if missing_filters:
        raise RuntimeError(f"matrix filters were not exercised: {missing_filters}")


def case_name(row: MatrixRow) -> str:
    return "_".join(
        (
            row.mode.name,
            row.scope.name,
            row.stored_type.id,
            row.exposed_type.name,
            row.resolved_type.name,
            row.container.name,
        )
    )


def check_lines(row: MatrixRow) -> tuple[str, ...]:
    lines = check_lines_for(row)
    if lines:
        return lines
    raise RuntimeError(
        f"row {case_name(row)} has no checks for feature {row.feature.name}"
    )


def make_case(row: MatrixRow) -> GeneratedCase:
    static_bindings = None
    setup_lines = ()
    if row.mode.name == "static":
        static_bindings = bindings_type(row.plan.static_bindings)
    elif row.mode.name == "runtime":
        setup_lines = row.plan.runtime_setup
    elif row.mode.name == "mixed":
        if row.plan.mixed_static_bindings is None:
            raise RuntimeError(f"row {case_name(row)} has no mixed static bindings")
        static_bindings = bindings_type(row.plan.mixed_static_bindings)
        setup_lines = row.plan.mixed_runtime_setup
    else:
        raise RuntimeError(f"unknown registration mode: {row.mode.name}")

    return GeneratedCase(
        name=case_name(row),
        container_type=row.container.container_type,
        static_bindings=static_bindings,
        setup_lines=setup_lines,
        check_lines=check_lines(row),
    )


def generate_feature(
    feature: Feature, rows: tuple[MatrixRow, ...], out_dir: Path, env: Environment
) -> Path:
    cases: list[GeneratedCase] = []
    tests: list[GeneratedTest] = []

    for row in rows:
        if row.feature != feature:
            continue
        case = make_case(row)
        cases.append(case)
        tests.append(
            GeneratedTest(
                suite=feature.name,
                name=case.name,
                case_name=case.name,
            )
        )

    if not tests:
        raise RuntimeError(f"feature {feature.name} did not generate any tests")

    source = out_dir / f"{feature.name}.cpp"
    rendered = env.get_template("source.cpp.j2").render(
        cases=cases,
        tests=tests,
    )
    source.write_text(rendered, encoding="utf-8")
    return source


def generate(out_dir: Path, cmake_file: Path) -> None:
    script_dir = Path(__file__).resolve().parent
    env = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    out_dir.mkdir(parents=True, exist_ok=True)
    rows = generate_rows()
    sources = [
        generate_feature(plugin.feature, rows, out_dir, env)
        for plugin in FEATURE_CASE_PLUGINS
    ]
    cmake_file.parent.mkdir(parents=True, exist_ok=True)
    cmake_file.write_text(
        env.get_template("cmake.cmake.j2").render(
            sources=[source.as_posix() for source in sources]
        ),
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--cmake", required=True, type=Path)
    args = parser.parse_args()

    generate(args.out, args.cmake)


if __name__ == "__main__":
    main()
