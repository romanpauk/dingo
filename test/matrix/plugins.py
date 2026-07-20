#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from schema import (
    ExposedType,
    Feature,
    FeatureCaseSpec,
    MixedRegistrationPlacement,
    RegistrationMode,
    RegistrationSpec,
    ResolvedType,
    ScopeSpec,
    StoredType,
)


class CheckRow(Protocol):
    feature: Feature
    feature_case: FeatureCaseSpec
    mode: RegistrationMode
    scope: ScopeSpec
    stored_type: StoredType
    exposed_type: ExposedType
    resolved_type: ResolvedType


@dataclass(frozen=True, slots=True)
class CaseLines:
    policy: str
    policy_source: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class RegistrationPlan:
    static_bindings: tuple[str, ...]
    runtime_setup: tuple[str, ...]
    mixed_static_bindings: tuple[str, ...] | None
    mixed_runtime_setup: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class RegistrationRecipe:
    scope: ScopeSpec
    storage: str
    registrations: tuple[RegistrationSpec, ...]
    runtime_setup: tuple[str, ...] = ()
    runtime_argument: str | None = None
    factory: str | None = None
    runtime_prefix: tuple[str, ...] = ()
    mixed_runtime_prefix: tuple[str, ...] = ()
    mixed: bool = True


@dataclass(frozen=True, slots=True)
class RenderedRegistrationPlan:
    static_bindings: str | None
    setup_lines: tuple[str, ...]


LIFETIME_POLICIES: dict[tuple[str, str, str, str], str] = {
    (
        "shared",
        "value_type",
        "concrete",
        "value_ref_ptr",
    ): "resolution::lifetime<resolution::constructed<resolution::requests::direct<value_type&>>, value_type, 1, 0, 0, 0, 0>",
    (
        "shared",
        "value_shared_ptr",
        "concrete",
        "value_shared_ptr",
    ): "resolution::lifetime<resolution::pointee_constructed<resolution::requests::direct<std::shared_ptr<value_type>>>, value_type, 1, 0, 0, 0, 0>",
    (
        "shared",
        "value_optional",
        "concrete",
        "value_optional_ref",
    ): "resolution::lifetime<resolution::optional_ref<value_type>, value_type, 1, 0, 0, 1, 3>",
    (
        "unique",
        "unique_value_type",
        "concrete",
        "value_rvalue",
    ): "resolution::lifetime<resolution::constructed<resolution::requests::direct<value_type&&>>, value_type, 1, 0, 0, 1, 3>",
    (
        "unique",
        "value_unique_ptr",
        "concrete",
        "value_unique_ptr",
    ): "resolution::lifetime<resolution::unique_ptr<value_type>, value_type, 1, 0, 0, 0, 0>",
    (
        "unique",
        "value_optional",
        "concrete",
        "value_optional",
    ): "resolution::lifetime<resolution::optional<value_type>, value_type, 1, 0, 1, 1, 3>",
}


def select_case_lines(row: CheckRow) -> CaseLines | None:
    if row.feature.name == "lifetime_counts":
        policy_key = (
            row.scope.name,
            row.stored_type.id,
            row.exposed_type.name,
            row.resolved_type.name,
        )
        policy = LIFETIME_POLICIES.get(policy_key)
        if policy is not None:
            return CaseLines(
                policy=policy,
                policy_source=("lifetime", *policy_key),
            )
        return None

    if row.feature_case.policy is not None:
        return CaseLines(
            policy=row.feature_case.policy,
            policy_source=(
                "feature_case",
                row.feature_case.feature,
                row.feature_case.name,
            ),
        )

    resolution_policy = dict(row.resolved_type.policies).get(row.feature.name)
    if resolution_policy is not None:
        return CaseLines(
            policy=resolution_policy,
            policy_source=("resolved_type", row.resolved_type.name, row.feature.name),
        )

    if row.feature.policy is not None:
        return CaseLines(
            policy=row.feature.policy,
            policy_source=("feature", row.feature.name),
        )

    return None


def registration_arguments(
    scope: str,
    storage: str,
    *,
    interfaces: str | None = None,
    local_bindings: str | None = None,
    factory: str | None = None,
    dependencies: str | None = None,
    key: str | None = None,
) -> tuple[str, ...]:
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
    return tuple(parts)


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
    parts = registration_arguments(
        scope,
        storage,
        interfaces=interfaces,
        local_bindings=local_bindings,
        factory=factory,
        dependencies=dependencies,
        key=key,
    )
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
    argument: str | None = None,
) -> str:
    parts = registration_arguments(
        scope,
        storage,
        interfaces=interfaces,
        local_bindings=local_bindings,
        factory=factory,
        dependencies=dependencies,
        key=key,
    )
    call_arguments = "" if argument is None else argument
    return (
        "container.template register_type<"
        + ", ".join(parts)
        + f">({call_arguments});"
    )


def bindings_type(bindings: tuple[str, ...]) -> str:
    return "dingo::bindings<" + ", ".join(bindings) + ">"


def spec_storage(spec: RegistrationSpec, recipe: RegistrationRecipe) -> str:
    return spec.storage or recipe.storage


def spec_scope(recipe: RegistrationRecipe, spec: RegistrationSpec) -> str:
    return spec.scope or recipe.scope.type_name


def spec_runtime_argument(
    spec: RegistrationSpec, recipe: RegistrationRecipe
) -> str | None:
    if spec.runtime_argument is not None:
        return spec.runtime_argument
    if spec.storage is None:
        return recipe.runtime_argument
    return None


def spec_runtime_setup(
    spec: RegistrationSpec, recipe: RegistrationRecipe
) -> tuple[str, ...]:
    if spec.runtime_argument is not None:
        return spec.runtime_setup
    if spec.storage is None and recipe.runtime_argument is not None:
        return recipe.runtime_setup
    return ()


def recipe_factory(
    recipe: RegistrationRecipe, spec: RegistrationSpec
) -> str | None:
    if spec.storage is None:
        return recipe.factory
    return None


def render_static_binding(
    recipe: RegistrationRecipe, spec: RegistrationSpec
) -> str:
    return binding(
        spec_scope(recipe, spec),
        spec_storage(spec, recipe),
        interfaces=spec.interfaces,
        local_bindings=spec.local_bindings,
        factory=spec.factory or recipe_factory(recipe, spec),
        dependencies=spec.dependencies,
        key=spec.key,
    )


def render_runtime_registration(
    recipe: RegistrationRecipe, spec: RegistrationSpec
) -> str:
    storage = spec_storage(spec, recipe)
    scope_type = spec_scope(recipe, spec)
    if spec.indexed_key is not None:
        return register_type(
            scope_type,
            storage,
            interfaces=spec.interfaces,
            argument=f"dingo::key_value{{{spec.indexed_key}}}",
        )

    return register_type(
        scope_type,
        storage,
        interfaces=spec.interfaces,
        local_bindings=spec.local_bindings,
        factory=spec.factory or recipe_factory(recipe, spec),
        dependencies=spec.dependencies,
        key=spec.key,
        argument=spec_runtime_argument(spec, recipe),
    )


def runtime_setup_prefix(
    recipe: RegistrationRecipe, specs: tuple[RegistrationSpec, ...]
) -> tuple[str, ...]:
    lines: list[str] = []
    for spec in specs:
        for line in spec_runtime_setup(spec, recipe):
            if line not in lines:
                lines.append(line)
    return tuple(lines)


def build_registration_plan_from_recipe(
    recipe: RegistrationRecipe,
) -> RegistrationPlan:
    specs = recipe.registrations
    static_specs = tuple(spec for spec in specs if spec.include_in_static)
    runtime_specs = tuple(spec for spec in specs if spec.include_in_runtime)
    mixed_static_specs = tuple(
        spec
        for spec in specs
        if spec.include_in_mixed
        if spec.mixed_placement is MixedRegistrationPlacement.STATIC
    )
    mixed_runtime_specs = tuple(
        spec
        for spec in specs
        if spec.include_in_mixed
        if spec.mixed_placement is MixedRegistrationPlacement.RUNTIME
    )

    return RegistrationPlan(
        static_bindings=tuple(
            render_static_binding(recipe, spec) for spec in static_specs
        ),
        runtime_setup=(
            runtime_setup_prefix(recipe, runtime_specs)
            + recipe.runtime_prefix
            + tuple(
                render_runtime_registration(recipe, spec)
                for spec in runtime_specs
            )
        ),
        mixed_static_bindings=(
            tuple(
                render_static_binding(recipe, spec)
                for spec in mixed_static_specs
            )
            if recipe.mixed
            else None
        ),
        mixed_runtime_setup=(
            runtime_setup_prefix(recipe, mixed_runtime_specs)
            + recipe.mixed_runtime_prefix
            + tuple(
                render_runtime_registration(recipe, spec)
                for spec in mixed_runtime_specs
            )
        ),
    )


def build_registration_plan(
    scope: ScopeSpec, stored_type: StoredType, exposed_type: ExposedType
) -> RegistrationPlan:
    return build_registration_plan_from_recipe(
        RegistrationRecipe(
            scope=scope,
            storage=stored_type.storage,
            registrations=exposed_type.registrations,
            runtime_setup=stored_type.runtime_setup,
            runtime_argument=stored_type.runtime_argument,
            factory=stored_type.factory,
            runtime_prefix=exposed_type.runtime_prefix,
            mixed_runtime_prefix=exposed_type.mixed_runtime_prefix,
            mixed=exposed_type.mixed,
        )
    )


def render_registration_plan(
    mode: str,
    plan: RegistrationPlan,
    *,
    context: str,
) -> RenderedRegistrationPlan:
    if mode == "static":
        if not plan.static_bindings:
            raise RuntimeError(f"{context} has no static bindings")
        return RenderedRegistrationPlan(
            static_bindings=bindings_type(plan.static_bindings),
            setup_lines=(),
        )
    if mode == "runtime":
        return RenderedRegistrationPlan(
            static_bindings=None,
            setup_lines=plan.runtime_setup,
        )
    if mode == "mixed":
        if not plan.mixed_static_bindings:
            raise RuntimeError(f"{context} has no mixed registration plan")
        return RenderedRegistrationPlan(
            static_bindings=bindings_type(plan.mixed_static_bindings),
            setup_lines=plan.mixed_runtime_setup,
        )
    raise RuntimeError(f"{context} has unknown registration mode: {mode}")
