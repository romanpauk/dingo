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
    StoredType,
)


ROOT_DIR = Path(__file__).resolve().parents[3]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from tools.model.grammar.catalog import FORM_BY_NAME  # noqa: E402


VALUE_HEADERS = ("matrix/fixtures/values.h",)
NESTED_WRAPPER_HEADERS = ("matrix/fixtures/nested_wrappers.h",)
CUSTOM_WRAPPER_HEADERS = ("matrix/fixtures/values.h", "support/custom_wrappers.h")


def _wrapped_stored(
    id: str,
    form_name: str,
    kind: str,
    scopes: set[str],
    provides: set[str],
    *,
    leaf: str = "value_type",
    factory: str | None = None,
    system_headers: tuple[str, ...] = (),
    headers: tuple[str, ...] = VALUE_HEADERS,
    name: str | None = None,
    storage: str | None = None,
    modes: set[str] | None = None,
    runtime_setup: tuple[str, ...] = (),
    runtime_argument: str | None = None,
) -> StoredType:
    """One leaf wrapped by a catalog form, spelled by the shared catalog."""
    form = FORM_BY_NAME[form_name]
    spelled = form.cpp_type(leaf)
    optional: dict[str, object] = {}
    if modes is not None:
        optional["supported_modes"] = frozenset(modes)
    if runtime_argument is not None:
        optional["runtime_argument"] = runtime_argument
    return StoredType(
        id=id,
        name=name or spelled,
        kind=kind,
        storage=storage or form.storage(leaf),
        supported_scopes=frozenset(scopes),
        provides=frozenset(provides),
        factory=factory,
        system_headers=system_headers,
        headers=headers,
        runtime_setup=runtime_setup,
        **optional,
    )


# id, form, kind, scopes, provides, extra keyword arguments
VALUE_WRAPPERS = (
    ("value_unique_ptr", "unique_handle", "value_type", ("unique",), ("stored_unique_ptr", "lifetime_countable"), {}),
    ("value_shared_ptr", "shared_handle", "value_type", ("shared",), ("stored_shared_ptr", "lifetime_countable"), {}),
    ("value_optional", "optional", "value_type", ("shared", "unique"), ("stored_optional", "lifetime_countable"), {"system_headers": ("optional",)}),
)

EXTERNAL_VALUE_WRAPPERS = (
    ("external_value_shared_ptr", "shared_handle", "value_type", ("external",), ("stored_external_shared_ptr", "direct_value_resolution"), {"name": "external<std::shared_ptr<value_type>>", "modes": {"runtime"}, "runtime_setup": ("auto external_value = std::make_shared<value_type>();",), "runtime_argument": "external_value"}),
)

ARRAY_WRAPPERS = (
    ("value_unique_array", "unique_array_handle", "array_type", ("unique",), ("stored_unique_array", "array_smart_unique"), {"factory": "dingo::factory<dingo::function<&make_unique_value_array>>"}),
    ("value_shared_array", "shared_array_handle", "array_type", ("shared",), ("stored_shared_array", "array_smart_shared"), {"factory": "dingo::factory<dingo::function<&make_shared_value_array>>"}),
)

LEAF_WRAPPERS = (
    ("unique_ptr_leaf", "unique_handle", "nested_wrapper_type", ("shared",), ("stored_leaf_access",), {"headers": NESTED_WRAPPER_HEADERS}),
    ("unique_ptr_move_only_leaf", "unique_handle", "nested_wrapper_type", ("shared",), ("stored_leaf_access_move_only",), {"leaf": "move_only_leaf_type", "headers": NESTED_WRAPPER_HEADERS}),
    ("raw_pointer_leaf", "pointer", "nested_wrapper_type", ("shared",), ("stored_leaf_access",), {"name": "value_type*", "factory": "dingo::factory<dingo::function<&make_value_pointer>>", "headers": NESTED_WRAPPER_HEADERS}),
    ("raw_pointer_move_only_leaf", "pointer", "nested_wrapper_type", ("shared",), ("stored_leaf_access_move_only",), {"leaf": "move_only_leaf_type", "name": "move_only_leaf_type*", "factory": "dingo::factory<dingo::function<&make_move_only_pointer>>", "headers": NESTED_WRAPPER_HEADERS}),
)

EXTERNAL_LEAF_WRAPPERS = (
    ("external_unique_ptr_leaf", "unique_handle", "nested_wrapper_type", ("external",), ("stored_leaf_access",), {"name": "external<std::unique_ptr<value_type>>", "modes": {"runtime"}, "runtime_setup": ("auto external_value = std::make_unique<value_type>();",), "runtime_argument": "std::move(external_value)", "headers": NESTED_WRAPPER_HEADERS}),
    ("external_unique_ptr_move_only_leaf", "unique_handle", "nested_wrapper_type", ("external",), ("stored_leaf_access_move_only",), {"leaf": "move_only_leaf_type", "name": "external<std::unique_ptr<move_only_leaf_type>>", "modes": {"runtime"}, "runtime_setup": ("auto external_value = std::make_unique<move_only_leaf_type>();",), "runtime_argument": "std::move(external_value)", "headers": NESTED_WRAPPER_HEADERS}),
)

IMPLEMENTATION_WRAPPERS = (
    ("implementation_shared_ptr", "shared_handle", "implementation_type", ("shared",), ("stored_shared_ptr",), {"leaf": "implementation_type"}),
    ("element_shared_ptr", "shared_handle", "element_type", ("shared",), ("stored_shared_ptr",), {"leaf": "element_type<0>"}),
    ("unique_implementation_unique_ptr", "unique_handle", "unique_implementation_type", ("unique",), ("stored_unique_ptr",), {"leaf": "unique_implementation_type"}),
)

EXTERNAL_VALUE_FORMS = (
    ("external_value_type", "plain", "value_type", ("external",), ("stored_external_value", "direct_value_resolution"), {"name": "external<value_type>", "modes": {"runtime"}, "runtime_setup": ("value_type external_value;",), "runtime_argument": "std::move(external_value)"}),
    ("external_value_ref", "plain", "value_type", ("external",), ("stored_external_reference", "direct_value_resolution"), {"name": "external<value_type&>", "storage": "dingo::storage<value_type&>", "modes": {"runtime"}, "runtime_setup": ("value_type external_value;",), "runtime_argument": "external_value"}),
    ("external_value_ptr", "plain", "value_type", ("external",), ("stored_external_pointer", "direct_value_resolution"), {"name": "external<value_type*>", "storage": "dingo::storage<value_type*>", "modes": {"runtime"}, "runtime_setup": ("value_type external_value;",), "runtime_argument": "&external_value"}),
    ("value_ptr", "plain", "value_type", ("external",), ("stored_pointer", "direct_value_resolution"), {"name": "value_type*", "storage": "dingo::storage<value_type*>", "modes": {"runtime"}, "runtime_setup": ("value_type external_value;",), "runtime_argument": "&external_value"}),
    ("value_ref", "plain", "value_type", ("external",), ("stored_reference", "direct_value_resolution"), {"name": "value_type&", "storage": "dingo::storage<value_type&>", "modes": {"runtime"}, "runtime_setup": ("value_type external_value;",), "runtime_argument": "external_value"}),
)

RAW_ARRAY_FORMS = (
    ("value_array_2", "bounded_array", "array_type", ("shared",), ("stored_raw_array", "array_1d"), {"modes": {"runtime", "static", "mixed"}}),
    ("value_array_2_3", "bounded_array", "array_type", ("shared",), ("stored_raw_array", "array_2d"), {"name": "value_type[2][3]", "storage": "dingo::storage<value_type[2][3]>", "modes": {"runtime", "static", "mixed"}}),
)


def _wrapped_types(specs) -> tuple[StoredType, ...]:
    return tuple(
        _wrapped_stored(*spec[:5], **spec[5])
        for spec in specs
    )


LOCAL_HEADERS = ("matrix/fixtures/local_bindings.h",)
FACTORY_HEADERS = ("matrix/fixtures/factory.h",)
LOCAL_TYPES = (
    "local_value_type",
    "local_override_value_type",
    "local_collection_value_type",
)
FACTORY_TYPES = (
    "factory_function_value_type",
    "factory_function_dependency_value_type",
    "factory_constructor_value_type",
    "factory_detected_constructor_value_type",
    "factory_typedef_constructor_value_type",
)


def _leaf_stored(
    name: str,
    provides: set[str],
    headers: tuple[str, ...],
    *,
    scopes: set[str] | None = None,
    modes: set[str] | None = None,
) -> StoredType:
    optional: dict[str, object] = {}
    if modes is not None:
        optional["supported_modes"] = frozenset(modes)
    return StoredType(
        id=name,
        name=name,
        kind=name,
        storage=f"dingo::storage<{name}>",
        supported_scopes=frozenset(scopes or {"shared"}),
        provides=frozenset(provides),
        headers=headers,
        **optional,
    )


def _custom_stored(
    id: str,
    wrapper: str,
    leaf: str,
    kind: str,
    scopes: set[str],
    provides: set[str],
    *,
    external: bool = False,
    reference: bool = False,
    moves: bool = False,
) -> StoredType:
    inner = f"dingo::{wrapper}<{leaf}>"
    storage_type = f"{inner}&" if reference else inner
    name = f"external<{storage_type}>" if external else inner
    optional: dict[str, object] = {}
    if external:
        optional["supported_modes"] = frozenset({"runtime"})
        optional["runtime_setup"] = (
            f"auto external_value = dingo::type_traits<{inner}>::make();",
        )
        optional["runtime_argument"] = (
            "std::move(external_value)" if moves else "external_value"
        )
    return StoredType(
        id=id,
        name=name,
        kind=kind,
        storage=f"dingo::storage<{storage_type}>",
        supported_scopes=frozenset(scopes),
        provides=frozenset(provides),
        headers=CUSTOM_WRAPPER_HEADERS,
        **optional,
    )


def _alias_stored(
    id: str,
    name: str,
    kind: str,
    scopes: set[str],
    provides: set[str],
    *,
    alias: str | None = None,
    factory: str | None = None,
) -> StoredType:
    """A fixture type whose storage is a named alias and factory function."""
    storage = alias or f"{id}_type"
    maker = factory or f"make_{id}"
    return StoredType(
        id=id,
        name=name,
        kind=kind,
        storage=f"dingo::storage<{storage}>",
        supported_scopes=frozenset(scopes),
        provides=frozenset(provides),
        factory=f"dingo::factory<dingo::function<&{maker}>>",
        headers=NESTED_WRAPPER_HEADERS,
    )


def _external_alias_stored(
    id: str,
    name: str,
    storage_type: str,
    provides: set[str],
    setup: str,
) -> StoredType:
    return StoredType(
        id=id,
        name=name,
        kind="nested_wrapper_type",
        storage=f"dingo::storage<{storage_type}>",
        supported_scopes=frozenset({"external"}),
        provides=frozenset(provides),
        supported_modes=frozenset({"runtime"}),
        runtime_setup=(setup,),
        runtime_argument="external_value",
        headers=NESTED_WRAPPER_HEADERS,
    )


STORED_TYPES = (
    StoredType(
        id="value_type",
        name="value_type",
        kind="value_type",
        storage="dingo::storage<value_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset(
            {"stored_value", "direct_value_resolution", "lifetime_countable"}
        ),
        headers=VALUE_HEADERS,
    ),
    StoredType(
        id="unique_value_type",
        name="value_type",
        kind="value_type",
        storage="dingo::storage<value_type>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset(
            {"stored_value", "direct_value_resolution", "lifetime_countable"}
        ),
        headers=VALUE_HEADERS,
    ),
    StoredType(
        id="dependent_type",
        name="dependent_type",
        kind="dependent_type",
        storage="dingo::storage<dependent_type>",
        supported_scopes=frozenset({"shared"}),
        provides=frozenset({"stored_value"}),
        headers=VALUE_HEADERS,
    ),
    StoredType(
        id="explicit_dependencies_type",
        name="explicit_dependencies_type",
        kind="explicit_dependencies_type",
        storage="dingo::storage<explicit_dependencies_type>",
        supported_scopes=frozenset({"unique"}),
        provides=frozenset({"stored_explicit_dependencies"}),
        headers=VALUE_HEADERS,
    ),
    *_wrapped_types(EXTERNAL_VALUE_FORMS),
    *_wrapped_types(VALUE_WRAPPERS),
    *_wrapped_types(EXTERNAL_VALUE_WRAPPERS),
    _custom_stored("custom_shared_value", "test_shared", "value_type", "custom_value_type", {"shared"}, {"stored_custom_shared"}),
    _custom_stored("external_custom_shared_value", "test_shared", "value_type", "custom_value_type", {"external"}, {"stored_custom_shared"}, external=True, moves=True),
    _custom_stored("external_custom_shared_value_ref", "test_shared", "value_type", "custom_value_type", {"external"}, {"stored_custom_shared"}, external=True, reference=True),
    _custom_stored("custom_shared_interface", "test_shared", "custom_interface_implementation_type", "custom_interface_type", {"shared"}, {"stored_custom_shared"}),
    _custom_stored("custom_unique_interface", "test_unique", "custom_unique_implementation_type", "custom_unique_interface_type", {"unique"}, {"stored_custom_unique"}),
    _custom_stored("custom_optional_value", "test_optional", "value_type", "custom_value_type", {"shared", "unique"}, {"stored_custom_optional"}),
    _custom_stored("external_custom_optional_value", "test_optional", "value_type", "custom_value_type", {"external"}, {"stored_custom_optional"}, external=True, moves=True),
    _custom_stored("external_custom_optional_value_ref", "test_optional", "value_type", "custom_value_type", {"external"}, {"stored_custom_optional"}, external=True, reference=True),
    *_wrapped_types(RAW_ARRAY_FORMS),
    *_wrapped_types(ARRAY_WRAPPERS),
    StoredType(
        id="value_variant",
        name="std::variant<variant_a, variant_b>",
        kind="variant_type",
        storage="dingo::storage<std::variant<variant_a, variant_b>>",
        supported_scopes=frozenset({"unique", "shared"}),
        provides=frozenset({"stored_variant"}),
        factory="dingo::factory<dingo::constructor<variant_a(value_type&)>>",
        headers=("matrix/fixtures/variant_types.h",),
    ),
    _alias_stored("shared_unique_value", "std::shared_ptr<std::unique_ptr<value_type>>", "nested_wrapper_type", {"shared"}, {"stored_shared_unique", "stored_leaf_access"}),
    _alias_stored("shared_unique_array_value", "std::shared_ptr<std::unique_ptr<value_type[]>>", "nested_wrapper_type", {"shared"}, {"stored_shared_unique_array"}),
    *_wrapped_types(LEAF_WRAPPERS),
    _alias_stored("shared_unique_move_only_value", "std::shared_ptr<std::unique_ptr<move_only_leaf_type>>", "nested_wrapper_type", {"shared"}, {"stored_leaf_access_move_only"}, alias="shared_unique_move_only_type", factory="make_shared_unique_move_only"),
    _alias_stored("shared_optional_value", "std::shared_ptr<std::optional<value_type>>", "nested_wrapper_type", {"shared"}, {"stored_leaf_copy"}),
    _alias_stored("shared_optional_move_only_value", "std::shared_ptr<std::optional<move_only_leaf_type>>", "nested_wrapper_type", {"shared"}, {"stored_leaf_refusal"}, alias="shared_optional_move_only_type", factory="make_shared_optional_move_only"),
    _alias_stored("shared_const_value", "std::shared_ptr<const value_type>", "nested_wrapper_type", {"shared"}, {"stored_leaf_copy"}),
    _alias_stored("shared_custom_optional_value", "std::shared_ptr<dingo::test_optional<value_type>>", "nested_wrapper_type", {"shared"}, {"stored_leaf_copy"}),
    *_wrapped_types(EXTERNAL_LEAF_WRAPPERS),
    _external_alias_stored("external_shared_unique_value", "external<std::shared_ptr<std::unique_ptr<value_type>>>", "std::shared_ptr<std::unique_ptr<value_type>>", {"stored_leaf_access"}, "auto external_value = std::make_shared<std::unique_ptr<value_type>>(std::make_unique<value_type>());"),
    _external_alias_stored("external_shared_unique_move_only_value", "external<std::shared_ptr<std::unique_ptr<move_only_leaf_type>>>", "std::shared_ptr<std::unique_ptr<move_only_leaf_type>>", {"stored_leaf_access_move_only"}, "auto external_value = std::make_shared<std::unique_ptr<move_only_leaf_type>>(std::make_unique<move_only_leaf_type>());"),
    _external_alias_stored("external_shared_optional_value", "external<std::shared_ptr<std::optional<value_type>>>", "std::shared_ptr<std::optional<value_type>>", {"stored_leaf_copy"}, "auto external_value = std::make_shared<std::optional<value_type>>(std::in_place);"),
    _external_alias_stored("external_shared_optional_move_only_value", "external<std::shared_ptr<std::optional<move_only_leaf_type>>>", "std::shared_ptr<std::optional<move_only_leaf_type>>", {"stored_leaf_refusal"}, "auto external_value = std::make_shared<std::optional<move_only_leaf_type>>(std::in_place);"),
    _external_alias_stored("external_shared_const_value", "external<std::shared_ptr<const value_type>>", "std::shared_ptr<const value_type>", {"stored_leaf_copy"}, "auto external_value = std::make_shared<const value_type>();"),
    _external_alias_stored("external_shared_custom_optional_value", "external<std::shared_ptr<dingo::test_optional<value_type>>>", "std::shared_ptr<dingo::test_optional<value_type>>", {"stored_leaf_copy"}, "auto external_value = std::make_shared<dingo::test_optional<value_type>>(dingo::type_traits<dingo::test_optional<value_type>>::make());"),
    _alias_stored("unique_shared_value", "std::unique_ptr<std::shared_ptr<value_type>>", "nested_wrapper_type", {"unique"}, {"stored_unique_shared"}),
    _alias_stored("variant_unique_value", "std::variant<std::unique_ptr<value_type>, nested_variant_b>", "variant_unique_wrapper_type", {"unique"}, {"stored_variant_unique"}),
    _alias_stored("variant_shared_value", "std::variant<std::shared_ptr<value_type>, nested_variant_b>", "variant_shared_wrapper_type", {"shared"}, {"stored_variant_shared"}),
    _alias_stored("variant_shared_unique_value", "std::variant<std::shared_ptr<std::unique_ptr<value_type>>, nested_variant_b>", "variant_shared_unique_wrapper_type", {"shared"}, {"stored_variant_shared_unique"}),
    _alias_stored("unique_variant_value", "std::unique_ptr<std::variant<nested_variant_a, nested_variant_b>>", "nested_wrapper_type", {"unique"}, {"stored_unique_variant"}),
    _alias_stored("shared_variant_value", "std::shared_ptr<std::variant<nested_variant_a, nested_variant_b>>", "nested_wrapper_type", {"shared"}, {"stored_shared_variant"}),
    _alias_stored("array_variant_value", "std::variant<std::array<value_type, 2>, nested_variant_b>", "nested_wrapper_type", {"shared"}, {"stored_array_variant"}),
    *(
        _leaf_stored(name, {"stored_value"}, LOCAL_HEADERS)
        for name in LOCAL_TYPES
    ),
    *(
        _leaf_stored(name, {"stored_value"}, FACTORY_HEADERS)
        for name in FACTORY_TYPES
    ),
    _leaf_stored(
        "factory_callable_value_type",
        {"stored_value"},
        FACTORY_HEADERS,
        modes={"runtime", "mixed"},
    ),
    *_wrapped_types(IMPLEMENTATION_WRAPPERS),
)
