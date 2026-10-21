"""Tests of the contract schema, its validation, and the shipped contracts."""

import json
import re
from dataclasses import replace
from fnmatch import fnmatchcase
from pathlib import Path

import pytest

from spec import validate
from spec.content.contracts import HEADER_LINES, contracts  # noqa: F401
from spec.content.cpp_contract_probes import HEADER_PATHS, cpp_contract_probes
from spec.schema import (
    Bound,
    BoundKind,
    ContractDimension,
    CppContractProbe,
    CppProjection,
    Spec,
)
from spec.tests.fixtures import sample_spec

BASELINE = Path(__file__).resolve().parents[2] / "test" / "contracts" / "baseline.json"

# Only the contract sections, so these tests do not depend on other sections.
CONTRACT_SPEC = Spec(
    contracts=contracts, cpp=CppProjection(contract_probes=cpp_contract_probes)
)


def errors_of(spec):
    """The errors of the contract sections; other sections are tested elsewhere."""
    return [
        error
        for error in validate(spec)
        if error.startswith(("contracts.", "cpp.contract_probes"))
    ]


def assert_one_error(spec, fragment):
    errors = errors_of(spec)
    assert any(fragment in error for error in errors), errors


def with_contract(**changes):
    base = sample_spec()
    return replace(base, contracts=(replace(base.contracts[0], **changes),))


def with_probes(*probes):
    base = sample_spec()
    return replace(base, cpp=replace(base.cpp, contract_probes=probes))


def probe(**changes):
    fields = dict(id="p", contract="resolve_static_allocations", metrics=("a/b",))
    fields.update(changes)
    return CppContractProbe(**fields)


def flatten(values, prefix=""):
    for key, value in values.items():
        if isinstance(value, dict):
            yield from flatten(value, f"{prefix}{key}/")
        else:
            yield f"{prefix}{key}", value


def baseline_paths():
    baselines = json.loads(BASELINE.read_text())
    return {key: {path for path, _ in flatten(value)} for key, value in baselines.items()}


# Schema and validation


def test_contract_needs_a_description():
    assert_one_error(with_contract(description=" "), "description is empty")


@pytest.mark.parametrize(
    "bound",
    [
        Bound(kind=BoundKind.LINEAR, variable="bindings", unit="instantiations", value=1),
        Bound(kind=BoundKind.LINEAR, variable="bindings", unit="bytes"),
        Bound(kind=BoundKind.RECORDED),
        Bound(kind=BoundKind.RECORDED, unit="bytes"),
    ],
)
def test_valid_bounds(bound):
    assert errors_of(with_contract(bound=bound)) == []


@pytest.mark.parametrize(
    "bound, fragment",
    [
        (Bound(kind=BoundKind.LINEAR, variable="n", value=-1), "linear takes a non-negative value"),
        (Bound(kind=BoundKind.LINEAR, variable="Bindings"), "variable 'Bindings' is not snake_case"),
        (Bound(kind=BoundKind.RECORDED, value=1), "recorded takes no value"),
        (Bound(kind=BoundKind.RECORDED, variable="n"), "recorded takes no variable"),
        (Bound(kind=BoundKind.AT_MOST, value=0, unit="Bytes"), "unit 'Bytes' is not snake_case"),
        (Bound(kind=BoundKind.CONSTANT, unit="bytes"), "constant takes no unit"),
    ],
)
def test_invalid_bounds(bound, fragment):
    assert_one_error(with_contract(bound=bound), fragment)


def test_recorded_does_not_fit_inlining():
    spec = with_contract(
        dimension=ContractDimension.INLINING, bound=Bound(kind=BoundKind.RECORDED)
    )
    assert_one_error(spec, "recorded does not fit inlining")


def test_construct_is_for_compile_cost():
    compile_cost = with_contract(
        dimension=ContractDimension.COMPILE_COST,
        bound=Bound(kind=BoundKind.AT_MOST, value=0),
        construct="kernel",
    )
    assert errors_of(compile_cost) == []
    assert_one_error(with_contract(construct="kernel"), "only a compile cost contract names a construct")
    assert_one_error(
        replace(compile_cost, contracts=(replace(compile_cost.contracts[0], construct="Kernel"),)),
        "construct 'Kernel' is not snake_case",
    )


def test_valid_probe():
    assert errors_of(with_probes(probe())) == []
    unobserved = probe(metrics=(), not_observable="No probe isolates it.")
    assert errors_of(with_probes(unobserved)) == []
    excluded = probe(excluding=("a/c",))
    assert errors_of(with_probes(excluded)) == []


def test_probe_contract_must_exist():
    assert_one_error(with_probes(probe(contract="missing")), "contract 'missing' does not exist")


@pytest.mark.parametrize(
    "changes, fragment",
    [
        (dict(metrics=()), "set metrics or not_observable, exactly one"),
        (dict(not_observable="x"), "set metrics or not_observable, exactly one"),
        (dict(metrics=(), not_observable=" "), "not_observable gives no reason"),
        (dict(metrics=(), not_observable="x", excluding=("a/b",)), "excluding needs metrics"),
        (dict(metrics=("a/b", "a/b")), "metrics has duplicates"),
        (dict(metrics=("nopath",)), "metric pattern 'nopath' is not a metric path"),
        (dict(excluding=(" a/b",)), "is not a metric path"),
    ],
)
def test_invalid_probe(changes, fragment):
    assert_one_error(with_probes(probe(**changes)), fragment)


def test_one_probe_per_contract():
    assert_one_error(with_probes(probe(id="p"), probe(id="q")), "contracts has duplicates")


# The shipped contracts


def test_shipped_contract_sections_are_valid():
    assert errors_of(CONTRACT_SPEC) == []


def test_every_contract_has_a_description_and_a_probe_entry():
    probed = {entry.contract for entry in cpp_contract_probes}
    assert {contract.id for contract in contracts} == probed
    assert all(contract.description.strip().endswith(".") for contract in contracts)


def test_every_dimension_is_covered():
    assert {contract.dimension for contract in contracts} == set(ContractDimension)


def test_the_design_contracts_are_present():
    by_id = {contract.id: contract for contract in contracts}
    assert by_id["object_size_static_container"].bound.kind is BoundKind.LINEAR
    assert by_id["object_size_mixed_container"].bound.kind is BoundKind.LINEAR
    assert by_id["object_size_runtime_container"].bound.kind is BoundKind.CONSTANT
    inlining = {
        contract.id: contract.bound.kind
        for contract in contracts
        if contract.dimension is ContractDimension.INLINING
    }
    assert inlining == {
        "inline_static_resolve": BoundKind.ALWAYS,
        "inline_runtime_resolve_memoized": BoundKind.ALWAYS,
        "inline_runtime_resolve_first_resolution": BoundKind.NEVER,
        "inline_register_type": BoundKind.NEVER,
        "inline_diagnostics_throw_never": BoundKind.NEVER,
        "inline_diagnostics_throw_cold": BoundKind.COLD,
    }
    constructs = {
        contract.construct: contract
        for contract in contracts
        if contract.construct is not None and contract.bound.kind is BoundKind.LINEAR
    }
    per = {
        "descriptor": "distinct_types",
        "route_plan": "distinct_shapes",
        "binding_facts": "bindings",
        "selection": "request_types",
        "kernel": "used_routes",
        "static_kernel": "used_pairs",
    }
    for construct, variable in per.items():
        bound = constructs[construct].bound
        assert (bound.variable, bound.value) == (variable, 1), construct
    register_type = {
        contract.construct: contract.bound
        for contract in contracts
        if contract.operation == "register_type" and contract.bound.kind is BoundKind.AT_MOST
    }
    assert set(register_type) == {"kernel", "static_kernel", "selection"}
    assert all(bound.value == 0 for bound in register_type.values())


def test_semantic_contracts_have_no_cpp_text():
    cpp_text = re.compile(r"::|[<>]|\bstd\b|\bdingo\b|\.h\b")
    for contract in contracts:
        texts = [
            contract.id,
            contract.description,
            contract.operation,
            contract.construct or "",
            contract.bound.unit or "",
            contract.bound.variable or "",
        ]
        for text in texts:
            assert not cpp_text.search(text), (contract.id, text)


def test_every_metric_pattern_matches_a_recorded_path():
    recorded = baseline_paths()
    for entry in cpp_contract_probes:
        for pattern in entry.metrics:
            assert any(
                fnmatchcase(path, pattern) for paths in recorded.values() for path in paths
            ), (entry.contract, pattern)
        for pattern in entry.excluding:
            assert any(
                fnmatchcase(path, pattern) for paths in recorded.values() for path in paths
            ), (entry.contract, pattern)


# The header lines are not checked for now.
# def test_header_limits_are_the_maximum_over_the_recorded_toolchains():
#     baselines = json.loads(BASELINE.read_text())
#     paths = dict(HEADER_PATHS)
#     assert [name for name, _ in HEADER_LINES] == [name for name, _ in HEADER_PATHS]
#     by_id = {contract.id: contract for contract in contracts}
#     for name, lines in HEADER_LINES:
#         recorded = [
#             toolchain["headers"][paths[name]]["dingo_lines"] for toolchain in baselines.values()
#         ]
#         contract = by_id[f"header_cost_{name}"]
#         assert contract.bound.kind is BoundKind.AT_MOST
#         assert contract.bound.value == lines == max(recorded), name


def test_every_public_header_of_the_baseline_has_a_contract():
    baselines = json.loads(BASELINE.read_text())
    headers = {path for toolchain in baselines.values() for path in toolchain["headers"]}
    assert headers == {path for _, path in HEADER_PATHS}
