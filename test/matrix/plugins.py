#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from model import (
    FEATURES,
    ExposedType,
    Feature,
    RegistrationSpec,
    ResolvedType,
    ScopeSpec,
    StoredType,
)


class CheckRow(Protocol):
    feature: Feature
    mode: object
    scope: ScopeSpec
    resolved_type: ResolvedType


@dataclass(frozen=True)
class RegistrationPlan:
    static_bindings: tuple[str, ...]
    runtime_setup: tuple[str, ...]
    mixed_static_bindings: tuple[str, ...] | None
    mixed_runtime_setup: tuple[str, ...]


class CheckPlugin(Protocol):
    def emit(self, row: CheckRow) -> tuple[str, ...] | None:
        ...


class ResolvedTypeCheckPlugin:
    def emit(self, row: CheckRow) -> tuple[str, ...] | None:
        return dict(row.resolved_type.checks).get(row.feature.name)


class FeatureCheckPlugin:
    def emit(self, row: CheckRow) -> tuple[str, ...] | None:
        if row.feature.checks:
            return row.feature.checks
        return None


RowFilter = Callable[[CheckRow], bool]


ROW_FILTERS: tuple[RowFilter, ...] = ()


@dataclass(frozen=True)
class FeatureCasePlugin:
    feature: Feature
    check_plugins: tuple[CheckPlugin, ...]
    row_filters: tuple[RowFilter, ...]

    def emit(self, row: CheckRow) -> tuple[str, ...] | None:
        if row.feature != self.feature:
            return None
        if not all(row_filter(row) for row_filter in self.row_filters):
            return None
        for plugin in self.check_plugins:
            lines = plugin.emit(row)
            if lines is not None:
                return lines
        return None


FEATURE_CASE_PLUGINS: tuple[FeatureCasePlugin, ...] = tuple(
    FeatureCasePlugin(
        feature=feature,
        check_plugins=(
            ResolvedTypeCheckPlugin(),
            FeatureCheckPlugin(),
        ),
        row_filters=ROW_FILTERS,
    )
    for feature in FEATURES
    if feature.implemented
)


def check_lines_for(row: CheckRow) -> tuple[str, ...] | None:
    for plugin in FEATURE_CASE_PLUGINS:
        lines = plugin.emit(row)
        if lines is not None:
            return lines
    return None


def binding(
    scope: str,
    storage: str,
    *,
    interfaces: str | None = None,
    local_bindings: str | None = None,
    factory: str | None = None,
    dependencies: str | None = None,
    key: str | None = None,
) -> str:
    parts = [scope, storage]
    if interfaces:
        parts.append(interfaces)
    if local_bindings:
        parts.append(local_bindings)
    if factory:
        parts.append(factory)
    if dependencies:
        parts.append(dependencies)
    if key:
        parts.append(key)
    return "dingo::bind<" + ", ".join(parts) + ">"


def register_type(
    scope: str,
    storage: str,
    *,
    interfaces: str | None = None,
    local_bindings: str | None = None,
    factory: str | None = None,
    dependencies: str | None = None,
    key: str | None = None,
) -> str:
    parts = [scope, storage]
    if interfaces:
        parts.append(interfaces)
    if local_bindings:
        parts.append(local_bindings)
    if factory:
        parts.append(factory)
    if dependencies:
        parts.append(dependencies)
    if key:
        parts.append(key)
    return "container.template register_type<" + ", ".join(parts) + ">();"


def register_type_with_arg(
    scope: str,
    storage: str,
    argument: str,
    *,
    interfaces: str | None = None,
    local_bindings: str | None = None,
    factory: str | None = None,
    dependencies: str | None = None,
    key: str | None = None,
) -> str:
    registration = register_type(
        scope,
        storage,
        interfaces=interfaces,
        local_bindings=local_bindings,
        factory=factory,
        dependencies=dependencies,
        key=key,
    )
    return registration[:-3] + f"({argument});"


def bindings_type(bindings: tuple[str, ...]) -> str:
    return "dingo::bindings<" + ", ".join(bindings) + ">"


def spec_storage(spec: RegistrationSpec, stored_type: StoredType) -> str:
    return spec.storage or stored_type.storage


def spec_scope(scope: ScopeSpec, spec: RegistrationSpec) -> str:
    return spec.scope or scope.type_name


def spec_runtime_argument(spec: RegistrationSpec, stored_type: StoredType) -> str | None:
    if spec.runtime_argument is not None:
        return spec.runtime_argument
    if spec.storage is None:
        return stored_type.runtime_argument
    return None


def spec_runtime_setup(spec: RegistrationSpec, stored_type: StoredType) -> tuple[str, ...]:
    if spec.runtime_argument is not None:
        return spec.runtime_setup
    if spec.storage is None and stored_type.runtime_argument is not None:
        return stored_type.runtime_setup
    return ()


def stored_type_factory(stored_type: StoredType, spec: RegistrationSpec) -> str | None:
    if spec.storage is None:
        return stored_type.factory
    return None


class RegistrationPlugin:
    def render_static_binding(
        self, scope: ScopeSpec, stored_type: StoredType, spec: RegistrationSpec
    ) -> str:
        return binding(
            spec_scope(scope, spec),
            spec_storage(spec, stored_type),
            interfaces=spec.interfaces,
            local_bindings=spec.local_bindings,
            factory=spec.factory or stored_type_factory(stored_type, spec),
            dependencies=spec.dependencies,
            key=spec.key,
        )

    def render_runtime_registration(
        self, scope: ScopeSpec, stored_type: StoredType, spec: RegistrationSpec
    ) -> str:
        storage = spec_storage(spec, stored_type)
        factory = spec.factory or stored_type_factory(stored_type, spec)
        scope_type = spec_scope(scope, spec)
        if spec.indexed_key is not None:
            parts = [scope_type, storage]
            if spec.interfaces:
                parts.append(spec.interfaces)
            return (
                "container.template register_indexed_type<"
                + ", ".join(parts)
                + f">({spec.indexed_key});"
            )

        argument = spec_runtime_argument(spec, stored_type)
        if argument is not None:
            return register_type_with_arg(
                scope_type,
                storage,
                argument,
                interfaces=spec.interfaces,
                local_bindings=spec.local_bindings,
                factory=factory,
                dependencies=spec.dependencies,
                key=spec.key,
            )
        return register_type(
            scope_type,
            storage,
            interfaces=spec.interfaces,
            local_bindings=spec.local_bindings,
            factory=factory,
            dependencies=spec.dependencies,
            key=spec.key,
        )

    def runtime_setup_prefix(
        self, stored_type: StoredType, specs: tuple[RegistrationSpec, ...]
    ) -> tuple[str, ...]:
        lines: list[str] = []
        for spec in specs:
            for line in spec_runtime_setup(spec, stored_type):
                if line not in lines:
                    lines.append(line)
        return tuple(lines)

    def build(
        self, scope: ScopeSpec, stored_type: StoredType, exposed_type: ExposedType
    ) -> RegistrationPlan:
        specs = exposed_type.registrations
        static_specs = tuple(spec for spec in specs if spec.static)
        mixed_static_specs = tuple(spec for spec in specs if spec.mixed == "static")
        mixed_runtime_specs = tuple(spec for spec in specs if spec.mixed == "runtime")

        return RegistrationPlan(
            static_bindings=tuple(
                self.render_static_binding(scope, stored_type, spec)
                for spec in static_specs
            ),
            runtime_setup=(
                self.runtime_setup_prefix(stored_type, specs)
                + exposed_type.runtime_prefix
                + tuple(
                    self.render_runtime_registration(scope, stored_type, spec)
                    for spec in specs
                )
            ),
            mixed_static_bindings=(
                tuple(
                    self.render_static_binding(scope, stored_type, spec)
                    for spec in mixed_static_specs
                )
                if exposed_type.mixed
                else None
            ),
            mixed_runtime_setup=(
                self.runtime_setup_prefix(stored_type, mixed_runtime_specs)
                + exposed_type.mixed_runtime_prefix
                + tuple(
                    self.render_runtime_registration(scope, stored_type, spec)
                    for spec in mixed_runtime_specs
                )
            ),
        )


REGISTRATION_PLUGIN = RegistrationPlugin()
