"""Tests of the behavior changes whose cases are tests of the next
implementation.

A case `test:<Suite>.<Name>` names a test of dingo_next_test that asserts the
behavior the change declares, and a case `diagnostic:<id>` a diagnostic whose
generated test rejects it. No table observes such a behavior, so the test is
the observation, and every case must name a test that exists.
"""

import re
from pathlib import Path

from spec import build_spec

PROJECT_ROOT = Path(__file__).resolve().parents[2]
NEXT_TESTS = PROJECT_ROOT / "test" / "next"
TEST_RE = re.compile(r"\b(?:TYPED_)?TEST(?:_F|_P)?\(\s*(\w+)\s*,\s*(\w+)\s*\)")


def next_tests():
    found = set()
    for source in NEXT_TESTS.rglob("*.cpp"):
        for suite, name in TEST_RE.findall(source.read_text()):
            found.add(f"{suite}.{name}")
    return found


def test_every_test_case_names_a_test_of_the_next_implementation():
    tests = next_tests()
    missing = [
        f"{change.id}: {case}"
        for change in build_spec().behavior_changes
        for case in change.cases
        if case.startswith("test:") and case.removeprefix("test:") not in tests
    ]
    assert not missing, missing


def test_every_diagnostic_case_names_a_generated_diagnostic_test():
    spec = build_spec()
    examples = {example.diagnostic for example in spec.cpp.diagnostic_examples}
    missing = [
        f"{change.id}: {case}"
        for change in spec.behavior_changes
        for case in change.cases
        if case.startswith("diagnostic:")
        and case.removeprefix("diagnostic:") not in examples
    ]
    assert not missing, missing


def test_the_tests_of_the_next_implementation_are_found():
    assert "next_object_routes.a_shared_pointer_owns_the_object_it_constructs" in (
        next_tests()
    )
