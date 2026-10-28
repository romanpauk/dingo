#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

from tools.lsp_refs import Declaration, declarations, uri


def test_uri_is_a_file_url(tmp_path: Path) -> None:
    assert uri(tmp_path / "a b.h").startswith("file://")
    assert "a%20b.h" in uri(tmp_path / "a b.h")


def test_declarations_report_positions(tmp_path: Path) -> None:
    header = tmp_path / "sample.h"
    header.write_text(
        "#pragma once\n// a comment mentioning sample_type\n"
        "struct sample_type {\n};\n"
    )
    rows = declarations(tmp_path)
    assert rows == [Declaration(header, "sample_type", 2, 7)]


def test_declarations_are_deduplicated(tmp_path: Path) -> None:
    header = tmp_path / "sample.h"
    header.write_text(
        "#pragma once\n"
        "template <typename T> struct sample_type {};\n"
        "template <> struct sample_type<int> {};\n"
    )
    assert len(declarations(tmp_path)) == 1
