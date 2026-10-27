#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generating constructor-detection probes under the detection phase.

One file per constructor shape:

    detection/compile/constructor/<shape>.cpp

The file contains the type declaration, a static assertion on the detected
arity, and a presence test. This is the first slice of the detection phase; the
richer backend/mode/shape matrix can move here next.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog


@dataclass(frozen=True)
class ConstructorShape:
    name: str
    declaration: str
    type: str
    arity: int
    kind: str = "concrete"


SHAPES = (
    ConstructorShape(
        "zero_argument",
        "struct probe_type {};",
        "probe_type",
        0,
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
    ),
    ConstructorShape(
        "aggregate",
        "struct probe_type { int first; float second; };",
        "probe_type",
        2,
    ),
    ConstructorShape(
        "two_values",
        "struct probe_type { probe_type(int, float); };",
        "probe_type",
        2,
    ),
    ConstructorShape(
        "dependency",
        "struct probe_type { explicit probe_type(int); };",
        "probe_type",
        1,
    ),
    ConstructorShape(
        "annotated_constructor",
        """struct probe_type {
  using dingo_constructor_type = dingo::constructor<probe_type(int)>;
};""",
        "probe_type",
        1,
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
    ),
)


def render(shape: ConstructorShape) -> str:
    return render_template("detection_constructor.cpp.j2", shape=shape)


def manifest() -> str:
    payload = {
        "shapes": [
            {
                "name": shape.name,
                "path": f"detection/compile/constructor/{shape.name}",
                "file": f"detection/compile/constructor/{shape.name}.cpp",
                "arity": shape.arity,
                "kind": shape.kind,
            }
            for shape in SHAPES
        ]
    }
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    for shape in SHAPES:
        emit(
            arguments.out_dir
            / "detection"
            / "compile"
            / "constructor"
            / f"{shape.name}.cpp",
            render(shape),
            check=arguments.check,
        )
    if arguments.manifest is not None:
        emit(arguments.manifest, manifest(), check=arguments.check)


if __name__ == "__main__":
    main()
