#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Construct operator classes.

The per-cell claim ``resolution/runtime/<scope>/<storage>/construct.cpp`` asks
whether one row's request can be consumed by ``construct``. This family asks a
different question: which constructor shapes does the operator support, and what
does "supported" mean. It is not tied to a storage cell, so each case names one
canonical cell for its dependency and one constructor shape:

    resolution/runtime/global/construct/<shape>/<traits>.cpp

The optional constructor parameter is a known compile-time gap; it is recorded
in ``GAPS`` and has no generated case.
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

GAPS = {
    "optional_parameter": (
        "constructor parameter of type std::optional<...> is ambiguous in the "
        "constructor argument adaptor; resolve and invoke accept the same row"
    ),
}


def _cell(scope: str, form: str, leaf: str = "dependency") -> str:
    line, _ = registration(FORM_BY_NAME[form], scope, "runtime", leaf=leaf)
    return line


@dataclass(frozen=True)
class ConstructShape:
    shape: str
    traits: str
    structs: str
    setup: str
    call: str
    expected: str
    accessor: str = "value.value"


_SHARED = _cell("shared", "plain")
_UNIQUE = _cell("unique", "plain", "movable_dependency")
_SHARED_HANDLE = _cell("shared", "shared_handle")
_UNIQUE_HANDLE = _cell("unique", "unique_handle")
_SHARED_VARIANT = _cell("shared", "alternative")


def build_shapes() -> tuple[ConstructShape, ...]:
    return (
        ConstructShape(
            shape="default",
            traits="copyable",
            structs="""
struct default_constructible {
  default_constructible() : value(7) {}
  int value;
};
""",
            setup="",
            call="container.template construct<default_constructible>()",
            expected="7",
        ),
        ConstructShape(
            shape="lvalue_reference",
            traits="copyable",
            structs="""
struct constructed_lvalue {
  explicit constructed_lvalue(dependency &dep) : value(dep.value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<constructed_lvalue>()",
            expected="7",
        ),
        ConstructShape(
            shape="const_lvalue_reference",
            traits="copyable",
            structs="""
struct constructed_const_lvalue {
  explicit constructed_const_lvalue(const dependency &dep) : value(dep.value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<constructed_const_lvalue>()",
            expected="7",
        ),
        ConstructShape(
            shape="value",
            traits="copyable",
            structs="""
struct constructed_value {
  explicit constructed_value(dependency dep) : value(dep.value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<constructed_value>()",
            expected="7",
        ),
        ConstructShape(
            shape="value",
            traits="move_only",
            structs="""
struct movable_constructed_value {
  explicit movable_constructed_value(movable_dependency dep)
      : value(dep.value) {}
  int value;
};
""",
            setup=_UNIQUE,
            call="container.template construct<movable_constructed_value>()",
            expected="9",
        ),
        ConstructShape(
            shape="rvalue_reference",
            traits="move_only",
            structs="""
struct constructed_rvalue {
  explicit constructed_rvalue(movable_dependency &&dep) : value(dep.value) {}
  int value;
};
""",
            setup=_UNIQUE,
            call="container.template construct<constructed_rvalue>()",
            expected="9",
        ),
        ConstructShape(
            shape="pointer",
            traits="copyable",
            structs="""
struct constructed_pointer {
  explicit constructed_pointer(dependency *dep) : value(dep->value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<constructed_pointer>()",
            expected="7",
        ),
        ConstructShape(
            shape="const_pointer",
            traits="copyable",
            structs="""
struct constructed_const_pointer {
  explicit constructed_const_pointer(const dependency *dep)
      : value(dep->value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<constructed_const_pointer>()",
            expected="7",
        ),
        ConstructShape(
            shape="shared_handle",
            traits="copyable",
            structs="""
struct constructed_shared_handle {
  explicit constructed_shared_handle(std::shared_ptr<dependency> dep)
      : value(dep->value) {}
  int value;
};
""",
            setup=_SHARED_HANDLE,
            call="container.template construct<constructed_shared_handle>()",
            expected="7",
        ),
        ConstructShape(
            shape="unique_handle",
            traits="copyable",
            structs="""
struct constructed_unique_handle {
  explicit constructed_unique_handle(std::unique_ptr<dependency> dep)
      : value(dep->value) {}
  int value;
};
""",
            setup=_UNIQUE_HANDLE,
            call="container.template construct<constructed_unique_handle>()",
            expected="7",
        ),
        ConstructShape(
            shape="variant",
            traits="copyable",
            structs="""
struct constructed_variant {
  explicit constructed_variant(std::variant<dependency, long> dep)
      : value(std::get<0>(dep).value) {}
  int value;
};
""",
            setup=_SHARED_VARIANT,
            call="container.template construct<constructed_variant>()",
            expected="7",
        ),
        ConstructShape(
            shape="two_arguments",
            traits="copyable",
            structs="""
struct constructed_twice {
  constructed_twice(dependency &first, dependency &second)
      : value(first.value + second.value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<constructed_twice>()",
            expected="14",
        ),
        ConstructShape(
            shape="selected",
            traits="copyable",
            structs="""
struct selected_constructor {
  DINGO_CONSTRUCTOR(selected_constructor(dependency &dep))
      : value(dep.value) {}
  selected_constructor(int) : value(-1) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<selected_constructor>()",
            expected="7",
        ),
        ConstructShape(
            shape="explicit_constructor",
            traits="copyable",
            structs="""
struct explicit_constructor {
  explicit explicit_constructor(dependency &dep) : value(dep.value) {}
  int value;
};
""",
            setup=_SHARED,
            call=(
                "container.template construct<\n"
                "      explicit_constructor,\n"
                "      dingo::constructor<explicit_constructor(dependency &)>>()"
            ),
            expected="7",
        ),
        ConstructShape(
            shape="highest_arity",
            traits="copyable",
            structs="""
struct overloaded_constructor {
  overloaded_constructor() : value(0) {}
  explicit overloaded_constructor(dependency &dep) : value(dep.value) {}
  int value;
};
""",
            setup=_SHARED,
            call="container.template construct<overloaded_constructor>()",
            expected="7",
        ),
        ConstructShape(
            shape="aggregate",
            traits="copyable",
            structs="""
struct aggregate_target {
  int first;
  int second;
};
""",
            setup=(
                "container.register_type<dingo::scope<dingo::shared>, "
                "dingo::storage<int>>();"
            ),
            call="container.template construct<aggregate_target>()",
            expected="0",
            accessor="value.first",
        ),
        ConstructShape(
            shape="registered_target",
            traits="copyable",
            structs="""
struct registered_target {
  explicit registered_target(dependency &dep) : value(dep.value) {}
  int value;
};
""",
            setup=(
                f"{_SHARED}\n"
                "  container.register_type<dingo::scope<dingo::shared>,\n"
                "                          dingo::storage<registered_target>>();"
            ),
            call="container.template construct<registered_target>()",
            expected="7",
        ),
        ConstructShape(
            shape="wrapper_target",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call="container.template construct<std::shared_ptr<dependency>>()",
            expected="7",
            accessor="value->value",
        ),
    )


@dataclass(frozen=True)
class ConstructShard:
    shape: str
    traits: str
    path: str
    case_path: str
    suite: str
    namespace: str
    structs: str
    setup: str
    call: str
    expected: str
    accessor: str


def build_shards() -> list[ConstructShard]:
    shards = []
    for shape in build_shapes():
        case = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"construct/{shape.shape}",
            traits=shape.traits,
        )
        shards.append(
            ConstructShard(
                shape=shape.shape,
                traits=shape.traits,
                path=f"{case.file[:-4]}/{shape.traits}.cpp",
                case_path=case.path,
                suite=f"{case.suite}_{shape.traits}",
                namespace=f"{case.suite}_{shape.traits}",
                structs=shape.structs,
                setup=shape.setup,
                call=shape.call,
                expected=shape.expected,
                accessor=shape.accessor,
            )
        )
    return shards


def render(shard: ConstructShard) -> str:
    return render_template("construct_shapes.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
