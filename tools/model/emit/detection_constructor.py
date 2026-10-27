#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Constructor detection under the classification phase.

Detection has two independent axes: the backend and the detection mode.

- ``default`` is the backend the current compiler selects;
- ``msvc`` is the MSVC-specific signature recovery, only buildable there.

- ``shape`` reports the selected constructor's arity and kind;
- ``signature`` additionally recovers the parameter ``type_list``.

    classification/constructor/<backend>/<mode>/<shape>.cpp

Signature mode is where the two can disagree: the shape is chosen first and the
signature is recovered for that arity, so the recovered list is what injection
later uses. Asserting the list is the only way to catch a recovery that returns
a different set of types than the constructor takes.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog

BACKENDS = ("default", "msvc")
MODES = ("shape", "signature")


@dataclass(frozen=True)
class ConstructorShape:
    name: str
    declaration: str
    type: str
    arity: int
    arguments: str = ""
    kind: str = "concrete"
    # A shape whose selection happens in the public alias rather than in a
    # detection backend: the typedef wins before any backend runs.
    public_only: bool = False


SHAPES = (
    ConstructorShape(
        "zero_argument",
        "struct probe_type {};",
        "probe_type",
        0,
        "dingo::type_list<>",
    ),
    ConstructorShape(
        "copy_only",
        """struct probe_type {
  probe_type() = default;
  probe_type(const probe_type &) = default;
  probe_type(probe_type &&) = delete;
};""",
        "probe_type",
        0,
        "dingo::type_list<>",
    ),
    ConstructorShape(
        "move_only",
        """struct probe_type {
  probe_type() = default;
  probe_type(const probe_type &) = delete;
  probe_type(probe_type &&) = default;
};""",
        "probe_type",
        0,
        "dingo::type_list<>",
    ),
    ConstructorShape(
        "aggregate",
        "struct probe_type { int first; float second; };",
        "probe_type",
        2,
        "dingo::type_list<int, float>",
    ),
    ConstructorShape(
        "two_values",
        "struct probe_type { probe_type(int, float); };",
        "probe_type",
        2,
        "dingo::type_list<int, float>",
    ),
    ConstructorShape(
        "dependency",
        "struct probe_type { explicit probe_type(int); };",
        "probe_type",
        1,
        "dingo::type_list<int>",
    ),
    ConstructorShape(
        "annotated_constructor",
        """struct probe_type {
  using dingo_constructor_type = dingo::constructor<probe_type(int)>;
};""",
        "probe_type",
        1,
        public_only=True,
    ),
    ConstructorShape(
        "dependency_reference",
        """struct dependency { int value; };
struct probe_type { explicit probe_type(dependency &); };""",
        "probe_type",
        1,
        "dingo::type_list<dependency &>",
    ),
    ConstructorShape(
        "dependency_const_reference",
        """struct dependency { int value; };
struct probe_type { explicit probe_type(const dependency &); };""",
        "probe_type",
        1,
        "dingo::type_list<const dependency &>",
    ),
    ConstructorShape(
        "dependency_rvalue_reference",
        """struct dependency { int value; };
struct probe_type { explicit probe_type(dependency &&); };""",
        "probe_type",
        1,
        # Signature recovery records an rvalue-reference parameter as the
        # value type: the injected prvalue binds to `T &&`, and the recovered
        # list is reused as the construction signature.
        "dingo::type_list<dependency>",
    ),
    ConstructorShape(
        "dependency_pointer",
        """struct dependency { int value; };
struct probe_type { explicit probe_type(dependency *); };""",
        "probe_type",
        1,
        "dingo::type_list<dependency *>",
    ),
    ConstructorShape(
        "std_array_aggregate",
        "struct element { int first; float second; };",
        "std::array<element, 2>",
        2,
        "dingo::type_list<element, element>",
    ),
    ConstructorShape(
        "nested_std_array_aggregate",
        "struct element { int first; float second; };",
        "std::array<std::array<element, 2>, 2>",
        2,
        "dingo::type_list<std::array<element, 2>, std::array<element, 2>>",
    ),
    ConstructorShape(
        "array_move_only",
        """struct element {
  element() = default;
  element(const element &) = delete;
  element(element &&) = default;
};""",
        "std::array<element, 2>",
        2,
        "dingo::type_list<element, element>",
    ),
    ConstructorShape(
        "nested_array_move_only",
        """struct element {
  element() = default;
  element(const element &) = delete;
  element(element &&) = default;
};""",
        "std::array<std::array<element, 2>, 2>",
        2,
        "dingo::type_list<std::array<element, 2>, std::array<element, 2>>",
    ),
)


@dataclass(frozen=True)
class ConstructorShard:
    backend: str
    mode: str
    shape: str
    path: str
    case_path: str
    suite: str
    namespace: str
    declaration: str
    type: str
    arity: int
    arguments: str
    kind: str


def build_shards() -> list[ConstructorShard]:
    shards = []
    for backend in BACKENDS:
        for mode in MODES:
            for shape in SHAPES:
                if backend == "msvc" and shape.public_only:
                    continue
                case_path = (
                    f"classification/constructor/{backend}/{mode}/{shape.name}"
                )
                suite = f"classification_constructor_{backend}_{mode}_{shape.name}"
                shards.append(
                    ConstructorShard(
                        backend=backend,
                        mode=mode,
                        shape=shape.name,
                        path=(
                            f"classification/constructor/{backend}/{mode}/"
                            f"{shape.name}.cpp"
                        ),
                        case_path=case_path,
                        suite=suite,
                        namespace=suite,
                        declaration=shape.declaration,
                        type=shape.type,
                        arity=shape.arity,
                        arguments=shape.arguments,
                        kind=shape.kind,
                    )
                )
    return shards


def render(shard: ConstructorShard) -> str:
    return render_template("detection_constructor.cpp.j2", shard=shard)


def manifest(shards: list[ConstructorShard]) -> str:
    payload = {
        "shards": [
            {
                "backend": shard.backend,
                "mode": shard.mode,
                "shape": shard.shape,
                "path": shard.case_path,
                "file": shard.path,
                "arity": shard.arity,
                "kind": shard.kind,
                "arguments": shard.arguments,
            }
            for shard in shards
        ]
    }
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--include", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    shards = build_shards()
    for shard in shards:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)
    if arguments.manifest is not None:
        emit(arguments.manifest, manifest(shards), check=arguments.check)


if __name__ == "__main__":
    main()
