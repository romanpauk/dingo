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

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from route_derivation import interface_route_counts
from storage_cells import validate_catalog


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


INTERFACE_ROUTES = (
    InterfaceRoute(
        name="shared_shared_ptr",
        scope="shared",
        storage="std::shared_ptr<implementation_type>",
        constructor="implementation_type",
        interface="interface_type",
        form="shared_handle",
    ),
    InterfaceRoute(
        name="shared_shared_ptr_second",
        scope="shared",
        storage="std::shared_ptr<implementation_type>",
        constructor="implementation_type",
        interface="interface_type2",
        form="shared_handle",
    ),
    InterfaceRoute(
        name="shared_copyable_value",
        scope="shared",
        storage="std::shared_ptr<copyable_implementation>",
        constructor="copyable_implementation",
        interface="copyable_interface",
        form="shared_handle",
        copyable_value=True,
    ),
    InterfaceRoute(
        name="shared_value",
        scope="shared",
        storage="implementation_type",
        constructor="implementation_type",
        interface="interface_type",
        form="plain",
    ),
    InterfaceRoute(
        name="unique_unique_ptr",
        scope="unique",
        storage="std::unique_ptr<implementation_type>",
        constructor="implementation_type",
        interface="interface_type",
        form="unique_handle",
    ),
    InterfaceRoute(
        name="unique_unique_ptr_second",
        scope="unique",
        storage="std::unique_ptr<implementation_type>",
        constructor="implementation_type",
        interface="interface_type2",
        form="unique_handle",
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
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("interface_cells.cpp.j2")
    return template.render(rows=INTERFACE_ROUTES)


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


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
    content = render(arguments.out)
    if arguments.check:
        if not arguments.out.exists() or arguments.out.read_text(
            encoding="utf-8"
        ) != content:
            raise SystemExit(f"{arguments.out} is out of date")
        return
    write_text_if_changed(arguments.out, content)


if __name__ == "__main__":
    main()
