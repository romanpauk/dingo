"""Tests of the JSON export: determinism, type tree, and round trip."""

import json
import shutil
from pathlib import Path

import pytest

from spec import load_spec
from spec.export import main, to_json, to_jsonable
from spec.schema import CppSection, Spec
from spec.tests.fixtures import sample_spec

# Scratch output goes under a gitignored build directory, never /tmp.
SCRATCH = Path(__file__).resolve().parents[2] / "build-spec-tests"


@pytest.fixture
def scratch():
    SCRATCH.mkdir(exist_ok=True)
    yield SCRATCH
    shutil.rmtree(SCRATCH, ignore_errors=True)


def test_export_is_deterministic():
    assert to_json(load_spec()) == to_json(load_spec())
    assert to_json(sample_spec()) == to_json(sample_spec())


def test_export_format():
    text = to_json(sample_spec())
    assert text.endswith("}\n") and not text.endswith("\n\n")
    assert text.startswith('{\n  "behavior_changes": [')
    assert json.dumps(json.loads(text), indent=2, sort_keys=True) + "\n" == text


def test_keys_are_sorted_at_every_level():
    def check(node):
        if isinstance(node, dict):
            assert list(node) == sorted(node)
            for value in node.values():
                check(value)
        elif isinstance(node, list):
            for value in node:
                check(value)

    check(json.loads(to_json(sample_spec())))


def test_top_level_type_tree_matches_the_schema():
    data = json.loads(to_json(load_spec()))
    expected = set(Spec.__dataclass_fields__)
    assert set(data) == expected
    assert set(data["cpp"]) == set(CppSection.__dataclass_fields__)
    for name in expected - {"cpp"}:
        assert isinstance(data[name], list)


def test_shipped_capabilities_export():
    data = json.loads(to_json(load_spec()))
    capabilities = data["cpp"]["capabilities"]
    assert [c["id"] for c in capabilities] == sorted(c["id"] for c in capabilities)
    by_id = {c["id"]: c for c in capabilities}
    assert by_id["type_pack_element"] == {
        "builtin": "__type_pack_element",
        "description": by_id["type_pack_element"]["description"],
        "feature_macro": None,
        "header": None,
        "id": "type_pack_element",
        "minimum": None,
    }
    assert by_id["move_only_function"]["minimum"] == 202110
    assert by_id["move_only_function"]["header"] == "version"


def test_enums_export_as_values_and_tuples_as_lists():
    data = to_jsonable(sample_spec())
    scope = data["scopes"][1]
    assert scope["slot"] == "two_phase"
    assert scope["materializes"] == "lvalue"
    assert scope["phases"] == ["publish", "complete"]
    assert isinstance(data["rules"], list)
    route = data["rules"][0]
    assert route["from_type_tree"] == {
        "args": [],
        "category": "lvalue",
        "form": "variable",
        "name": "X",
    }
    assert route["access"] == "borrow"
    assert route["requires"] == [{"predicate": "stable", "subject": "X"}]
    assert route["cache"] is None


def test_round_trip_through_json_is_stable():
    text = to_json(sample_spec())
    data = json.loads(text)
    assert json.dumps(data, indent=2, sort_keys=True) + "\n" == text
    assert to_jsonable(sample_spec()) == data


def test_main_writes_to_stdout(capsys):
    assert main([]) == 0
    assert capsys.readouterr().out == to_json(load_spec())


def test_main_writes_the_output_file(scratch):
    target = scratch / "nested" / "spec.json"
    assert main(["--output", str(target)]) == 0
    assert target.read_bytes() == to_json(load_spec()).encode("utf-8")
    first = target.read_bytes()
    assert main(["--output", str(target)]) == 0
    assert target.read_bytes() == first
    assert b"\r" not in first
