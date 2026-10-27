#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Conversion classes.

A conversion is the rule that decides whether a value registered as ``S`` can be
delivered as a request ``R``. Two halves matter and they fail differently:

- qualification: a reference or pointer conversion may add cv qualifiers and may
  not remove them, and nested pointers follow the language rule;
- delivery: a value conversion copies or moves, which is observable.

The row probes already request ``T &``, ``const T &``, ``T *`` and
``const T *``, so the add-qualifier direction is covered there. This family owns
the rejecting direction, the volatile axis, nested pointers, the delivery
counts, and the claim that every front-end agrees.

    resolution/runtime/global/conversion/<case>.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.hierarchy import CasePath


@dataclass(frozen=True)
class ConversionCase:
    name: str
    body: str


CASES = (
    ConversionCase(
        name="const_reference",
        body="""const int value = 11;
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::external>,
                          dingo::storage<const int &>>(value);
  EXPECT_EQ(&container.resolve<const int &>(), &value);
  EXPECT_EQ(&container.resolve<const volatile int &>(), &value);
  EXPECT_EQ(container.resolve<const int *>(), &value);
  EXPECT_EQ(container.resolve<const volatile int *>(), &value);
  EXPECT_THROW(require_address(container.resolve<int &>()),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(require_address(container.resolve<volatile int &>()),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<int *>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<volatile int *>(),
               dingo::type_not_convertible_exception);""",
    ),
    ConversionCase(
        name="volatile_reference",
        body="""volatile int value = 7;
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::external>,
                          dingo::storage<volatile int &>>(value);
  EXPECT_EQ(&container.resolve<volatile int &>(), &value);
  EXPECT_EQ(&container.resolve<const volatile int &>(), &value);
  EXPECT_EQ(container.resolve<volatile int *>(), &value);
  EXPECT_EQ(container.resolve<const volatile int *>(), &value);
  EXPECT_THROW(require_address(container.resolve<int &>()),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(require_address(container.resolve<const int &>()),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<int *>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<const int *>(),
               dingo::type_not_convertible_exception);""",
    ),
    ConversionCase(
        name="nested_pointer",
        body="""int value = 13;
  int *pointer = &value;
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::external>,
                          dingo::storage<int **>>(&pointer);
  EXPECT_EQ(container.resolve<int **>(), &pointer);
  EXPECT_EQ(container.resolve<const int *const *>(),
            static_cast<const int *const *>(&pointer));
  EXPECT_THROW(container.resolve<const int **>(),
               dingo::type_not_convertible_exception);""",
    ),
    ConversionCase(
        name="const_value_from_move_only",
        body="""dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>,
                          dingo::storage<move_target>,
                          dingo::factory<dingo::function<&make_move_target>>>();
  auto first = container.resolve<const move_target>();
  auto second = container.resolve<move_target>();
  EXPECT_EQ(first.value, 23);
  EXPECT_EQ(second.value, 23);""",
    ),
    ConversionCase(
        name="copy_only_alternative",
        body="""copy_only::copies = 0;
  copy_only source(41);
  std::variant<copy_only, int> value(std::in_place_type<copy_only>, source);
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::external>,
                          dingo::storage<std::variant<copy_only, int> &>>(
      value);
  copy_only::copies = 0;
  auto resolved = container.resolve<copy_only>();
  EXPECT_EQ(resolved.value, 41);
  EXPECT_EQ(copy_only::copies, 1);""",
    ),
    ConversionCase(
        name="copy_only_value",
        body="""copy_only::copies = 0;
  copy_only source(41);
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::external>,
                          dingo::storage<copy_only &>>(source);
  copy_only::copies = 0;
  auto resolved = container.resolve<copy_only>();
  EXPECT_EQ(resolved.value, 41);
  EXPECT_EQ(copy_only::copies, 1);""",
    ),
    ConversionCase(
        name="front_end_parity",
        body="""dingo::runtime_container<> runtime;
  runtime.register_type<dingo::scope<dingo::shared>, dingo::storage<int>,
                        dingo::factory<dingo::function<&make_eleven>>>();
  check_qualification(runtime);

  dingo::container<> root;
  root.register_type<dingo::scope<dingo::shared>, dingo::storage<int>,
                     dingo::factory<dingo::function<&make_eleven>>>();
  check_qualification(root);

  dingo::container<static_source> mixed;
  check_qualification(mixed);

  dingo::static_container<static_source> fixed;
  check_qualification(fixed);""",
    ),
)


@dataclass(frozen=True)
class ConversionShard:
    name: str
    path: str
    case_path: str
    suite: str
    namespace: str
    body: str


def build_shards() -> list[ConversionShard]:
    shards = []
    for case in CASES:
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"conversion/{case.name}",
        )
        shards.append(
            ConversionShard(
                name=case.name,
                path=path.file,
                case_path=path.path,
                suite=path.suite,
                namespace=path.suite,
                body=case.body,
            )
        )
    return shards


def render(shard: ConversionShard) -> str:
    return render_template("conversion_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
