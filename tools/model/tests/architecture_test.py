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
    "emit": 3,
    "report": 4,
}
MODEL_ROOT = Path(__file__).resolve().parents[1]


def _module_layer(path: Path) -> int | None:
    relative = path.relative_to(MODEL_ROOT)
    if len(relative.parts) < 2:
        return None
    return LAYERS.get(relative.parts[0])


def _imported_layer(node: ast.AST) -> int | None:
    if isinstance(node, ast.ImportFrom) and node.module:
        parts = node.module.split(".")
    elif isinstance(node, ast.Import):
        return None
    else:
        return None
    if parts[:2] != ["tools", "model"] or len(parts) < 3:
        return None
    return LAYERS.get(parts[2])


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


def _imports_legacy(tree: ast.AST) -> bool:
    for node in ast.walk(tree):
        module = None
        if isinstance(node, ast.ImportFrom):
            module = node.module
        elif isinstance(node, ast.Import):
            module = node.names[0].name
        if module and (
            module == "tools.model.legacy"
            or module.startswith("tools.model.legacy.")
        ):
            return True
    return False


def test_only_the_matrix_tests_import_the_legacy_layer() -> None:
    root = MODEL_ROOT.parents[1]
    violations = []
    for base in (root / "tools", root / "test"):
        for path in sorted(base.rglob("*.py")):
            relative = path.relative_to(root)
            if relative.parts[:3] == ("tools", "model", "legacy"):
                continue
            if relative.parts[:2] == ("test", "matrix"):
                continue
            tree = ast.parse(path.read_text(encoding="utf-8"))
            if _imports_legacy(tree):
                violations.append(str(relative))
    assert not violations, (
        "the legacy algebra is test/matrix-only; unexpected importers: "
        + ", ".join(violations)
    )


def test_legacy_layer_does_not_reach_into_emit_or_report() -> None:
    legacy_root = MODEL_ROOT / "legacy"
    offenders = []
    for path in sorted(legacy_root.rglob("*.py")):
        tree = ast.parse(path.read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            if isinstance(node, ast.ImportFrom) and node.module:
                if node.module.startswith(
                    ("tools.model.emit", "tools.model.report")
                ):
                    offenders.append(str(path.relative_to(MODEL_ROOT)))
    assert not offenders, (
        "the legacy algebra must not depend on emit/report: "
        + ", ".join(offenders)
    )
