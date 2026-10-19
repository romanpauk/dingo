"""Tests of compare.py, the comparison step of the differential harness."""

import json
import os
import shutil
import tempfile
from pathlib import Path

import pytest

import compare

REPO_ROOT = Path(__file__).resolve().parents[2]
EXE = "dingo_matrix_test_lifetime_counts"
CASE = "Lifetime.Counts"


@pytest.fixture
def work(request):
    base = Path(
        os.environ.get(
            "DINGO_DIFFERENTIAL_TEST_DIR", REPO_ROOT / "build" / "differential-test"
        )
    )
    base.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="compare-", dir=base))
    yield directory
    shutil.rmtree(directory, ignore_errors=True)


def check(case=CASE, site="a.cpp:10", occurrence=0, **fields):
    record = {
        "kind": "check",
        "case": case,
        "site": site,
        "occurrence": occurrence,
        "op": "eq",
        "expr": "a == b",
        "result": True,
        "left": "1",
        "right": "1",
    }
    record.update(fields)
    return record


def case_record(case=CASE, status="passed", message=""):
    return {"kind": "case", "case": case, "status": status, "message": message}


def write_records(directory, executables):
    directory.mkdir(parents=True, exist_ok=True)
    for name, records in executables.items():
        lines = "".join(json.dumps(record) + "\n" for record in records)
        (directory / f"{name}.jsonl").write_text(lines)


def write_changes(path, changes):
    path.write_text(json.dumps({"changes": changes}))


def change(change_id="relaxed", cases=(f"{EXE}/{CASE}",), **overrides):
    entry = {
        "id": change_id,
        "description": "d",
        "decision": "docs/x.md",
        "cases": list(cases),
    }
    entry.update(overrides)
    return entry


def run(work, baseline, current, changes=(), capsys=None, extra=()):
    write_records(work / "baseline", baseline)
    write_records(work / "current", current)
    write_changes(work / "changes.json", list(changes))
    code = compare.main(
        [
            "--baseline",
            str(work / "baseline"),
            "--current",
            str(work / "current"),
            "--behavior-changes",
            str(work / "changes.json"),
            *extra,
        ]
    )
    captured = capsys.readouterr()
    return code, captured.out, captured.err


def records(*items):
    return {EXE: list(items)}


def test_identical_records_pass(work, capsys):
    side = records(check(), check(occurrence=1), case_record())
    code, out, _ = run(work, side, side, capsys=capsys)
    assert code == 0
    assert "compared 1 executables, 1 cases, 2 checks" in out
    assert out.splitlines()[-1] == "OK"


def test_changed_check_value_is_undeclared(work, capsys):
    baseline = records(check(result=True), case_record())
    current = records(check(result=False), case_record())
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "undeclared differences (1)" in out
    assert f"{EXE}/{CASE} a.cpp:10#0: check result true -> false" in out


def test_declared_difference_passes(work, capsys):
    baseline = records(check(left="1"), case_record())
    current = records(check(left="2"), case_record())
    code, out, _ = run(work, baseline, current, [change()], capsys)
    assert code == 0
    assert "undeclared" not in out
    assert "declared differences (1)" in out
    assert "relaxed (1)" in out


def test_glob_pattern_declares_differences(work, capsys):
    baseline = records(check(), case_record())
    current = records(check(result=False), case_record())
    code, _, _ = run(work, baseline, current, [change(cases=["dingo_matrix_*/Lifetime.*"])], capsys)
    assert code == 0


def test_stale_change_fails(work, capsys):
    side = records(check(), case_record())
    code, out, _ = run(work, side, side, [change("never_used")], capsys)
    assert code == 1
    assert "stale changes (1)" in out
    assert "never_used" in out


def test_case_missing_on_one_side(work, capsys):
    baseline = records(case_record(), case_record("Lifetime.Other"), check("Lifetime.Other"))
    current = records(case_record())
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "undeclared differences (1)" in out
    assert f"{EXE}/Lifetime.Other: case only in baseline" in out


def test_case_missing_can_be_declared(work, capsys):
    baseline = records(case_record(), case_record("Lifetime.Other"))
    current = records(case_record())
    code, _, _ = run(work, baseline, current, [change(cases=[f"{EXE}/Lifetime.Other"])], capsys)
    assert code == 0


def test_check_missing_on_one_side(work, capsys):
    baseline = records(case_record(), check(), check(site="a.cpp:11"))
    current = records(case_record(), check())
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "a.cpp:11#0: check only in baseline" in out


def test_executable_missing_on_one_side(work, capsys):
    baseline = {EXE: [case_record()], "dingo_matrix_test_other": [case_record()]}
    current = {EXE: [case_record()]}
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "dingo_matrix_test_other/*: executable only in baseline" in out
    assert "compared 1 executables" in out


def test_executable_missing_can_be_declared(work, capsys):
    baseline = {EXE: [case_record()]}
    current = {EXE: [case_record()], "dingo_matrix_test_new": [case_record()]}
    code, _, _ = run(work, baseline, current, [change(cases=["dingo_matrix_test_new/*"])], capsys)
    assert code == 0


def test_failed_versus_passed_status(work, capsys):
    baseline = records(case_record(status="passed"))
    current = records(case_record(status="failed", message="boom"))
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert 'status "passed" -> "failed"' in out
    assert 'message "" -> "boom"' in out


def test_occurrence_distinguishes_repeated_sites(work, capsys):
    baseline = records(case_record(), check(occurrence=0), check(occurrence=1))
    current = records(case_record(), check(occurrence=0), check(occurrence=1, result=False))
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "undeclared differences (1)" in out
    assert "a.cpp:10#1: check result true -> false" in out
    assert "a.cpp:10#0" not in out


def test_occurrence_missing_on_one_side(work, capsys):
    baseline = records(case_record(), check(occurrence=0), check(occurrence=1))
    current = records(case_record(), check(occurrence=0))
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "a.cpp:10#1: check only in baseline" in out


def test_malformed_jsonl_line_names_file_and_line(work, capsys):
    side = records(case_record())
    write_records(work / "baseline", side)
    write_records(work / "current", side)
    with (work / "current" / f"{EXE}.jsonl").open("a") as handle:
        handle.write("{not json\n")
    write_changes(work / "changes.json", [])
    code = compare.main(
        [
            "--baseline", str(work / "baseline"),
            "--current", str(work / "current"),
            "--behavior-changes", str(work / "changes.json"),
        ]
    )
    err = capsys.readouterr().err
    assert code == 2
    assert f"{EXE}.jsonl:2" in err


def test_record_without_required_key_is_an_input_error(work, capsys):
    broken = check()
    del broken["site"]
    code, _, err = run(work, records(broken), records(broken), capsys=capsys)
    assert code == 2
    assert f"{EXE}.jsonl:1" in err
    assert "site" in err


def test_missing_records_directory(work, capsys):
    write_changes(work / "changes.json", [])
    code = compare.main(
        [
            "--baseline", str(work / "absent"),
            "--current", str(work / "absent"),
            "--behavior-changes", str(work / "changes.json"),
        ]
    )
    assert code == 2
    assert "not a directory" in capsys.readouterr().err


def test_duplicate_change_id_is_malformed(work, capsys):
    side = records(case_record())
    changes = [change("same"), change("same", cases=["x/*"])]
    code, _, err = run(work, side, side, changes, capsys)
    assert code == 1
    assert "duplicate id same" in err


def test_unknown_change_key_is_malformed(work, capsys):
    side = records(case_record())
    code, _, err = run(work, side, side, [change(reason="x")], capsys)
    assert code == 1
    assert "unknown keys: reason" in err


def test_missing_change_key_is_malformed(work, capsys):
    side = records(case_record())
    entry = change()
    del entry["decision"]
    code, _, err = run(work, side, side, [entry], capsys)
    assert code == 1
    assert "lacks keys: decision" in err


def test_committed_behavior_changes_file_is_valid():
    path = Path(__file__).resolve().parent / "behavior_changes.json"
    assert isinstance(compare.load_changes(path), list)


def test_report_lists_all_differences(work, capsys):
    other = "Lifetime.Other"
    baseline = records(
        check(left="1"), check(other, "b.cpp:1"), case_record(), case_record(other)
    )
    current = records(check(left="2"), case_record(), case_record(other))
    report = work / "report.json"
    code, _, _ = run(
        work, baseline, current, [change()], capsys, extra=("--report", str(report))
    )
    assert code == 1
    data = json.loads(report.read_text())
    assert len(data["differences"]) == 2
    assert len(data["undeclared"]) == 1
    assert list(data["declared"]) == ["relaxed"]
    assert data["stale"] == []


def test_output_is_capped(work, capsys):
    baseline = records(case_record(), *(check(site=f"a.cpp:{i}") for i in range(250)))
    current = records(case_record())
    code, out, _ = run(work, baseline, current, capsys=capsys)
    assert code == 1
    assert "undeclared differences (250)" in out
    assert "... 50 more" in out
