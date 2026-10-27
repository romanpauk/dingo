#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Construct: one test per (argument type, constructor shape).

The parameter of a constructor is a dependency type; the constructor itself has
a shape (one argument, two arguments, a selected overload, an explicit
signature, or a higher-arity overload). Neither axis alone answers whether an
arbitrary target is constructible, so the family is their product:

    resolution/runtime/global/construct/<argument>/<shape>.cpp

Targets that are not about the parameter shape (default construction, an
aggregate, a registered target, a wrapper target) are separate cases.

A combination that cannot be formed is listed in ``GAPS`` with a reason instead
of being dropped.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.recipe import registration
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.hierarchy import CasePath
from tools.model.grammar.catalog import FORM_BY_NAME

GAPS: dict[str, str] = {}


def _registration(scope: str, form: str, leaf: str = "dependency") -> str:
    line, _ = registration(FORM_BY_NAME[form], scope, "runtime", leaf=leaf)
    return line


@dataclass(frozen=True)
class Argument:
    name: str
    param: str
    value: str
    registration: str
    expected: int = 7
    prelude: str = ""
    declarator: str = "{type} {name}"


@dataclass(frozen=True)
class Shape:
    name: str


ARGUMENTS = (
    Argument(
        name="value",
        param="dependency",
        value="{x}.value",
        registration=_registration("shared", "plain"),
    ),
    Argument(
        name="value_move_only",
        param="movable_dependency",
        value="{x}.value",
        registration=_registration("unique", "plain", "movable_dependency"),
        expected=9,
    ),
    Argument(
        name="lvalue_reference",
        param="dependency &",
        value="{x}.value",
        registration=_registration("shared", "plain"),
    ),
    Argument(
        name="const_lvalue_reference",
        param="const dependency &",
        value="{x}.value",
        registration=_registration("shared", "plain"),
    ),
    Argument(
        name="rvalue_reference",
        param="movable_dependency &&",
        value="{x}.value",
        registration=_registration("unique", "plain", "movable_dependency"),
        expected=9,
    ),
    Argument(
        name="pointer",
        param="dependency *",
        value="{x}->value",
        registration=_registration("shared", "plain"),
    ),
    Argument(
        name="const_pointer",
        param="const dependency *",
        value="{x}->value",
        registration=_registration("shared", "plain"),
    ),
    Argument(
        name="shared_handle",
        param="std::shared_ptr<dependency>",
        value="{x}->value",
        registration=_registration("shared", "shared_handle"),
    ),
    Argument(
        name="shared_handle_reference",
        param="std::shared_ptr<dependency> &",
        value="{x}->value",
        registration=_registration("shared", "shared_handle"),
    ),
    Argument(
        name="shared_handle_const_reference",
        param="const std::shared_ptr<dependency> &",
        value="{x}->value",
        registration=_registration("shared", "shared_handle"),
    ),
    Argument(
        name="unique_handle",
        param="std::unique_ptr<dependency>",
        value="{x}->value",
        registration=_registration("unique", "unique_handle"),
    ),
    Argument(
        name="optional_reference",
        param="std::optional<dependency> &",
        value="{x}->value",
        registration=(
            "container.register_type<dingo::scope<dingo::shared>,\n"
            "    dingo::storage<std::optional<dependency>>,\n"
            "    dingo::factory<dingo::function<&make_optional_dependency>>>();"
        ),
    ),
    Argument(
        name="variant",
        param="std::variant<dependency, long>",
        value="std::get<0>({x}).value",
        registration=_registration("shared", "alternative"),
    ),
    Argument(
        name="variant_reference",
        param="std::variant<dependency, long> &",
        value="std::get<0>({x}).value",
        registration=_registration("shared", "alternative"),
    ),
    Argument(
        name="array_reference",
        param="dependency (&)[2]",
        value="{x}[0].value",
        declarator="dependency (&{name})[2]",
        registration=(
            "container.register_type<dingo::scope<dingo::shared>,\n"
            "                          dingo::storage<dependency[2]>>();"
        ),
    ),
    Argument(
        name="annotated_reference",
        param="dingo::annotated<dependency &, annotation_tag>",
        value="static_cast<dependency &>({x}).value",
        registration=(
            "container.register_type<\n"
            "      dingo::scope<dingo::shared>, dingo::storage<dependency>,\n"
            "      dingo::interfaces<dingo::annotated<dependency, "
            "annotation_tag>>>();"
        ),
    ),
)

SHAPES = (
    Shape("deduced"),
    Shape("twice"),
    Shape("selected"),
    Shape("explicit"),
    Shape("highest_arity"),
)


def _param(argument: Argument, name: str) -> str:
    return argument.declarator.format(type=argument.param, name=name)


def _body(argument: Argument, shape: Shape) -> tuple[str, str, str]:
    """The target struct, the call, and the expected value."""
    value = argument.value
    first = _param(argument, "first")
    second = _param(argument, "second")
    expected = str(argument.expected)
    twice = str(argument.expected * 2)
    if shape.name == "deduced":
        structs = f"""
struct target {{
  explicit target({first}) : value({value.format(x="first")}) {{}}
  int value;
}};
"""
        return structs, "container.template construct<target>()", expected
    if shape.name == "twice":
        structs = f"""
struct target {{
  target({first}, {second})
      : value({value.format(x="first")} + {value.format(x="second")}) {{}}
  int value;
}};
"""
        return structs, "container.template construct<target>()", twice
    if shape.name == "selected":
        structs = f"""
struct target {{
  DINGO_CONSTRUCTOR(target({first}))
      : value({value.format(x="first")}) {{}}
  target(int) : value(-1) {{}}
  int value;
}};
"""
        return structs, "container.template construct<target>()", expected
    if shape.name == "explicit":
        structs = f"""
struct target {{
  explicit target({first}) : value({value.format(x="first")}) {{}}
  int value;
}};
"""
        call = (
            "container.template construct<\n"
            f"      target, dingo::constructor<target({argument.param})>>()"
        )
        return structs, call, expected
    structs = f"""
struct target {{
  target() : value(-1) {{}}
  explicit target({first}) : value({value.format(x="first")}) {{}}
  int value;
}};
"""
    return structs, "container.template construct<target>()", expected


TARGETS = (
    (
        "default_construction",
        """
struct target {
  target() : value(7) {}
  int value;
};
""",
        "",
        "container.template construct<target>()",
        "7",
        "value.value",
    ),
    (
        "aggregate",
        """
struct target {
  int first;
  int second;
};
""",
        "container.register_type<dingo::scope<dingo::shared>, "
        "dingo::storage<int>>();",
        "container.template construct<target>()",
        "0",
        "value.first",
    ),
    (
        "registered_target",
        """
struct target {
  explicit target(dependency &dep) : value(dep.value) {}
  int value;
};
""",
        "container.register_type<dingo::scope<dingo::shared>, "
        "dingo::storage<dependency>>();\n"
        "  container.register_type<dingo::scope<dingo::shared>,\n"
        "                          dingo::storage<target>>();",
        "container.template construct<target>()",
        "7",
        "value.value",
    ),
    (
        "wrapper_target",
        "",
        "container.register_type<dingo::scope<dingo::shared>, "
        "dingo::storage<dependency>>();",
        "container.template construct<std::shared_ptr<dependency>>()",
        "7",
        "value->value",
    ),
)


def build_shards() -> list["ConstructShard"]:
    shards: list[ConstructShard] = []
    for argument in ARGUMENTS:
        for shape in SHAPES:
            key = f"{argument.name}/{shape.name}"
            if key in GAPS:
                continue
            structs, call, expected = _body(argument, shape)
            path = CasePath(
                phase="resolution",
                container="runtime",
                scope="global",
                category="construct",
                traits=argument.name,
                request=shape.name,
            )
            shards.append(
                ConstructShard(
                    argument=argument.name,
                    shape=shape.name,
                    structs=argument.prelude + structs,
                    registration=argument.registration,
                    call=call,
                    expected=expected,
                    path=f"{path.file[:-4]}/{argument.name}/{shape.name}.cpp",
                    case_path=path.path,
                    suite=f"{path.suite}_{argument.name}_{shape.name}",
                    namespace=f"{path.suite}_{argument.name}_{shape.name}",
                    accessor="value.value",
                )
            )
    for name, structs, registration, call, expected, accessor in TARGETS:
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"construct/{name}",
            request="shape",
        )
        shards.append(
            ConstructShard(
                argument=name,
                shape=name,
                structs=structs,
                registration=registration,
                call=call,
                expected=expected,
                path=f"{path.file[:-4]}/shape.cpp",
                case_path=path.path,
                suite=f"{path.suite}_{name}",
                namespace=f"{path.suite}_{name}",
                accessor=accessor,
            )
        )
    return shards


@dataclass(frozen=True)
class ConstructShard:
    argument: str
    shape: str
    structs: str
    registration: str
    call: str
    expected: str
    path: str
    case_path: str
    suite: str
    namespace: str
    accessor: str


def render(shard: ConstructShard) -> str:
    return render_template("construct_types.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--include", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    if arguments.include is not None:
        validate_catalog(arguments.include)
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
