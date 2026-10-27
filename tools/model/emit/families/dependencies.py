#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Declared dependency order.

A type with two constructors that differ only in the order of their arguments
is ambiguous to constructor detection. ``dingo::dependencies<...>`` on the
registration declares which argument goes where, and the container must use it.

The family is the two ways of declaring it, runtime and compile-time, each
resolving the same type. ``construct<T>()`` cannot use the registration
metadata for an ambiguous type, so it is a declared gap:

    resolution/runtime/global/dependencies/<case>.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.hierarchy import CasePath

GAPS = {
    "construct_with_registration_dependencies": (
        "construct<T>() re-derives the constructor and cannot use the "
        "registration's dependencies<...> for an ambiguous type; pass "
        "constructor<T(args...)> instead"
    ),
}


@dataclass(frozen=True)
class DependencyCase:
    name: str
    body: str


CASES = (
    DependencyCase(
        name="runtime_dependencies",
        body="""dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<first_dependency>>();
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<second_dependency>>();
  container.register_type<
      dingo::scope<dingo::shared>, dingo::storage<selected_arguments>,
      dingo::dependencies<second_dependency &, first_dependency &>>();
  EXPECT_EQ(container.resolve<selected_arguments &>().value, 17);""",
    ),
    DependencyCase(
        name="static_dependencies",
        body="""using source = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>,
                 dingo::storage<first_dependency>>,
      dingo::bind<dingo::scope<dingo::shared>,
                 dingo::storage<second_dependency>>,
      dingo::bind<dingo::scope<dingo::shared>,
                 dingo::storage<selected_arguments>,
                 dingo::dependencies<second_dependency &,
                                     first_dependency &>>>;
  dingo::container<source> container;
  EXPECT_EQ(container.resolve<selected_arguments &>().value, 17);""",
    ),
)


@dataclass(frozen=True)
class DependencyShard:
    name: str
    path: str
    case_path: str
    suite: str
    namespace: str
    body: str


def build_shards() -> list[DependencyShard]:
    shards = []
    for case in CASES:
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category="dependencies",
            request=case.name,
        )
        shards.append(
            DependencyShard(
                name=case.name,
                path=f"{path.file[:-4]}/{case.name}.cpp",
                case_path=path.path,
                suite=f"{path.suite}_{case.name}",
                namespace=f"{path.suite}_{case.name}",
                body=case.body,
            )
        )
    return shards


def render(shard: DependencyShard) -> str:
    return render_template("dependency_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
