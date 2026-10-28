#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.report.resolves import _access, compare, render


def _manifest() -> dict[str, dict[str, object]]:
    def row(name: str, request: str, declared: bool) -> dict[str, object]:
        return {
            "name": name,
            "request": request,
            "declared": declared,
            "declared_reason": "declared reason" if declared else "",
            "scope": "shared",
            "storage": "plain",
            "binding": "concrete",
            "traits": "copyable",
            "form": "value",
            "shard": "shared_plain",
        }

    return {
        "ok": row("ok", "T", False),
        "over": row("over", "T &", False),
        "declared_fail": row("declared_fail", "T *", True),
        "declared_ok": row("declared_ok", "T &&", True),
        "missing": row("missing", "T", False),
    }


def test_compare_splits_outcomes() -> None:
    outcomes = {
        "ok": {"name": "ok", "result": "ok"},
        "over": {"name": "over", "result": "exception:type not found"},
        "declared_fail": {
            "name": "declared_fail",
            "result": "exception:type not found",
        },
        "declared_ok": {"name": "declared_ok", "result": "ok"},
    }
    report = compare(_manifest(), outcomes)
    assert report["totals"] == {
        "expected": 5,
        "actual": 4,
        "covered": 1,
        "overclaim": 1,
        "declared_failure": 1,
        "declared_success": 1,
        "missing": 1,
    }
    assert report["overclaims"][0]["access"] == "lvalue"
    assert report["overclaim_by_form"] == {"value": 1}
    assert "\n## Overclaims\n" in render(report)


def test_access_classification() -> None:
    assert _access("T") == "value"
    assert _access("T &") == "lvalue"
    assert _access("T &&") == "rvalue"
    assert _access("T *") == "pointer"


if __name__ == "__main__":
    raise SystemExit(0)
