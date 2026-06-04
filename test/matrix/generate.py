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

from jinja2 import Environment, FileSystemLoader, StrictUndefined, Template

from data import (
    CONTAINERS,
    EXPOSED_TYPES,
    FEATURE_CASES,
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
    FeatureCaseSpec,
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
)


@dataclass(frozen=True, slots=True)
class CandidateRow:
    feature: Feature
    feature_case: FeatureCaseSpec
    mode: RegistrationMode
    scope: ScopeSpec
    stored_type: StoredType
    exposed_type: ExposedType
    resolved_type: ResolvedType
    container: ContainerSpec


@dataclass(frozen=True, slots=True)
class MatrixRow(CandidateRow):
    plan: RegistrationPlan
    name: str
    lines: CaseLines = CaseLines()


@dataclass(frozen=True, slots=True)
class GeneratedCase:
    suite: str
    name: str
    container_type: str
    static_bindings: str | None
    before_lines: tuple[str, ...]
    setup_lines: tuple[str, ...]
    check_lines: tuple[str, ...]
    after_lines: tuple[str, ...]
    system_headers: tuple[str, ...]
    support_headers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class GeneratedExecutable:
    name: str
    sources: tuple[Path, ...]


@dataclass(frozen=True, slots=True)
class AxisIndex:
    feature_cases_by_feature: dict[str, tuple[FeatureCaseSpec, ...]]
    scopes: tuple[ScopeSpec, ...]
    stored_by_mode_scope: dict[tuple[str, str], tuple[StoredType, ...]]
    exposed_by_stored_kind: dict[str, tuple[ExposedType, ...]]
    resolved_by_mode_exposure: dict[tuple[str, str], tuple[ResolvedType, ...]]
    containers_by_mode: dict[str, tuple[ContainerSpec, ...]]
    rejected: dict[str, int]


def implemented(axis: tuple) -> tuple:
    return tuple(member for member in axis if member.implemented)


def index_axes() -> AxisIndex:
    rejected: dict[str, int] = defaultdict(int)
    modes = REGISTRATION_MODES
    scopes = implemented(SCOPES)
    stored_types = implemented(STORED_TYPES)
    exposed_types = implemented(EXPOSED_TYPES)
    resolved_types = implemented(RESOLVED_TYPES)
    containers = implemented(CONTAINERS)
    feature_cases = implemented(FEATURE_CASES)

    explicit_feature_cases: dict[str, list[FeatureCaseSpec]] = defaultdict(list)
    for feature_case in feature_cases:
        explicit_feature_cases[feature_case.feature].append(feature_case)

    feature_cases_by_feature: dict[str, tuple[FeatureCaseSpec, ...]] = {}
    for feature in implemented(tuple(plugin.feature for plugin in FEATURE_CASE_PLUGINS)):
        feature_cases_by_feature[feature.name] = tuple(
            explicit_feature_cases.get(
                feature.name,
                [
                    FeatureCaseSpec(
                        name="default",
                        feature=feature.name,
                    )
                ],
            )
        )

    containers_by_mode: dict[str, tuple[ContainerSpec, ...]] = {}
    for mode in modes:
        mode_containers: list[ContainerSpec] = []
        for container in containers:
            if mode.name not in container.modes:
                rejected["container_mode"] += 1
                continue
            mode_containers.append(container)
        containers_by_mode[mode.name] = tuple(mode_containers)

    stored_by_mode_scope: dict[tuple[str, str], tuple[StoredType, ...]] = {}
    for mode in modes:
        for scope in scopes:
            mode_scope_stored: list[StoredType] = []
            for stored_type in stored_types:
                if mode.name not in stored_type.supported_modes:
                    rejected["stored_type_supports_mode"] += 1
                    continue
                if scope.name not in stored_type.supported_scopes:
                    rejected["scope_supports_stored_type"] += 1
                    continue
                mode_scope_stored.append(stored_type)
            stored_by_mode_scope[(mode.name, scope.name)] = tuple(mode_scope_stored)

    exposed_by_stored_kind: dict[str, tuple[ExposedType, ...]] = {}
    stored_kinds = {stored_type.kind for stored_type in stored_types}
    for stored_kind in stored_kinds:
        matching_exposures: list[ExposedType] = []
        for exposed_type in exposed_types:
            if stored_kind not in exposed_type.supported_stored_kinds:
                rejected["exposure_supports_stored_type"] += 1
                continue
            matching_exposures.append(exposed_type)
        exposed_by_stored_kind[stored_kind] = tuple(matching_exposures)

    resolved_by_mode_exposure: dict[tuple[str, str], tuple[ResolvedType, ...]] = {}
    for mode in modes:
        for exposed_type in exposed_types:
            matching_resolved: list[ResolvedType] = []
            for resolved_type in resolved_types:
                if exposed_type.name not in resolved_type.supported_exposed_types:
                    rejected["resolved_type_supports_exposure"] += 1
                    continue
                if mode.name not in resolved_type.supported_modes:
                    rejected["resolved_type_supports_mode"] += 1
                    continue
                matching_resolved.append(resolved_type)
            resolved_by_mode_exposure[(mode.name, exposed_type.name)] = tuple(
                matching_resolved
            )

    return AxisIndex(
        feature_cases_by_feature=feature_cases_by_feature,
        scopes=scopes,
        stored_by_mode_scope=stored_by_mode_scope,
        exposed_by_stored_kind=exposed_by_stored_kind,
        resolved_by_mode_exposure=resolved_by_mode_exposure,
        containers_by_mode=containers_by_mode,
        rejected=rejected,
    )


def generate_rows() -> tuple[MatrixRow, ...]:
    rows: list[MatrixRow] = []
    axes = index_axes()
    rejected = axes.rejected
    plan_cache: dict[
        tuple[ScopeSpec, StoredType, ExposedType], RegistrationPlan
    ] = {}

    def plan_for(
        scope: ScopeSpec, stored_type: StoredType, exposed_type: ExposedType
    ) -> RegistrationPlan:
        key = (scope, stored_type, exposed_type)
        plan = plan_cache.get(key)
        if plan is None:
            plan = REGISTRATION_PLUGIN.build(scope, stored_type, exposed_type)
            plan_cache[key] = plan
        return plan

    for feature_plugin in FEATURE_CASE_PLUGINS:
        feature = feature_plugin.feature
        feature_cases = axes.feature_cases_by_feature[feature.name]
        for mode in REGISTRATION_MODES:
            if mode.name not in feature.modes:
                rejected["feature_mode"] += 1
                continue

            mode_containers = axes.containers_by_mode[mode.name]

            for scope in axes.scopes:
                for stored_type in axes.stored_by_mode_scope[
                    (mode.name, scope.name)
                ]:
                    for exposed_type in axes.exposed_by_stored_kind[stored_type.kind]:
                        resolved_candidates = axes.resolved_by_mode_exposure[
                            (mode.name, exposed_type.name)
                        ]
                        if not resolved_candidates:
                            continue

                        plan: RegistrationPlan | None = None
                        for resolved_type in resolved_candidates:
                            if plan is None:
                                plan = plan_for(scope, stored_type, exposed_type)
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

                                for feature_case in feature_cases:
                                    if not feature_case.requires <= tags:
                                        rejected["feature_case_requires_tags"] += 1
                                        continue
                                    if (
                                        feature_case.supported_exposed_types
                                        and exposed_type.name
                                        not in feature_case.supported_exposed_types
                                    ):
                                        rejected[
                                            "feature_case_supports_exposure"
                                        ] += 1
                                        continue
                                    if (
                                        feature_case.supported_resolved_types
                                        and resolved_type.name
                                        not in feature_case.supported_resolved_types
                                    ):
                                        rejected[
                                            "feature_case_supports_resolution"
                                        ] += 1
                                        continue

                                    candidate = CandidateRow(
                                        feature=feature,
                                        feature_case=feature_case,
                                        mode=mode,
                                        scope=scope,
                                        stored_type=stored_type,
                                        exposed_type=exposed_type,
                                        resolved_type=resolved_type,
                                        container=container,
                                    )
                                    lines = feature_plugin.emit(candidate)
                                    if lines is None:
                                        rejected["feature_has_check"] += 1
                                        continue

                                    rows.append(
                                        MatrixRow(
                                            feature=feature,
                                            feature_case=feature_case,
                                            mode=mode,
                                            scope=scope,
                                            stored_type=stored_type,
                                            exposed_type=exposed_type,
                                            resolved_type=resolved_type,
                                            container=container,
                                            plan=plan,
                                            name=case_name(candidate),
                                            lines=lines,
                                        )
                                    )

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
    used_feature_cases = {
        (row.feature_case.feature, row.feature_case.name)
        for row in row_tuple
        if row.feature_case.name != "default"
    }
    missing_feature_cases = sorted(
        {
            (feature_case.feature, feature_case.name)
            for feature_case in implemented(FEATURE_CASES)
        }
        - used_feature_cases
    )
    if missing_feature_cases:
        raise RuntimeError(
            "matrix feature_case axis did not generate rows for: "
            + ", ".join(
                f"{feature} / {feature_case}"
                for feature, feature_case in missing_feature_cases
            )
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


def case_name(row: CandidateRow) -> str:
    parts = [
        row.mode.name,
        row.scope.name,
        row.stored_type.id,
        row.exposed_type.name,
        row.resolved_type.name,
    ]
    if row.feature_case.name != "default":
        parts.append(row.feature_case.name)
    parts.append(row.container.name)
    return "_".join(parts)


def make_case(row: MatrixRow) -> GeneratedCase:
    static_bindings = None
    setup_lines = ()
    if row.mode.name == "static":
        static_bindings = bindings_type(row.plan.static_bindings)
    elif row.mode.name == "runtime":
        setup_lines = row.plan.runtime_setup
    elif row.mode.name == "mixed":
        if row.plan.mixed_static_bindings is None:
            raise RuntimeError(f"row {row.name} has no mixed static bindings")
        static_bindings = bindings_type(row.plan.mixed_static_bindings)
        setup_lines = row.plan.mixed_runtime_setup
    else:
        raise RuntimeError(f"unknown registration mode: {row.mode.name}")

    return GeneratedCase(
        suite=row.feature.name,
        name=row.name,
        container_type=row.container.container_type,
        static_bindings=static_bindings,
        before_lines=row.lines.before,
        setup_lines=setup_lines,
        check_lines=row.lines.check,
        after_lines=row.lines.after,
        system_headers=tuple(
            sorted(
                {
                    *row.feature.system_headers,
                    *row.feature_case.system_headers,
                    *row.stored_type.system_headers,
                    *row.exposed_type.system_headers,
                    *row.resolved_type.system_headers,
                    *row.container.system_headers,
                }
            )
        ),
        support_headers=tuple(
            sorted(
                {
                    *row.feature.support_headers,
                    *row.feature_case.support_headers,
                    *row.stored_type.support_headers,
                    *row.exposed_type.support_headers,
                    *row.resolved_type.support_headers,
                    *row.container.support_headers,
                }
            )
        ),
    )


def render_source(
    source: Path,
    rows: list[MatrixRow],
    template: Template,
) -> Path:
    cases: list[GeneratedCase] = []

    for row in rows:
        cases.append(make_case(row))

    if not cases:
        raise RuntimeError(f"source {source} did not generate any tests")

    rendered = template.render(
        cases=cases,
        system_headers=tuple(
            sorted({header for case in cases for header in case.system_headers})
        ),
        support_headers=tuple(
            sorted({header for case in cases for header in case.support_headers})
        ),
    )
    write_text_if_changed(source, rendered)
    return source


def render_runner(
    source: Path,
    rows: tuple[MatrixRow, ...],
    template: Template,
) -> Path:
    cases = [make_case(row) for row in rows]
    rendered = template.render(cases=cases)
    write_text_if_changed(source, rendered)
    return source


def executable_name(feature_name: str) -> str:
    return f"dingo_matrix_test_{feature_name}"


def executable_groups(rows: tuple[MatrixRow, ...]) -> dict[str, list[MatrixRow]]:
    grouped: dict[str, list[MatrixRow]] = defaultdict(list)
    for row in rows:
        grouped[row.feature.name].append(row)
    return grouped


def source_groups(rows: list[MatrixRow]) -> dict[str, list[MatrixRow]]:
    grouped: dict[str, list[MatrixRow]] = defaultdict(list)
    for row in rows:
        if row.feature_case.name == "default":
            source_name = row.feature.name
        else:
            source_name = f"{row.feature.name}__{row.feature_case.name}"
        grouped[source_name].append(row)
    return grouped


def generate_sources(
    rows: tuple[MatrixRow, ...],
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
) -> tuple[GeneratedExecutable, ...]:
    executables: list[GeneratedExecutable] = []
    for feature_name, feature_rows in executable_groups(rows).items():
        sources: list[Path] = []
        for source_name, source_rows in sorted(source_groups(feature_rows).items()):
            sources.append(
                render_source(
                    out_dir / f"{source_name}.cpp",
                    source_rows,
                    source_template,
                )
            )
            sources.append(
                render_runner(
                    out_dir / f"matrix_runner_{source_name}.cpp",
                    tuple(source_rows),
                    runner_template,
                )
            )
        executables.append(
            GeneratedExecutable(
                name=executable_name(feature_name),
                sources=tuple(sources),
            )
        )

    return tuple(sorted(executables, key=lambda executable: executable.name))


def remove_stale_sources(out_dir: Path, live_sources: set[Path]) -> None:
    for source in out_dir.glob("*.cpp"):
        if source not in live_sources:
            source.unlink()


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
    executables = generate_sources(
        rows,
        out_dir,
        env.get_template("source.cpp.j2"),
        env.get_template("runner.cpp.j2"),
    )
    remove_stale_sources(
        out_dir,
        {source for executable in executables for source in executable.sources},
    )
    cmake_file.parent.mkdir(parents=True, exist_ok=True)
    write_text_if_changed(
        cmake_file,
        env.get_template("cmake.cmake.j2").render(
            executables=[
                {
                    "name": executable.name,
                    "sources": [source.as_posix() for source in executable.sources],
                }
                for executable in executables
            ]
        ),
    )


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--cmake", required=True, type=Path)
    args = parser.parse_args()

    generate(args.out, args.cmake)


if __name__ == "__main__":
    main()
