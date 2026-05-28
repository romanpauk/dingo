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

from data import (
    CONTAINERS,
    EXPOSED_TYPES,
    FILTER_RULES,
    REGISTRATION_MODES,
    RESOLVED_TYPES,
    SCOPES,
    STORED_TYPES,
)
from schema import (
    ContainerSpec,
    ExposedType,
    Feature,
    RegistrationMode,
    ResolvedType,
    ScopeSpec,
    StoredType,
)
from plugins import (
    CaseLines,
    FEATURE_CASE_PLUGINS,
    REGISTRATION_PLUGIN,
    RegistrationPlan,
    bindings_type,
    case_lines_for,
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
    before_lines: tuple[str, ...]
    setup_lines: tuple[str, ...]
    check_lines: tuple[str, ...]
    after_lines: tuple[str, ...]


@dataclass(frozen=True)
class GeneratedTest:
    suite: str
    name: str
    case_name: str


@dataclass(frozen=True)
class SourceGroup:
    feature: Feature
    mode: RegistrationMode
    container: ContainerSpec

    @property
    def name(self) -> str:
        return "_".join(
            (
                self.feature.name,
                self.mode.name,
                self.container.name,
            )
        )


def implemented(axis: tuple) -> tuple:
    return tuple(member for member in axis if member.implemented)


def generate_rows() -> tuple[MatrixRow, ...]:
    rows: list[MatrixRow] = []
    rejected: dict[str, int] = defaultdict(int)
    modes = REGISTRATION_MODES
    scopes = implemented(SCOPES)
    stored_types = implemented(STORED_TYPES)
    exposed_types = implemented(EXPOSED_TYPES)
    resolved_types = implemented(RESOLVED_TYPES)
    containers = implemented(CONTAINERS)

    for feature_plugin in FEATURE_CASE_PLUGINS:
        feature = feature_plugin.feature
        for mode in modes:
            if mode.name not in feature.modes:
                rejected["feature_mode"] += 1
                continue

            mode_containers: list[ContainerSpec] = []
            for container in containers:
                if mode.name not in container.modes:
                    rejected["container_mode"] += 1
                    continue
                mode_containers.append(container)

            for scope in scopes:
                for stored_type in stored_types:
                    if mode.name not in stored_type.supported_modes:
                        rejected["stored_type_supports_mode"] += 1
                        continue
                    if scope.name not in stored_type.supported_scopes:
                        rejected["scope_supports_stored_type"] += 1
                        continue

                    for exposed_type in exposed_types:
                        if stored_type.kind not in exposed_type.supported_stored_kinds:
                            rejected["exposure_supports_stored_type"] += 1
                            continue

                        plan: RegistrationPlan | None = None
                        for resolved_type in resolved_types:
                            if (
                                exposed_type.name
                                not in resolved_type.supported_exposed_types
                            ):
                                rejected["resolved_type_supports_exposure"] += 1
                                continue
                            if mode.name not in resolved_type.supported_modes:
                                rejected["resolved_type_supports_mode"] += 1
                                continue

                            if plan is None:
                                plan = REGISTRATION_PLUGIN.build(
                                    scope, stored_type, exposed_type
                                )
                            if mode.name == "static" and not plan.static_bindings:
                                rejected["static_registration_plan"] += 1
                                continue
                            if (
                                mode.name == "mixed"
                                and plan.mixed_static_bindings is None
                            ):
                                rejected["mixed_registration_plan"] += 1
                                continue

                            base_tags = frozenset({mode.name}) | (
                                scope.provides
                                | stored_type.provides
                                | exposed_type.provides
                                | resolved_type.provides
                            )

                            for container in mode_containers:
                                tags = base_tags | container.provides
                                if not feature.requires <= tags:
                                    rejected["feature_requires_tags"] += 1
                                    continue
                                if not resolved_type.requires <= tags:
                                    rejected["resolved_type_requires_tags"] += 1
                                    continue
                                if not feature.container_requires <= container.provides:
                                    rejected["container_requires"] += 1
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


def assert_feature_mode_container_coverage(rows: tuple[MatrixRow, ...]) -> None:
    generated = {
        (row.feature.name, row.mode.name, row.container.name) for row in rows
    }
    container_tags = frozenset().union(
        *(container.provides for container in implemented(CONTAINERS))
    )
    missing: list[str] = []

    for feature_plugin in FEATURE_CASE_PLUGINS:
        feature = feature_plugin.feature
        required_container_tags = feature.container_requires | (
            feature.requires & container_tags
        )
        for mode in REGISTRATION_MODES:
            if mode.name not in feature.modes:
                continue
            for container in implemented(CONTAINERS):
                if mode.name not in container.modes:
                    continue
                if not required_container_tags <= container.provides:
                    continue
                key = (feature.name, mode.name, container.name)
                if key not in generated:
                    missing.append(" / ".join(key))

    if missing:
        raise RuntimeError(
            "matrix feature/mode/container combinations did not generate rows: "
            + ", ".join(sorted(missing))
        )


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
    assert_feature_mode_container_coverage(row_tuple)

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


def case_lines(row: MatrixRow) -> CaseLines:
    lines = case_lines_for(row)
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

    lines = case_lines(row)

    return GeneratedCase(
        name=case_name(row),
        container_type=row.container.container_type,
        static_bindings=static_bindings,
        before_lines=lines.before,
        setup_lines=setup_lines,
        check_lines=lines.check,
        after_lines=lines.after,
    )


def render_source(
    source: Path,
    rows: tuple[MatrixRow, ...],
    env: Environment,
) -> Path:
    cases: list[GeneratedCase] = []
    tests: list[GeneratedTest] = []

    for row in rows:
        case = make_case(row)
        cases.append(case)
        tests.append(
            GeneratedTest(
                suite=row.feature.name,
                name=case.name,
                case_name=case.name,
            )
        )

    if not tests:
        raise RuntimeError(f"source {source} did not generate any tests")

    rendered = env.get_template("source.cpp.j2").render(
        cases=cases,
        tests=tests,
    )
    source.write_text(rendered, encoding="utf-8")
    return source


def source_groups(rows: tuple[MatrixRow, ...]) -> dict[SourceGroup, list[MatrixRow]]:
    grouped: dict[SourceGroup, list[MatrixRow]] = defaultdict(list)
    for row in rows:
        grouped[
            SourceGroup(
                feature=row.feature,
                mode=row.mode,
                container=row.container,
            )
        ].append(row)
    return grouped


def generate_sources(
    rows: tuple[MatrixRow, ...], out_dir: Path, env: Environment
) -> tuple[Path, ...]:
    sources: list[Path] = []
    for group, group_rows in source_groups(rows).items():
        source = out_dir / f"{group.name}.cpp"
        sources.append(render_source(source, tuple(group_rows), env))
    return tuple(sorted(sources))


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
    sources = generate_sources(rows, out_dir, env)
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
