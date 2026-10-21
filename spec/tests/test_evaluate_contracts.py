"""Tests of test/contracts/evaluate.py on small synthetic baselines."""

import importlib.util
import json
import shutil
import sys
from pathlib import Path

import pytest

from spec.content.contracts import contracts
from spec.content.cpp_contract_probes import cpp_contract_probes
from spec.schema import (
    Bound,
    BoundKind,
    Contract,
    ContractDimension,
    CppContractProbe,
    CppProjection,
    Spec,
)

ROOT = Path(__file__).resolve().parents[2]
SCRATCH = ROOT / "build-spec-contract-tests"


def load_evaluate():
    path = ROOT / "test" / "contracts" / "evaluate.py"
    module_spec = importlib.util.spec_from_file_location("contracts_evaluate", path)
    module = importlib.util.module_from_spec(module_spec)
    sys.modules[module_spec.name] = module
    module_spec.loader.exec_module(module)
    return module


evaluate = load_evaluate()
SHIPPED = Spec(contracts=contracts, cpp=CppProjection(contract_probes=cpp_contract_probes))


@pytest.fixture
def scratch():
    SCRATCH.mkdir(exist_ok=True)
    yield SCRATCH
    shutil.rmtree(SCRATCH, ignore_errors=True)


def contract(kind, dimension=ContractDimension.OBJECT_SIZE, **bound):
    return Contract(
        id="c",
        description="A contract.",
        operation="op",
        dimension=dimension,
        bound=Bound(kind=kind, **bound),
    )


def probe(*metrics, excluding=()):
    return CppContractProbe(id="c", contract="c", metrics=metrics, excluding=excluding)


def verdict(the_contract, the_probe, metrics):
    return evaluate.evaluate_contract(the_contract, the_probe, metrics)


def sizes(prefix, values, sizes=(1, 8, 32)):
    return {f"{prefix}.{n}": value for n, value in zip(sizes, values)}


# Linear


def test_object_size_on_a_line_holds():
    metrics = sizes("object_size/a.static", (16, 72, 264))
    result = verdict(
        contract(BoundKind.LINEAR, variable="n"), probe("object_size/a.static.*"), metrics
    )
    assert result.status == evaluate.HOLDS
    assert result.metrics == 1


def test_object_size_off_a_line_is_violated():
    metrics = sizes("object_size/a.static", (16, 72, 300))
    result = verdict(
        contract(BoundKind.LINEAR, variable="n"), probe("object_size/a.static.*"), metrics
    )
    assert result.status == evaluate.VIOLATED
    assert "object_size/a.static.* is not a line (1: 16, 8: 72, 32: 300)" in result.reason


def test_object_size_series_are_judged_separately():
    metrics = {
        **sizes("object_size/a.static", (16, 72, 264)),
        **sizes("object_size/b.static", (32, 200, 776)),
    }
    result = verdict(
        contract(BoundKind.LINEAR, variable="n"), probe("object_size/*.static.*"), metrics
    )
    assert (result.status, result.metrics) == (evaluate.HOLDS, 2)


def test_instantiations_at_most_linear():
    cost = ContractDimension.COMPILE_COST
    linear = contract(BoundKind.LINEAR, cost, variable="n")
    sub = sizes("instantiations/a.static", (100, 800, 2000))
    result = verdict(linear, probe("instantiations/a.*"), {f"{k}/classes": v for k, v in sub.items()})
    assert result.status == evaluate.HOLDS
    super_linear = sizes("instantiations/a.static", (100, 200, 2000))
    result = verdict(
        linear, probe("instantiations/a.*"), {f"{k}/classes": v for k, v in super_linear.items()}
    )
    assert result.status == evaluate.VIOLATED
    assert "instantiations/a.static.*/classes grows faster than linear" in result.reason
    assert "slopes 14.3 then 75.0" in result.reason


def test_linear_with_a_coefficient_limits_the_slope():
    cost = ContractDimension.COMPILE_COST
    metrics = {f"{k}/classes": v for k, v in sizes("instantiations/a.static", (10, 80, 320)).items()}
    within = contract(BoundKind.LINEAR, cost, variable="n", value=10)
    above = contract(BoundKind.LINEAR, cost, variable="n", value=9)
    assert verdict(within, probe("instantiations/*"), metrics).status == evaluate.HOLDS
    result = verdict(above, probe("instantiations/*"), metrics)
    assert result.status == evaluate.VIOLATED
    assert "above 9" in result.reason


def test_linear_needs_three_sizes():
    metrics = {"object_size/a.static.1": 16, "object_size/a.static.8": 72}
    result = verdict(
        contract(BoundKind.LINEAR, variable="n"), probe("object_size/*"), metrics
    )
    assert result.status == evaluate.UNOBSERVABLE
    assert "three measured sizes" in result.reason


# Constant, at most, exactly


def test_constant_across_sizes():
    constant = contract(BoundKind.CONSTANT)
    equal = sizes("object_size/a.runtime", (8, 8, 8))
    assert verdict(constant, probe("object_size/*"), equal).status == evaluate.HOLDS
    varying = sizes("object_size/a.runtime", (8, 8, 16))
    result = verdict(constant, probe("object_size/*"), varying)
    assert result.status == evaluate.VIOLATED
    assert "1: 8, 8: 8, 32: 16" in result.reason


def test_constant_with_one_measurement_holds_and_says_so():
    result = verdict(contract(BoundKind.CONSTANT), probe("object_size/runtime"), {"object_size/runtime": 8})
    assert result.status == evaluate.HOLDS
    assert result.reason == "one measurement per series"


def test_at_most_and_exactly_compare_values():
    metrics = {"headers/a.h/dingo_lines": 10, "headers/b.h/dingo_lines": 12}
    header = ContractDimension.HEADER_COST
    holds = contract(BoundKind.AT_MOST, header, value=12)
    assert verdict(holds, probe("headers/*"), metrics).status == evaluate.HOLDS
    result = verdict(contract(BoundKind.AT_MOST, header, value=11), probe("headers/*"), metrics)
    assert result.status == evaluate.VIOLATED
    assert result.reason == "headers/b.h/dingo_lines is 12, above 11"
    result = verdict(contract(BoundKind.EXACTLY, header, value=10), probe("headers/*"), metrics)
    assert result.status == evaluate.VIOLATED
    assert result.reason == "headers/b.h/dingo_lines is 12, not 10"
    exact = {"headers/a.h/dingo_lines": 10}
    assert verdict(contract(BoundKind.EXACTLY, header, value=10), probe("headers/*"), exact).status == evaluate.HOLDS


def test_many_violations_are_summarized():
    metrics = {f"headers/{n}.h/dingo_lines": 99 for n in "abcde"}
    result = verdict(
        contract(BoundKind.AT_MOST, ContractDimension.HEADER_COST, value=1),
        probe("headers/*"),
        metrics,
    )
    assert result.status == evaluate.VIOLATED
    assert result.reason.endswith("and 2 more")
    assert result.reason.count("above 1") == 3


# Inlining


def test_always_holds_when_probes_make_no_dingo_calls():
    inlining = contract(BoundKind.ALWAYS, ContractDimension.INLINING)
    metrics = {
        "codegen/probes/probe_static_x/dingo_calls": 0,
        "codegen/probes/probe_static_x/calls": 3,
    }
    result = verdict(inlining, probe("codegen/probes/probe_static_*/dingo_calls"), metrics)
    assert (result.status, result.metrics) == (evaluate.HOLDS, 1)


def test_always_is_violated_by_dingo_calls():
    inlining = contract(BoundKind.ALWAYS, ContractDimension.INLINING)
    metrics = {"codegen/probes/probe_static_x/dingo_calls": 2}
    result = verdict(inlining, probe("codegen/probes/probe_static_*/dingo_calls"), metrics)
    assert result.status == evaluate.VIOLATED
    assert result.reason == "probe_static_x/dingo_calls makes 2 Dingo calls"


def test_excluded_paths_are_not_judged():
    inlining = contract(BoundKind.ALWAYS, ContractDimension.INLINING)
    metrics = {
        "codegen/probes/probe_static_x/dingo_calls": 0,
        "codegen/probes/probe_static_x.cold/dingo_calls": 1,
        "codegen/probes/probe_static_mixed_y/dingo_calls": 4,
    }
    selected = probe(
        "codegen/probes/probe_static_*/dingo_calls",
        excluding=("codegen/probes/*.cold/*", "codegen/probes/probe_static_mixed_*"),
    )
    assert verdict(inlining, selected, metrics).status == evaluate.HOLDS


@pytest.mark.parametrize("kind", [BoundKind.NEVER, BoundKind.COLD])
def test_other_inlining_bounds_are_not_interpreted(kind):
    result = verdict(
        contract(kind, ContractDimension.INLINING), probe("codegen/*"), {"codegen/a": 1}
    )
    assert result.status == evaluate.UNOBSERVABLE
    assert f"the {kind.value} bound is not interpreted" in result.reason


def test_recorded_values_are_the_limits():
    result = verdict(
        contract(BoundKind.RECORDED, ContractDimension.CODE_SIZE),
        probe("codegen/*"),
        {"codegen/a": 1},
    )
    assert result.status == evaluate.HOLDS
    assert "probe.py check" in result.reason


# Unobservable


def test_missing_probe_entry_and_stated_reason():
    bound = contract(BoundKind.CONSTANT)
    assert verdict(bound, None, {}).reason == "no contract probe entry"
    stated = CppContractProbe(id="c", contract="c", not_observable="Not isolated.")
    result = verdict(bound, stated, {"object_size/runtime": 8})
    assert (result.status, result.reason) == (evaluate.UNOBSERVABLE, "Not isolated.")


def test_pattern_without_a_recorded_metric_is_unobservable():
    result = verdict(
        contract(BoundKind.LINEAR, variable="n"),
        probe("instantiations/*"),
        {"object_size/runtime": 8},
    )
    assert result.status == evaluate.UNOBSERVABLE
    assert "records none of: instantiations/*" in result.reason


# The shipped contracts on a synthetic baseline


def synthetic_baseline(**overrides):
    baseline = {
        "object_size": {
            **sizes("flat_resolved.static", (16, 72, 264)),
            **sizes("flat_resolved.mixed", (128, 184, 376)),
            "runtime": 8,
        },
        "codegen": {
            "out_of_line_dingo_functions": 3,
            "probes": {
                "probe_static_resolution_consumer_read": {
                    "calls": 0,
                    "dingo_calls": 0,
                    "indirect_calls": 0,
                },
            },
        },
        "headers": {"dingo/container.h": {"dingo_lines": 7776}},
    }
    baseline.update(overrides)
    return baseline


def test_shipped_contracts_on_a_synthetic_baseline():
    verdicts = evaluate.evaluate_toolchain(SHIPPED, synthetic_baseline())
    assert set(verdicts) == {c.id for c in contracts}
    assert verdicts["object_size_static_container"].status == evaluate.HOLDS
    assert verdicts["object_size_mixed_container"].status == evaluate.HOLDS
    assert verdicts["object_size_runtime_container"].status == evaluate.HOLDS
    assert verdicts["inline_static_resolve"].status == evaluate.HOLDS
    assert verdicts["header_cost_container"].status == evaluate.HOLDS
    assert verdicts["header_cost_static_container"].status == evaluate.UNOBSERVABLE
    assert verdicts["compile_instantiations_bindings"].status == evaluate.UNOBSERVABLE
    assert verdicts["compile_kernel"].status == evaluate.UNOBSERVABLE
    counts = evaluate.summarize(verdicts)
    assert counts[evaluate.VIOLATED] == 0
    assert sum(counts.values()) == len(contracts)


def test_shipped_contracts_detect_regressions():
    baseline = synthetic_baseline()
    baseline["object_size"]["flat_resolved.static.32"] = 300
    baseline["headers"]["dingo/container.h"]["dingo_lines"] = 7777
    baseline["codegen"]["probes"]["probe_static_resolution_consumer_read"]["dingo_calls"] = 1
    verdicts = evaluate.evaluate_toolchain(SHIPPED, baseline)
    assert verdicts["object_size_static_container"].status == evaluate.VIOLATED
    assert verdicts["header_cost_container"].status == evaluate.VIOLATED
    assert verdicts["inline_static_resolve"].status == evaluate.VIOLATED


def test_recorded_baseline_classification():
    baselines = json.loads((ROOT / "test" / "contracts" / "baseline.json").read_text())
    for key, measured in baselines.items():
        verdicts = evaluate.evaluate_toolchain(SHIPPED, measured)
        for contract_id in (
            "object_size_static_container",
            "object_size_mixed_container",
            "object_size_runtime_container",
        ):
            assert verdicts[contract_id].status == evaluate.HOLDS, (key, contract_id)
        # The header lines are not checked for now.
        # for contract_id, verdict_ in verdicts.items():
        #     if contract_id.startswith("header_cost_"):
        #         assert verdict_.status == evaluate.HOLDS, (key, contract_id)
        recorded = "instantiations" in measured
        status = verdicts["compile_instantiations_bindings"].status
        assert (status == evaluate.HOLDS) == recorded, key


# The command line


def write_baseline(directory, baselines):
    path = directory / "baseline.json"
    path.write_text(json.dumps(baselines))
    return path


@pytest.fixture
def shipped_spec(monkeypatch):
    monkeypatch.setattr(evaluate, "load_spec", lambda: SHIPPED)


def test_main_prints_a_report_per_toolchain(scratch, shipped_spec, capsys):
    path = write_baseline(scratch, {"tc-b": synthetic_baseline(), "tc-a": synthetic_baseline()})
    assert evaluate.main(["--baseline", str(path)]) == 0
    out = capsys.readouterr().out
    assert out.index("tc-a\n") < out.index("tc-b\n")
    assert "  holds        object_size_static_container [object_size]" in out
    assert "  unobservable compile_kernel [compile_cost]: " in out
    assert f"of {len(contracts)} contracts" in out


def test_main_json(scratch, shipped_spec, capsys):
    path = write_baseline(scratch, {"tc": synthetic_baseline()})
    assert evaluate.main(["--baseline", str(path), "--json"]) == 0
    report = json.loads(capsys.readouterr().out)
    assert list(report) == ["tc"]
    entries = {entry["id"]: entry for entry in report["tc"]["contracts"]}
    assert len(entries) == len(contracts)
    assert entries["object_size_static_container"]["status"] == "holds"
    assert entries["object_size_static_container"]["dimension"] == "object_size"
    assert sum(report["tc"]["summary"].values()) == len(contracts)


def test_main_selects_toolchains(scratch, shipped_spec, capsys):
    path = write_baseline(scratch, {"tc-a": synthetic_baseline(), "tc-b": synthetic_baseline()})
    assert evaluate.main(["--baseline", str(path), "--toolchain", "tc-b", "--json"]) == 0
    assert list(json.loads(capsys.readouterr().out)) == ["tc-b"]
    assert evaluate.main(["--baseline", str(path), "--toolchain", "other"]) == 2


def test_violations_do_not_fail_the_report(scratch, shipped_spec, capsys):
    broken = synthetic_baseline()
    broken["object_size"]["flat_resolved.static.32"] = 300
    path = write_baseline(scratch, {"tc": broken})
    assert evaluate.main(["--baseline", str(path)]) == 0
    assert "violated" in capsys.readouterr().out


def test_unreadable_baseline_fails(scratch, shipped_spec, capsys):
    assert evaluate.main(["--baseline", str(scratch / "missing.json")]) == 2
    broken = scratch / "broken.json"
    broken.write_text("{")
    assert evaluate.main(["--baseline", str(broken)]) == 2
    assert "cannot read the baseline" in capsys.readouterr().err
