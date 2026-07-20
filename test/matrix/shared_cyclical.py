#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Generate real two-node cycles from independent shared-cyclical axes."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from jinja2 import Template

from axes.modes import REGISTRATION_MODES
from axes.scopes import SHARED_CYCLICAL_SCOPE
from axes.shared_cyclical import (
    SHARED_CYCLICAL_CONTAINERS,
    SHARED_CYCLICAL_DEPENDENCY_EDGE_SHAPES,
    SHARED_CYCLICAL_STORAGE_SHAPES,
)
from family import (
    SourceShard,
    assert_axis_members_used,
    assert_unique_axis_members,
    render_family_executables,
)
from plugins import (
    RegistrationRecipe,
    build_registration_plan_from_recipe,
    render_registration_plan,
)
from schema import (
    GeneratedExecutable,
    MixedRegistrationPlacement,
    RegistrationMode,
    RegistrationSpec,
    SharedCyclicalContainer,
    SharedCyclicalDependencyEdgeShape,
    SharedCyclicalStorageShape,
)


@dataclass(frozen=True, slots=True)
class SharedCyclicalRow:
    container: SharedCyclicalContainer
    mode: RegistrationMode
    storage: SharedCyclicalStorageShape
    edges: SharedCyclicalDependencyEdgeShape
    name: str
    supported: bool
    unsupported_reason: str | None


@dataclass(frozen=True, slots=True)
class GeneratedSharedCyclicalCase:
    suite: str
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]
    a_type: str
    b_type: str
    a_shared_ownership: bool
    b_shared_ownership: bool
    headers: tuple[str, ...]
    supported: bool
    unsupported_reason: str | None


def _unsupported_reason(
    container: SharedCyclicalContainer,
    mode: RegistrationMode,
    storage: SharedCyclicalStorageShape,
    edges: SharedCyclicalDependencyEdgeShape,
) -> str | None:
    if mode.name not in container.supported_modes:
        return f"{container.name} does not support {mode.name} registration"
    if (
        edges.a_to_b.name
        not in storage.b.supported_dependency_edges
    ):
        return (
            f"{storage.b.name} storage for cycle_b does not expose "
            f"{edges.a_to_b.name} dependencies"
        )
    if (
        edges.b_to_a.name
        not in storage.a.supported_dependency_edges
    ):
        return (
            f"{storage.a.name} storage for cycle_a does not expose "
            f"{edges.b_to_a.name} dependencies"
        )
    return None


def generate_shared_cyclical_rows(
    containers: tuple[
        SharedCyclicalContainer, ...
    ] = SHARED_CYCLICAL_CONTAINERS,
    modes: tuple[RegistrationMode, ...] = REGISTRATION_MODES,
    storage_shapes: tuple[
        SharedCyclicalStorageShape, ...
    ] = SHARED_CYCLICAL_STORAGE_SHAPES,
    edge_shapes: tuple[
        SharedCyclicalDependencyEdgeShape, ...
    ] = SHARED_CYCLICAL_DEPENDENCY_EDGE_SHAPES,
) -> tuple[SharedCyclicalRow, ...]:
    for axis, members in (
        ("container", tuple(container.name for container in containers)),
        ("registration mode", tuple(mode.name for mode in modes)),
        ("storage shape", tuple(storage.name for storage in storage_shapes)),
        ("dependency edge shape", tuple(edges.name for edges in edge_shapes)),
    ):
        assert_unique_axis_members("shared cyclical", axis, members)

    rows = tuple(
        SharedCyclicalRow(
            container=container,
            mode=mode,
            storage=storage,
            edges=edges,
            name=(
                f"{container.name}_{mode.name}_{storage.name}_{edges.name}"
            ),
            supported=(reason := _unsupported_reason(
                container,
                mode,
                storage,
                edges,
            ))
            is None,
            unsupported_reason=reason,
        )
        for container in containers
        for mode in modes
        for storage in storage_shapes
        for edges in edge_shapes
    )
    for axis, declared, used in (
        (
            "container",
            {container.name for container in containers},
            {row.container.name for row in rows},
        ),
        (
            "registration mode",
            {mode.name for mode in modes},
            {row.mode.name for row in rows},
        ),
        (
            "storage shape",
            {storage.name for storage in storage_shapes},
            {row.storage.name for row in rows},
        ),
        (
            "dependency edge shape",
            {edges.name for edges in edge_shapes},
            {row.edges.name for row in rows},
        ),
    ):
        assert_axis_members_used("shared cyclical", axis, declared, used)
    return rows


def _container_type(row: SharedCyclicalRow) -> str:
    container_types = dict(row.container.container_types)
    try:
        return container_types[row.mode.name]
    except KeyError as error:
        raise ValueError(
            f"shared cyclical row {row.name} has no container type"
        ) from error


def _cycle_types(row: SharedCyclicalRow) -> tuple[str, str]:
    arguments = (
        f"{row.edges.a_to_b.type_name}, {row.edges.b_to_a.type_name}"
    )
    return (
        f"shared_cyclical_a<{arguments}>",
        f"shared_cyclical_b<{arguments}>",
    )


def _make_case(row: SharedCyclicalRow) -> GeneratedSharedCyclicalCase:
    a_type, b_type = _cycle_types(row)
    if not row.supported:
        return GeneratedSharedCyclicalCase(
            suite="shared_cyclical",
            name=row.name,
            container_type="",
            static_bindings=None,
            setup_lines=(),
            a_type=a_type,
            b_type=b_type,
            a_shared_ownership=row.storage.a.shared_ownership,
            b_shared_ownership=row.storage.b.shared_ownership,
            headers=(),
            supported=False,
            unsupported_reason=row.unsupported_reason,
        )

    a_storage = "dingo::storage<" + row.storage.a.type_expression.format(
        type=a_type
    ) + ">"
    b_storage = "dingo::storage<" + row.storage.b.type_expression.format(
        type=b_type
    ) + ">"
    a_dependencies = (
        "dingo::dependencies<cycle_dependency_t<"
        f"{row.edges.a_to_b.type_name}, {b_type}>>"
    )
    b_dependencies = (
        "dingo::dependencies<cycle_dependency_t<"
        f"{row.edges.b_to_a.type_name}, {a_type}>>"
    )
    static_registrations = (
        RegistrationSpec(
            storage=a_storage,
            dependencies=a_dependencies,
            include_in_runtime=False,
            include_in_mixed=False,
        ),
        RegistrationSpec(
            storage=b_storage,
            dependencies=b_dependencies,
            include_in_runtime=False,
            include_in_mixed=False,
        ),
    )
    runtime_registrations = (
        RegistrationSpec(
            storage=a_storage,
            dependencies=a_dependencies,
            mixed_placement=MixedRegistrationPlacement.RUNTIME,
            include_in_static=False,
        ),
        RegistrationSpec(
            storage=b_storage,
            dependencies=b_dependencies,
            mixed_placement=MixedRegistrationPlacement.RUNTIME,
            include_in_static=False,
        ),
    )
    mixed_anchor = RegistrationSpec(
        scope="dingo::scope<dingo::shared>",
        storage="dingo::storage<shared_cyclical_mixed_anchor>",
        include_in_static=False,
        include_in_runtime=False,
    )
    plan = build_registration_plan_from_recipe(
        RegistrationRecipe(
            scope=SHARED_CYCLICAL_SCOPE,
            storage=a_storage,
            registrations=(
                *static_registrations,
                *runtime_registrations,
                mixed_anchor,
            ),
        )
    )
    registration = render_registration_plan(
        row.mode.name,
        plan,
        context=f"shared cyclical row {row.name}",
    )
    return GeneratedSharedCyclicalCase(
        suite="shared_cyclical",
        name=row.name,
        container_type=_container_type(row),
        static_bindings=registration.static_bindings,
        setup_lines=registration.setup_lines,
        a_type=a_type,
        b_type=b_type,
        a_shared_ownership=row.storage.a.shared_ownership,
        b_shared_ownership=row.storage.b.shared_ownership,
        headers=tuple(
            sorted(
                {
                    *row.container.headers,
                    "matrix/fixtures/shared_cyclical.h",
                }
            )
        ),
        supported=True,
        unsupported_reason=None,
    )


def generate_shared_cyclical_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    claimed_sources: set[Path] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    grouped: dict[tuple[str, str], list[SharedCyclicalRow]] = {}
    for row in generate_shared_cyclical_rows():
        grouped.setdefault((row.container.name, row.mode.name), []).append(row)

    shards = tuple(
        SourceShard(
            executable="dingo_matrix_test_shared_cyclical",
            name=f"shared_cyclical_{container}_{mode}",
            source_context={
                "cases": tuple(_make_case(row) for row in rows),
                "headers": tuple(
                    sorted(
                        {
                            header
                            for row in rows
                            for header in _make_case(row).headers
                        }
                    )
                ),
            },
            runner_context={
                "cases": tuple(_make_case(row) for row in rows),
            },
        )
        for (container, mode), rows in sorted(grouped.items())
    )
    return render_family_executables(
        out_dir,
        source_template,
        runner_template,
        shards,
        claimed_sources,
    )


__all__ = (
    "SharedCyclicalRow",
    "generate_shared_cyclical_executables",
    "generate_shared_cyclical_rows",
)
