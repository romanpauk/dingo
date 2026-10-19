#!/usr/bin/env python3
"""Reports which contracts of the spec the current implementation satisfies.

The contracts come from spec/content/contracts.py and the metrics from the
baseline recorded by probe.py. The C++ contract probes of the spec map each
contract to the metric paths that observe it, or state why it cannot be
observed. For every recorded toolchain the report classifies each contract:

- holds: every observing metric satisfies the bound
- violated: some observing metric does not, with the reason
- unobservable: no recorded metric isolates the contract

A series is the metrics of one scenario over the measured sizes. LINEAR object
sizes must fit a line exactly, LINEAR instantiations must grow at most
linearly (each step is no steeper than the one before), and CONSTANT metrics
must be equal across sizes. AT_MOST and EXACTLY compare the values. An ALWAYS
inlining contract holds when the observed probes make no Dingo calls. The
other bounds are not interpreted.

This is a report. The exit status is 0 unless the spec or the baseline cannot
be read.
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass
from fnmatch import fnmatchcase
from fractions import Fraction
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BASELINE = Path(__file__).resolve().parent / "baseline.json"

HOLDS = "holds"
VIOLATED = "violated"
UNOBSERVABLE = "unobservable"

# The last numeric dotted component of a metric path is the size of its series.
SIZE_RE = re.compile(r"^(.*\.)(\d+)((?:/.*)?)$")
SHOWN_VIOLATIONS = 3


@dataclass(frozen=True)
class Verdict:
    status: str
    reason: str = ""
    metrics: int = 0


def flatten(values, prefix=""):
    for key, value in values.items():
        path = f"{prefix}{key}"
        if isinstance(value, dict):
            yield from flatten(value, f"{path}/")
        else:
            yield path, value


def matching_paths(metrics, probe):
    return [
        path
        for path in sorted(metrics)
        if any(fnmatchcase(path, pattern) for pattern in probe.metrics)
        and not any(fnmatchcase(path, pattern) for pattern in probe.excluding)
    ]


def series_of(metrics, paths):
    """Groups metric paths by series, each as a sorted list of (size, value)."""
    series = {}
    for path in paths:
        match = SIZE_RE.match(path)
        if match is None:
            series[path] = [(None, metrics[path])]
            continue
        key = f"{match.group(1)}*{match.group(3)}"
        series.setdefault(key, []).append((int(match.group(2)), metrics[path]))
    return {key: sorted(points) for key, points in sorted(series.items())}


def slopes(points):
    return [
        Fraction(b[1] - a[1], b[0] - a[0]) for a, b in zip(points, points[1:])
    ]


def describe(violations):
    shown = "; ".join(violations[:SHOWN_VIOLATIONS])
    hidden = len(violations) - SHOWN_VIOLATIONS
    return f"{shown}; and {hidden} more" if hidden > 0 else shown


def short(path):
    return path.removeprefix("codegen/probes/")


def judge(failures, checked, holds_reason=""):
    if failures:
        return Verdict(VIOLATED, describe(failures), checked)
    return Verdict(HOLDS, holds_reason, checked)


def check_values(contract, metrics, paths):
    bound = contract.bound
    kind = bound.kind.value
    failures = []
    for path in paths:
        value = metrics[path]
        if kind == "at_most" and value > bound.value:
            failures.append(f"{short(path)} is {value}, above {bound.value}")
        elif kind == "exactly" and value != bound.value:
            failures.append(f"{short(path)} is {value}, not {bound.value}")
        elif kind == "always" and value > 0:
            failures.append(f"{short(path)} makes {value} Dingo calls")
    return judge(failures, len(paths))


def check_constant(metrics, paths):
    failures = []
    single = 0
    for key, points in series_of(metrics, paths).items():
        if len({value for _, value in points}) > 1:
            failures.append(
                f"{key} varies: " + ", ".join(f"{n}: {v}" for n, v in points)
            )
        elif len(points) == 1:
            single += 1
    note = "one measurement per series" if single else ""
    return judge(failures, len(paths), note)


def check_linear(contract, metrics, paths):
    bound = contract.bound
    exact = contract.dimension.value == "object_size"
    failures = []
    judged = 0
    for key, points in series_of(metrics, paths).items():
        if len(points) < 3 or points[0][0] is None:
            continue
        judged += 1
        steps = slopes(points)
        values = ", ".join(f"{n}: {v}" for n, v in points)
        if exact and len(set(steps)) > 1:
            failures.append(f"{key} is not a line ({values})")
        elif not exact and any(later > earlier for earlier, later in zip(steps, steps[1:])):
            failures.append(
                f"{key} grows faster than linear, slopes "
                + " then ".join(f"{float(step):.1f}" for step in steps)
                + f" ({values})"
            )
        elif bound.value is not None and max(steps) > bound.value:
            failures.append(
                f"{key} grows {float(max(steps)):.1f} per {bound.variable}, above {bound.value}"
            )
    if not judged:
        return Verdict(UNOBSERVABLE, "no series has three measured sizes")
    return judge(failures, judged)


def evaluate_contract(contract, probe, metrics):
    """Classifies one contract against the flattened metrics of one toolchain."""
    if probe is None:
        return Verdict(UNOBSERVABLE, "no contract probe entry")
    if probe.not_observable is not None:
        return Verdict(UNOBSERVABLE, probe.not_observable)
    paths = matching_paths(metrics, probe)
    if not paths:
        return Verdict(
            UNOBSERVABLE,
            "the baseline of this toolchain records none of: " + ", ".join(probe.metrics),
        )
    kind = contract.bound.kind.value
    if kind in ("at_most", "exactly", "always"):
        return check_values(contract, metrics, paths)
    if kind == "constant":
        return check_constant(metrics, paths)
    if kind == "linear":
        return check_linear(contract, metrics, paths)
    return Verdict(UNOBSERVABLE, f"the {kind} bound is not interpreted from metrics")


def evaluate_toolchain(spec, measured):
    metrics = dict(flatten(measured))
    probes = {probe.contract: probe for probe in spec.cpp.contract_probes}
    return {
        contract.id: evaluate_contract(contract, probes.get(contract.id), metrics)
        for contract in spec.contracts
    }


def summarize(verdicts):
    counts = {HOLDS: 0, VIOLATED: 0, UNOBSERVABLE: 0}
    for verdict in verdicts.values():
        counts[verdict.status] += 1
    return counts


def to_json(spec, results):
    dimensions = {contract.id: contract.dimension.value for contract in spec.contracts}
    return {
        toolchain: {
            "summary": summarize(verdicts),
            "contracts": [
                {
                    "id": contract_id,
                    "dimension": dimensions[contract_id],
                    "status": verdict.status,
                    "reason": verdict.reason,
                    "metrics": verdict.metrics,
                }
                for contract_id, verdict in verdicts.items()
            ],
        }
        for toolchain, verdicts in results.items()
    }


def format_report(spec, results):
    dimensions = {contract.id: contract.dimension.value for contract in spec.contracts}
    lines = []
    for toolchain, verdicts in results.items():
        lines.append(f"{toolchain}")
        for contract_id, verdict in verdicts.items():
            detail = f": {verdict.reason}" if verdict.reason else ""
            lines.append(
                f"  {verdict.status:<12} {contract_id} [{dimensions[contract_id]}]{detail}"
            )
        counts = summarize(verdicts)
        lines.append(
            f"  {counts[HOLDS]} hold, {counts[VIOLATED]} violated, "
            f"{counts[UNOBSERVABLE]} unobservable of {len(verdicts)} contracts"
        )
        lines.append("")
    return "\n".join(lines)


def load_spec():
    # The report must not leave bytecode in the repository.
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(PROJECT_ROOT))
    from spec import SpecError, load_spec

    try:
        return load_spec()
    except SpecError as error:
        raise SystemExit(f"the spec is invalid:\n{error}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument(
        "--toolchain",
        action="append",
        help="report only this toolchain key; may be repeated",
    )
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    args = parser.parse_args(argv)

    try:
        baselines = json.loads(args.baseline.read_text())
    except (OSError, ValueError) as error:
        print(f"cannot read the baseline {args.baseline}: {error}", file=sys.stderr)
        return 2
    spec = load_spec()

    keys = sorted(baselines)
    if args.toolchain:
        unknown = sorted(set(args.toolchain) - set(keys))
        if unknown:
            print(f"no baseline for {', '.join(unknown)}; recorded: {', '.join(keys)}", file=sys.stderr)
            return 2
        keys = sorted(set(args.toolchain))
    results = {key: evaluate_toolchain(spec, baselines[key]) for key in keys}

    if args.json:
        print(json.dumps(to_json(spec, results), indent=2, sort_keys=True))
    else:
        print(format_report(spec, results), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
