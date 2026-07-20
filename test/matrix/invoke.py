#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Generate the independent container-invocation matrix."""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Template

from axes.exposed_types import EXPOSED_TYPES
from axes.invoke import (
    INVOKE_CALLABLES,
    INVOKE_CONTAINERS,
    INVOKE_PROVISIONINGS,
    INVOKE_MODES,
)
from axes.resolved_types import RESOLVED_TYPES
from axes.scopes import SCOPES
from axes.stored_types import STORED_TYPES
from family import (
    SourceShard,
    assert_axis_members_used,
    assert_unique_axis_members,
    render_case_family_executables,
)
from plugins import (
    RegistrationPlan,
    build_registration_plan,
    render_registration_plan,
)
from schema import (
    ContainerSpec,
    DependencyProvisioning,
    ExposedType,
    GeneratedExecutable,
    InvokeCallable,
    RegistrationMode,
    ResolvedType,
    ScopeSpec,
    StoredType,
)


@dataclass(frozen=True, slots=True)
class InvokeRow:
    callable: InvokeCallable
    provisioning: DependencyProvisioning
    mode: RegistrationMode
    container: ContainerSpec
    scope: ScopeSpec
    stored_type: StoredType
    exposed_type: ExposedType
    resolved_type: ResolvedType
    plan: RegistrationPlan
    name: str


@dataclass(frozen=True, slots=True)
class GeneratedInvokeCase:
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]
    policy: str
    system_headers: tuple[str, ...]
    headers: tuple[str, ...]

    @property
    def suite(self) -> str:
        return "invoke"


def _by_name(axis: tuple, identity: str = "name") -> dict[str, object]:
    return {getattr(member, identity): member for member in axis}


def generate_invoke_rows(
    callables: tuple[InvokeCallable, ...] = INVOKE_CALLABLES,
    provisionings: tuple[DependencyProvisioning, ...] = (
        INVOKE_PROVISIONINGS
    ),
    modes: tuple[RegistrationMode, ...] = INVOKE_MODES,
    containers: tuple[ContainerSpec, ...] = INVOKE_CONTAINERS,
) -> tuple[InvokeRow, ...]:
    assert_unique_axis_members(
        "invoke", "callable", tuple(member.name for member in callables)
    )
    assert_unique_axis_members(
        "invoke",
        "provisioning",
        tuple(member.name for member in provisionings),
    )
    assert_unique_axis_members(
        "invoke", "mode", tuple(member.name for member in modes)
    )
    assert_unique_axis_members(
        "invoke", "container", tuple(member.name for member in containers)
    )

    known_provisionings = {provisioning.name for provisioning in provisionings}
    known_containers = {container.name for container in containers}
    scopes = _by_name(SCOPES)
    stored_types = _by_name(STORED_TYPES, "id")
    exposed_types = _by_name(EXPOSED_TYPES)
    resolved_types = _by_name(RESOLVED_TYPES)
    known_modes = {mode.name for mode in modes}

    dependencies: dict[
        str, tuple[ScopeSpec, StoredType, ExposedType, ResolvedType, RegistrationPlan]
    ] = {}
    for provisioning in provisionings:
        missing_modes = sorted(provisioning.supported_modes - known_modes)
        if missing_modes:
            raise ValueError(
                f"invoke provisioning {provisioning.name} references unknown modes: "
                f"{missing_modes}"
            )
        try:
            scope = scopes[provisioning.scope]
            stored_type = stored_types[provisioning.stored_type]
            exposed_type = exposed_types[provisioning.exposed_type]
            resolved_type = resolved_types[provisioning.resolved_type]
        except KeyError as error:
            raise ValueError(
                f"invoke provisioning {provisioning.name} references unknown "
                f"registration metadata: {error.args[0]}"
            ) from error
        if scope.name not in stored_type.supported_scopes:
            raise ValueError(
                f"invoke provisioning {provisioning.name} uses unsupported scope"
            )
        if stored_type.kind not in exposed_type.supported_stored_kinds:
            raise ValueError(
                f"invoke provisioning {provisioning.name} uses unsupported exposure"
            )
        if exposed_type.name not in resolved_type.supported_exposed_types:
            raise ValueError(
                f"invoke provisioning {provisioning.name} uses unsupported resolution"
            )
        plan = build_registration_plan(scope, stored_type, exposed_type)
        dependencies[provisioning.name] = (
            scope,
            stored_type,
            exposed_type,
            resolved_type,
            plan,
        )

    rows: list[InvokeRow] = []
    for callable_spec in callables:
        if callable_spec.supported_containers is not None:
            missing_containers = sorted(
                callable_spec.supported_containers - known_containers
            )
            if missing_containers:
                raise ValueError(
                    f"invoke callable {callable_spec.name} references unknown "
                    f"containers: {missing_containers}"
                )
        missing_provisionings = sorted(
            callable_spec.supported_provisionings - known_provisionings
        )
        if missing_provisionings:
            raise ValueError(
                f"invoke callable {callable_spec.name} references unknown "
                f"provisionings: {missing_provisionings}"
            )
        for provisioning in provisionings:
            if provisioning.name not in callable_spec.supported_provisionings:
                continue
            scope, stored_type, exposed_type, resolved_type, plan = dependencies[
                provisioning.name
            ]
            for mode in modes:
                if mode.name not in provisioning.supported_modes:
                    continue
                if mode.name not in stored_type.supported_modes:
                    continue
                if mode.name not in resolved_type.supported_modes:
                    continue
                for container in containers:
                    if mode.name not in container.modes:
                        continue
                    if (
                        callable_spec.supported_containers is not None
                        and container.name
                        not in callable_spec.supported_containers
                    ):
                        continue
                    if mode.name == "static" and not plan.static_bindings:
                        raise RuntimeError(
                            f"invoke provisioning {provisioning.name} has no "
                            "static bindings"
                        )
                    if mode.name == "mixed" and not plan.mixed_static_bindings:
                        raise RuntimeError(
                            f"invoke provisioning {provisioning.name} has no mixed plan"
                        )
                    name = "_".join(
                        (
                            mode.name,
                            scope.name,
                            stored_type.id,
                            exposed_type.name,
                            resolved_type.name,
                            callable_spec.name,
                            container.name,
                        )
                    )
                    rows.append(
                        InvokeRow(
                            callable=callable_spec,
                            provisioning=provisioning,
                            mode=mode,
                            container=container,
                            scope=scope,
                            stored_type=stored_type,
                            exposed_type=exposed_type,
                            resolved_type=resolved_type,
                            plan=plan,
                            name=name,
                        )
                    )

    generated_axes = (
        (
            "callable",
            {member.name for member in callables},
            {row.callable.name for row in rows},
        ),
        (
            "provisioning",
            {member.name for member in provisionings},
            {row.provisioning.name for row in rows},
        ),
        ("mode", {member.name for member in modes}, {row.mode.name for row in rows}),
        (
            "container",
            {member.name for member in containers},
            {row.container.name for row in rows},
        ),
    )
    for axis, declared, used in generated_axes:
        assert_axis_members_used("invoke", axis, declared, used)
    return tuple(rows)


def _make_case(row: InvokeRow) -> GeneratedInvokeCase:
    registration = render_registration_plan(
        row.mode.name,
        row.plan,
        context=f"invoke row {row.name}",
    )

    return GeneratedInvokeCase(
        name=row.name,
        container_type=row.container.container_type,
        static_bindings=registration.static_bindings,
        setup_lines=registration.setup_lines,
        policy=row.callable.policy,
        system_headers=tuple(
            sorted(
                {
                    *row.callable.system_headers,
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
                    *row.callable.headers,
                    *row.stored_type.headers,
                    *row.exposed_type.headers,
                    *row.resolved_type.headers,
                    *row.container.headers,
                    "matrix/policies/resolution.h",
                }
            )
        ),
    )


def generate_invoke_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    claimed_sources: set[Path] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    grouped: dict[str, list[InvokeRow]] = defaultdict(list)
    for row in generate_invoke_rows():
        grouped[row.callable.name].append(row)

    shards: list[SourceShard] = []
    for callable_name, rows in grouped.items():
        cases = tuple(_make_case(row) for row in rows)
        source_name = f"invoke_{callable_name}"
        shards.append(
            SourceShard(
                executable="dingo_matrix_test_invoke",
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
            ),
        )
    return render_case_family_executables(
        out_dir,
        source_template,
        runner_template,
        tuple(shards),
        claimed_sources,
    )
