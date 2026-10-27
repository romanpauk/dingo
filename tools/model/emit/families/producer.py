#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Producer classes.

The per-row claims register a storage form with the factory it chooses.
This family asks the other half of the question: when the storage form stays the same
and only the way its value is produced changes, does the capability change?

Each pair is registered with every producer it accepts, and every producer gets
the same three claims against the same dependency:

    resolution/runtime/global/producer/<pair>/<kind>/{resolve,construct,invoke}

The asserted value is the same for every producer, so a producer that quietly
changes the result fails.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.hierarchy import CasePath

TARGETS = (
    "shared_plain",
    "shared_pointer",
    "shared_shared_handle",
    "unique_unique_handle",
    "shared_alternative",
    "shared_optional",
)

CLAIMS = ("resolve", "construct", "invoke")


@dataclass(frozen=True)
class ProducerCase:
    kind: str
    registration: str
    request: str = "produced &"


@dataclass(frozen=True)
class ProducerTarget:
    name: str
    cases: tuple[ProducerCase, ...]


def _plain(scope: str, storage: str, kind: str) -> str:
    head = (
        f"container.register_type<dingo::scope<dingo::{scope}>, "
        f"dingo::storage<{storage}>"
    )
    if kind == "detected":
        return head + ">();"
    if kind == "typedef":
        return (
            "container.register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<produced_typedef>>();"
        )
    if kind == "constructor":
        return (
            head
            + ", dingo::factory<dingo::constructor<produced(dependency &)>>>();"
        )
    if kind == "function":
        return head + ", dingo::factory<dingo::function<&make_produced>>>();"
    if kind == "callable":
        return (
            head
            + ">(\n      dingo::callable([](dependency &dep) {\n"
            "        return produced(dep);\n      }));"
        )
    if kind == "callable_signature":
        return (
            head
            + ">(\n      dingo::callable<produced(dependency &)>("
            "[](auto &dep) {\n        return produced(dep);\n      }));"
        )
    if kind == "overloaded_callable":
        return (
            head
            + ">(\n      dingo::callable<produced(dependency &)>("
            "overloaded_producer{}));"
        )
    raise ValueError(kind)


def _wrapped(scope: str, storage: str, kind: str, factory: str, call: str) -> str:
    head = (
        f"container.register_type<dingo::scope<dingo::{scope}>, "
        f"dingo::storage<{storage}>"
    )
    if kind == "function":
        return head + f", dingo::factory<dingo::function<&{factory}>>>();"
    if kind == "callable":
        return (
            head
            + ">(\n      dingo::callable([](dependency &dep) {\n"
            f"        return {call}(dep);\n      }}));"
        )
    raise ValueError(kind)


def _wrapped_cases(
    scope: str,
    storage: str,
    factory: str,
    call: str,
    request: str = "produced &",
) -> tuple[ProducerCase, ...]:
    return tuple(
        ProducerCase(
            kind=kind,
            registration=_wrapped(scope, storage, kind, factory, call),
            request=request,
        )
        for kind in ("function", "callable")
    )


def build_targets() -> tuple[ProducerTarget, ...]:
    plain_kinds = (
        "detected",
        "constructor",
        "function",
        "callable",
        "callable_signature",
        "typedef",
        "overloaded_callable",
    )
    return (
        ProducerTarget(
            name="shared_plain",
            cases=tuple(
                ProducerCase(
                    kind=kind,
                    registration=_plain("shared", "produced", kind),
                    request=(
                        "produced_typedef &" if kind == "typedef" else "produced &"
                    ),
                )
                for kind in plain_kinds
            ),
        ),
        ProducerTarget(
            name="shared_pointer",
            cases=_wrapped_cases(
                "shared", "produced *", "make_pointer", "make_pointer"
            ),
        ),
        ProducerTarget(
            name="shared_shared_handle",
            cases=_wrapped_cases(
                "shared",
                "std::shared_ptr<produced>",
                "make_shared",
                "make_shared",
            ),
        ),
        ProducerTarget(
            name="unique_unique_handle",
            cases=_wrapped_cases(
                "unique",
                "std::unique_ptr<produced>",
                "make_unique",
                "make_unique",
                "std::unique_ptr<produced>",
            ),
        ),
        ProducerTarget(
            name="shared_alternative",
            cases=_wrapped_cases(
                "shared",
                "std::variant<produced, long>",
                "make_variant",
                "make_variant",
            ),
        ),
        ProducerTarget(
            name="shared_optional",
            cases=_wrapped_cases(
                "shared", "std::optional<produced>", "make_optional", "make_optional"
            ),
        ),
    )


@dataclass(frozen=True)
class ProducerShard:
    target: str
    path: str
    case_path: str
    suite: str
    namespace: str
    cases: tuple[ProducerCase, ...]


def build_shards() -> list[ProducerShard]:
    shards = []
    for target in build_targets():
        case = CasePath(
            phase="resolution",
            container="runtime",
            scope="global",
            category=f"producer/{target.name}",
        )
        shards.append(
            ProducerShard(
                target=target.name,
                path=case.file,
                case_path=f"{case.path}",
                suite=case.suite,
                namespace=case.suite,
                cases=target.cases,
            )
        )
    return shards


def render(shard: ProducerShard) -> str:
    return render_template("producer_cases.cpp.j2", shard=shard, claims=CLAIMS)


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
