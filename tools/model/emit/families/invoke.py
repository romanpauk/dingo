#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Invoke operator classes.

The per-cell claim ``resolution/runtime/<scope>/<storage>/invoke.cpp`` asks
whether one row's request can be consumed by ``invoke``. This family asks which
callable shapes the operator supports, and what "supported" means. It is not
tied to a storage cell, so each case names one canonical cell for its dependency
and one callable shape:

    resolution/runtime/global/invoke/<shape>/<traits>.cpp

An overloaded callable without an explicit signature is a declared gap; it is
recorded in ``GAPS`` and has no generated case.
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
    "ambiguous_overload": (
        "an overloaded callable without an explicit signature is ambiguous; "
        "invoke<Signature>(callable) selects one"
    ),
}


def _cell(scope: str, form: str, leaf: str = "dependency") -> str:
    line, _ = registration(FORM_BY_NAME[form], scope, "runtime", leaf=leaf)
    return line


@dataclass(frozen=True)
class InvokeShape:
    shape: str
    traits: str
    structs: str
    setup: str
    call: str
    expected: str
    expects: str = "value"


_SHARED = _cell("shared", "plain")
_UNIQUE = _cell("unique", "plain", "movable_dependency")
_SHARED_HANDLE = _cell("shared", "shared_handle")


def build_shapes() -> tuple[InvokeShape, ...]:
    return (
        InvokeShape(
            shape="no_arguments",
            traits="copyable",
            structs="",
            setup="",
            call="container.invoke([] { return 7; })",
            expected="7",
        ),
        InvokeShape(
            shape="lvalue_reference",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call="container.invoke([](dependency &dep) { return dep.value; })",
            expected="7",
        ),
        InvokeShape(
            shape="const_lvalue_reference",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke([](const dependency &dep) "
                "{ return dep.value; })"
            ),
            expected="7",
        ),
        InvokeShape(
            shape="value",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call="container.invoke([](dependency dep) { return dep.value; })",
            expected="7",
        ),
        InvokeShape(
            shape="value",
            traits="move_only",
            structs="",
            setup=_UNIQUE,
            call=(
                "container.invoke([](movable_dependency dep) "
                "{ return dep.value; })"
            ),
            expected="9",
        ),
        InvokeShape(
            shape="rvalue_reference",
            traits="move_only",
            structs="",
            setup=_UNIQUE,
            call=(
                "container.invoke([](movable_dependency &&dep) "
                "{ return dep.value; })"
            ),
            expected="9",
        ),
        InvokeShape(
            shape="pointer",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call="container.invoke([](dependency *dep) { return dep->value; })",
            expected="7",
        ),
        InvokeShape(
            shape="const_pointer",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke([](const dependency *dep) "
                "{ return dep->value; })"
            ),
            expected="7",
        ),
        InvokeShape(
            shape="shared_handle",
            traits="copyable",
            structs="",
            setup=_SHARED_HANDLE,
            call=(
                "container.invoke([](std::shared_ptr<dependency> dep) "
                "{ return dep->value; })"
            ),
            expected="7",
        ),
        InvokeShape(
            shape="two_arguments",
            traits="copyable",
            structs="""
struct other_dependency {
  other_dependency() : value(3) {}
  int value;
};
""",
            setup=(
                f"{_SHARED}\n"
                "  container.register_type<dingo::scope<dingo::shared>,\n"
                "                          dingo::storage<other_dependency>>();"
            ),
            call=(
                "container.invoke([](dependency &dep, other_dependency &other) "
                "{ return dep.value + other.value; })"
            ),
            expected="10",
        ),
        InvokeShape(
            shape="void_return",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke([&seen](dependency &dep) "
                "{ seen = dep.value; })"
            ),
            expected="7",
            expects="void",
        ),
        InvokeShape(
            shape="std_function",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke(\n"
                "      std::function<int(dependency &)>([](dependency &dep) {\n"
                "        return dep.value;\n"
                "      }))"
            ),
            expected="7",
        ),
        InvokeShape(
            shape="function_pointer",
            traits="copyable",
            structs="""
inline int free_function(dependency &dep) { return dep.value + 1; }
""",
            setup=_SHARED,
            call="container.invoke(&free_function)",
            expected="8",
        ),
        InvokeShape(
            shape="functor",
            traits="copyable",
            structs="""
struct functor {
  int operator()(dependency &dep) const { return dep.value + 2; }
};
""",
            setup=_SHARED,
            call="container.invoke(functor{})",
            expected="9",
        ),
        InvokeShape(
            shape="explicit_signature",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke<int(dependency &)>(\n"
                "      [](dependency &dep) { return dep.value + 4; })"
            ),
            expected="11",
        ),
        InvokeShape(
            shape="overloaded_signature",
            traits="copyable",
            structs="""
struct overloaded_callable {
  int operator()(dependency &dep) const { return dep.value + 3; }
  int operator()(dependency &dep, long extra) const {
    return dep.value + static_cast<int>(extra);
  }
};
""",
            setup=_SHARED,
            call="container.invoke<int(dependency &)>(overloaded_callable{})",
            expected="10",
        ),
        InvokeShape(
            shape="move_only_return",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke([](dependency &dep) {\n"
                "        return std::make_unique<dependency>(dep);\n"
                "      })->value"
            ),
            expected="7",
        ),
        InvokeShape(
            shape="reference_return",
            traits="copyable",
            structs="",
            setup=_SHARED,
            call=(
                "container.invoke([](dependency &dep) -> dependency & {\n"
                "        return dep;\n"
                "      }).value"
            ),
            expected="7",
        ),
        InvokeShape(
            shape="missing_dependency",
            traits="copyable",
            structs="",
            setup="",
            call="container.invoke([](dependency &dep) { return dep.value; })",
            expected="",
            expects="throw",
        ),
    )


@dataclass(frozen=True)
class InvokeShard:
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
    expects: str


def build_shards() -> list[InvokeShard]:
    shards = []
    for shape in build_shapes():
        case = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"invoke/{shape.shape}",
            traits=shape.traits,
        )
        shards.append(
            InvokeShard(
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
                expects=shape.expects,
            )
        )
    return shards


def render(shard: InvokeShard) -> str:
    return render_template("invoke_shapes.cpp.j2", shard=shard)


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
