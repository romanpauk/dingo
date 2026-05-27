#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Feature:
    name: str
    requires: frozenset[str]
    modes: frozenset[str]
    checks: tuple[str, ...] = ()
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
    factory: str | None = None
    implemented: bool = True


@dataclass(frozen=True)
class RegistrationSpec:
    storage: str | None = None
    interfaces: str | None = None
    local_bindings: str | None = None
    factory: str | None = None
    dependencies: str | None = None
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
    requires: frozenset[str] = frozenset()
    supported_modes: frozenset[str] = frozenset({"runtime", "static", "mixed"})
    checks: tuple[tuple[str, tuple[str, ...]], ...] = ()
    implemented: bool = True


@dataclass(frozen=True)
class ContainerSpec:
    name: str
    modes: frozenset[str]
    container_type: str
    provides: frozenset[str] = frozenset()
    implemented: bool = True


FEATURES = (
    Feature(
        name="construct_invoke",
        requires=frozenset(
            {
                "concrete_binding",
                "constructable_dependency",
                "invokable_dependency",
                "stable_concrete_storage",
                "resolved_concrete",
            }
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
        checks=(
            "auto constructed = container.template construct<dependent_type>();",
            "ASSERT_EQ(constructed.value, 3);",
            "auto invoked = container.invoke([](value_type& dependency) { return dependency.marker(); });",
            "ASSERT_EQ(invoked, 3);",
        ),
    ),
    Feature(
        name="resolve_concrete",
        requires=frozenset({"concrete_binding", "resolved_concrete"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_interface",
        requires=frozenset({"interface_binding", "resolved_interface"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_collection",
        requires=frozenset(
            {"collection_binding", "shared_storage", "resolved_collection"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_keyed",
        requires=frozenset({"keyed_binding", "shared_storage", "resolved_keyed"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_unique_wrapper",
        requires=frozenset(
            {"interface_binding", "unique_storage", "resolved_unique_wrapper"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_wrapper",
        requires=frozenset({"resolved_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_indexed",
        requires=frozenset(
            {"indexed_binding", "shared_storage", "resolved_indexed"}
        ),
        modes=frozenset({"runtime"}),
        container_requires=frozenset({"indexed_container"}),
    ),
    Feature(
        name="resolve_keyed_collection",
        requires=frozenset({"keyed_collection_binding", "resolved_keyed_collection"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="construct",
        requires=frozenset({"constructable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        checks=(
            "auto constructed = container.template construct<dependent_type>();",
            "ASSERT_EQ(constructed.value, 3);",
        ),
    ),
    Feature(
        name="invoke",
        requires=frozenset({"invokable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        checks=(
            "auto invoked = container.invoke([](value_type& dependency) { return dependency.marker(); });",
            "ASSERT_EQ(invoked, 3);",
        ),
    ),
    Feature(
        name="construct_collection",
        requires=frozenset({"collection_binding", "constructable_collection"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        checks=(
            "auto elements = container.template construct_collection<std::vector<std::shared_ptr<element_interface>>>();",
            "std::vector<int> ids;",
            "for (auto& element : elements) { ids.push_back(element->id()); }",
            "std::sort(ids.begin(), ids.end());",
            "ASSERT_EQ(ids, (std::vector<int>{0, 1}));",
        ),
    ),
    Feature(
        name="local_bindings",
        requires=frozenset({"local_bindings"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="nested_container",
        requires=frozenset(
            {
                "concrete_binding",
                "direct_value_resolution",
                "nested_container",
                "resolved_concrete",
            }
        ),
        modes=frozenset({"runtime"}),
        checks=(
            "typename std::remove_reference_t<decltype(container)>::allocator_type allocator;",
            "std::remove_reference_t<decltype(container)> parent(allocator);",
            "parent.template register_type<dingo::scope<dingo::external>, dingo::storage<int>>(1);",
            "parent.template register_type<dingo::scope<dingo::unique>, dingo::storage<nested_value_type>>();",
            "std::remove_reference_t<decltype(container)> child(&parent, allocator);",
            "child.template register_type<dingo::scope<dingo::external>, dingo::storage<int>>(2);",
            "child.template register_type<dingo::scope<dingo::unique>, dingo::storage<nested_value_type>>();",
            "ASSERT_EQ(parent.template resolve<nested_value_type>().value, 1);",
            "ASSERT_EQ(child.template resolve<nested_value_type>().value, 2);",
        ),
    ),
    Feature(
        name="annotated",
        requires=frozenset({"annotated_binding", "resolved_annotated"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="variant",
        requires=frozenset({"variant_binding", "resolved_variant"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="array",
        requires=frozenset({"array_binding", "resolved_array"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="factory_override",
        requires=frozenset({"factory_override"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="custom_allocator",
        requires=frozenset(
            {
                "custom_allocator_container",
                "concrete_binding",
                "direct_value_resolution",
                "stable_concrete_storage",
            }
        ),
        modes=frozenset({"runtime"}),
        checks=(
            "ASSERT_GT(test_allocator_stats<void>::allocated(), 0u);",
            "auto& resolved = container.template resolve<value_type&>();",
            "ASSERT_EQ(resolved.marker(), 3);",
        ),
    ),
    Feature(
        name="custom_rtti",
        requires=frozenset(
            {
                "custom_rtti_container",
                "concrete_binding",
                "direct_value_resolution",
                "stable_concrete_storage",
            }
        ),
        modes=frozenset({"runtime"}),
        checks=(
            "using rtti_type = typename std::remove_reference_t<decltype(container)>::rtti_type;",
            "static_assert(std::is_same_v<rtti_type, dingo::rtti<dingo::static_provider>>);",
            "auto& resolved = container.template resolve<value_type&>();",
            "ASSERT_EQ(resolved.marker(), 3);",
        ),
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
    ),
)


STORED_TYPES = (
    StoredType(
        id="value_type",
        name="value_type",
        kind="value_type",
        storage="dingo::storage<value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value", "direct_value_resolution"}),
    ),
    StoredType(
        id="external_value_type",
        name="external<value_type>",
        kind="value_type",
        storage="dingo::storage<value_type>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_external_value", "direct_value_resolution"}),
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
        provides=frozenset({"stored_external_reference", "direct_value_resolution"}),
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
        provides=frozenset({"stored_external_pointer", "direct_value_resolution"}),
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
        provides=frozenset({"stored_pointer", "direct_value_resolution"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("value_type external_value;",),
        runtime_argument="&external_value",
    ),
    StoredType(
        id="value_ref",
        name="value_type&",
        kind="value_type",
        storage="dingo::storage<value_type&>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_reference", "direct_value_resolution"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("value_type external_value;",),
        runtime_argument="external_value",
    ),
    StoredType(
        id="external_value_shared_ptr",
        name="external<std::shared_ptr<value_type>>",
        kind="value_type",
        storage="dingo::storage<std::shared_ptr<value_type>>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset({"stored_external_shared_ptr", "direct_value_resolution"}),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=("auto external_value = std::make_shared<value_type>();",),
        runtime_argument="external_value",
    ),
    StoredType(
        id="value_unique_ptr",
        name="std::unique_ptr<value_type>",
        kind="value_type",
        storage="dingo::storage<std::unique_ptr<value_type>>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset({"stored_unique_ptr"}),
    ),
    StoredType(
        id="value_shared_ptr",
        name="std::shared_ptr<value_type>",
        kind="value_type",
        storage="dingo::storage<std::shared_ptr<value_type>>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_shared_ptr"}),
    ),
    StoredType(
        id="value_optional",
        name="std::optional<value_type>",
        kind="value_type",
        storage="dingo::storage<std::optional<value_type>>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset({"stored_optional"}),
    ),
    StoredType(
        id="value_array_2",
        name="value_type[2]",
        kind="array_type",
        storage="dingo::storage<value_type[2]>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_raw_array", "array_1d"}),
        supported_modes=frozenset({"runtime", "static", "mixed"}),
    ),
    StoredType(
        id="value_array_2_3",
        name="value_type[2][3]",
        kind="array_type",
        storage="dingo::storage<value_type[2][3]>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_raw_array", "array_2d"}),
        supported_modes=frozenset({"runtime", "static", "mixed"}),
    ),
    StoredType(
        id="value_unique_array",
        name="std::unique_ptr<value_type[]>",
        kind="array_type",
        storage="dingo::storage<std::unique_ptr<value_type[]>>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset({"stored_unique_array", "array_smart_unique"}),
        factory="dingo::factory<dingo::function<&make_unique_value_array>>",
    ),
    StoredType(
        id="value_shared_array",
        name="std::shared_ptr<value_type[]>",
        kind="array_type",
        storage="dingo::storage<std::shared_ptr<value_type[]>>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_shared_array", "array_smart_shared"}),
        factory="dingo::factory<dingo::function<&make_shared_value_array>>",
    ),
    StoredType(
        id="value_variant",
        name="std::variant<variant_a, variant_b>",
        kind="variant_type",
        storage="dingo::storage<std::variant<variant_a, variant_b>>",
        supported_scopes=frozenset({"unique", "shared"}),
        provides=frozenset({"stored_variant"}),
        factory="dingo::factory<dingo::constructor<variant_a(value_type&)>>",
    ),
    StoredType(
        id="local_value_type",
        name="local_value_type",
        kind="local_value_type",
        storage="dingo::storage<local_value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        id="local_override_value_type",
        name="local_override_value_type",
        kind="local_override_value_type",
        storage="dingo::storage<local_override_value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        id="local_collection_value_type",
        name="local_collection_value_type",
        kind="local_collection_value_type",
        storage="dingo::storage<local_collection_value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        id="factory_function_value_type",
        name="factory_function_value_type",
        kind="factory_function_value_type",
        storage="dingo::storage<factory_function_value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        id="factory_constructor_value_type",
        name="factory_constructor_value_type",
        kind="factory_constructor_value_type",
        storage="dingo::storage<factory_constructor_value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
    ),
    StoredType(
        id="cycle_a_type",
        name="cycle_a_type",
        kind="cycle_type",
        storage="dingo::storage<cycle_a_type>",
        supported_scopes=frozenset({"shared_cyclical"}),
        provides=frozenset({"stored_value", "cycle_graph"}),
        supported_modes=frozenset({"runtime"}),
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
    ),
    ExposedType(
        name="copyable_interface_type",
        kind="interface",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"concrete_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces="dingo::interfaces<copyable_interface_type>",
                mixed="runtime",
            ),
        ),
    ),
    ExposedType(
        name="keyed_concrete",
        kind="keyed",
        supported_stored_kinds=frozenset({"value_type"}),
        provides=frozenset({"keyed_binding"}),
        registrations=(RegistrationSpec(key="dingo::key<key_a>"),),
    ),
    ExposedType(
        name="keyed_interface",
        kind="keyed",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"keyed_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces="dingo::interfaces<interface_type>",
                key="dingo::key<key_a>",
            ),
        ),
    ),
    ExposedType(
        name="annotated_concrete",
        kind="annotated",
        supported_stored_kinds=frozenset({"value_type"}),
        provides=frozenset({"annotated_binding"}),
        registrations=(
            RegistrationSpec(
                interfaces="dingo::interfaces<dingo::annotated<value_type, tag_a>>"
            ),
        ),
    ),
    ExposedType(
        name="annotated_interface",
        kind="annotated",
        supported_stored_kinds=frozenset({"implementation_type"}),
        provides=frozenset({"annotated_binding", "interface_binding"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces="dingo::interfaces<dingo::annotated<interface_type, tag_a>>",
            ),
        ),
    ),
    ExposedType(
        name="array_concrete",
        kind="array",
        supported_stored_kinds=frozenset({"array_type"}),
        provides=frozenset({"array_binding"}),
        registrations=(RegistrationSpec(),),
    ),
    ExposedType(
        name="variant_concrete",
        kind="variant",
        supported_stored_kinds=frozenset({"variant_type"}),
        provides=frozenset({"variant_binding"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(),
        ),
    ),
    ExposedType(
        name="local_value",
        kind="local_value_type",
        supported_stored_kinds=frozenset({"local_value_type"}),
        provides=frozenset({"local_bindings"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                local_bindings=(
                    "dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, "
                    "dingo::storage<local_dependency_type>>>"
                )
            ),
        ),
    ),
    ExposedType(
        name="local_override_value",
        kind="local_override_value_type",
        supported_stored_kinds=frozenset({"local_override_value_type"}),
        provides=frozenset({"local_bindings"}),
        registrations=(
            RegistrationSpec(
                storage="dingo::storage<local_dependency_type>",
                factory="dingo::factory<dingo::constructor<local_dependency_type()>>",
            ),
            RegistrationSpec(
                local_bindings=(
                    "dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, "
                    "dingo::storage<local_dependency_type>>>"
                )
            ),
        ),
    ),
    ExposedType(
        name="local_collection_value",
        kind="local_collection_value_type",
        supported_stored_kinds=frozenset({"local_collection_value_type"}),
        provides=frozenset({"local_bindings"}),
        registrations=(
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<1>>>",
                interfaces="dingo::interfaces<element_interface>",
            ),
            RegistrationSpec(
                dependencies="dingo::dependencies<std::vector<std::shared_ptr<element_interface>>>",
                local_bindings=(
                    "dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, "
                    "dingo::storage<std::shared_ptr<element_type<0>>>, "
                    "dingo::interfaces<element_interface>>>"
                )
            ),
        ),
    ),
    ExposedType(
        name="factory_function_value",
        kind="factory_function_value_type",
        supported_stored_kinds=frozenset({"factory_function_value_type"}),
        provides=frozenset({"factory_override"}),
        registrations=(
            RegistrationSpec(
                factory="dingo::factory<dingo::function<&factory_function_value_type::create>>"
            ),
        ),
    ),
    ExposedType(
        name="factory_constructor_value",
        kind="factory_constructor_value_type",
        supported_stored_kinds=frozenset({"factory_constructor_value_type"}),
        provides=frozenset({"factory_override"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                factory="dingo::factory<dingo::constructor<factory_constructor_value_type(value_type&)>>"
            ),
        ),
    ),
    ExposedType(
        name="cycle_concrete",
        kind="cycle",
        supported_stored_kinds=frozenset({"cycle_type"}),
        provides=frozenset({"concrete_binding"}),
        registrations=(
            RegistrationSpec(),
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<cycle_b_type>>"),
        ),
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
        name="element_keyed_collection",
        kind="keyed_collection",
        supported_stored_kinds=frozenset({"element_type"}),
        provides=frozenset({"keyed_collection_binding"}),
        registrations=(
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<0>>>",
                interfaces="dingo::interfaces<element_interface>",
                key="dingo::key<key_a>",
            ),
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<1>>>",
                interfaces="dingo::interfaces<element_interface>",
                key="dingo::key<key_a>",
                mixed="runtime",
            ),
        ),
        runtime_prefix=(
            "container.template register_type_collection<"
            "dingo::scope<dingo::unique>, "
            "dingo::storage<std::vector<std::shared_ptr<"
            "element_interface>>>>(dingo::key<key_a>{});",
        ),
        mixed_runtime_prefix=(
            "container.template register_type_collection<"
            "dingo::scope<dingo::unique>, "
            "dingo::storage<std::vector<std::shared_ptr<"
            "element_interface>>>>(dingo::key<key_a>{});",
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
                indexed_key="std::size_t{0}",
                mixed="runtime",
                static=False,
            ),
            RegistrationSpec(
                storage="dingo::storage<std::shared_ptr<element_type<1>>>",
                interfaces="dingo::interfaces<element_interface>",
                indexed_key="std::size_t{1}",
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
        provides=frozenset({"resolved_concrete", "constructable_dependency", "invokable_dependency"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "value_type& instance = container.template resolve<value_type&>();",
                    "value_type* pointer = container.template resolve<value_type*>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                    "ASSERT_EQ(pointer->marker(), 3);",
                    "ASSERT_EQ(&instance, pointer);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_concrete_value"}),
        requires=frozenset({"direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "auto instance = container.template resolve<value_type>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="cycle_ref",
        supported_exposed_types=frozenset({"cycle_concrete"}),
        provides=frozenset({"resolved_concrete"}),
        requires=frozenset({"cycle_graph"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "auto& instance = container.template resolve<cycle_a_type&>();",
                    "ASSERT_NE(instance.dependency, nullptr);",
                    "ASSERT_EQ(instance.dependency->dependency, &instance);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="const_value_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_const_concrete"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "const value_type& instance = container.template resolve<const value_type&>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="const_value_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_const_concrete"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "const value_type* instance = container.template resolve<const value_type*>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_value",
        supported_exposed_types=frozenset({"copyable_interface_type"}),
        provides=frozenset({"resolved_interface", "resolved_interface_value"}),
        checks=(
            (
                "resolve_interface",
                (
                    "auto instance = container.template resolve<copyable_interface_type>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_ref_ptr_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_interface", "resolved_shared_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "interface_type& instance = container.template resolve<interface_type&>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                    "interface_type* pointer = container.template resolve<interface_type*>();",
                    "ASSERT_EQ(pointer->marker(), 3);",
                    "auto& handle = container.template resolve<std::shared_ptr<interface_type>&>();",
                    "ASSERT_EQ(handle->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_ref",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "interface_type& instance = container.template resolve<interface_type&>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_ptr",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "interface_type* instance = container.template resolve<interface_type*>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="second_interface_ref",
        supported_exposed_types=frozenset({"multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "second_interface_type& instance = container.template resolve<second_interface_type&>();",
                    "ASSERT_EQ(instance.second_marker(), 4);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="unique_interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_unique_wrapper"}),
        requires=frozenset({"unique_storage"}),
        checks=(
            (
                "resolve_unique_wrapper",
                (
                    "auto instance = container.template resolve<std::unique_ptr<unique_interface_type>&&>();",
                    "ASSERT_EQ(instance->value(), 7);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_unique_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_unique_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::unique_ptr<value_type>&&>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_unique_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::unique_ptr<unique_interface_type>&&>();",
                    "ASSERT_EQ(instance->value(), 7);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_shared_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::shared_ptr<value_type>>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_shared_ptr_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto& instance = container.template resolve<std::shared_ptr<value_type>&>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_optional",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_optional"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::optional<value_type>>();",
                    "ASSERT_TRUE(instance.has_value());",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::shared_ptr<interface_type>>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_vector",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"resolved_collection", "constructable_collection"}),
        checks=(
            (
                "resolve_collection",
                (
                    "auto elements = container.template resolve<std::vector<std::shared_ptr<element_interface>>>();",
                    "std::vector<int> ids;",
                    "for (auto& element : elements) { ids.push_back(element->id()); }",
                    "std::sort(ids.begin(), ids.end());",
                    "ASSERT_EQ(ids, (std::vector<int>{0, 1}));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_map_custom_insert",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"constructable_collection"}),
        checks=(
            (
                "construct_collection",
                (
                    "auto elements = container.template construct_collection<std::map<int, std::shared_ptr<element_interface>>>(",
                    "    [](auto& collection, auto&& value) { collection.emplace(value->id(), std::move(value)); });",
                    "ASSERT_EQ(elements.size(), 2u);",
                    "ASSERT_EQ(elements.at(0)->id(), 0);",
                    "ASSERT_EQ(elements.at(1)->id(), 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_keyed_shared_ptr_ref",
        supported_exposed_types=frozenset({"element_keyed"}),
        provides=frozenset({"resolved_keyed"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto& first = container.template resolve<std::shared_ptr<element_interface>&>(dingo::key<key_a>{});",
                    "auto& second = container.template resolve<std::shared_ptr<element_interface>&>(dingo::key<key_b>{});",
                    "ASSERT_EQ(first->id(), 0);",
                    "ASSERT_EQ(second->id(), 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_value_dependency",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"constructable_dependency", "invokable_dependency"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "construct",
                (
                    "auto constructed = container.template construct<keyed_value_dependency_type>();",
                    "ASSERT_EQ(constructed.value, 3);",
                ),
            ),
            (
                "invoke",
                (
                    "auto invoked = container.invoke([](dingo::keyed<value_type&, key_a> dependency) {",
                    "    return static_cast<value_type&>(dependency).marker();",
                    "});",
                    "ASSERT_EQ(invoked, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_value",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto instance = container.template resolve<value_type>(dingo::key<key_a>{});",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_value_ref",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto& instance = container.template resolve<value_type&>(dingo::key<key_a>{});",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_interface_ref",
        supported_exposed_types=frozenset({"keyed_interface"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto& instance = container.template resolve<interface_type&>(dingo::key<key_a>{});",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_keyed_vector",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        provides=frozenset({"resolved_keyed_collection"}),
        checks=(
            (
                "resolve_keyed_collection",
                (
                    "auto elements = container.template resolve<std::vector<std::shared_ptr<element_interface>>>(dingo::key<key_a>{});",
                    "std::vector<int> ids;",
                    "for (auto& element : elements) { ids.push_back(element->id()); }",
                    "std::sort(ids.begin(), ids.end());",
                    "ASSERT_EQ(ids, (std::vector<int>{0, 1}));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_collection_dependency",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        provides=frozenset({"constructable_dependency", "invokable_dependency"}),
        checks=(
            (
                "construct",
                (
                    "auto constructed = container.template construct<keyed_collection_dependency_type>();",
                    "ASSERT_EQ(constructed.count, 2u);",
                    "ASSERT_EQ(constructed.sum, 1);",
                ),
            ),
            (
                "invoke",
                (
                    "auto invoked = container.invoke([](dingo::keyed<std::vector<std::shared_ptr<element_interface>>, key_a> elements) {",
                    "    auto values = static_cast<std::vector<std::shared_ptr<element_interface>>>(elements);",
                    "    return values.size();",
                    "});",
                    "ASSERT_EQ(invoked, 2u);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_indexed_shared_ptr",
        supported_exposed_types=frozenset({"element_indexed"}),
        provides=frozenset({"resolved_indexed"}),
        checks=(
            (
                "resolve_indexed",
                (
                    "auto first = container.template resolve<std::shared_ptr<element_interface>>(std::size_t{0});",
                    "auto second = container.template resolve<std::shared_ptr<element_interface>>(std::size_t{1});",
                    "ASSERT_EQ(first->id(), 0);",
                    "ASSERT_EQ(second->id(), 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_array_ptr",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
        checks=(
            (
                "array",
                (
                    "auto* values = container.template resolve<value_type*>();",
                    "ASSERT_EQ(values[0].marker(), 3);",
                    "ASSERT_EQ(values[1].marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_array_ref",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
        checks=(
            (
                "array",
                (
                    "auto& values = container.template resolve<value_type(&)[2]>();",
                    "ASSERT_EQ(values[0].marker(), 3);",
                    "ASSERT_EQ(values[1].marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_array_ptr_to_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
        checks=(
            (
                "array",
                (
                    "auto values = container.template resolve<value_type(*)[2]>();",
                    "ASSERT_EQ((*values)[0].marker(), 3);",
                    "ASSERT_EQ((*values)[1].marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_nd_array_ref",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_2d"}),
        checks=(
            (
                "array",
                (
                    "auto& values = container.template resolve<value_type(&)[2][3]>();",
                    "ASSERT_EQ(values[0][0].marker(), 3);",
                    "ASSERT_EQ(values[1][2].marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="unique_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"unique_storage", "array_smart_unique"}),
        checks=(
            (
                "array",
                (
                    "auto values = container.template resolve<std::unique_ptr<value_type[]>&&>();",
                    "ASSERT_EQ(values[0].marker(), 3);",
                    "ASSERT_EQ(values[1].marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="shared_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"shared_storage", "array_smart_shared"}),
        checks=(
            (
                "array",
                (
                    "auto values = container.template resolve<std::shared_ptr<value_type[]>>();",
                    "ASSERT_EQ(values[0].marker(), 3);",
                    "ASSERT_EQ(values[1].marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_value",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant"}),
        checks=(
            (
                "variant",
                (
                    "auto instance = container.template resolve<std::variant<variant_a, variant_b>>();",
                    "ASSERT_TRUE(std::holds_alternative<variant_a>(instance));",
                    "ASSERT_EQ(std::get<variant_a>(instance).value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_alternative_value",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        checks=(
            (
                "variant",
                (
                    "auto instance = container.template resolve<variant_a>();",
                    "ASSERT_EQ(instance.value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_alternative_ref",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "variant",
                (
                    "auto& instance = container.template resolve<variant_a&>();",
                    "ASSERT_EQ(instance.value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_alternative_ptr",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "variant",
                (
                    "auto* instance = container.template resolve<variant_a*>();",
                    "ASSERT_EQ(instance->value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_value_ref",
        supported_exposed_types=frozenset({"annotated_concrete"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "annotated",
                (
                    "auto& instance = container.template resolve<dingo::annotated<value_type&, tag_a>>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_ref",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "annotated",
                (
                    "auto& instance = container.template resolve<dingo::annotated<interface_type&, tag_a>>();",
                    "ASSERT_EQ(instance.marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_ptr",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "annotated",
                (
                    "auto instance = container.template resolve<dingo::annotated<interface_type*, tag_a>>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_shared_ptr",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "annotated",
                (
                    "auto instance = container.template resolve<dingo::annotated<std::shared_ptr<interface_type>, tag_a>>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_shared_ptr_ref",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "annotated",
                (
                    "auto& instance = container.template resolve<dingo::annotated<std::shared_ptr<interface_type>&, tag_a>>();",
                    "ASSERT_EQ(instance->marker(), 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="local_value_ref",
        supported_exposed_types=frozenset({"local_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "local_bindings",
                (
                    "auto& resolved = container.template resolve<local_value_type&>();",
                    "ASSERT_EQ(resolved.value, 12);",
                    "ASSERT_EQ(container.template construct<local_value_type>().value, 12);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="local_override_value_ref",
        supported_exposed_types=frozenset({"local_override_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "local_bindings",
                (
                    "auto& resolved = container.template resolve<local_override_value_type&>();",
                    "ASSERT_EQ(resolved.value, 4);",
                    "ASSERT_EQ(container.template construct<local_override_value_type>().value, 4);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="local_collection_value_ref",
        supported_exposed_types=frozenset({"local_collection_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "local_bindings",
                (
                    "auto& resolved = container.template resolve<local_collection_value_type&>();",
                    "ASSERT_EQ(resolved.count, 2u);",
                    "ASSERT_EQ(resolved.sum, 1);",
                    "auto constructed = container.template construct<local_collection_value_type>();",
                    "ASSERT_EQ(constructed.count, 2u);",
                    "ASSERT_EQ(constructed.sum, 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="factory_function_value_ref",
        supported_exposed_types=frozenset({"factory_function_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "factory_override",
                (
                    "auto& resolved = container.template resolve<factory_function_value_type&>();",
                    "ASSERT_EQ(resolved.value, 9);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="factory_constructor_value_ref",
        supported_exposed_types=frozenset({"factory_constructor_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "factory_override",
                (
                    "auto& resolved = container.template resolve<factory_constructor_value_type&>();",
                    "ASSERT_EQ(resolved.value, 9);",
                ),
            ),
        ),
    ),
)


CONTAINERS = (
    ContainerSpec(
        name="runtime_container",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<>",
        provides=frozenset({"nested_container"}),
    ),
    ContainerSpec(
        name="container_runtime",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<>",
        provides=frozenset({"nested_container"}),
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
    ),
    ContainerSpec(
        name="container_indexed_array",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_array_container_traits>",
        provides=frozenset({"indexed_container"}),
    ),
    ContainerSpec(
        name="container_allocator",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<dingo::dynamic_container_traits, test_allocator<char>>",
        provides=frozenset({"custom_allocator_container"}),
    ),
    ContainerSpec(
        name="container_custom_rtti",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<custom_rtti_container_traits>",
        provides=frozenset({"custom_rtti_container"}),
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
    "resolved_type_requires_tags",
    "container_requires",
)
