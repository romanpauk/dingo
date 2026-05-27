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
    implemented: bool = True


@dataclass(frozen=True)
class RegistrationMode:
    name: str


@dataclass(frozen=True)
class ScopeSpec:
    name: str
    type_name: str
    provides: frozenset[str]
    implemented: bool = True


@dataclass(frozen=True)
class StoredType:
    id: str
    name: str
    kind: str
    storage: str
    supported_scopes: frozenset[str]
    provides: frozenset[str]
    supported_modes: frozenset[str] = frozenset({"runtime", "static", "mixed"})
    runtime_setup: tuple[str, ...] = ()
    runtime_argument: str | None = None
    implemented: bool = True


@dataclass(frozen=True)
class RegistrationSpec:
    storage: str | None = None
    interfaces: str | None = None
    key: str | None = None
    indexed_key: str | None = None
    mixed: str = "static"
    static: bool = True


@dataclass(frozen=True)
class ExposedType:
    name: str
    kind: str
    supported_stored_kinds: frozenset[str]
    provides: frozenset[str]
    registrations: tuple[RegistrationSpec, ...]
    runtime_prefix: tuple[str, ...] = ()
    mixed_runtime_prefix: tuple[str, ...] = ()
    mixed: bool = True
    implemented: bool = True


@dataclass(frozen=True)
class ResolvedType:
    name: str
    supported_exposed_types: frozenset[str]
    provides: frozenset[str]
    implemented: bool = True


@dataclass(frozen=True)
class ContainerSpec:
    name: str
    modes: frozenset[str]
    container_type: str
    provides: frozenset[str] = frozenset()
    implemented: bool = True


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
            {"concrete_binding", "stable_concrete_storage", "resolved_concrete"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_concrete",
        function="run_resolve_concrete",
        include="matrix/features/resolve_concrete.h",
        requires=frozenset(
            {"concrete_binding", "stable_concrete_storage", "resolved_concrete"}
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
        name="resolve_wrapper",
        function="",
        include="",
        requires=frozenset({"resolved_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
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
    Feature(
        name="resolve_keyed_collection",
        function="",
        include="",
        requires=frozenset({"keyed_collection_binding", "resolved_keyed_collection"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="construct",
        function="",
        include="",
        requires=frozenset({"constructable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="invoke",
        function="",
        include="",
        requires=frozenset({"invokable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="construct_collection",
        function="",
        include="",
        requires=frozenset({"collection_binding", "constructable_collection"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="local_bindings",
        function="",
        include="",
        requires=frozenset({"local_bindings"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="nested_container",
        function="",
        include="",
        requires=frozenset({"nested_container"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="annotated",
        function="",
        include="",
        requires=frozenset({"annotated_binding", "resolved_annotated"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="variant",
        function="",
        include="",
        requires=frozenset({"variant_binding", "resolved_variant"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="array",
        function="",
        include="",
        requires=frozenset({"array_binding", "resolved_array"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
    ),
    Feature(
        name="factory_override",
        function="",
        include="",
        requires=frozenset({"factory_override"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        implemented=False,
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
        provides=frozenset({"shared_storage", "stable_concrete_storage"}),
    ),
    ScopeSpec(
        name="unique",
        type_name="dingo::scope<dingo::unique>",
        provides=frozenset({"unique_storage"}),
    ),
    ScopeSpec(
        name="external",
        type_name="dingo::scope<dingo::external>",
        provides=frozenset({"external_storage", "stable_concrete_storage"}),
    ),
    ScopeSpec(
        name="shared_cyclical",
        type_name="dingo::scope<dingo::shared_cyclical>",
        provides=frozenset({"shared_storage", "stable_concrete_storage"}),
        implemented=False,
    ),
)


STORED_TYPES = (
    StoredType(
        id="value_type",
        name="value_type",
        kind="value_type",
        storage="dingo::storage<value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        id="external_value_type",
        name="external<value_type>",
        kind="value_type",
        storage="dingo::storage<value_type>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_external_value"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("value_type external_value;",),
        runtime_argument="std::move(external_value)",
    ),
    StoredType(
        id="external_value_ref",
        name="external<value_type&>",
        kind="value_type",
        storage="dingo::storage<value_type&>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_external_reference"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("value_type external_value;",),
        runtime_argument="external_value",
    ),
    StoredType(
        id="external_value_ptr",
        name="external<value_type*>",
        kind="value_type",
        storage="dingo::storage<value_type*>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_external_pointer"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("value_type external_value;",),
        runtime_argument="&external_value",
    ),
    StoredType(
        id="value_ptr",
        name="value_type*",
        kind="value_type",
        storage="dingo::storage<value_type*>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_pointer"}),
        implemented=False,
    ),
    StoredType(
        id="value_ref",
        name="value_type&",
        kind="value_type",
        storage="dingo::storage<value_type&>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_reference"}),
        implemented=False,
    ),
    StoredType(
        id="external_value_shared_ptr",
        name="external<std::shared_ptr<value_type>>",
        kind="value_type",
        storage="dingo::storage<std::shared_ptr<value_type>>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_external_shared_ptr"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("auto external_value = std::make_shared<value_type>();",),
        runtime_argument="external_value",
    ),
    StoredType(
        id="value_unique_ptr",
        name="std::unique_ptr<value_type>",
        kind="value_type",
        storage="dingo::storage<std::unique_ptr<value_type>>",
        supported_scopes=frozenset({"unique", "external"}),
        provides=frozenset({"stored_unique_ptr"}),
        implemented=False,
    ),
    StoredType(
        id="value_shared_ptr",
        name="std::shared_ptr<value_type>",
        kind="value_type",
        storage="dingo::storage<std::shared_ptr<value_type>>",
        supported_scopes=frozenset({"shared", "external"}),
        provides=frozenset({"stored_shared_ptr"}),
        implemented=False,
    ),
    StoredType(
        id="value_optional",
        name="std::optional<value_type>",
        kind="value_type",
        storage="dingo::storage<std::optional<value_type>>",
        supported_scopes=frozenset({"unique", "external"}),
        provides=frozenset({"stored_optional"}),
        implemented=False,
    ),
    StoredType(
        id="value_array_2",
        name="value_type[2]",
        kind="array_type",
        storage="dingo::storage<value_type[2]>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_raw_array"}),
        implemented=False,
    ),
    StoredType(
        id="value_array_2_3",
        name="value_type[2][3]",
        kind="array_type",
        storage="dingo::storage<value_type[2][3]>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_raw_array"}),
        implemented=False,
    ),
    StoredType(
        id="value_unique_array",
        name="std::unique_ptr<value_type[]>",
        kind="array_type",
        storage="dingo::storage<std::unique_ptr<value_type[]>>",
        supported_scopes=frozenset({"unique", "external"}),
        provides=frozenset({"stored_unique_array"}),
        implemented=False,
    ),
    StoredType(
        id="value_shared_array",
        name="std::shared_ptr<value_type[]>",
        kind="array_type",
        storage="dingo::storage<std::shared_ptr<value_type[]>>",
        supported_scopes=frozenset({"shared", "external"}),
        provides=frozenset({"stored_shared_array"}),
        implemented=False,
    ),
    StoredType(
        id="value_variant",
        name="std::variant<variant_a, variant_b>",
        kind="variant_type",
        storage="dingo::storage<std::variant<variant_a, variant_b>>",
        supported_scopes=frozenset({"unique", "shared", "external"}),
        provides=frozenset({"stored_variant"}),
        implemented=False,
    ),
    StoredType(
        id="implementation_shared_ptr",
        name="std::shared_ptr<implementation_type>",
        kind="implementation_type",
        storage="dingo::storage<std::shared_ptr<implementation_type>>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_shared_ptr"}),
    ),
    StoredType(
        id="element_shared_ptr",
        name="std::shared_ptr<element_type<0>>",
        kind="element_type",
        storage="dingo::storage<std::shared_ptr<element_type<0>>>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_shared_ptr"}),
    ),
    StoredType(
        id="unique_implementation_unique_ptr",
        name="std::unique_ptr<unique_implementation_type>",
        kind="unique_implementation_type",
        storage="dingo::storage<std::unique_ptr<unique_implementation_type>>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset({"stored_unique_ptr"}),
    ),
)


EXPOSED_TYPES = (
    ExposedType(
        name="concrete",
        kind="concrete",
        supported_stored_kinds=frozenset({"value_type"}),
        provides=frozenset({"concrete_binding"}),
        registrations=(RegistrationSpec(),),
    ),
    ExposedType(
        name="interface_type",
        kind="interface",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"concrete_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces="dingo::interfaces<interface_type>",
                mixed="runtime",
            ),
        ),
    ),
    ExposedType(
        name="multiple_interfaces",
        kind="interface",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"concrete_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces="dingo::interfaces<interface_type, second_interface_type>"
            ),
        ),
        implemented=False,
    ),
    ExposedType(
        name="keyed_concrete",
        kind="keyed",
        supported_stored_kinds=frozenset({"value_type"}),
        provides=frozenset({"keyed_binding"}),
        registrations=(RegistrationSpec(key="dingo::key<key_a>"),),
        implemented=False,
    ),
    ExposedType(
        name="keyed_interface",
        kind="keyed",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"keyed_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(
                interfaces="dingo::interfaces<interface_type>",
                key="dingo::key<key_a>",
            ),
        ),
        implemented=False,
    ),
    ExposedType(
        name="annotated_concrete",
        kind="annotated",
        supported_stored_kinds=frozenset({"value_type"}),
        provides=frozenset({"annotated_binding"}),
        registrations=(RegistrationSpec(key="dingo::annotated<value_type, tag_a>"),),
        implemented=False,
    ),
    ExposedType(
        name="annotated_interface",
        kind="annotated",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"annotated_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(
                interfaces="dingo::interfaces<interface_type>",
                key="dingo::annotated<interface_type, tag_a>",
            ),
        ),
        implemented=False,
    ),
    ExposedType(
        name="unique_interface_type",
        kind="interface",
        supported_stored_kinds=frozenset({"unique_implementation_type"}),
        provides=frozenset({"interface_binding"}),
        registrations=(
            RegistrationSpec(interfaces="dingo::interfaces<unique_interface_type>"),
        ),
    ),
    ExposedType(
        name="element_collection",
        kind="collection",
        supported_stored_kinds=frozenset({"element_type"}),
        provides=frozenset({"collection_binding"}),
        registrations=(
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<0>>>",
                interfaces="dingo::interfaces<element_interface>",
            ),
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<1>>>",
                interfaces="dingo::interfaces<element_interface>",
                mixed="runtime",
            ),
        ),
        runtime_prefix=(
            "container.template register_type_collection<"
            "dingo::scope<dingo::unique>, "
            "dingo::storage<std::vector<std::shared_ptr<"
            "element_interface>>>>();",
        ),
        mixed_runtime_prefix=(
            "container.template register_type_collection<"
            "dingo::scope<dingo::unique>, "
            "dingo::storage<std::vector<std::shared_ptr<"
            "element_interface>>>>();",
        ),
    ),
    ExposedType(
        name="element_keyed",
        kind="keyed",
        supported_stored_kinds=frozenset({"element_type"}),
        provides=frozenset({"keyed_binding"}),
        registrations=(
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<0>>>",
                interfaces="dingo::interfaces<element_interface>",
                key="dingo::key<key_a>",
            ),
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<1>>>",
                interfaces="dingo::interfaces<element_interface>",
                key="dingo::key<key_b>",
                mixed="runtime",
            ),
        ),
    ),
    ExposedType(
        name="element_indexed",
        kind="indexed",
        supported_stored_kinds=frozenset({"element_type"}),
        provides=frozenset({"indexed_binding"}),
        registrations=(
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<0>>>",
                interfaces="dingo::interfaces<element_interface>",
                indexed_key="0",
                mixed="runtime",
                static=False,
            ),
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<1>>>",
                interfaces="dingo::interfaces<element_interface>",
                indexed_key="1",
                mixed="runtime",
                static=False,
            ),
        ),
        mixed=False,
    ),
)


RESOLVED_TYPES = (
    ResolvedType(
        name="value_ref_ptr",
        supported_exposed_types=frozenset({"concrete", "interface_type"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="value",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete_value"}),
        implemented=False,
    ),
    ResolvedType(
        name="const_value_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_const_concrete"}),
        implemented=False,
    ),
    ResolvedType(
        name="const_value_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_const_concrete"}),
        implemented=False,
    ),
    ResolvedType(
        name="interface_value",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface_value"}),
        implemented=False,
    ),
    ResolvedType(
        name="interface_ref_ptr_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_shared_interface"}),
    ),
    ResolvedType(
        name="interface_ref",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        implemented=False,
    ),
    ResolvedType(
        name="interface_ptr",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        implemented=False,
    ),
    ResolvedType(
        name="unique_interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_unique_wrapper"}),
    ),
    ResolvedType(
        name="value_unique_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        implemented=False,
    ),
    ResolvedType(
        name="interface_unique_ptr",
        supported_exposed_types=frozenset({"interface_type", "unique_interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        implemented=False,
    ),
    ResolvedType(
        name="value_shared_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        implemented=False,
    ),
    ResolvedType(
        name="value_shared_ptr_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        implemented=False,
    ),
    ResolvedType(
        name="interface_shared_ptr",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_wrapper"}),
        implemented=False,
    ),
    ResolvedType(
        name="element_vector",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"resolved_collection"}),
    ),
    ResolvedType(
        name="element_keyed_shared_ptr_ref",
        supported_exposed_types=frozenset({"element_keyed"}),
        provides=frozenset({"resolved_keyed"}),
    ),
    ResolvedType(
        name="keyed_value",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        implemented=False,
    ),
    ResolvedType(
        name="keyed_value_ref",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        implemented=False,
    ),
    ResolvedType(
        name="element_keyed_vector",
        supported_exposed_types=frozenset({"element_keyed"}),
        provides=frozenset({"resolved_keyed_collection"}),
        implemented=False,
    ),
    ResolvedType(
        name="element_indexed_shared_ptr",
        supported_exposed_types=frozenset({"element_indexed"}),
        provides=frozenset({"resolved_indexed"}),
    ),
    ResolvedType(
        name="raw_array_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_array"}),
        implemented=False,
    ),
    ResolvedType(
        name="raw_array_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_array"}),
        implemented=False,
    ),
    ResolvedType(
        name="unique_array",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_array"}),
        implemented=False,
    ),
    ResolvedType(
        name="shared_array",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_array"}),
        implemented=False,
    ),
    ResolvedType(
        name="variant_value",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_variant"}),
        implemented=False,
    ),
    ResolvedType(
        name="variant_alternative_value",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_variant_alternative"}),
        implemented=False,
    ),
    ResolvedType(
        name="variant_alternative_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_variant_alternative"}),
        implemented=False,
    ),
    ResolvedType(
        name="variant_alternative_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_variant_alternative"}),
        implemented=False,
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
    ContainerSpec(
        name="runtime_container_indexed_unordered",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_unordered_container_traits>",
        provides=frozenset({"indexed_container"}),
    ),
    ContainerSpec(
        name="container_indexed_unordered",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_unordered_container_traits>",
        provides=frozenset({"indexed_container"}),
    ),
    ContainerSpec(
        name="runtime_container_indexed_array",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_array_container_traits>",
        provides=frozenset({"indexed_container"}),
        implemented=False,
    ),
    ContainerSpec(
        name="container_indexed_array",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_array_container_traits>",
        provides=frozenset({"indexed_container"}),
        implemented=False,
    ),
    ContainerSpec(
        name="container_allocator",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<dingo::dynamic_container_traits, std::allocator<char>>",
        provides=frozenset({"custom_allocator_container"}),
        implemented=False,
    ),
    ContainerSpec(
        name="container_custom_rtti",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<custom_rtti_container_traits>",
        provides=frozenset({"custom_rtti_container"}),
        implemented=False,
    ),
)


FILTER_RULES = (
    "container_mode",
    "feature_mode",
    "stored_type_supports_mode",
    "scope_supports_stored_type",
    "exposure_supports_stored_type",
    "resolved_type_supports_exposure",
    "feature_requires_tags",
    "container_requires",
)


def implemented(axis: tuple) -> tuple:
    return tuple(member for member in axis if member.implemented)


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


def register_type_with_arg(
    scope: ScopeSpec,
    storage: str,
    argument: str,
    *,
    interfaces: str | None = None,
    key: str | None = None,
) -> str:
    registration = register_type(scope, storage, interfaces=interfaces, key=key)
    return registration[:-3] + f"({argument});"


def bindings_type(bindings: tuple[str, ...]) -> str:
    return "dingo::bindings<" + ", ".join(bindings) + ">"


def spec_storage(spec: RegistrationSpec, stored_type: StoredType) -> str:
    return spec.storage or stored_type.storage


def spec_runtime_argument(spec: RegistrationSpec, stored_type: StoredType) -> str | None:
    if spec.storage is None:
        return stored_type.runtime_argument
    return None


def render_binding(
    scope: ScopeSpec, stored_type: StoredType, spec: RegistrationSpec
) -> str:
    return binding(
        scope,
        spec_storage(spec, stored_type),
        interfaces=spec.interfaces,
        key=spec.key,
    )


def render_registration(
    scope: ScopeSpec, stored_type: StoredType, spec: RegistrationSpec
) -> str:
    storage = spec_storage(spec, stored_type)
    if spec.indexed_key is not None:
        parts = [scope.type_name, storage]
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
            scope,
            storage,
            argument,
            interfaces=spec.interfaces,
            key=spec.key,
        )
    return register_type(
        scope,
        storage,
        interfaces=spec.interfaces,
        key=spec.key,
    )


def runtime_setup_prefix(
    stored_type: StoredType, specs: tuple[RegistrationSpec, ...]
) -> tuple[str, ...]:
    if any(spec_runtime_argument(spec, stored_type) is not None for spec in specs):
        return stored_type.runtime_setup
    return ()


def build_plan(
    scope: ScopeSpec, stored_type: StoredType, exposed_type: ExposedType
) -> BindingPlan | None:
    specs = exposed_type.registrations
    static_specs = tuple(spec for spec in specs if spec.static)
    mixed_static_specs = tuple(spec for spec in specs if spec.mixed == "static")
    mixed_runtime_specs = tuple(spec for spec in specs if spec.mixed == "runtime")

    return BindingPlan(
        static_bindings=tuple(
            render_binding(scope, stored_type, spec) for spec in static_specs
        ),
        runtime_setup=(
            runtime_setup_prefix(stored_type, specs)
            + exposed_type.runtime_prefix
            + tuple(render_registration(scope, stored_type, spec) for spec in specs)
        ),
        mixed_static_bindings=(
            tuple(
                render_binding(scope, stored_type, spec) for spec in mixed_static_specs
            )
            if exposed_type.mixed
            else None
        ),
        mixed_runtime_setup=(
            runtime_setup_prefix(stored_type, mixed_runtime_specs)
            + exposed_type.mixed_runtime_prefix
            + tuple(
                render_registration(scope, stored_type, spec)
                for spec in mixed_runtime_specs
            )
        ),
    )


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

    tags = row_tags(mode, scope, stored_type, exposed_type, resolved_type, container)
    if not feature.requires <= tags:
        return "feature_requires_tags"
    if not feature.container_requires <= container.provides:
        return "container_requires"

    return None


def generate_rows() -> tuple[MatrixRow, ...]:
    rows: list[MatrixRow] = []
    rejected: dict[str, int] = defaultdict(int)

    for feature in implemented(FEATURES):
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
    assert_axis_used(
        row_tuple, "feature", {feature.name for feature in implemented(FEATURES)}
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
    sources = [
        generate_feature(feature, rows, out_dir, env)
        for feature in implemented(FEATURES)
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
