#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Collection assembly classes.

A collection is several bindings assembled into one value. The mechanism has
three degrees of freedom and nothing else:

- registration mode: static bindings;
- keyed or unkeyed: the key selects which collection is assembled;
- collection shape: a vector of elements, or a map built with an insert policy.

The element storage and its request form are the row probes, so they are not a
dimension here. Every case registers two interface implementations and asserts
that both arrive.

    resolution/<mode>/global/collection/<case>.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.gen.shared.runner import emit, render_template
from spec.gen.shared.hierarchy import CasePath

BIND = """dingo::bind<dingo::scope<dingo::shared>,
                    dingo::storage<std::shared_ptr<implementation_{impl}>>,
                    dingo::interfaces<interface_type>{key}>"""


def _bind(impl: str, key: str = "") -> str:
    return BIND.format(impl=impl, key=key)


def _bindings(*items: str) -> str:
    inner = ",\n    ".join(items)
    return f"using source = dingo::bindings<\n    {inner}>;"


def _runtime(impl: str, key: str = "") -> str:
    return (
        "container.register_type<dingo::scope<dingo::shared>,\n"
        f"                          dingo::storage<std::shared_ptr<implementation_{impl}>>,\n"
        f"                          dingo::interfaces<interface_type>{key}>();"
    )


@dataclass(frozen=True)
class CollectionCase:
    name: str
    source: str
    container: str
    setup: str
    body: str


def build_cases(mode: str) -> tuple[CollectionCase, ...]:
    key = ", dingo::key_type<key_a>"
    keyed_arg = "(dingo::key_type<key_a>{})"
    if mode == "static":
        source = _bindings(_bind("a"), _bind("b"))
        keyed_source = _bindings(_bind("a", key), _bind("b", key))
        container = "dingo::static_container<source> container;"
        setup = ""
        keyed_setup = ""
    else:
        source = _bindings(_bind("a"))
        keyed_source = _bindings(_bind("a", key))
        container = "dingo::container<source> container;"
        setup = _runtime("b")
        keyed_setup = _runtime("b", key)

    return (
        CollectionCase(
            name="interface_vector",
            source=source,
            container=container,
            setup=setup,
            body="""auto values = container.resolve<std::vector<std::shared_ptr<interface_type>>>();
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0]->id() + values[1]->id(), 1);""",
        ),
        CollectionCase(
            name="keyed_vector",
            source=keyed_source,
            container=container,
            setup=keyed_setup,
            body="""auto values =
      container.resolve<std::vector<std::shared_ptr<interface_type>>>(
          dingo::key_type<key_a>{});
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0]->id() + values[1]->id(), 1);""",
        ),
        CollectionCase(
            name="construct_vector",
            source=source,
            container=container,
            setup=setup,
            body="""auto values = container.construct_collection<
      std::vector<std::shared_ptr<interface_type>>>();
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0]->id() + values[1]->id(), 1);""",
        ),
        CollectionCase(
            name="construct_map",
            source=source,
            container=container,
            setup=setup,
            body="""auto values = container.construct_collection<
      std::map<int, std::shared_ptr<interface_type>>>(
      [](auto &collection, auto &&value) {
        collection.emplace(value->id(), std::move(value));
      });
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values.at(0)->id(), 0);
  EXPECT_EQ(values.at(1)->id(), 1);""",
        ),
        CollectionCase(
            name="keyed_construct_vector",
            source=keyed_source,
            container=container,
            setup=keyed_setup,
            body=f"""auto values = container.construct_collection<
      std::vector<std::shared_ptr<interface_type>>>{keyed_arg};
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0]->id() + values[1]->id(), 1);""",
        ),
    )


@dataclass(frozen=True)
class CollectionShard:
    mode: str
    path: str
    case_path: str
    suite: str
    namespace: str
    cases: tuple[CollectionCase, ...]


def build_shards() -> list[CollectionShard]:
    shards = []
    for mode in ("static",):
        case = CasePath(
            phase="resolution",
            container=mode,
            scope="global",
            category="collection",
        )
        shards.append(
            CollectionShard(
                mode=mode,
                path=case.file,
                case_path=case.path,
                suite=case.suite,
                namespace=case.suite,
                cases=build_cases(mode),
            )
        )
    return shards


def render(shard: CollectionShard) -> str:
    return render_template("collection_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
