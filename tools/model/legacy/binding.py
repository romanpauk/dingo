#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The binding/exposure algebra for the standard exposed types.

An exposed type is a binding form applied to a stored kind. The kind, the
provided capability tags, and the registration recipe follow from the binding
form and the interface/key/tag qualifiers; only the case name and the stored
kind are declared.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

# The legacy binding algebra is still consumed by test/matrix, whose schema
# module lives beside those tests. Keep that dependency explicit until the
# matrix axes move to the packaged model.
_MATRIX_DIR = Path(__file__).resolve().parents[3] / "test" / "matrix"
if str(_MATRIX_DIR) not in sys.path:
    sys.path.insert(0, str(_MATRIX_DIR))

from schema import (
    ExposedType,
    MixedRegistrationPlacement,
    RegistrationSpec,
)


@dataclass(frozen=True)
class BindingCase:
    name: str
    binding: str
    stored_kind: str
    interfaces: tuple[str, ...] = ()
    key: str | None = None
    tag: str | None = None
    mixed: bool = False


def _interfaces(names: tuple[str, ...]) -> str:
    return f"dingo::interfaces<{', '.join(names)}>"


def _kind(case: BindingCase) -> str:
    if case.binding == "custom":
        return "custom_interface" if case.interfaces else "custom_concrete"
    return case.binding


def _provides(case: BindingCase) -> set[str]:
    if case.binding == "concrete":
        return {"concrete_binding"}
    if case.binding == "interface":
        return {"concrete_binding", "interface_binding"}
    if case.binding == "custom":
        return {"custom_wrapper_binding"}
    if case.binding == "keyed":
        provides = {"keyed_binding"}
        if case.interfaces:
            provides.add("interface_binding")
        return provides
    if case.binding == "annotated":
        provides = {"annotated_binding"}
        if case.interfaces:
            provides.add("interface_binding")
        return provides
    raise ValueError(f"unknown binding form {case.binding}")


def _registrations(case: BindingCase) -> tuple[RegistrationSpec, ...]:
    placement = (
        MixedRegistrationPlacement.RUNTIME
        if case.mixed
        else MixedRegistrationPlacement.STATIC
    )
    if case.binding == "concrete":
        return (RegistrationSpec(),)
    if case.binding == "custom":
        if not case.interfaces:
            return (RegistrationSpec(),)
        return (RegistrationSpec(interfaces=_interfaces(case.interfaces)),)
    if case.binding == "interface":
        return (
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces=_interfaces(case.interfaces),
                mixed_placement=placement,
            ),
        )
    if case.binding == "keyed":
        key = f"dingo::key_type<{case.key}>"
        if not case.interfaces:
            return (RegistrationSpec(key=key),)
        return (
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces=_interfaces(case.interfaces),
                key=key,
            ),
        )
    if case.binding == "annotated":
        if not case.interfaces:
            return (
                RegistrationSpec(
                    interfaces=(
                        "dingo::interfaces<"
                        f"dingo::annotated<value_type, {case.tag}>>"
                    )
                ),
            )
        return (
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                interfaces=(
                    "dingo::interfaces<"
                    f"dingo::annotated<{case.interfaces[0]}, "
                    f"{case.tag}>>"
                )
            ),
        )
    raise ValueError(f"unknown binding form {case.binding}")


CASES = (
    BindingCase("concrete", "concrete", "value_type"),
    BindingCase("interface_type", "interface", "implementation_type", ("interface_type",), mixed=True),
    BindingCase("multiple_interfaces", "interface", "implementation_type", ("interface_type", "second_interface_type")),
    BindingCase("copyable_interface_type", "interface", "implementation_type", ("copyable_interface_type",), mixed=True),
    BindingCase("custom_concrete", "custom", "custom_value_type"),
    BindingCase("custom_interface", "custom", "custom_interface_type", ("interface_type",)),
    BindingCase("custom_unique_interface", "custom", "custom_unique_interface_type", ("unique_interface_type",)),
    BindingCase("keyed_concrete", "keyed", "value_type", key="key_a"),
    BindingCase("keyed_interface", "keyed", "implementation_type", ("interface_type",), key="key_a"),
    BindingCase("annotated_concrete", "annotated", "value_type", tag="tag_a"),
    BindingCase("annotated_interface", "annotated", "implementation_type", ("interface_type",), tag="tag_a"),
)

CASE_BY_NAME = {case.name: case for case in CASES}


def exposed(name: str) -> ExposedType:
    case = CASE_BY_NAME[name]
    return ExposedType(
        name=case.name,
        kind=_kind(case),
        supported_stored_kinds=frozenset({case.stored_kind}),
        provides=frozenset(_provides(case)),
        registrations=_registrations(case),
    )
