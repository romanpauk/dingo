#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from collections.abc import Iterator
from dataclasses import dataclass, replace
from enum import StrEnum
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined, Template

from axes.dependency_forms import DEPENDENCY_PROVISIONINGS
from data import (
    CATALOG,
    CONTAINERS,
    EXPOSED_TYPES,
    FEATURES,
    FEATURE_CASES,
    REGISTRATION_MODES,
    RESOLVED_TYPES,
    SCOPES,
    STORED_TYPES,
)
from dependency_contract import validate_dependency_contract
from schema import (
    ContainerSpec,
    ExposedType,
    Feature,
    FeatureCaseSpec,
    GeneratedExecutable,
    RegistrationMode,
    ResolvedType,
    ScopeSpec,
    StoredType,
)
from constructor_detection import generate_constructor_detection_executables
from dependency_composition import (
    DEPENDENCY_COMPOSITION_COVERAGE_REPORT,
    DEPENDENCY_COMPOSITION_PROFILES,
    build_dependency_composition_coverage,
    generate_dependency_composition_executables,
    generate_dependency_composition_rows,
    project_dependency_composition_rows,
    render_dependency_composition_coverage,
)
from family import SourceShard, render_family_executables, write_text_if_changed
from invoke import generate_invoke_executables
from plugins import (
    CaseLines,
    LIFETIME_POLICIES,
    RegistrationPlan,
    build_registration_plan,
    render_registration_plan,
    select_case_lines,
)
from scenarios import generate_scenario_executables
from shared_cyclical import generate_shared_cyclical_executables


class RejectionReason(StrEnum):
    CONTAINER_MODE = "container_mode"
    FEATURE_MODE = "feature_mode"
    STORED_TYPE_SUPPORTS_MODE = "stored_type_supports_mode"
    SCOPE_SUPPORTS_STORED_TYPE = "scope_supports_stored_type"
    EXPOSURE_SUPPORTS_STORED_TYPE = "exposure_supports_stored_type"
    RESOLVED_TYPE_SUPPORTS_EXPOSURE = "resolved_type_supports_exposure"
    RESOLVED_TYPE_SUPPORTS_MODE = "resolved_type_supports_mode"
    FEATURE_REQUIRES_TAGS = "feature_requires_tags"
    RESOLVED_TYPE_REQUIRES_TAGS = "resolved_type_requires_tags"
    CONTAINER_REQUIRES = "container_requires"
    FEATURE_CASE_SUPPORTS_EXPOSURE = "feature_case_supports_exposure"
    FEATURE_HAS_CHECK = "feature_has_check"


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
    lines: CaseLines


@dataclass(frozen=True, slots=True)
class GeneratedCase:
    suite: str
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]
    policy: str
    system_headers: tuple[str, ...]
    headers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class AxisIndex:
    feature_cases_by_feature: dict[str, tuple[FeatureCaseSpec, ...]]
    scopes: tuple[ScopeSpec, ...]
    stored_by_mode_scope: dict[tuple[str, str], tuple[StoredType, ...]]
    exposed_by_stored_kind: dict[str, tuple[ExposedType, ...]]
    resolved_by_mode_exposure: dict[tuple[str, str], tuple[ResolvedType, ...]]
    containers_by_mode: dict[str, tuple[ContainerSpec, ...]]
    rejected: Counter[RejectionReason]


def implemented(axis: tuple) -> tuple:
    return tuple(member for member in axis if member.implemented)


def index_axes() -> AxisIndex:
    rejected: Counter[RejectionReason] = Counter()
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
    for feature in implemented(FEATURES):
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
                rejected[RejectionReason.CONTAINER_MODE] += 1
                continue
            mode_containers.append(container)
        containers_by_mode[mode.name] = tuple(mode_containers)

    stored_by_mode_scope: dict[tuple[str, str], tuple[StoredType, ...]] = {}
    for mode in modes:
        for scope in scopes:
            mode_scope_stored: list[StoredType] = []
            for stored_type in stored_types:
                if mode.name not in stored_type.supported_modes:
                    rejected[RejectionReason.STORED_TYPE_SUPPORTS_MODE] += 1
                    continue
                if scope.name not in stored_type.supported_scopes:
                    rejected[RejectionReason.SCOPE_SUPPORTS_STORED_TYPE] += 1
                    continue
                mode_scope_stored.append(stored_type)
            stored_by_mode_scope[(mode.name, scope.name)] = tuple(mode_scope_stored)

    exposed_by_stored_kind: dict[str, tuple[ExposedType, ...]] = {}
    stored_kinds = {stored_type.kind for stored_type in stored_types}
    for stored_kind in stored_kinds:
        matching_exposures: list[ExposedType] = []
        for exposed_type in exposed_types:
            if stored_kind not in exposed_type.supported_stored_kinds:
                rejected[RejectionReason.EXPOSURE_SUPPORTS_STORED_TYPE] += 1
                continue
            matching_exposures.append(exposed_type)
        exposed_by_stored_kind[stored_kind] = tuple(matching_exposures)

    resolved_by_mode_exposure: dict[tuple[str, str], tuple[ResolvedType, ...]] = {}
    for mode in modes:
        for exposed_type in exposed_types:
            matching_resolved: list[ResolvedType] = []
            for resolved_type in resolved_types:
                if exposed_type.name not in resolved_type.supported_exposed_types:
                    rejected[RejectionReason.RESOLVED_TYPE_SUPPORTS_EXPOSURE] += 1
                    continue
                if mode.name not in resolved_type.supported_modes:
                    rejected[RejectionReason.RESOLVED_TYPE_SUPPORTS_MODE] += 1
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


PlanCache = dict[
    tuple[ScopeSpec, StoredType, ExposedType, FeatureCaseSpec], RegistrationPlan
]


def iter_candidates(
    axes: AxisIndex, rejected: Counter[RejectionReason]
) -> Iterator[CandidateRow]:
    for feature in implemented(FEATURES):
        feature_cases = axes.feature_cases_by_feature[feature.name]
        for mode in REGISTRATION_MODES:
            if mode.name not in feature.modes:
                rejected[RejectionReason.FEATURE_MODE] += 1
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

                        for resolved_type in resolved_candidates:
                            base_tags = frozenset({mode.name}) | (
                                scope.provides
                                | stored_type.provides
                                | exposed_type.provides
                                | resolved_type.provides
                            )

                            for container in mode_containers:
                                tags = base_tags | container.provides
                                if not feature.requires <= tags:
                                    rejected[RejectionReason.FEATURE_REQUIRES_TAGS] += 1
                                    continue
                                if not resolved_type.requires <= tags:
                                    rejected[
                                        RejectionReason.RESOLVED_TYPE_REQUIRES_TAGS
                                    ] += 1
                                    continue
                                if not feature.container_requires <= container.provides:
                                    rejected[RejectionReason.CONTAINER_REQUIRES] += 1
                                    continue

                                for feature_case in feature_cases:
                                    if (
                                        feature_case.supported_exposed_types
                                        and exposed_type.name
                                        not in feature_case.supported_exposed_types
                                    ):
                                        rejected[
                                            RejectionReason.FEATURE_CASE_SUPPORTS_EXPOSURE
                                        ] += 1
                                        continue
                                    yield CandidateRow(
                                        feature=feature,
                                        feature_case=feature_case,
                                        mode=mode,
                                        scope=scope,
                                        stored_type=stored_type,
                                        exposed_type=exposed_type,
                                        resolved_type=resolved_type,
                                        container=container,
                                    )


def plan_for_candidate(candidate: CandidateRow, cache: PlanCache) -> RegistrationPlan:
    key = (
        candidate.scope,
        candidate.stored_type,
        candidate.exposed_type,
        candidate.feature_case,
    )
    plan = cache.get(key)
    if plan is not None:
        return plan

    exposed_type = candidate.exposed_type
    if candidate.feature_case.registrations:
        exposed_type = replace(
            exposed_type,
            registrations=candidate.feature_case.registrations,
        )
    plan = build_registration_plan(
        candidate.scope,
        candidate.stored_type,
        exposed_type,
    )
    cache[key] = plan
    return plan


def materialize_row(
    candidate: CandidateRow,
    cache: PlanCache,
    rejected: Counter[RejectionReason],
) -> MatrixRow | None:
    plan = plan_for_candidate(candidate, cache)
    if candidate.mode.name == "static" and not plan.static_bindings:
        raise RuntimeError(f"static row {case_name(candidate)} has no bindings")
    if candidate.mode.name == "mixed" and not plan.mixed_static_bindings:
        raise RuntimeError(f"mixed row {case_name(candidate)} has no registration plan")

    lines = select_case_lines(candidate)
    if lines is None:
        rejected[RejectionReason.FEATURE_HAS_CHECK] += 1
        return None

    return MatrixRow(
        feature=candidate.feature,
        feature_case=candidate.feature_case,
        mode=candidate.mode,
        scope=candidate.scope,
        stored_type=candidate.stored_type,
        exposed_type=candidate.exposed_type,
        resolved_type=candidate.resolved_type,
        container=candidate.container,
        plan=plan,
        name=case_name(candidate),
        lines=lines,
    )


def generate_rows() -> tuple[MatrixRow, ...]:
    validate_dependency_contract()
    axes = index_axes()
    rejected = axes.rejected
    plan_cache: PlanCache = {}
    rows = tuple(
        row
        for candidate in iter_candidates(axes, rejected)
        if (row := materialize_row(candidate, plan_cache, rejected)) is not None
    )

    assert_coverage(rows, rejected)
    return rows


def assert_axis_used(
    rows: tuple[MatrixRow, ...],
    axis: str,
    expected: set[str],
    *,
    identity: str = "name",
) -> None:
    used = {getattr(getattr(row, axis), identity) for row in rows}
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

    for feature in implemented(FEATURES):
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


def assert_dependency_provisioning_coverage(
    rows: tuple[MatrixRow, ...],
) -> None:
    declared_resolution_cases = {
        (form, resolved_type, feature)
        for provisioning in DEPENDENCY_PROVISIONINGS
        for form, resolved_type, feature in provisioning.resolution_cases
    }
    generated_resolution_cases = {
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
        if (form, row.resolved_type.name, row.feature.name)
        in declared_resolution_cases
    }
    expected_resolution_cases = {
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
    }
    missing_resolution_cases = sorted(
        expected_resolution_cases - generated_resolution_cases
    )
    if missing_resolution_cases:
        raise RuntimeError(
            "dependency provisionings: "
            "form/resolution/mode/container "
            "combinations did not generate registration rows: "
            + ", ".join(
                " / ".join(case) for case in missing_resolution_cases
            )
        )


def assert_coverage(
    rows: tuple[MatrixRow, ...], rejected: Counter[RejectionReason]
) -> None:
    if not rows:
        raise RuntimeError("matrix did not generate any rows")

    row_tuple = tuple(rows)
    assert_axis_used(
        row_tuple, "feature", {feature.name for feature in implemented(FEATURES)}
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
        {stored_type.id for stored_type in implemented(STORED_TYPES)},
        identity="id",
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
    assert_dependency_provisioning_coverage(row_tuple)

    untyped_rows = [row.name for row in row_tuple if not row.lines.policy]
    if untyped_rows:
        raise RuntimeError(
            "matrix rows must use typed C++ policies: " + ", ".join(untyped_rows)
        )

    declared_policy_sources = {
        *CATALOG.policy_sources,
        *(
            ("lifetime", scope, stored_type, exposed_type, resolved_type)
            for scope, stored_type, exposed_type, resolved_type in LIFETIME_POLICIES
        ),
    }
    used_policy_sources = {row.lines.policy_source for row in row_tuple}
    unused_policy_sources = sorted(declared_policy_sources - used_policy_sources)
    if unused_policy_sources:
        raise RuntimeError(
            "matrix policies did not generate rows: "
            + ", ".join(" / ".join(source) for source in unused_policy_sources)
        )

    assert_filter_coverage(rejected)


def assert_filter_coverage(rejected: Counter[RejectionReason]) -> None:
    unknown_filters = sorted(
        str(reason)
        for reason in rejected
        if not isinstance(reason, RejectionReason)
    )
    if unknown_filters:
        raise RuntimeError(f"matrix used undeclared filters: {unknown_filters}")

    missing_filters = sorted(
        reason.value for reason in RejectionReason if not rejected[reason]
    )
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
    registration = render_registration_plan(
        row.mode.name,
        row.plan,
        context=f"row {row.name}",
    )

    return GeneratedCase(
        suite=row.feature.name,
        name=row.name,
        container_type=row.container.container_type,
        static_bindings=registration.static_bindings,
        setup_lines=registration.setup_lines,
        policy=row.lines.policy,
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
        headers=tuple(
            sorted(
                {
                    *row.feature.headers,
                    *row.feature_case.headers,
                    *row.stored_type.headers,
                    *row.exposed_type.headers,
                    *row.resolved_type.headers,
                    *row.container.headers,
                    row.feature.policy_header,
                }
            )
        ),
    )


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
            source_name = f"{row.feature.name}_{row.feature_case.name}"
        grouped[source_name].append(row)
    return grouped


def generate_registration_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    claimed_sources: set[Path] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    rows = generate_rows()
    shards: list[SourceShard] = []
    for feature_name, feature_rows in executable_groups(rows).items():
        for source_name, source_rows in sorted(source_groups(feature_rows).items()):
            cases = tuple(make_case(row) for row in source_rows)
            shards.append(
                SourceShard(
                    executable=executable_name(feature_name),
                    name=source_name,
                    source_context={
                        "cases": cases,
                        "system_headers": tuple(
                            sorted(
                                {
                                    header
                                    for case in cases
                                    for header in case.system_headers
                                }
                            )
                        ),
                        "headers": tuple(
                            sorted(
                                {
                                    header
                                    for case in cases
                                    for header in case.headers
                                }
                            )
                        ),
                    },
                    runner_context={"cases": cases},
                )
            )
    return render_family_executables(
        out_dir,
        source_template,
        runner_template,
        tuple(shards),
        claimed_sources,
    )


def remove_stale_sources(out_dir: Path, live_sources: set[Path]) -> None:
    for source in out_dir.glob("*.cpp"):
        if source not in live_sources:
            source.unlink()


def remove_stale_reports(out_dir: Path, live_reports: set[Path]) -> None:
    for report in out_dir.glob("*-coverage.md"):
        if report not in live_reports:
            report.unlink()


def merge_executables(
    *executable_groups: tuple[GeneratedExecutable, ...],
) -> tuple[GeneratedExecutable, ...]:
    all_sources = [
        source
        for group in executable_groups
        for executable in group
        for source in executable.sources
    ]
    duplicate_sources = sorted(
        source for source, count in Counter(all_sources).items() if count > 1
    )
    if duplicate_sources:
        raise RuntimeError(
            "matrix families generated duplicate sources: "
            + ", ".join(str(source) for source in duplicate_sources)
        )

    sources_by_executable: dict[str, list[Path]] = defaultdict(list)
    for executable in (
        executable
        for group in executable_groups
        for executable in group
    ):
        sources_by_executable[executable.name].extend(executable.sources)

    executables: list[GeneratedExecutable] = []
    for name, sources in sources_by_executable.items():
        executables.append(
            GeneratedExecutable(name=name, sources=tuple(sorted(sources)))
        )
    return tuple(sorted(executables, key=lambda executable: executable.name))


def generate(out_dir: Path, cmake_file: Path, profile: str = "full") -> None:
    script_dir = Path(__file__).resolve().parent
    env = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    out_dir.mkdir(parents=True, exist_ok=True)
    claimed_sources: set[Path] = set()
    family_specs = (
        (
            generate_registration_executables,
            "source.cpp.j2",
            "runner.cpp.j2",
            {},
        ),
        (
            generate_scenario_executables,
            "scenario.cpp.j2",
            "runner.cpp.j2",
            {},
        ),
        (
            generate_constructor_detection_executables,
            "constructor_detection.cpp.j2",
            "constructor_detection_runner.cpp.j2",
            {},
        ),
        (
            generate_invoke_executables,
            "source.cpp.j2",
            "runner.cpp.j2",
            {},
        ),
        (
            generate_dependency_composition_executables,
            "dependency_composition.cpp.j2",
            "dependency_composition_runner.cpp.j2",
            {"profile": profile},
        ),
        (
            generate_shared_cyclical_executables,
            "shared_cyclical.cpp.j2",
            "shared_cyclical_runner.cpp.j2",
            {},
        ),
    )
    executables = merge_executables(
        *(
            generator(
                out_dir,
                env.get_template(source_template),
                env.get_template(runner_template),
                claimed_sources,
                **kwargs,
            )
            for generator, source_template, runner_template, kwargs in family_specs
        )
    )
    remove_stale_sources(
        out_dir,
        {source for executable in executables for source in executable.sources},
    )
    coverage_report = out_dir / DEPENDENCY_COMPOSITION_COVERAGE_REPORT
    composition_rows = generate_dependency_composition_rows()
    projected_composition_rows = project_dependency_composition_rows(
        composition_rows, profile
    )
    write_text_if_changed(
        coverage_report,
        render_dependency_composition_coverage(
            build_dependency_composition_coverage(composition_rows),
            profile=profile,
            compiled_rows=len(projected_composition_rows),
        ),
    )
    remove_stale_reports(out_dir, {coverage_report})
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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--cmake", required=True, type=Path)
    parser.add_argument(
        "--profile",
        choices=sorted(DEPENDENCY_COMPOSITION_PROFILES),
        default="full",
    )
    args = parser.parse_args()

    generate(args.out, args.cmake, args.profile)


if __name__ == "__main__":
    main()
