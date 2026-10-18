#!/usr/bin/env python3

import argparse
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
from pathlib import Path


TRACE_COUNTERS = {
    "instantiate_class": "Total InstantiateClass",
    "instantiate_function": "Total InstantiateFunction",
    "parse_class": "Total ParseClass",
}


def resolve_compiler(compiler: str) -> str:
    executable = shutil.which(compiler)
    if executable is None:
        raise RuntimeError(f"compiler not found: {compiler}")
    resolved = str(Path(executable).resolve())
    if Path(resolved).name == "ccache":
        raise RuntimeError(
            "compile-time check requires the real Clang executable, not a ccache shim"
        )
    return resolved


def read_json(path: Path) -> dict:
    return json.loads(path.read_text())


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def digest_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def compiler_identity(compiler: str) -> dict:
    version = subprocess.run(
        [compiler, "--version"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    target = subprocess.run(
        [compiler, "-dumpmachine"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    match = re.search(r"clang version (\d+)(?:\.(\d+))?", version)
    if not match:
        raise RuntimeError(f"compile-time check requires Clang, got: {version}")
    return {
        "family": "clang",
        "major": int(match.group(1)),
        "target": target,
        "version": version.splitlines()[0],
    }


def fixture_digest(project_root: Path, fixture: dict) -> str:
    source = project_root / "test" / "compile_time" / fixture["source"]
    data = source.read_bytes()
    settings = json.dumps(
        {
            "defines": fixture.get("defines", []),
            "families": fixture.get("families", {}),
            "header_metrics": fixture.get("header_metrics", False),
        },
        sort_keys=True,
    ).encode()
    return digest_bytes(data + b"\0" + settings)


def extract_trace_metrics(trace: dict, families: dict) -> dict:
    metrics = {}
    events = trace.get("traceEvents", [])
    totals = {
        event.get("name"): event.get("args", {}).get("count")
        for event in events
        if event.get("name", "").startswith("Total ")
    }
    for metric, event_name in TRACE_COUNTERS.items():
        count = totals.get(event_name, 0)
        if not isinstance(count, int):
            raise RuntimeError(f"invalid count for {event_name}")
        metrics[metric] = count

    instantiations = [
        event.get("args", {}).get("detail", "")
        for event in events
        if event.get("name") in {"InstantiateClass", "InstantiateFunction"}
    ]
    for name, patterns in families.items():
        metrics[f"family.{name}"] = sum(
            1
            for detail in instantiations
            if any(pattern in detail for pattern in patterns)
        )
    return metrics


def run_command(command: list[str], cwd: Path) -> subprocess.CompletedProcess:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        rendered = " ".join(command)
        raise RuntimeError(
            f"command failed ({result.returncode}): {rendered}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result


def measure_header_metrics(
    compiler: str,
    source: Path,
    include_dir: Path,
    standard: int,
    defines: list[str],
    project_root: Path,
) -> dict:
    common = [
        compiler,
        f"-std=c++{standard}",
        f"-I{include_dir}",
        *(f"-D{value}" for value in defines),
    ]
    includes = run_command(
        [*common, "-E", "-H", str(source), "-o", "/dev/null"], project_root
    )
    header_paths = set()
    for line in includes.stderr.splitlines():
        match = re.match(r"^\.+\s+(.+)$", line)
        if match:
            header_paths.add(match.group(1))

    preprocessed = run_command([*common, "-E", "-P", str(source)], project_root)
    tokens = re.findall(r"[A-Za-z_]\w*|\d+(?:\.\d+)?|\S", preprocessed.stdout)
    return {
        "preprocessed_tokens": len(tokens),
        "transitive_headers": len(header_paths),
    }


def measure_fixture(
    compiler: str,
    project_root: Path,
    include_dir: Path,
    output_dir: Path,
    standard: int,
    fixture: dict,
) -> dict:
    name = fixture["name"]
    source = project_root / "test" / "compile_time" / fixture["source"]
    fixture_dir = output_dir / name
    fixture_dir.mkdir(parents=True, exist_ok=True)
    trace_path = fixture_dir / "trace.json"
    object_path = fixture_dir / "probe.o"
    rss_path = fixture_dir / "max-rss-kb.txt"
    for artifact in (trace_path, object_path, rss_path):
        artifact.unlink(missing_ok=True)
    defines = fixture.get("defines", [])
    time_executable = shutil.which("time")
    if not time_executable:
        raise RuntimeError("compile-time check requires /usr/bin/time")

    compile_command = [
        compiler,
        f"-std=c++{standard}",
        f"-I{include_dir}",
        "-O0",
        "-Werror",
        "-ftime-trace-granularity=0",
        f"-ftime-trace={trace_path}",
        *(f"-D{value}" for value in defines),
        "-c",
        str(source),
        "-o",
        str(object_path),
    ]
    run_command(
        [time_executable, "-f", "%M", "-o", str(rss_path), *compile_command],
        project_root,
    )
    trace = read_json(trace_path)
    metrics = extract_trace_metrics(trace, fixture.get("families", {}))
    metrics["max_rss_kb"] = int(rss_path.read_text().strip())
    if fixture.get("header_metrics", False):
        metrics.update(
            measure_header_metrics(
                compiler,
                source,
                include_dir,
                standard,
                defines,
                project_root,
            )
        )
    return {
        "digest": fixture_digest(project_root, fixture),
        "metrics": metrics,
    }


def load_manifest(project_root: Path) -> tuple[dict, Path]:
    path = project_root / "test" / "compile_time" / "manifest.json"
    return read_json(path), path


def measure(args: argparse.Namespace) -> dict:
    project_root = args.project_root.resolve()
    include_dir = args.include_dir.resolve()
    manifest, manifest_path = load_manifest(project_root)
    compiler = resolve_compiler(args.compiler)
    identity = compiler_identity(compiler)
    output_dir = args.work_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    fixtures = {}
    for fixture in manifest["fixtures"]:
        if args.budgeted_only and not fixture.get("budget", True):
            continue
        print(f"Measuring compile-time fixture {fixture['name']}")
        fixtures[fixture["name"]] = measure_fixture(
            compiler,
            project_root,
            include_dir,
            output_dir,
            manifest["standard"],
            fixture,
        )
    result = {
        "compiler": identity,
        "fixtures": fixtures,
        "manifest_digest": digest_bytes(manifest_path.read_bytes()),
        "schema": 1,
    }
    write_json(args.output.resolve(), result)
    return result


def metric_limit(name: str, reference: int) -> int:
    if name == "max_rss_kb":
        return math.ceil(reference * 1.05)
    return reference


def make_budget(args: argparse.Namespace) -> dict:
    measurement = read_json(args.measurement.resolve())
    project_root = args.project_root.resolve()
    manifest, manifest_path = load_manifest(project_root)
    manifest_digest = digest_bytes(manifest_path.read_bytes())
    if measurement["manifest_digest"] != manifest_digest:
        raise RuntimeError("measurement and current fixture manifests differ")

    manifest_fixtures = {
        fixture["name"]: fixture for fixture in manifest["fixtures"]
    }
    fixtures = {}
    for name, measured_fixture in measurement["fixtures"].items():
        fixture = manifest_fixtures[name]
        if not fixture.get("budget", True):
            continue
        metrics = {}
        for metric, reference_value in measured_fixture["metrics"].items():
            metrics[metric] = {
                "limit": metric_limit(metric, reference_value),
                "reference": reference_value,
            }
        fixtures[name] = {
            "digest": measured_fixture["digest"],
            "metrics": metrics,
        }

    budget = {
        "compiler": measurement["compiler"],
        "fixtures": fixtures,
        "manifest_digest": manifest_digest,
        "reference_commit": args.reference_commit,
        "schema": 2,
    }
    write_json(args.output.resolve(), budget)
    return budget


def validate_identity(expected: dict, observed: dict) -> list[str]:
    failures = []
    for key in ("family", "major", "target"):
        if expected[key] != observed[key]:
            failures.append(
                f"compiler {key}: expected {expected[key]!r}, "
                f"observed {observed[key]!r}"
            )
    return failures


def check(args: argparse.Namespace) -> int:
    budget = read_json(args.budget.resolve())
    if budget.get("schema") != 2:
        raise RuntimeError("unsupported compile-time budget schema")
    observed = measure(args)
    failures = validate_identity(budget["compiler"], observed["compiler"])
    if budget["manifest_digest"] != observed["manifest_digest"]:
        failures.append("fixture manifest changed; regenerate and review budgets")

    for name, expected_fixture in budget["fixtures"].items():
        actual_fixture = observed["fixtures"].get(name)
        if actual_fixture is None:
            failures.append(f"{name}: fixture was not measured")
            continue
        if expected_fixture["digest"] != actual_fixture["digest"]:
            failures.append(f"{name}: source or settings changed; regenerate budgets")
            continue
        for metric, expected in expected_fixture["metrics"].items():
            actual = actual_fixture["metrics"].get(metric)
            if actual is None:
                failures.append(f"{name}.{metric}: metric was not measured")
            elif actual > expected["limit"]:
                failures.append(
                    f"{name}.{metric}: observed {actual}, limit "
                    f"{expected['limit']} (reference {expected['reference']})"
                )

    if failures:
        print("Compile-time regression check failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        f"Compile-time regression check passed for {len(budget['fixtures'])} "
        "budgeted fixtures"
    )
    return 0


def common_measure_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--budgeted-only", action="store_true")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    measure_parser = subparsers.add_parser("measure")
    common_measure_arguments(measure_parser)

    budget_parser = subparsers.add_parser("make-budget")
    budget_parser.add_argument("--measurement", type=Path, required=True)
    budget_parser.add_argument("--reference-commit", required=True)
    budget_parser.add_argument("--output", type=Path, required=True)
    budget_parser.add_argument("--project-root", type=Path, required=True)

    check_parser = subparsers.add_parser("check")
    common_measure_arguments(check_parser)
    check_parser.add_argument("--budget", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "measure":
            measure(args)
            return 0
        if args.command == "make-budget":
            make_budget(args)
            return 0
        return check(args)
    except (OSError, RuntimeError, KeyError, ValueError) as error:
        print(f"compile-time check error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
