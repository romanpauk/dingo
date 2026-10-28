#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate interface route probes.

A binding registered with `dingo::interfaces<...>` is looked up through the
interface type, and the interface routes are derived from the storage
conversions. These probes instantiate the real storage and the real interface
resolution kernel, then check that every published target is reachable from
both request matchers and that the scope access satisfies every route.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.model.grammar import cpp
from spec.model.grammar.catalog import FORM_BY_NAME
from spec.model.semantics.routes.derivation import interface_route_counts
from spec.gen.shared.runner import emit, render_template
from spec.gen.shared.validation import validate_catalog


@dataclass(frozen=True)
class InterfaceRoute:
    name: str
    scope: str
    storage: str
    constructor: str
    interface: str
    form: str
    copyable_value: bool = False

    @property
    def counts(self) -> dict[str, int]:
        return interface_route_counts(
            self.scope, self.form, copyable_value=self.copyable_value
        )

    @property
    def value_count(self) -> int:
        return self.counts["value"]

    @property
    def object_count(self) -> int:
        return self.counts["object"]

    @property
    def rvalue_count(self) -> int:
        return self.counts["rvalue"]

    @property
    def value(self) -> bool:
        return self.value_count != 0

    @property
    def object(self) -> bool:
        return self.object_count != 0

    @property
    def rvalue(self) -> bool:
        return self.rvalue_count != 0

    @property
    def access(self) -> str:
        return "borrow" if self.scope == "shared" else "consume"


def _route(
    name: str,
    scope: str,
    form_name: str,
    interface: str,
    *,
    leaf: str = "implementation_type",
    copyable: bool = False,
) -> InterfaceRoute:
    """Render a route from the catalog form and the test interface leaf."""
    if copyable:
        storage = cpp.template("std::shared_ptr", cpp.leaf(leaf)).render()
    else:
        storage = FORM_BY_NAME[form_name].cpp_type(leaf)
    return InterfaceRoute(
        name=name,
        scope=scope,
        storage=storage,
        constructor=leaf,
        interface=interface,
        form=form_name,
        copyable_value=copyable,
    )


INTERFACE_ROUTES = (
    _route("shared_shared_ptr", "shared", "shared_handle", "interface_type"),
    _route(
        "shared_shared_ptr_second",
        "shared",
        "shared_handle",
        "interface_type2",
    ),
    _route(
        "shared_copyable_value",
        "shared",
        "shared_handle",
        "copyable_interface",
        leaf="copyable_implementation",
        copyable=True,
    ),
    _route("shared_value", "shared", "plain", "interface_type"),
    _route("unique_unique_ptr", "unique", "unique_handle", "interface_type"),
    _route(
        "unique_unique_ptr_second",
        "unique",
        "unique_handle",
        "interface_type2",
    ),
)


def validate() -> None:
    if not INTERFACE_ROUTES:
        raise ValueError("interface route catalog must not be empty")
    names = [route.name for route in INTERFACE_ROUTES]
    if len(names) != len(set(names)):
        raise ValueError("interface route catalog contains duplicate names")
    for route in INTERFACE_ROUTES:
        if route.scope not in {"shared", "unique"}:
            raise ValueError(f"interface route {route.name} uses {route.scope}")
        counts = route.counts
        if set(counts) != {"value", "object", "rvalue"}:
            raise ValueError(f"interface route {route.name} has no full counts")
        if any(count < 0 for count in counts.values()):
            raise ValueError(f"interface route {route.name} has a negative count")
        if route.value != (route.value_count != 0):
            raise ValueError(f"interface route {route.name} has inconsistent value")
        if route.object != (route.object_count != 0):
            raise ValueError(f"interface route {route.name} has inconsistent object")
        if route.rvalue != (route.rvalue_count != 0):
            raise ValueError(f"interface route {route.name} has inconsistent rvalue")


def render(output: Path) -> str:
    validate()
    return render_template("interface_routes.cpp.j2", rows=INTERFACE_ROUTES)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    emit(arguments.out, render(arguments.out), check=arguments.check)


if __name__ == "__main__":
    main()
