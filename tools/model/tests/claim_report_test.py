#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.report.claims import compare


def _row(slug: str, binding: str = "concrete") -> dict[str, object]:
    return {
        "path": f"resolution/runtime/shared/plain/resolve/copyable/{binding}/{slug}",
        "name": f"copyable_{binding}_{slug}",
        "binding": binding,
        "scope": "shared",
        "storage": "plain",
        "traits": "copyable",
        "request": "copyable &",
        "declared": False,
    }


def _rows(*rows: dict[str, object]) -> dict[str, dict[str, object]]:
    return {str(row["path"]): row for row in rows}


def _outcome(claim: str, row: dict[str, object], result: str) -> tuple:
    return (claim, str(row["path"])), {
        "path": row["path"],
        "claim": claim,
        "result": result,
    }


def _both(row: dict[str, object], result: str) -> list[tuple]:
    return [
        _outcome(claim, row, result) for claim in ("construct", "invoke")
    ]


def test_claim_states_are_separated() -> None:
    concrete = _row("t")
    keyed = _row("t", binding="keyed")
    gap = _row("unlisted_t")
    rows = _rows(concrete, keyed, gap)
    claims = dict(
        [
            *_both(concrete, "ok"),
            *_both(keyed, "skip:missing key"),
            *_both(gap, "skip:type not found"),
        ]
    )
    report = compare(rows, claims, {str(gap["path"]): "skip:type not found"})
    assert report.ok["construct"] == 1
    assert report.declared["construct"] == 1
    assert report.shared["construct"] == 1
    assert not report.diverged
    assert not report.failed


def test_resolve_ok_and_claim_skip_is_a_divergence() -> None:
    concrete = _row("t")
    claims = dict(_both(concrete, "skip:no route"))
    report = compare(_rows(concrete), claims, {})
    assert {row["claim"] for row in report.diverged} == {"construct", "invoke"}
    assert report.failed


def test_missing_outcome_is_reported() -> None:
    concrete = _row("t")
    report = compare(_rows(concrete), {}, {})
    assert report.missing
    assert report.failed


def test_declared_binding_that_passes_is_recovered() -> None:
    keyed = _row("t", binding="keyed")
    claims = dict(_both(keyed, "ok"))
    report = compare(_rows(keyed), claims, {})
    assert not report.declared["construct"]
    assert report.recovered
    assert not report.failed
