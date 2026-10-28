#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

from tools.deadcode import render, scan

USED = """\
#pragma once
struct used_type {};
struct used_alias {};
using used_alias_t = used_alias;
"""

USER = """\
#pragma once
struct user_type {
  used_type field;
  used_alias_t alias;
};
"""

DEAD = """\
#pragma once
struct dead_type {};
struct dead_alias {};
using dead_alias_t = dead_alias;
"""

TEST_ONLY = """\
#pragma once
struct only_tested {};
"""


def build_tree(root: Path) -> Path:
    (root / "include" / "dingo").mkdir(parents=True)
    (root / "examples").mkdir()
    (root / "test").mkdir()
    (root / "include" / "dingo" / "used.h").write_text(USED)
    (root / "include" / "dingo" / "user.h").write_text(USER)
    (root / "include" / "dingo" / "dead.h").write_text(DEAD)
    (root / "include" / "dingo" / "test_only.h").write_text(TEST_ONLY)
    (root / "examples" / "sample.cpp").write_text('#include "used.h"\n')
    (root / "test" / "checks.cpp").write_text(
        '#include "test_only.h"\nonly_tested value;\n'
    )
    return root / "include" / "dingo"


def test_scan_separates_dead_from_used_and_test_only(tmp_path: Path) -> None:
    root = build_tree(tmp_path)
    dead, test_only = scan(root)
    dead_names = {finding.name for finding in dead}
    test_names = {finding.name for finding in test_only}

    assert "dead_type" in dead_names
    assert "dead_alias_t" in dead_names
    # The struct is referenced by its own alias, so only the alias is dead.
    assert "dead_alias" not in dead_names
    assert "used_type" not in dead_names
    assert "used_alias" not in dead_names
    assert "only_tested" in test_names
    assert "used_type" not in test_names


def test_scan_accepts_the_alias_suffix(tmp_path: Path) -> None:
    root = build_tree(tmp_path)
    dead, _ = scan(root)
    dead_names = {finding.name for finding in dead}
    assert "dead_alias_t" in dead_names


def test_render_lists_both_sections(tmp_path: Path) -> None:
    root = build_tree(tmp_path)
    dead, test_only = scan(root)
    text = render(dead, test_only)
    assert "Unreferenced declarations" in text
    assert "Referenced only by tests" in text
    assert "dead_type" in text
    assert "only_tested" in text
