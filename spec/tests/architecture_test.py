#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Keep the model layered: grammar -> domain -> semantics -> emit -> report."""

from __future__ import annotations

import ast
from pathlib import Path

LAYERS = {
    "grammar": 0,
    "domain": 1,
    "semantics": 2,
    "gen": 3,
    "report": 4,
}
MODEL_ROOT = Path(__file__).resolve().parents[1]


def _module_layer(path: Path) -> int | None:
    relative = path.relative_to(MODEL_ROOT)
    parts = relative.parts
    if len(parts) < 2:
        return None
    if parts[0] == "model":
        return LAYERS.get(parts[1]) if len(parts) > 2 else None
    return LAYERS.get(parts[0])


def _imported_layer(node: ast.AST) -> int | None:
    if isinstance(node, ast.ImportFrom) and node.module:
        parts = node.module.split(".")
    else:
        return None
    if not parts or parts[0] != "spec":
        return None
    if parts[1] == "model":
        return LAYERS.get(parts[2]) if len(parts) > 2 else None
    return LAYERS.get(parts[1])


def test_model_imports_only_flow_downwards() -> None:
    violations = []
    for path in sorted(MODEL_ROOT.rglob("*.py")):
        source_layer = _module_layer(path)
        if source_layer is None:
            continue
        tree = ast.parse(path.read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            target_layer = _imported_layer(node)
            if target_layer is None:
                continue
            if target_layer > source_layer:
                violations.append(
                    f"{path.relative_to(MODEL_ROOT)} imports "
                    f"layer {target_layer} from layer {source_layer}"
                )
    assert not violations, "\n".join(violations)
