#!/usr/bin/env python3
"""Compares what the matrix tests observed against two library versions.

The differential harness builds the generated matrix test executables twice,
once against the current library headers and once against a pinned older copy,
and runs each with recording enabled. Every executable writes one JSON Lines
file named <executable>.jsonl into a records directory per side. Two kinds of
records are written:

- check: one per assertion, identified by (case, site, occurrence), with the
  operation, the expression text, the result, and the printed operands
- case: one per test case, identified by its name, with the status and message

This script loads both directories and reports every difference: an executable
present on one side only, a case present on one side only, a case whose status
or message differs, a check present on one side only, and a check whose op,
expr, result, left, or right differs. The checks of a case that exists on one
side only are not listed separately; the case difference covers them.

Each difference belongs to the case path "<executable>/<Suite>.<Name>", or
"<executable>/*" when a whole executable is missing on one side. A difference
is declared when its case path matches a glob pattern of a change listed in the
behavior changes file (fnmatch syntax). The comparison fails when there is an
undeclared difference, when a change matches no difference (a stale entry), or
when the behavior changes file is malformed.

Exit status: 0 when the sides agree up to declared changes, 1 when the
comparison fails, 2 on usage or input errors.
"""

import argparse
import fnmatch
import json
import sys
from pathlib import Path

CHANGE_KEYS = {"id", "description", "decision", "cases"}
CHECK_FIELDS = ("op", "expr", "result", "left", "right")
CASE_FIELDS = ("status", "message")
CHECK_REQUIRED = ("case", "site", "occurrence", *CHECK_FIELDS)
CASE_REQUIRED = ("case", *CASE_FIELDS)
MAX_LINES = 200


class InputError(Exception):
    """The records could not be read or are malformed."""


class ChangesError(Exception):
    """The behavior changes file is malformed."""


def parse_record(path, number, line):
    where = f"{path}:{number}"
    try:
        record = json.loads(line)
    except json.JSONDecodeError as error:
        raise InputError(f"{where}: malformed JSON: {error}") from error
    if not isinstance(record, dict):
        raise InputError(f"{where}: record is not a JSON object")
    kind = record.get("kind")
    if kind == "check":
        required = CHECK_REQUIRED
    elif kind == "case":
        required = CASE_REQUIRED
    else:
        raise InputError(f"{where}: unknown record kind {kind!r}")
    missing = [key for key in required if key not in record]
    if missing:
        raise InputError(f"{where}: {kind} record lacks {', '.join(missing)}")
    if not isinstance(record["case"], str):
        raise InputError(f"{where}: case is not a string")
    if kind == "check":
        if not isinstance(record["site"], str):
            raise InputError(f"{where}: site is not a string")
        occurrence = record["occurrence"]
        if isinstance(occurrence, bool) or not isinstance(occurrence, int):
            raise InputError(f"{where}: occurrence is not an integer")
    return record


def load_executable(path):
    """Returns ({case: record}, {(case, site, occurrence): record})."""
    cases = {}
    checks = {}
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise InputError(f"{path}: cannot read: {error}") from error
    for number, line in enumerate(text.splitlines(), start=1):
        if not line.strip():
            continue
        record = parse_record(path, number, line)
        if record["kind"] == "case":
            if record["case"] in cases:
                raise InputError(f"{path}:{number}: duplicate case {record['case']}")
            cases[record["case"]] = record
        else:
            key = (record["case"], record["site"], record["occurrence"])
            if key in checks:
                raise InputError(
                    f"{path}:{number}: duplicate check {key[0]} {key[1]}#{key[2]}"
                )
            checks[key] = record
    return cases, checks


def load_records(directory):
    """Returns {executable: (cases, checks)} for every *.jsonl in directory."""
    if not directory.is_dir():
        raise InputError(f"{directory}: not a directory")
    files = sorted(directory.glob("*.jsonl"))
    if not files:
        raise InputError(f"{directory}: contains no *.jsonl records")
    return {path.stem: load_executable(path) for path in files}


def load_changes(path):
    """Returns the validated list of changes, sorted by id."""
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise InputError(f"{path}: cannot read: {error}") from error
    try:
        data = json.loads(text)
    except json.JSONDecodeError as error:
        raise ChangesError(f"{path}: malformed JSON: {error}") from error
    if not isinstance(data, dict) or set(data) != {"changes"}:
        raise ChangesError(f"{path}: expected an object with the single key 'changes'")
    if not isinstance(data["changes"], list):
        raise ChangesError(f"{path}: 'changes' is not a list")
    seen = set()
    for index, change in enumerate(data["changes"]):
        where = f"{path}: changes[{index}]"
        if not isinstance(change, dict):
            raise ChangesError(f"{where} is not an object")
        unknown = sorted(set(change) - CHANGE_KEYS)
        missing = sorted(CHANGE_KEYS - set(change))
        if unknown:
            raise ChangesError(f"{where} has unknown keys: {', '.join(unknown)}")
        if missing:
            raise ChangesError(f"{where} lacks keys: {', '.join(missing)}")
        for key in ("id", "description", "decision"):
            if not isinstance(change[key], str) or not change[key]:
                raise ChangesError(f"{where}: {key} is not a non-empty string")
        patterns = change["cases"]
        if (
            not isinstance(patterns, list)
            or not patterns
            or not all(isinstance(pattern, str) and pattern for pattern in patterns)
        ):
            raise ChangesError(f"{where}: cases is not a non-empty list of strings")
        if change["id"] in seen:
            raise ChangesError(f"{where}: duplicate id {change['id']}")
        seen.add(change["id"])
    return sorted(data["changes"], key=lambda change: change["id"])


def load_spec_changes(implementation="next"):
    """Returns the behavior changes of the spec that the matrix observes.

    A change takes part when one of its case patterns starts with `matrix:`;
    the prefix is removed, and the patterns of other checks are ignored. The
    changes describe the target semantics, which the next implementation
    implements, against the baseline; the current implementation is the
    baseline code and takes none.
    """
    if implementation == "current":
        return []
    project_root = Path(__file__).resolve().parents[2]
    # Importing the spec writes no bytecode into the source tree.
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(project_root))
    import spec

    changes = []
    for change in spec.load_spec().behavior_changes:
        patterns = [
            pattern[len("matrix:") :]
            for pattern in change.cases
            if pattern.startswith("matrix:")
        ]
        if patterns:
            changes.append(
                {
                    "id": change.id,
                    "description": change.description,
                    "decision": change.decision,
                    "cases": patterns,
                }
            )
    return sorted(changes, key=lambda change: change["id"])


def encode(value):
    return json.dumps(value, sort_keys=True)


def differing_fields(baseline, current, fields):
    return [
        field
        for field in fields
        if encode(baseline.get(field)) != encode(current.get(field))
    ]


def one_sided(path, kind, subject, side, record, fields):
    values = {field: record.get(field) for field in fields}
    return {
        "path": path,
        "kind": kind,
        "subject": subject,
        "side": side,
        "baseline": values if side == "baseline" else None,
        "current": values if side == "current" else None,
    }


def changed(path, kind, subject, baseline, current, fields):
    return {
        "path": path,
        "kind": kind,
        "subject": subject,
        "side": None,
        "baseline": {field: baseline.get(field) for field in fields},
        "current": {field: current.get(field) for field in fields},
    }


def compare_executable(name, baseline, current):
    """Returns (differences, number of cases compared, number of checks compared)."""
    differences = []
    base_cases, base_checks = baseline
    cur_cases, cur_checks = current

    one_sided_cases = set()
    compared_cases = 0
    for case in sorted(base_cases.keys() | cur_cases.keys()):
        path = f"{name}/{case}"
        if case not in cur_cases:
            one_sided_cases.add(case)
            differences.append(
                one_sided(path, "case", case, "baseline", base_cases[case], CASE_FIELDS)
            )
        elif case not in base_cases:
            one_sided_cases.add(case)
            differences.append(
                one_sided(path, "case", case, "current", cur_cases[case], CASE_FIELDS)
            )
        else:
            compared_cases += 1
            fields = differing_fields(base_cases[case], cur_cases[case], CASE_FIELDS)
            if fields:
                differences.append(
                    changed(path, "case", case, base_cases[case], cur_cases[case], fields)
                )

    compared_checks = 0
    for key in sorted(base_checks.keys() | cur_checks.keys()):
        case, site, occurrence = key
        if case in one_sided_cases:
            continue
        path = f"{name}/{case}"
        subject = f"{site}#{occurrence}"
        if key not in cur_checks:
            differences.append(
                one_sided(path, "check", subject, "baseline", base_checks[key], CHECK_FIELDS)
            )
        elif key not in base_checks:
            differences.append(
                one_sided(path, "check", subject, "current", cur_checks[key], CHECK_FIELDS)
            )
        else:
            compared_checks += 1
            fields = differing_fields(base_checks[key], cur_checks[key], CHECK_FIELDS)
            if fields:
                differences.append(
                    changed(path, "check", subject, base_checks[key], cur_checks[key], fields)
                )
    return differences, compared_cases, compared_checks


def compare_records(baseline, current):
    """Returns (differences, counts) for two {executable: (cases, checks)} maps."""
    differences = []
    counts = {"executables": 0, "cases": 0, "checks": 0}
    for name in sorted(baseline.keys() | current.keys()):
        if name not in current or name not in baseline:
            side = "baseline" if name in baseline else "current"
            differences.append(
                {
                    "path": f"{name}/*",
                    "kind": "executable",
                    "subject": name,
                    "side": side,
                    "baseline": None,
                    "current": None,
                }
            )
            continue
        found, cases, checks = compare_executable(name, baseline[name], current[name])
        differences += found
        counts["executables"] += 1
        counts["cases"] += cases
        counts["checks"] += checks
    differences.sort(
        key=lambda d: (d["path"], d["kind"], d["subject"], d["side"] or "")
    )
    return differences, counts


def classify(differences, changes):
    """Returns (undeclared, {change id: differences}, stale change ids)."""
    undeclared = []
    declared = {change["id"]: [] for change in changes}
    for difference in differences:
        matching = [
            change["id"]
            for change in changes
            if any(
                fnmatch.fnmatchcase(difference["path"], pattern)
                for pattern in change["cases"]
            )
        ]
        for change_id in matching:
            declared[change_id].append(difference)
        if not matching:
            undeclared.append(difference)
    stale = sorted(change_id for change_id, found in declared.items() if not found)
    return undeclared, {k: v for k, v in sorted(declared.items()) if v}, stale


def describe(difference):
    kind = difference["kind"]
    if kind == "executable":
        return f"{difference['path']}: executable only in {difference['side']}"
    label = difference["path"]
    if kind == "check":
        label = f"{label} {difference['subject']}"
    baseline, current = difference["baseline"], difference["current"]
    if difference["side"] is not None:
        values = baseline if baseline is not None else current
        shown = ", ".join(f"{key}={encode(value)}" for key, value in values.items())
        return f"{label}: {kind} only in {difference['side']} ({shown})"
    shown = ", ".join(
        f"{key} {encode(baseline[key])} -> {encode(current[key])}" for key in baseline
    )
    return f"{label}: {kind} {shown}"


def capped(lines, indent):
    for line in lines[:MAX_LINES]:
        yield f"{indent}{line}"
    if len(lines) > MAX_LINES:
        yield f"{indent}... {len(lines) - MAX_LINES} more"


def summarize(counts, undeclared, declared, stale):
    lines = [
        f"compared {counts['executables']} executables, "
        f"{counts['cases']} cases, {counts['checks']} checks"
    ]
    if undeclared:
        lines.append(f"undeclared differences ({len(undeclared)}):")
        lines += capped([describe(d) for d in undeclared], "  ")
    if declared:
        total = sum(len(found) for found in declared.values())
        lines.append(f"declared differences ({total}):")
        for change_id, found in declared.items():
            lines.append(f"  {change_id} ({len(found)}):")
            lines += capped([describe(d) for d in found], "    ")
    if stale:
        lines.append(f"stale changes ({len(stale)}):")
        lines += [f"  {change_id}: matches no difference" for change_id in stale]
    failed = bool(undeclared or stale)
    lines.append("FAIL" if failed else "OK")
    return lines


def write_report(path, counts, differences, undeclared, declared, stale):
    report = {
        "compared": counts,
        "differences": differences,
        "undeclared": undeclared,
        "declared": declared,
        "stale": stale,
    }
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--baseline", type=Path, required=True, help="baseline records directory")
    parser.add_argument("--current", type=Path, required=True, help="current records directory")
    changes_source = parser.add_mutually_exclusive_group(required=True)
    changes_source.add_argument(
        "--behavior-changes",
        type=Path,
        help="JSON file listing the declared behavior changes",
    )
    changes_source.add_argument(
        "--spec",
        action="store_true",
        help="use the behavior changes of the spec with matrix: case patterns",
    )
    parser.add_argument("--report", type=Path, help="write all differences as JSON")
    parser.add_argument(
        "--implementation",
        choices=("current", "next"),
        default="current",
        help="the implementation of the current records; the spec changes "
        "apply to the next implementation",
    )
    args = parser.parse_args(argv)

    try:
        if args.spec:
            changes = load_spec_changes(args.implementation)
        else:
            changes = load_changes(args.behavior_changes)
    except ChangesError as error:
        print(f"malformed behavior changes: {error}", file=sys.stderr)
        return 1
    except InputError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    try:
        baseline = load_records(args.baseline)
        current = load_records(args.current)
        differences, counts = compare_records(baseline, current)
        undeclared, declared, stale = classify(differences, changes)
        if args.report is not None:
            write_report(args.report, counts, differences, undeclared, declared, stale)
    except (InputError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    print("\n".join(summarize(counts, undeclared, declared, stale)))
    return 1 if undeclared or stale else 0


if __name__ == "__main__":
    sys.exit(main())
