#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""User wrapper classes.

A wrapper is a type the library is taught about through ``type_traits`` or
``alternative_type_traits``. Wrapper *properties* are classified at compile time;
what is not covered elsewhere is a storage form whose storage is a user wrapper and the
row that unwraps it.

    resolution/runtime/global/wrapper/<case>.cpp

The wrappers live in ``test/support/custom_wrappers.h`` and share nothing with
the table fixtures.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.gen.shared.runner import emit, render_template
from spec.gen.shared.hierarchy import CasePath


@dataclass(frozen=True)
class WrapperCase:
    name: str
    body: str


CASES = (
    WrapperCase(
        name="shared_wrapper",
        body="""dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::shared>, dingo::storage<dingo::test_shared<dependency>>,
      dingo::factory<dingo::function<&make_shared_wrapper>>>();
  auto wrapper = container.resolve<dingo::test_shared<dependency>>();
  ASSERT_NE(wrapper.get(), nullptr);
  EXPECT_EQ(wrapper.get()->value, 7);
  EXPECT_EQ(container.resolve<dependency &>().value, 7);""",
    ),
    WrapperCase(
        name="unique_wrapper",
        body="""dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::unique>, dingo::storage<dingo::test_unique<dependency>>,
      dingo::factory<dingo::function<&make_unique_wrapper>>>();
  auto wrapper = container.resolve<dingo::test_unique<dependency>>();
  ASSERT_NE(wrapper.get(), nullptr);
  EXPECT_EQ(wrapper.get()->value, 7);""",
    ),
    WrapperCase(
        name="optional_wrapper",
        body="""dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<dingo::test_optional<dependency>>,
      dingo::factory<dingo::function<&make_optional_wrapper>>>();
  EXPECT_EQ(container.resolve<dependency &>().value, 7);""",
    ),
    WrapperCase(
        name="variant_wrapper",
        body="""dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::shared>, dingo::storage<dingo::test_variant<dependency>>,
      dingo::factory<dingo::function<&make_variant_wrapper>>>();
  EXPECT_EQ(container.resolve<dependency &>().value, 7);""",
    ),
)


@dataclass(frozen=True)
class WrapperShard:
    name: str
    path: str
    case_path: str
    suite: str
    namespace: str
    body: str


def build_shards() -> list[WrapperShard]:
    shards = []
    for case in CASES:
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"wrapper/{case.name}",
        )
        shards.append(
            WrapperShard(
                name=case.name,
                path=path.file,
                case_path=path.path,
                suite=path.suite,
                namespace=path.suite,
                body=case.body,
            )
        )
    return shards


def render(shard: WrapperShard) -> str:
    return render_template("wrapper_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
