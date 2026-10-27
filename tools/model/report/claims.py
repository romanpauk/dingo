#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Compare the consumer claims with the resolve rows they consume.

The resolve manifest says which rows the model expects to resolve. ``resolve``,
``construct``, and ``invoke`` each write one outcome line per row they emitted.
This report joins them per row and separates:

- ``ok``: resolve resolves the row and the claim consumes it;
- ``shared gap``: resolve already skips the row, so the claim skipping it says
  nothing new;
- ``declared``: the claim cannot consume the row for a declared reason;
- ``recovered``: a declared reason no longer applies, so the declaration is
  stale;
- ``diverged``: resolve resolves the row but the claim cannot consume it;
- ``missing``: the row was emitted without an outcome line;
- ``pinned``: the claim file does not contain the row because the composition
  does not compile; the negative-compile check keeps that pin honest.

Only ``diverged``, ``missing``, and a failing negative-compile check are
errors.
"""

from __future__ import annotations

import argparse
import json
import subprocess
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.model.emit.consumers import (
    COMPILE_GAPS,
    CONSUMERS,
    NON_COMPOSABLE,
    RUNTIME_PINS,
    ConsumerFile,
    render,
)

DECLARED_DIVERGENCE: dict[str, dict[str, str]] = {
    "construct": {"keyed": "construct cannot supply a runtime lookup key"},
    "invoke": {"keyed": "invoke cannot supply a runtime lookup key"},
}


@dataclass
class ClaimReport:
    ok: Counter = field(default_factory=Counter)
    declared: Counter = field(default_factory=Counter)
    shared: Counter = field(default_factory=Counter)
    diverged: list[dict[str, object]] = field(default_factory=list)
    recovered: list[dict[str, object]] = field(default_factory=list)
    missing: list[dict[str, object]] = field(default_factory=list)
    pinned: dict[str, dict[str, str]] = field(default_factory=dict)

    @property
    def failed(self) -> bool:
        return bool(self.diverged or self.missing)


def read_rows(manifest: Path) -> dict[str, dict[str, object]]:
    payload = json.loads(manifest.read_text(encoding="utf-8"))
    rows: dict[str, dict[str, object]] = {}
    for shard in payload["shards"]:
        for case in shard["cases"]:
            rows[str(case["path"])] = {
                "path": case["path"],
                "name": case["name"],
                "binding": case["binding"],
                "scope": case["scope"],
                "storage": case["storage"],
                "traits": case["traits"],
                "request": case["request"],
                "declared": bool(case["declared"]),
            }
    return rows


def read_outcomes(paths: list[Path]) -> dict[tuple[str, str], dict[str, object]]:
    outcomes: dict[tuple[str, str], dict[str, object]] = {}
    for path in paths:
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            row = json.loads(line)
            if "claim" in row:
                outcomes[(str(row["claim"]), str(row["path"]))] = row
    return outcomes


def read_resolve_outcomes(paths: list[Path]) -> dict[str, str]:
    outcomes: dict[str, str] = {}
    for path in paths:
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            row = json.loads(line)
            key = row.get("path", row.get("name"))
            if key is None:
                continue
            outcomes[str(key)] = str(row.get("result", ""))
    return outcomes


def compare(
    rows: dict[str, dict[str, object]],
    claims: dict[tuple[str, str], dict[str, object]],
    resolves: dict[str, str],
) -> ClaimReport:
    report = ClaimReport()
    for consumer in CONSUMERS:
        pinned = NON_COMPOSABLE[consumer]
        declared = DECLARED_DIVERGENCE.get(consumer, {})
        report.pinned[consumer] = dict(pinned)
        for path, row in rows.items():
            if path in pinned:
                continue
            outcome = claims.get((consumer, path))
            if outcome is None:
                report.missing.append({"claim": consumer, **row})
                continue
            result = str(outcome.get("result", ""))
            if result == "ok":
                if str(row["binding"]) in declared:
                    report.recovered.append({"claim": consumer, **row})
                else:
                    report.ok[consumer] += 1
                continue
            if str(row["binding"]) in declared:
                report.declared[consumer] += 1
            elif resolves.get(path, "").startswith("skip"):
                report.shared[consumer] += 1
            else:
                report.diverged.append(
                    {"claim": consumer, "reason": result, **row}
                )
    return report


def _row_label(row: dict[str, object]) -> str:
    return (
        f"`{row['scope']}/{row['storage']}` "
        f"`{row['traits']}` `{row['binding']}` `{row['request']}`"
    )


def render_markdown(report: ClaimReport, rows: dict[str, dict[str, object]]) -> str:
    lines = ["# Consumer claims", ""]
    lines += [
        "Generated by `python -m tools.model.report.claims`.",
        "",
        "| claim | rows | ok | declared | shared gap | pinned | diverged | missing |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for consumer in CONSUMERS:
        total = (
            report.ok[consumer]
            + report.declared[consumer]
            + report.shared[consumer]
            + len(report.pinned[consumer])
        )
        lines.append(
            f"| `{consumer}` | {total} | {report.ok[consumer]} | "
            f"{report.declared[consumer]} | {report.shared[consumer]} | "
            f"{len(report.pinned[consumer])} | {len([r for r in report.diverged if r['claim'] == consumer])} | "
            f"{len([r for r in report.missing if r['claim'] == consumer])} |"
        )
    lines += ["", "## Declared", ""]
    for consumer in CONSUMERS:
        for binding, reason in DECLARED_DIVERGENCE.get(consumer, {}).items():
            count = sum(
                1
                for path, row in rows.items()
                if str(row["binding"]) == binding
                and path not in NON_COMPOSABLE[consumer]
            )
            lines.append(f"- `{consumer}/{binding}` ({count} rows): {reason}")
    lines.append("")

    for title, source in (
        ("Pinned compile gaps", COMPILE_GAPS),
        ("Pinned runtime failures", RUNTIME_PINS),
    ):
        lines += [f"## {title}", ""]
        by_reason: dict[str, int] = defaultdict(int)
        for consumer in CONSUMERS:
            for reason in source.get(consumer, {}).values():
                by_reason[f"{consumer}: {reason}"] += 1
        if not by_reason:
            lines.append("None.")
        for reason, count in sorted(by_reason.items()):
            lines.append(f"- {reason}: {count} rows")
        lines.append("")

    if report.diverged:
        lines += ["## Diverged", ""]
        for row in report.diverged:
            lines.append(
                f"- `{row['claim']}` {_row_label(row)}: {row['reason']}"
            )
        lines.append("")

    if report.recovered:
        lines += ["## Recovered (stale declarations)", ""]
        for row in report.recovered:
            lines.append(f"- `{row['claim']}` {_row_label(row)}")
        lines.append("")

    if report.missing:
        lines += ["## Missing outcomes", ""]
        for row in report.missing:
            lines.append(f"- `{row['claim']}` {_row_label(row)}")
        lines.append("")

    return "\n".join(lines)


def _pin_case_path(path: str, consumer: str) -> str:
    return path.replace("/resolve/", f"/{consumer}/", 1)


def _pin_case(
    manifest: Path, consumer: str
) -> list[tuple[str, str, dict[str, object]]]:
    payload = json.loads(manifest.read_text(encoding="utf-8"))
    cases = {
        str(case["path"]): case
        for shard in payload["shards"]
        for case in shard["cases"]
    }
    result: list[tuple[str, str, dict[str, object]]] = []
    for path in COMPILE_GAPS.get(consumer, {}):
        case = cases.get(path)
        if case is None:
            continue
        result.append(
            (
                consumer,
                path,
                {**case, "claim_path": _pin_case_path(path, consumer)},
            )
        )
    return result


def check_pins(
    manifest: Path,
    compiler: str,
    includes: list[Path],
    jobs: int,
) -> list[str]:
    """Assert every pinned row still fails to compose."""
    jobs_list = [
        item
        for consumer in CONSUMERS
        for item in _pin_case(manifest, consumer)
    ]
    results: list[str] = []

    def compile_one(item: tuple[str, str, dict[str, object]], work: Path) -> str | None:
        consumer, path, claim_case = item
        file = ConsumerFile(
            consumer=consumer,
            scope=str(claim_case["scope"]),
            storage=str(claim_case["storage"]),
            path="check.cpp",
            suite="check",
            cases=(claim_case,),
        )
        source = work / f"{abs(hash(path))}.cpp"
        source.write_text(render(file), encoding="utf-8")
        command = [compiler, "-std=c++17", "-fsyntax-only"]
        for include in includes:
            command += ["-I", str(include)]
        command.append(str(source))
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode == 0:
            return f"{consumer} {path}: pinned row compiles"
        return None

    from concurrent.futures import ThreadPoolExecutor

    with TemporaryDirectory() as directory:
        work = Path(directory)
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            for outcome in pool.map(
                lambda item: compile_one(item, work), jobs_list
            ):
                if outcome:
                    results.append(outcome)
    return results


def check_runtime_pins(
    manifest: Path,
    compiler: str,
    includes: list[Path],
    libraries: list[Path],
    jobs: int,
) -> list[str]:
    """Assert every runtime-pinned row still fails instead of passing."""
    cases = {
        str(case["path"]): case
        for shard in json.loads(manifest.read_text(encoding="utf-8"))["shards"]
        for case in shard["cases"]
    }
    jobs_list = [
        (consumer, path, {**cases[path], "claim_path": _pin_case_path(path, consumer)})
        for consumer in CONSUMERS
        for path in RUNTIME_PINS.get(consumer, {})
        if path in cases
    ]

    def run_one(item: tuple[str, str, dict[str, object]], work: Path) -> str | None:
        consumer, path, claim_case = item
        file = ConsumerFile(
            consumer=consumer,
            scope=str(claim_case["scope"]),
            storage=str(claim_case["storage"]),
            path="check.cpp",
            suite="check",
            cases=(claim_case,),
        )
        source = work / f"{abs(hash(path))}.cpp"
        binary = work / f"{abs(hash(path))}.bin"
        source.write_text(render(file), encoding="utf-8")
        command = [compiler, "-std=c++17", "-o", str(binary)]
        for include in includes:
            command += ["-I", str(include)]
        command.append(str(source))
        command += [str(library) for library in libraries]
        command += ["-lpthread"]
        built = subprocess.run(command, capture_output=True, text=True)
        if built.returncode != 0:
            return f"{consumer} {path}: runtime pin no longer builds"
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        if run.returncode == 0:
            return f"{consumer} {path}: runtime pin now passes"
        return None

    from concurrent.futures import ThreadPoolExecutor

    results: list[str] = []
    with TemporaryDirectory() as directory:
        work = Path(directory)
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            for outcome in pool.map(
                lambda item: run_one(item, work), jobs_list
            ):
                if outcome:
                    results.append(outcome)
    return results


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--outcomes-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    parser.add_argument(
        "--check-pins",
        action="store_true",
        help="assert every pinned row still fails to compose",
    )
    parser.add_argument("--compiler", default="c++")
    parser.add_argument(
        "--check-runtime-pins",
        action="store_true",
        help="assert every runtime-pinned row still fails instead of passing",
    )
    parser.add_argument("--library", type=Path, action="append")
    parser.add_argument("--include", type=Path, action="append")
    parser.add_argument("--jobs", type=int, default=8)
    arguments = parser.parse_args()

    rows = read_rows(arguments.manifest)
    claims = read_outcomes(
        sorted(arguments.outcomes_dir.glob("claim-*.jsonl"))
    )
    resolves = read_resolve_outcomes(
        sorted(arguments.outcomes_dir.glob("resolve-*.jsonl"))
    )
    if not claims:
        raise SystemExit("no claim outcome files")
    report = compare(rows, claims, resolves)

    arguments.out.parent.mkdir(parents=True, exist_ok=True)
    arguments.out.write_text(render_markdown(report, rows), encoding="utf-8")
    if arguments.json is not None:
        arguments.json.write_text(
            json.dumps(
                {
                    "ok": report.ok,
                    "declared": report.declared,
                    "shared": report.shared,
                    "diverged": report.diverged,
                    "recovered": report.recovered,
                    "missing": report.missing,
                    "pinned": report.pinned,
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )

    failed = report.failed
    includes = list(arguments.include or [])
    if arguments.check_pins:
        stale = check_pins(
            arguments.manifest, arguments.compiler, includes, arguments.jobs
        )
        for line in stale:
            print(f"stale pin: {line}")
        failed = failed or bool(stale)
    if arguments.check_runtime_pins:
        stale = check_runtime_pins(
            arguments.manifest,
            arguments.compiler,
            includes,
            list(arguments.library or []),
            arguments.jobs,
        )
        for line in stale:
            print(f"stale pin: {line}")
        failed = failed or bool(stale)
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
