#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

import pytest

from tools.model.emit.binding_matrix import build_rows, render, validate
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar.bindings import binding_for
from tools.model.grammar.traits import TRAITS_BY_NAME


def test_binding_matrix_is_non_empty_and_unique() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_binding_matrix_rows_are_supported() -> None:
    from tools.model.semantics.support import classify

    for row in build_rows():
        scope, name, request, traits = row["coverage"]
        assert (
            classify(
                scope,
                name,
                binding_for(request),
                request,
                TRAITS_BY_NAME[traits],
            ).outcome
            == "supported"
        )


def test_binding_matrix_render_is_deterministic(tmp_path: Path) -> None:
    content = render(tmp_path / "binding_matrix.cpp")
    assert content == render(tmp_path / "binding_matrix.cpp")


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
