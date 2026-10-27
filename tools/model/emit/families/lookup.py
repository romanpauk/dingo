#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Lookup scope classes.

A lookup scope is where a binding is found. Two forms exist and they share the
same rule set: a binding may carry its own child container, and a container may
have a parent. The cases are therefore the rules, not the storage cells:

- a local binding is visible to the registration that declares it;
- a local binding shadows a root binding of the same type;
- a child inherits an unshadowed parent binding;
- a child shadows a parent binding without changing the parent;
- a grandchild inherits through the chain.

    resolution/runtime/global/lookup/<case>.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.hierarchy import CasePath

LOCAL_BINDINGS = """dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<dependency>,
               dingo::factory<dingo::function<&make_local>>>>"""

def _root(target: str = "container") -> str:
    return f"""{target}.register_type<
      dingo::scope<dingo::shared>, dingo::storage<dependency>,
      dingo::factory<dingo::function<&make_root>>>();"""


@dataclass(frozen=True)
class LookupCase:
    name: str
    body: str


CASES = (
    LookupCase(
        name="local_binding",
        body=f"""dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<local_host>, {LOCAL_BINDINGS}>();
  EXPECT_EQ(container.resolve<local_host &>().value, 2);""",
    ),
    LookupCase(
        name="local_override",
        body=f"""dingo::container<> container;
  {_root()}
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<local_host>, {LOCAL_BINDINGS}>();
  EXPECT_EQ(container.resolve<local_host &>().value, 2);
  EXPECT_EQ(container.resolve<dependency &>().value, 1);""",
    ),
    LookupCase(
        name="child_inherits",
        body=f"""dingo::runtime_container<> parent;
  {_root("parent")}
  dingo::runtime_container<> child(&parent);
  EXPECT_EQ(child.resolve<dependency &>().value, 1);""",
    ),
    LookupCase(
        name="child_shadows",
        body=f"""dingo::runtime_container<> parent;
  {_root("parent")}
  dingo::runtime_container<> child(&parent);
  child.register_type<dingo::scope<dingo::shared>, dingo::storage<dependency>,
                      dingo::factory<dingo::function<&make_child>>>();
  EXPECT_EQ(child.resolve<dependency &>().value, 3);
  EXPECT_EQ(parent.resolve<dependency &>().value, 1);""",
    ),
    LookupCase(
        name="grandchild",
        body=f"""dingo::runtime_container<> parent;
  {_root("parent")}
  dingo::runtime_container<> child(&parent);
  dingo::runtime_container<> grandchild(&child);
  EXPECT_EQ(grandchild.resolve<dependency &>().value, 1);""",
    ),
)


@dataclass(frozen=True)
class LookupShard:
    name: str
    path: str
    case_path: str
    suite: str
    namespace: str
    body: str


def build_shards() -> list[LookupShard]:
    shards = []
    for case in CASES:
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"lookup/{case.name}",
        )
        shards.append(
            LookupShard(
                name=case.name,
                path=path.file,
                case_path=path.path,
                suite=path.suite,
                namespace=path.suite,
                body=case.body,
            )
        )
    return shards


def render(shard: LookupShard) -> str:
    return render_template("lookup_cells.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
