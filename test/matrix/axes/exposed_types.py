#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import sys
from pathlib import Path

from schema import (
    ExposedType,
    MixedRegistrationPlacement,
    RegistrationSpec,
)


ROOT_DIR = Path(__file__).resolve().parents[3]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from tools.model.legacy.binding import exposed as _binding  # noqa: E402


LOCAL_BINDINGS = (
    "dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, "
    "dingo::storage<local_dependency_type>>>"
)
LOCAL_COLLECTION_STORAGE = "dingo::storage<std::shared_ptr<element_type<1>>>"
LOCAL_COLLECTION_DEPENDENCIES = (
    "dingo::dependencies<std::vector<std::shared_ptr<element_interface>>>"
)
LOCAL_COLLECTION_BINDINGS = (
    "dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, "
    "dingo::storage<std::shared_ptr<element_type<0>>>, "
    "dingo::interfaces<element_interface>>>"
)
COLLECTION_PREFIX = (
    "container.template register_type_collection<"
    "dingo::scope<dingo::unique>, "
    "dingo::storage<std::vector<std::shared_ptr<"
    "element_interface>>>>();",
)
KEYED_COLLECTION_PREFIX = (
    "container.template register_type_collection<"
    "dingo::scope<dingo::unique>, "
    "dingo::storage<std::vector<std::shared_ptr<"
    "element_interface>>>>(dingo::key_type<key_a>{});",
)


def _simple(
    name: str,
    kind: str,
    stored_kind: str,
    provides: set[str],
    registrations=None,
    *,
    runtime_prefix: tuple[str, ...] = (),
    mixed_runtime_prefix: tuple[str, ...] = (),
    mixed: bool = True,
) -> ExposedType:
    """A one-kind exposure whose recipe is already regular."""
    return ExposedType(
        name=name,
        kind=kind,
        supported_stored_kinds=frozenset({stored_kind}),
        provides=frozenset(provides),
        registrations=(
            registrations if registrations is not None else (RegistrationSpec(),)
        ),
        runtime_prefix=runtime_prefix,
        mixed_runtime_prefix=mixed_runtime_prefix,
        mixed=mixed,
    )


EXPOSED_TYPES = (
    _binding("concrete"),
    ExposedType(
        name="mixed_external_dependency",
        kind="mixed_external_dependency",
        supported_stored_kinds=frozenset({"dependent_type"}),
        provides=frozenset({"mixed_external_dependency"}),
        registrations=(
            RegistrationSpec(
                scope="dingo::scope<dingo::external>",
                storage="dingo::storage<value_type&>",
                runtime_setup=("value_type external_value;",),
                runtime_argument="external_value",
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
                include_in_static=False,
            ),
            RegistrationSpec(),
        ),
    ),
    ExposedType(
        name="mixed_singular_conflict",
        kind="value_type",
        supported_stored_kinds=frozenset({"value_type"}),
        provides=frozenset({"mixed_singular_conflict"}),
        registrations=(
            RegistrationSpec(),
            RegistrationSpec(
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
            ),
        ),
    ),
    ExposedType(
        name="explicit_dependencies",
        kind="explicit_dependencies",
        supported_stored_kinds=frozenset({"explicit_dependencies_type"}),
        provides=frozenset({"explicit_dependencies"}),
        registrations=(
            RegistrationSpec(
                scope="dingo::scope<dingo::shared>",
                storage="dingo::storage<explicit_dependency_first>",
            ),
            RegistrationSpec(
                scope="dingo::scope<dingo::shared>",
                storage="dingo::storage<explicit_dependency_second>",
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
            ),
            RegistrationSpec(
                dependencies=(
                    "dingo::dependencies<explicit_dependency_second &, "
                    "explicit_dependency_first &>"
                )
            ),
        ),
    ),
    _binding("interface_type"),
    _binding("multiple_interfaces"),
    _binding("copyable_interface_type"),
    _binding("custom_concrete"),
    _binding("custom_interface"),
    _binding("custom_unique_interface"),
    _binding("keyed_concrete"),
    _binding("keyed_interface"),
    _binding("annotated_concrete"),
    _binding("annotated_interface"),
    _simple("array_concrete", "array", "array_type", {"array_binding"}),
    _simple(
        "variant_concrete",
        "variant",
        "variant_type",
        {"variant_binding"},
        (
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(),
        ),
    ),
    _simple("nested_wrapper", "nested_wrapper", "nested_wrapper_type", {"nested_wrapper_binding"}),
    _simple("variant_unique_wrapper", "nested_wrapper", "variant_unique_wrapper_type", {"nested_wrapper_binding"}),
    _simple("variant_shared_wrapper", "nested_wrapper", "variant_shared_wrapper_type", {"nested_wrapper_binding"}),
    _simple("variant_shared_unique_wrapper", "nested_wrapper", "variant_shared_unique_wrapper_type", {"nested_wrapper_binding"}),
    _simple(
        "local_value",
        "local_value_type",
        "local_value_type",
        {"local_bindings"},
        (
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(local_bindings=LOCAL_BINDINGS),
        ),
    ),
    _simple(
        "local_override_value",
        "local_override_value_type",
        "local_override_value_type",
        {"local_bindings"},
        (
            RegistrationSpec(
                storage="dingo::storage<local_dependency_type>",
                factory="dingo::factory<dingo::constructor<local_dependency_type()>>",
            ),
            RegistrationSpec(local_bindings=LOCAL_BINDINGS),
        ),
    ),
    _simple(
        "local_collection_value",
        "local_collection_value_type",
        "local_collection_value_type",
        {"local_bindings"},
        (
            RegistrationSpec(
                storage=LOCAL_COLLECTION_STORAGE,
                interfaces="dingo::interfaces<element_interface>",
            ),
            RegistrationSpec(
                dependencies=LOCAL_COLLECTION_DEPENDENCIES,
                local_bindings=LOCAL_COLLECTION_BINDINGS,
            ),
        ),
    ),
    _simple(
        "local_collection_runtime_host_value",
        "local_collection_value_type",
        "local_collection_value_type",
        {"local_bindings"},
        (
            RegistrationSpec(
                storage=LOCAL_COLLECTION_STORAGE,
                interfaces="dingo::interfaces<element_interface>",
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
            ),
            RegistrationSpec(
                dependencies=LOCAL_COLLECTION_DEPENDENCIES,
                local_bindings=LOCAL_COLLECTION_BINDINGS,
            ),
        ),
    ),
    _simple("factory_function_value", "factory_function_value_type", "factory_function_value_type", {"factory_override"}, ()),
    _simple("factory_function_dependency_value", "factory_function_dependency_value_type", "factory_function_dependency_value_type", {"factory_override"}, ()),
    _simple("factory_constructor_value", "factory_constructor_value_type", "factory_constructor_value_type", {"factory_override"}, ()),
    _simple("factory_detected_constructor_value", "factory_detected_constructor_value_type", "factory_detected_constructor_value_type", {"factory_override"}, ()),
    _simple("factory_typedef_constructor_value", "factory_typedef_constructor_value_type", "factory_typedef_constructor_value_type", {"factory_override"}, ()),
    _simple("factory_callable_value", "factory_callable_value_type", "factory_callable_value_type", {"factory_override"}, ()),
    _simple(
        "unique_interface_type",
        "interface",
        "unique_implementation_type",
        {"interface_binding"},
        (RegistrationSpec(interfaces="dingo::interfaces<unique_interface_type>"),),
    ),
    _simple(
        "element_collection",
        "collection",
        "element_type",
        {"collection_binding"},
        (
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<0>>>", interfaces="dingo::interfaces<element_interface>"),
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<1>>>", interfaces="dingo::interfaces<element_interface>", mixed_placement=MixedRegistrationPlacement.RUNTIME),
        ),
        runtime_prefix=COLLECTION_PREFIX,
        mixed_runtime_prefix=COLLECTION_PREFIX,
    ),
    _simple(
        "element_keyed",
        "keyed",
        "element_type",
        {"keyed_binding"},
        (
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<0>>>", interfaces="dingo::interfaces<element_interface>", key="dingo::key_type<key_a>"),
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<1>>>", interfaces="dingo::interfaces<element_interface>", key="dingo::key_type<key_b>", mixed_placement=MixedRegistrationPlacement.RUNTIME),
        ),
    ),
    _simple(
        "element_keyed_collection",
        "keyed_collection",
        "element_type",
        {"keyed_collection_binding"},
        (
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<0>>>", interfaces="dingo::interfaces<element_interface>", key="dingo::key_type<key_a>"),
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<1>>>", interfaces="dingo::interfaces<element_interface>", key="dingo::key_type<key_a>", mixed_placement=MixedRegistrationPlacement.RUNTIME),
        ),
        runtime_prefix=KEYED_COLLECTION_PREFIX,
        mixed_runtime_prefix=KEYED_COLLECTION_PREFIX,
    ),
    _simple(
        "element_indexed",
        "indexed",
        "element_type",
        {"indexed_binding"},
        (
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<0>>>", interfaces="dingo::interfaces<element_interface>", indexed_key="std::size_t{0}", mixed_placement=MixedRegistrationPlacement.RUNTIME, include_in_static=False),
            RegistrationSpec(storage="dingo::storage<std::shared_ptr<element_type<1>>>", interfaces="dingo::interfaces<element_interface>", indexed_key="std::size_t{1}", mixed_placement=MixedRegistrationPlacement.RUNTIME, include_in_static=False),
        ),
        mixed=False,
    ),
)
