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


@dataclass(frozen=True)
class Feature:
    name: str
    function: str
    include: str
    requires: frozenset[str]
    modes: frozenset[str]
    container_requires: frozenset[str] = frozenset()


@dataclass(frozen=True)
class RegistrationMode:
    name: str


@dataclass(frozen=True)
class ScopeSpec:
    name: str
    type_name: str
    provides: frozenset[str]


@dataclass(frozen=True)
class StoredType:
    name: str
    kind: str
    storage: str
    supported_scopes: frozenset[str]
    provides: frozenset[str]


@dataclass(frozen=True)
class ExposedType:
    name: str
    kind: str
    supported_stored_kinds: frozenset[str]
    provides: frozenset[str]


@dataclass(frozen=True)
class ResolvedType:
    name: str
    supported_exposed_types: frozenset[str]
    provides: frozenset[str]


@dataclass(frozen=True)
class ContainerSpec:
    name: str
    modes: frozenset[str]
    container_type: str
    provides: frozenset[str] = frozenset()


@dataclass(frozen=True)
class BindingPlan:
    static_bindings: tuple[str, ...]
    runtime_setup: tuple[str, ...]
    mixed_static_bindings: tuple[str, ...] | None
    mixed_runtime_setup: tuple[str, ...]


@dataclass(frozen=True)
class MatrixRow:
    feature: Feature
    mode: RegistrationMode
    scope: ScopeSpec
    stored_type: StoredType
    exposed_type: ExposedType
    resolved_type: ResolvedType
    container: ContainerSpec
    plan: BindingPlan


@dataclass(frozen=True)
class GeneratedCase:
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]


@dataclass(frozen=True)
class GeneratedTest:
    suite: str
    name: str
    function: str
    case_name: str


FEATURES = (
    Feature(
        name="construct_invoke",
        function="run_construct_invoke",
        include="matrix/features/construct_invoke.h",
        requires=frozenset(
            {"concrete_binding", "shared_storage", "resolved_concrete"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_concrete",
        function="run_resolve_concrete",
        include="matrix/features/resolve_concrete.h",
        requires=frozenset(
            {"concrete_binding", "shared_storage", "resolved_concrete"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_interface",
        function="run_resolve_interface",
        include="matrix/features/resolve_interface.h",
        requires=frozenset(
            {"interface_binding", "shared_storage", "resolved_shared_interface"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_collection",
        function="run_resolve_collection",
        include="matrix/features/resolve_collection.h",
        requires=frozenset(
            {"collection_binding", "shared_storage", "resolved_collection"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_keyed",
        function="run_resolve_keyed",
        include="matrix/features/resolve_keyed.h",
        requires=frozenset({"keyed_binding", "shared_storage", "resolved_keyed"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_unique_wrapper",
        function="run_resolve_unique_wrapper",
        include="matrix/features/resolve_unique_wrapper.h",
        requires=frozenset(
            {"interface_binding", "unique_storage", "resolved_unique_wrapper"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_indexed",
        function="run_resolve_indexed",
        include="matrix/features/resolve_indexed.h",
        requires=frozenset(
            {"indexed_binding", "shared_storage", "resolved_indexed"}
        ),
        modes=frozenset({"runtime"}),
        container_requires=frozenset({"indexed_container"}),
    ),
)


REGISTRATION_MODES = (
    RegistrationMode("runtime"),
    RegistrationMode("static"),
    RegistrationMode("mixed"),
)


SCOPES = (
    ScopeSpec(
        name="shared",
        type_name="dingo::scope<dingo::shared>",
        provides=frozenset({"shared_storage"}),
    ),
    ScopeSpec(
        name="unique",
        type_name="dingo::scope<dingo::unique>",
        provides=frozenset({"unique_storage"}),
    ),
)


STORED_TYPES = (
    StoredType(
        name="config_value",
        kind="config",
        storage="dingo::storage<config>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        name="service_shared_ptr",
        kind="service",
        storage="dingo::storage<std::shared_ptr<service>>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_shared_ptr"}),
    ),
    StoredType(
        name="processor_shared_ptr",
        kind="processor",
        storage="dingo::storage<std::shared_ptr<processor<0>>>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_shared_ptr"}),
    ),
    StoredType(
        name="unique_service_unique_ptr",
        kind="unique_service",
        storage="dingo::storage<std::unique_ptr<unique_service>>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset({"stored_unique_ptr"}),
    ),
)


EXPOSED_TYPES = (
    ExposedType(
        name="concrete",
        kind="concrete",
        supported_stored_kinds=frozenset({"config"}),
        provides=frozenset({"concrete_binding"}),
    ),
    ExposedType(
        name="service_interface",
        kind="interface",
        supported_stored_kinds=frozenset({"service"}),
        provides=frozenset({"concrete_binding", "interface_binding"}),
    ),
    ExposedType(
        name="unique_interface",
        kind="interface",
        supported_stored_kinds=frozenset({"unique_service"}),
        provides=frozenset({"interface_binding"}),
    ),
    ExposedType(
        name="processor_collection",
        kind="collection",
        supported_stored_kinds=frozenset({"processor"}),
        provides=frozenset({"collection_binding"}),
    ),
    ExposedType(
        name="processor_keyed",
        kind="keyed",
        supported_stored_kinds=frozenset({"processor"}),
        provides=frozenset({"keyed_binding"}),
    ),
    ExposedType(
        name="processor_indexed",
        kind="indexed",
        supported_stored_kinds=frozenset({"processor"}),
        provides=frozenset({"indexed_binding"}),
    ),
)


RESOLVED_TYPES = (
    ResolvedType(
        name="config_ref_ptr",
        supported_exposed_types=frozenset({"concrete", "service_interface"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="service_interface_ref_ptr_shared_ptr",
        supported_exposed_types=frozenset({"service_interface"}),
        provides=frozenset({"resolved_shared_interface"}),
    ),
    ResolvedType(
        name="unique_interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface"}),
        provides=frozenset({"resolved_unique_wrapper"}),
    ),
    ResolvedType(
        name="processor_vector",
        supported_exposed_types=frozenset({"processor_collection"}),
        provides=frozenset({"resolved_collection"}),
    ),
    ResolvedType(
        name="processor_keyed_shared_ptr_ref",
        supported_exposed_types=frozenset({"processor_keyed"}),
        provides=frozenset({"resolved_keyed"}),
    ),
    ResolvedType(
        name="processor_indexed_shared_ptr",
        supported_exposed_types=frozenset({"processor_indexed"}),
        provides=frozenset({"resolved_indexed"}),
    ),
)


CONTAINERS = (
    ContainerSpec(
        name="runtime_container",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<>",
    ),
    ContainerSpec(
        name="container_runtime",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<>",
    ),
    ContainerSpec(
        name="static_container",
        modes=frozenset({"static"}),
        container_type="dingo::static_container<static_bindings>",
    ),
    ContainerSpec(
        name="container_static",
        modes=frozenset({"static"}),
        container_type="dingo::container<static_bindings>",
    ),
    ContainerSpec(
        name="container_mixed",
        modes=frozenset({"mixed"}),
        container_type="dingo::container<static_bindings>",
    ),
    ContainerSpec(
        name="runtime_container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_container_traits>",
        provides=frozenset({"indexed_container"}),
    ),
    ContainerSpec(
        name="container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_container_traits>",
        provides=frozenset({"indexed_container"}),
    ),
)


FILTER_RULES = (
    "container_mode",
    "feature_mode",
    "scope_supports_stored_type",
    "exposure_supports_stored_type",
    "resolved_type_supports_exposure",
    "feature_requires_tags",
    "container_requires",
)


def binding(
    scope: ScopeSpec,
    storage: str,
    *,
    interfaces: str | None = None,
    key: str | None = None,
) -> str:
    parts = [scope.type_name, storage]
    if interfaces:
        parts.append(interfaces)
    if key:
        parts.append(key)
    return "dingo::bind<" + ", ".join(parts) + ">"


def register_type(
    scope: ScopeSpec,
    storage: str,
    *,
    interfaces: str | None = None,
    key: str | None = None,
) -> str:
    parts = [scope.type_name, storage]
    if interfaces:
        parts.append(interfaces)
    if key:
        parts.append(key)
    return "container.template register_type<" + ", ".join(parts) + ">();"


def register_collection() -> str:
    return (
        "container.template register_type_collection<"
        "dingo::scope<dingo::unique>, "
        "dingo::storage<std::vector<std::shared_ptr<"
        "processor_interface>>>>();"
    )


def bindings_type(bindings: tuple[str, ...]) -> str:
    return "dingo::bindings<" + ", ".join(bindings) + ">"


def config_binding(scope: ScopeSpec) -> str:
    return binding(scope, "dingo::storage<config>")


def config_registration(scope: ScopeSpec) -> str:
    return register_type(scope, "dingo::storage<config>")


def processor_binding(scope: ScopeSpec, index: int, key: str | None = None) -> str:
    return binding(
        scope,
        f"dingo::storage<std::shared_ptr<processor<{index}>>>",
        interfaces="dingo::interfaces<processor_interface>",
        key=key,
    )


def processor_registration(scope: ScopeSpec, index: int, key: str | None = None) -> str:
    return register_type(
        scope,
        f"dingo::storage<std::shared_ptr<processor<{index}>>>",
        interfaces="dingo::interfaces<processor_interface>",
        key=key,
    )


def build_plan(
    scope: ScopeSpec, stored_type: StoredType, exposed_type: ExposedType
) -> BindingPlan | None:
    if exposed_type.name == "concrete" and stored_type.name == "config_value":
        bind_config = config_binding(scope)
        register_config = config_registration(scope)
        return BindingPlan(
            static_bindings=(bind_config,),
            runtime_setup=(register_config,),
            mixed_static_bindings=(bind_config,),
            mixed_runtime_setup=(),
        )

    if (
        exposed_type.name == "service_interface"
        and stored_type.name == "service_shared_ptr"
    ):
        bind_config = config_binding(scope)
        bind_service = binding(
            scope,
            stored_type.storage,
            interfaces="dingo::interfaces<service_interface>",
        )
        register_config = config_registration(scope)
        register_service = register_type(
            scope,
            stored_type.storage,
            interfaces="dingo::interfaces<service_interface>",
        )
        return BindingPlan(
            static_bindings=(bind_config, bind_service),
            runtime_setup=(register_config, register_service),
            mixed_static_bindings=(bind_config,),
            mixed_runtime_setup=(register_service,),
        )

    if (
        exposed_type.name == "unique_interface"
        and stored_type.name == "unique_service_unique_ptr"
    ):
        bind_unique = binding(
            scope,
            stored_type.storage,
            interfaces="dingo::interfaces<unique_interface>",
        )
        register_unique = register_type(
            scope,
            stored_type.storage,
            interfaces="dingo::interfaces<unique_interface>",
        )
        return BindingPlan(
            static_bindings=(bind_unique,),
            runtime_setup=(register_unique,),
            mixed_static_bindings=(bind_unique,),
            mixed_runtime_setup=(),
        )

    if (
        exposed_type.name == "processor_collection"
        and stored_type.name == "processor_shared_ptr"
    ):
        bind_first = processor_binding(scope, 0)
        bind_second = processor_binding(scope, 1)
        register_first = processor_registration(scope, 0)
        register_second = processor_registration(scope, 1)
        return BindingPlan(
            static_bindings=(bind_first, bind_second),
            runtime_setup=(register_collection(), register_first, register_second),
            mixed_static_bindings=(bind_first,),
            mixed_runtime_setup=(register_collection(), register_second),
        )

    if (
        exposed_type.name == "processor_keyed"
        and stored_type.name == "processor_shared_ptr"
    ):
        bind_first = processor_binding(scope, 0, "dingo::key<first_key>")
        bind_second = processor_binding(scope, 1, "dingo::key<second_key>")
        register_first = processor_registration(scope, 0, "dingo::key<first_key>")
        register_second = processor_registration(scope, 1, "dingo::key<second_key>")
        return BindingPlan(
            static_bindings=(bind_first, bind_second),
            runtime_setup=(register_first, register_second),
            mixed_static_bindings=(bind_first,),
            mixed_runtime_setup=(register_second,),
        )

    if (
        exposed_type.name == "processor_indexed"
        and stored_type.name == "processor_shared_ptr"
    ):
        return BindingPlan(
            static_bindings=(),
            runtime_setup=(
                "container.template register_indexed_type<"
                f"{scope.type_name}, "
                "dingo::storage<std::shared_ptr<processor<0>>>, "
                "dingo::interfaces<processor_interface>>(0);",
                "container.template register_indexed_type<"
                f"{scope.type_name}, "
                "dingo::storage<std::shared_ptr<processor<1>>>, "
                "dingo::interfaces<processor_interface>>(1);",
            ),
            mixed_static_bindings=None,
            mixed_runtime_setup=(),
        )

    return None


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
    if scope.name not in stored_type.supported_scopes:
        return "scope_supports_stored_type"
    if stored_type.kind not in exposed_type.supported_stored_kinds:
        return "exposure_supports_stored_type"
    if exposed_type.name not in resolved_type.supported_exposed_types:
        return "resolved_type_supports_exposure"

    tags = row_tags(mode, scope, stored_type, exposed_type, resolved_type, container)
    if not feature.requires <= tags:
        return "feature_requires_tags"
    if not feature.container_requires <= container.provides:
        return "container_requires"

    return None


def generate_rows() -> tuple[MatrixRow, ...]:
    rows: list[MatrixRow] = []
    rejected: dict[str, int] = defaultdict(int)

    for feature in FEATURES:
        for mode in REGISTRATION_MODES:
            for scope in SCOPES:
                for stored_type in STORED_TYPES:
                    for exposed_type in EXPOSED_TYPES:
                        for resolved_type in RESOLVED_TYPES:
                            for container in CONTAINERS:
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

                                plan = build_plan(scope, stored_type, exposed_type)
                                if plan is None:
                                    continue
                                if mode.name == "static" and not plan.static_bindings:
                                    continue
                                if (
                                    mode.name == "mixed"
                                    and plan.mixed_static_bindings is None
                                ):
                                    continue

                                rows.append(
                                    MatrixRow(
                                        feature=feature,
                                        mode=mode,
                                        scope=scope,
                                        stored_type=stored_type,
                                        exposed_type=exposed_type,
                                        resolved_type=resolved_type,
                                        container=container,
                                        plan=plan,
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


def assert_coverage(rows: list[MatrixRow], rejected: dict[str, int]) -> None:
    if not rows:
        raise RuntimeError("matrix did not generate any rows")

    row_tuple = tuple(rows)
    assert_axis_used(row_tuple, "feature", {feature.name for feature in FEATURES})
    assert_axis_used(row_tuple, "mode", {mode.name for mode in REGISTRATION_MODES})
    assert_axis_used(row_tuple, "scope", {scope.name for scope in SCOPES})
    assert_axis_used(
        row_tuple, "stored_type", {stored_type.name for stored_type in STORED_TYPES}
    )
    assert_axis_used(
        row_tuple, "exposed_type", {exposed_type.name for exposed_type in EXPOSED_TYPES}
    )
    assert_axis_used(
        row_tuple,
        "resolved_type",
        {resolved_type.name for resolved_type in RESOLVED_TYPES},
    )
    assert_axis_used(
        row_tuple, "container", {container.name for container in CONTAINERS}
    )

    missing_filters = sorted(rule for rule in FILTER_RULES if rejected[rule] == 0)
    if missing_filters:
        raise RuntimeError(f"matrix filters were not exercised: {missing_filters}")


def case_name(row: MatrixRow) -> str:
    return "_".join(
        (
            row.mode.name,
            row.scope.name,
            row.stored_type.name,
            row.exposed_type.name,
            row.resolved_type.name,
            row.container.name,
        )
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
                function=feature.function,
                case_name=case.name,
            )
        )

    if not tests:
        raise RuntimeError(f"feature {feature.name} did not generate any tests")

    source = out_dir / f"{feature.name}.cpp"
    rendered = env.get_template("source.cpp.j2").render(
        includes=[feature.include],
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
    sources = [generate_feature(feature, rows, out_dir, env) for feature in FEATURES]
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
