#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from tools.complexity import (
    Metrics,
    cluster_headers,
    collect,
    nearest_neighbours,
    render,
    render_clusters,
)

TOOL = Path(__file__).resolve().parent / "complexity.py"

SAMPLE = """\
#pragma once

template <typename T> struct rule {};

template <> struct rule<int> {};
template <> struct rule<long> {};
template <> struct rule<char> {};

template <typename T> struct forwarder {
  static int apply(int value) { return other(value); }
};

inline int decide(int value) {
  if constexpr (sizeof(int) == 4) {
    return value;
  } else {
    return value + 1;
  }
}
"""


def make_tree(root: Path) -> Path:
    headers = root / "include" / "sample"
    headers.mkdir(parents=True)
    (headers / "rule.h").write_text(SAMPLE)
    (headers / "forwarder.h").write_text(
        '#pragma once\n#include <sample/rule.h>\nstruct user {};\n'
    )
    examples = root / "examples"
    examples.mkdir()
    (examples / "one.cpp").write_text("#include <sample/rule.h>\n")
    (examples / "two.cpp").write_text("#include <sample/forwarder.h>\n")
    return headers


def test_metrics_count_shape_signals() -> None:
    rows = collect(Path("include/dingo"))
    assert rows
    by_path = {row.path: row for row in rows}
    conversions = by_path["include/dingo/type/type_conversion_traits.h"]
    assert conversions.specializations > 10
    assert conversions.branches > 0
    assert conversions.fan_in > 0
    assert conversions.lines > 1000


def test_forwarders_and_fan_in_from_a_sample_tree(tmp_path: Path) -> None:
    root = make_tree(tmp_path)
    result = subprocess.run(
        [sys.executable, str(TOOL), "--root", str(root), "--json", str(tmp_path / "m.json")],
        capture_output=True,
        text=True,
        check=True,
    )
    payload = json.loads((tmp_path / "m.json").read_text())
    by_path = {row["path"]: row for row in payload}
    rule = by_path["include/sample/rule.h"]
    assert rule["specializations"] == 3
    assert rule["forwarders"] == 1
    assert rule["fan_in"] == 2
    assert "rule.h" in result.stdout


def test_render_orders_by_score() -> None:
    rows = [
        Metrics("a.h", 10, 1, 1, 0, 0, 0, 0.0),
        Metrics("b.h", 10, 5, 1, 3, 0, 1, 15.0),
    ]
    text = render(sorted(rows, key=lambda row: row.score, reverse=True), 2)
    assert text.index("b.h") < text.index("a.h")
    assert text.startswith("| score |")


SHARED_A = """\
#pragma once
struct alpha_engine {
  void start();
  void stop();
  int step();
};
struct alpha_session {
  void begin();
  void end();
};
struct alpha_config {
  int depth;
  int width;
};
"""

SHARED_B = """\
#pragma once
struct beta_engine {
  void start();
  void stop();
  int step();
};
struct beta_session {
  void begin();
  void end();
};
struct beta_config {
  int depth;
  int width;
};
"""

UNRELATED = """\
#pragma once
struct gamma {
  void paint();
  void erase();
  void undo();
};
struct delta {
  void load();
  void store();
  void flush();
};
struct epsilon {
  int red;
  int green;
  int blue;
};
"""


def test_clusters_group_shared_vocabulary(tmp_path: Path) -> None:
    root = tmp_path / "include"
    root.mkdir()
    (root / "alpha.h").write_text(SHARED_A)
    (root / "beta.h").write_text(SHARED_B)
    (root / "gamma.h").write_text(UNRELATED)

    clusters = cluster_headers(root)
    members = {Path(member).name for cluster in clusters for member in cluster["members"]}
    assert any(len(cluster["members"]) >= 2 for cluster in clusters)
    assert members == {"alpha.h", "beta.h"}
    text = render_clusters(clusters)
    assert "alpha.h" in text and "beta.h" in text
    assert "gamma.h" not in text


def test_nearest_neighbours_finds_the_variant(tmp_path: Path) -> None:
    root = tmp_path / "include"
    root.mkdir()
    (root / "alpha.h").write_text(SHARED_A)
    (root / "beta.h").write_text(SHARED_B)
    (root / "gamma.h").write_text(UNRELATED)
    lines = nearest_neighbours(root, [(root / "alpha.h").as_posix()], limit=1)
    assert len(lines) == 1
    assert "beta.h" in lines[0]
