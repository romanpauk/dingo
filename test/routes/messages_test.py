"""Tests of the message comparison of test/routes/messages.py."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import messages  # noqa: E402


def row(case, resolve, message=None):
    return {"id": case, "resolve": resolve, "message": message}


def table(*rows):
    return {r["id"]: r for r in rows}


def test_reports_a_different_message_of_the_same_exception():
    baseline = table(row("a", "throws:type_not_found_exception", "type not found: x"))
    observed = table(row("a", "throws:type_not_found_exception", "missing: x"))
    assert messages.compare(baseline, observed) == [("a", "missing: x", "type not found: x")]


def test_ignores_cases_whose_outcomes_differ():
    baseline = table(
        row("a", "ok"),
        row("b", "throws:type_not_found_exception", "type not found: x"),
    )
    observed = table(
        row("a", "throws:type_not_found_exception", "type not found: x"),
        row("b", "throws:type_not_convertible_exception", "not convertible"),
    )
    assert messages.compare(baseline, observed) == []


def test_ignores_equal_messages_and_missing_ones():
    baseline = table(
        row("a", "throws:type_not_found_exception", "type not found: x"),
        row("b", "throws:type_not_found_exception"),
    )
    observed = table(
        row("a", "throws:type_not_found_exception", "type not found: x"),
        row("b", "throws:type_not_found_exception", "type not found: y"),
    )
    assert messages.compare(baseline, observed) == []


def test_ignores_the_spelling_of_default_deleters():
    spelled = "type is not convertible to std::unique_ptr<iface, std::default_delete<iface> >& from x"
    short = "type is not convertible to std::unique_ptr<iface>& from x"
    baseline = table(row("a", "throws:type_not_convertible_exception", spelled))
    observed = table(row("a", "throws:type_not_convertible_exception", short))
    assert messages.compare(baseline, observed) == []
    nested = "std::unique_ptr<std::unique_ptr<a, std::default_delete<a> >, std::default_delete<std::unique_ptr<a, std::default_delete<a> > > >*"
    assert messages.without_default_arguments(nested) == "std::unique_ptr<std::unique_ptr<a>>*"
