#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Registration recipes derived from a storage form.

The behavior, type-behavior, and annotation families all need the same three
facts: the stored type, the factory, and the external instance. They were
previously spelled out per family; here they follow from the form's wrapper
properties and the requested leaf.
"""

from __future__ import annotations

from dataclasses import dataclass

from spec.model.grammar import cpp
from spec.model.grammar.catalog import StorageForm


@dataclass(frozen=True)
class ExternalInstance:
    declaration: str
    argument: str
    moves: int


def external_instance(
    form: StorageForm,
    leaf: str = "tracked",
    *,
    const: bool = False,
    move: bool = False,
) -> ExternalInstance | None:
    """The caller-owned instance an external registration receives."""
    if const and form.kind == "pointer":
        declaration = (
            f"static const {leaf} pointee{{}}; "
            f"const {leaf} *external_instance = &pointee"
        )
        return ExternalInstance(declaration, "external_instance", 0)
    if form.external_leaf_only:
        declaration = f"{'const ' if const else ''}{leaf} external_instance"
        if form.kind == "array":
            declaration += "[2]"
        # An array instance is handed over by reference; a single movable leaf
        # is transferred into the registration instead of copied.
        transfer = move and form.kind != "array"
        argument = (
            "std::move(external_instance)" if transfer else "external_instance"
        )
        return ExternalInstance(declaration, argument, int(transfer))

    factory = form.const_factory(leaf) if const else form.factory(leaf)
    if factory is None:
        return None
    declaration = f"auto external_instance = {factory}()"
    moves = form.external_moves
    argument = (
        "std::move(external_instance)" if moves else "external_instance"
    )
    return ExternalInstance(declaration, argument, moves)


def registration(
    form: StorageForm,
    scope_name: str,
    container: str,
    *,
    leaf: str = "tracked",
    const: bool = False,
    interfaces: tuple[str, ...] = (),
    key: str | None = None,
    external_argument: str = "",
    storage: str | None = None,
    factory: str | None = None,
) -> tuple[str, str]:
    """Render the runtime registration and the static source for one storage form."""
    scope_type = cpp.template(
        "dingo::scope", cpp.text(f"dingo::{scope_name}")
    ).render()
    interface_args = (
        ", "
        + cpp.template(
            "dingo::interfaces", *(cpp.text(item) for item in interfaces)
        ).render()
        if interfaces
        else ""
    )
    key_arg = (
        ", "
        + cpp.template("dingo::key_type", cpp.text(key)).render()
        if key is not None
        else ""
    )
    if scope_name == "external":
        storage = storage or form.external_storage(leaf)
        return (
            f"container.register_type<{scope_type}, {storage}"
            f"{interface_args}{key_arg}>({external_argument});",
            "",
        )
    explicit_factory = factory is not None
    factory = factory or (
        form.const_factory(leaf) if const else form.factory(leaf)
    )
    # An explicitly supplied factory is always honored; the form's flag only
    # decides whether its default factory is used. A cyclical shared handle
    # constructs its leaf in place, so its factory is the leaf constructor.
    use_factory = (
        factory is not None
        and (form.registration_factory or explicit_factory)
        and not (
            scope_name == "shared_cyclical" and form.name == "shared_handle"
        )
    )
    factory_args = (
        ", "
        + cpp.template(
            "dingo::factory",
            cpp.template("dingo::function", cpp.text(f"&{factory}")),
        ).render()
        if use_factory
        else ""
    )
    storage = storage or form.storage(leaf)
    if container == "runtime":
        return (
            f"container.register_type<{scope_type}, {storage}"
            f"{interface_args}{key_arg}{factory_args}>();",
            "",
        )
    bind = cpp.template(
        "dingo::bind",
        cpp.text(
            f"{scope_type}, {storage}{interface_args}{key_arg}{factory_args}"
        ),
    )
    source = (
        "using source = "
        f"{cpp.template('dingo::bindings', bind).render()};"
    )
    return "", source
