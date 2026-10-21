#!/usr/bin/env python3
"""Measures the contract baseline of the library.

The measurements cover the contract dimensions described in
docs/architecture/spec-driven-design.md that can be observed:

- instantiations: Dingo class and function template instantiations of
  synthetic scaling programs, counted from Clang's -ftime-trace output
- object_size: sizeof of containers holding growing numbers of bindings
- codegen: the size, calls, indirect calls, calls to Dingo functions, and
  calls to the Dingo functions that throw a diagnostic of each probe function
  of test/lit/codegen-static-probes.cpp, and the number of out-of-line Dingo
  functions in its object file
- runtime_cost: the calls on the cheapest path of each probe function of
  test/contracts/runtime_probes.cpp, which test/contracts/paths.py finds, and
  the allocations of a first and of a repeated resolution of each probe
- headers: preprocessed lines that come from Dingo headers, per public header

Every value is deterministic for a given toolchain. Baselines are keyed by
compiler family, major version, architecture, and C++ standard. `check` fails
when a measured value exceeds its baseline and reports values that improved.
`update` records the measured values as the new baseline for the toolchain.
"""

import argparse
import concurrent.futures
import json
import os
import platform
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import paths  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parents[2]
INCLUDE_ROOT = PROJECT_ROOT / "include" / "dingo"
# The header trees whose lines count as Dingo lines: the public include paths
# of the next implementation, and the headers they include.
DINGO_ROOTS = [INCLUDE_ROOT]
CODEGEN_PROBES = PROJECT_ROOT / "test" / "lit" / "codegen-static-probes.cpp"
RUNTIME_PROBES = Path(__file__).resolve().parent / "runtime_probes.cpp"
DEFAULT_BASELINE = Path(__file__).resolve().parent / "baseline.json"

SIZES = (1, 8, 32)
CONTAINERS = ("static", "mixed", "runtime")

# Headers included by the examples and the documentation. The spec-driven
# design records the full public API in the spec; until then this list covers
# the include paths users rely on.
PUBLIC_HEADERS = (
    "dingo/container.h",
    "dingo/runtime_container.h",
    "dingo/static_container.h",
    "dingo/storage/external.h",
    "dingo/storage/shared.h",
    "dingo/storage/shared_cyclical.h",
    "dingo/storage/unique.h",
    "dingo/factory/callable.h",
    "dingo/factory/constructor.h",
    "dingo/factory/function.h",
)


@dataclass(frozen=True)
class Scenario:
    name: str
    description: str
    # Stored type of binding i.
    stored: str
    # Requests resolved for n bindings.
    requests: tuple
    containers: tuple = CONTAINERS


def _flat_requests(n):
    return tuple(f"node<{i}> &" for i in range(n))


SCENARIOS = (
    Scenario(
        name="flat_resolved",
        description="n independent shared bindings, each resolved once",
        stored="node<{i}>",
        requests=_flat_requests,
    ),
    Scenario(
        name="flat_unresolved",
        description="n independent shared bindings, none resolved",
        stored="node<{i}>",
        requests=lambda n: (),
    ),
    Scenario(
        name="wrapper_resolved",
        description="n shared std::shared_ptr bindings, each resolved by leaf",
        stored="std::shared_ptr<node<{i}>>",
        requests=_flat_requests,
    ),
    Scenario(
        name="chain",
        description="a dependency chain of n shared bindings, last resolved",
        stored="chain_link<{i}>",
        requests=lambda n: (f"chain_link<{n - 1}> &",),
    ),
)

PROGRAM_PRELUDE = """\
#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <memory>

template <int I> struct node {
  node() : value(I) {}
  int value;
};

template <int I> struct chain_link {
  explicit chain_link(chain_link<I - 1> &previous)
      : value(previous.value + 1) {}
  int value;
};

template <> struct chain_link<0> {
  chain_link() : value(0) {}
  int value;
};
"""


def static_source(stored_types, name="source"):
    binds = ",\n    ".join(
        f"dingo::bind<dingo::scope<dingo::shared>, dingo::storage<{stored}>>"
        for stored in stored_types
    )
    return f"using {name} = dingo::bindings<\n    {binds}>;\n"


def container_setup(container, stored_types, source="source"):
    if container == "static":
        return [f"dingo::static_container<{source}> container;"]
    if container == "mixed":
        return [f"dingo::container<{source}> container;"]
    lines = ["dingo::container<> container;"]
    lines += [
        "container.template register_type<dingo::scope<dingo::shared>, "
        f"dingo::storage<{stored}>>();"
        for stored in stored_types
    ]
    return lines


def scenario_program(scenario, container, n):
    stored_types = [scenario.stored.format(i=i) for i in range(n)]
    body = container_setup(container, stored_types)
    body.append("int sum = 0;")
    body += [
        f"sum += container.template resolve<{request}>().value;"
        for request in scenario.requests(n)
    ]
    body.append("return sum;")
    source = static_source(stored_types) if container != "runtime" else ""
    lines = "\n".join(f"  {line}" for line in body)
    return f'{PROGRAM_PRELUDE}\n{source}\nextern "C" int probe() {{\n{lines}\n}}\n'


def sizes_program():
    parts = [PROGRAM_PRELUDE, "#include <cstdio>\n"]
    prints = ['std::printf("runtime %zu\\n", sizeof(dingo::container<>));']
    for scenario in ("flat_resolved", "wrapper_resolved"):
        stored = next(s for s in SCENARIOS if s.name == scenario).stored
        for n in SIZES:
            name = f"source_{scenario}_{n}"
            parts.append(static_source([stored.format(i=i) for i in range(n)], name))
            for container, spelling in (
                ("static", f"dingo::static_container<{name}>"),
                ("mixed", f"dingo::container<{name}>"),
            ):
                key = f"{scenario}.{container}.{n}"
                prints.append(f'std::printf("{key} %zu\\n", sizeof({spelling}));')
    body = "\n".join(f"  {line}" for line in prints)
    parts.append(f"int main() {{\n{body}\n  return 0;\n}}\n")
    return "\n".join(parts)


class Toolchain:
    def __init__(self, command):
        self.command = shlex.split(command)
        version = self._run([*self.command, "--version"]).lower()
        if "clang" in version:
            self.family = "clang"
        elif "gcc" in version or "g++" in version:
            self.family = "gcc"
        else:
            self.family = "unknown"
        self.major = self._run([*self.command, "-dumpversion"]).split(".", 1)[0]
        machine = platform.machine().lower()
        self.arch = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
        std = next(
            (arg for arg in self.command if arg.startswith("-std=")), "-std=default"
        )
        self.std = std.split("=", 1)[1]

    @property
    def key(self):
        return f"{self.family}{self.major}-{self.arch}-{self.std}"

    @staticmethod
    def _run(args, **kwargs):
        return subprocess.run(
            args, check=True, capture_output=True, text=True, **kwargs
        ).stdout.strip()

    def compile(self, args, **kwargs):
        result = subprocess.run(
            [*self.command, *args], capture_output=True, text=True, **kwargs
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"compilation failed: {shlex.join([*self.command, *args])}\n"
                f"{result.stderr}"
            )
        return result.stdout


def strip_template_arguments(name):
    result = []
    depth = 0
    for index, char in enumerate(name):
        if char == "<" and not name[:index].endswith("operator"):
            depth += 1
        elif char == ">" and depth:
            depth -= 1
        elif depth == 0:
            result.append(char)
    return "".join(result)


def instantiation_events(trace_path):
    trace = json.loads(trace_path.read_text())
    for event in trace["traceEvents"]:
        if event.get("name") in ("InstantiateClass", "InstantiateFunction"):
            yield event["name"], event.get("args", {}).get("detail", "")


def measure_instantiation(toolchain, work_dir, scenario, container, n):
    stem = work_dir / f"{scenario.name}-{container}-{n}"
    source = Path(f"{stem}.cpp")
    source.write_text(scenario_program(scenario, container, n))
    trace = Path(f"{stem}.json")
    toolchain.compile(
        [
            "-c",
            "-O0",
            f"-ftime-trace={trace}",
            "-ftime-trace-granularity=0",
            str(source),
            "-o",
            os.devnull,
        ]
    )
    counts = {"classes": 0, "functions": 0}
    entities = {}
    events = list(instantiation_events(trace))
    # Traces of the larger programs exceed 100 MB.
    trace.unlink()
    for kind, detail in events:
        if not detail.startswith("dingo::"):
            continue
        counts["classes" if kind == "InstantiateClass" else "functions"] += 1
        entity = strip_template_arguments(detail)
        entities[entity] = entities.get(entity, 0) + 1
    return counts, entities


def measure_instantiations(toolchain, work_dir, jobs):
    if toolchain.family != "clang":
        return None
    tasks = [
        (scenario, container, n)
        for scenario in SCENARIOS
        for container in scenario.containers
        for n in SIZES
    ]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        results = pool.map(
            lambda task: measure_instantiation(toolchain, work_dir, *task), tasks
        )
        return {
            f"{scenario.name}.{container}.{n}": counts
            for (scenario, container, n), (counts, _) in zip(tasks, results)
        }


def measure_object_sizes(toolchain, work_dir):
    source = work_dir / "sizes.cpp"
    executable = work_dir / "sizes"
    source.write_text(sizes_program())
    toolchain.compile([str(source), "-o", str(executable)])
    output = subprocess.run(
        [str(executable)], check=True, capture_output=True, text=True
    ).stdout
    return {
        key: int(value)
        for key, value in (line.split() for line in output.splitlines())
    }


SYMBOL_RE = re.compile(r"^[0-9a-fA-F]+ <(.+)>:$")
INSTRUCTION_RE = re.compile(r"^\s*[0-9a-fA-F]+:\s+([A-Za-z][A-Za-z0-9.]*)\s*(.*)$")
RELOCATION_RE = re.compile(r"^\s*[0-9a-fA-F]+:\s+R_[A-Z0-9_]+\s+(.+?)(?:[-+]0x[0-9a-f]+)?$")
TARGET_RE = re.compile(r"<(.+?)(?:\+0x[0-9a-f]+)?>")
DIRECT_CALLS = {"call", "callq", "bl"}
INDIRECT_CALLS = {"blr"}


def demangle(text):
    return subprocess.run(
        ["c++filt"], input=text, check=True, capture_output=True, text=True
    ).stdout


def parse_probe_calls(disassembly):
    probes = {}
    current = None
    pending = None

    def finish_pending():
        nonlocal pending
        if pending is not None:
            probe, target = pending
            if target is not None and "dingo::" in target:
                name = strip_template_arguments(target).split("(", 1)[0]
                kind = "dingo_throws" if paths.DIAGNOSTIC_RE.search(name) else "dingo_calls"
                probes[probe][kind] += 1
            pending = None

    for line in disassembly.splitlines():
        symbol = SYMBOL_RE.match(line.strip())
        if symbol is not None:
            finish_pending()
            name = symbol.group(1)
            current = name if name.startswith("probe_") else None
            if current is not None:
                probes[current] = {
                    "calls": 0,
                    "indirect_calls": 0,
                    "dingo_calls": 0,
                    "dingo_throws": 0,
                }
            continue
        if current is None:
            continue
        relocation = RELOCATION_RE.match(line)
        if relocation is not None:
            if pending is not None:
                pending = (pending[0], relocation.group(1))
            finish_pending()
            continue
        instruction = INSTRUCTION_RE.match(line)
        if instruction is None:
            continue
        finish_pending()
        mnemonic, operands = instruction.group(1).lower(), instruction.group(2)
        if mnemonic in DIRECT_CALLS and "*" not in operands:
            probes[current]["calls"] += 1
            target = TARGET_RE.search(operands)
            name = target.group(1) if target else None
            # Calls resolved through a relocation name their own function.
            pending = (current, None if name == current else name)
        elif mnemonic in DIRECT_CALLS or mnemonic in INDIRECT_CALLS:
            probes[current]["calls"] += 1
            probes[current]["indirect_calls"] += 1
    finish_pending()
    return probes


def measure_codegen(toolchain, work_dir):
    obj = work_dir / "codegen-static-probes.o"
    toolchain.compile(["-O2", "-c", str(CODEGEN_PROBES), "-o", str(obj)])
    disassembly = demangle(
        subprocess.run(
            ["objdump", "-dr", "--no-show-raw-insn", str(obj)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
    )
    result = {"probes": parse_probe_calls(disassembly)}
    symbols = demangle(
        subprocess.run(
            ["nm", "--defined-only", str(obj)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
    )
    result["out_of_line_dingo_functions"] = sum(
        1
        for line in symbols.splitlines()
        if len(line.split(maxsplit=2)) == 3
        and line.split(maxsplit=2)[1] in "tTwW"
        and "dingo::" in line.split(maxsplit=2)[2]
    )
    sizes = subprocess.run(
        ["nm", "-S", "--defined-only", str(obj)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    for line in sizes.splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[3] in result["probes"]:
            result["probes"][fields[3]]["bytes"] = int(fields[1], 16)
    return result


def measure_runtime_cost(toolchain, work_dir):
    obj = work_dir / "runtime-probes.o"
    executable = work_dir / "runtime-probes"
    # Without jump tables an indirect jump is a tail call, which the path
    # analysis counts.
    toolchain.compile(["-O2", "-fno-jump-tables", "-c", str(RUNTIME_PROBES), "-o", str(obj)])
    toolchain.compile([str(obj), "-o", str(executable)])
    disassembly = demangle(
        subprocess.run(
            ["objdump", "-dr", "--no-show-raw-insn", str(obj)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
    )
    analysis = paths.Paths(disassembly)
    result = {"paths": {}, "allocations": {}}
    for name in sorted(analysis.functions):
        if name.startswith("probe_resolve_") and "." not in name:
            cost = analysis.cheapest(name)
            if cost is None:
                raise RuntimeError(f"{name} has no path to a return")
            result["paths"][name] = {
                "calls": cost.calls,
                "indirect_calls": cost.indirect_calls,
                "dingo_calls": cost.dingo_calls,
            }
    output = subprocess.run(
        [str(executable)], check=True, capture_output=True, text=True
    ).stdout
    for line in output.splitlines():
        key, value = line.split()
        result["allocations"][key] = int(value)
    return result


LINEMARKER_RE = re.compile(r'^# (\d+) "([^"]*)"')


def measure_header(toolchain, header):
    output = toolchain.compile(
        ["-E", "-x", "c++", "-"], input=f"#include <{header}>\n"
    )
    dingo_lines = 0
    in_dingo = False
    dingo_files = {}
    for line in output.splitlines():
        marker = LINEMARKER_RE.match(line)
        if marker is not None:
            name = marker.group(2)
            if name not in dingo_files:
                resolved = Path(name).resolve()
                dingo_files[name] = any(
                    resolved.is_relative_to(root) for root in DINGO_ROOTS
                )
            in_dingo = dingo_files[name]
        elif in_dingo and line.strip():
            dingo_lines += 1
    return {"dingo_lines": dingo_lines}


def measure_headers(toolchain, jobs):
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        results = pool.map(lambda header: measure_header(toolchain, header), PUBLIC_HEADERS)
        return dict(zip(PUBLIC_HEADERS, results))


def measure(toolchain, work_dir, jobs):
    work_dir.mkdir(parents=True, exist_ok=True)
    result = {
        "object_size": measure_object_sizes(toolchain, work_dir),
        "codegen": measure_codegen(toolchain, work_dir),
        "runtime_cost": measure_runtime_cost(toolchain, work_dir),
        "headers": measure_headers(toolchain, jobs),
    }
    instantiations = measure_instantiations(toolchain, work_dir, jobs)
    if instantiations is not None:
        result["instantiations"] = instantiations
    return result


def flatten(values, prefix=""):
    for key, value in values.items():
        path = f"{prefix}{key}"
        if isinstance(value, dict):
            yield from flatten(value, f"{path}/")
        else:
            yield path, value


def compare(baseline, measured):
    expected = dict(flatten(baseline))
    actual = dict(flatten(measured))
    regressions = []
    improvements = []
    for path in sorted(expected.keys() | actual.keys()):
        # The header lines are measured but not checked for now.
        if path.startswith("headers/"):
            continue
        if path not in actual:
            regressions.append(f"{path}: not measured, baseline {expected[path]}")
        elif path not in expected:
            regressions.append(f"{path}: no baseline, measured {actual[path]}")
        elif actual[path] > expected[path]:
            regressions.append(f"{path}: {actual[path]} > baseline {expected[path]}")
        elif actual[path] < expected[path]:
            improvements.append(f"{path}: {actual[path]} < baseline {expected[path]}")
    return regressions, improvements


def load_baselines(path):
    return json.loads(path.read_text()) if path.exists() else {}


def write_baselines(path, baselines):
    path.write_text(json.dumps(baselines, indent=2, sort_keys=True) + "\n")


def report(toolchain, work_dir, name, top):
    scenario_name, container, n = name.split(".")
    scenario = next(s for s in SCENARIOS if s.name == scenario_name)
    work_dir.mkdir(parents=True, exist_ok=True)
    counts, entities = measure_instantiation(
        toolchain, work_dir, scenario, container, int(n)
    )
    print(json.dumps(counts))
    for entity, count in sorted(entities.items(), key=lambda item: -item[1])[:top]:
        print(f"{count:6} {entity}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=("measure", "check", "update", "report"))
    parser.add_argument(
        "--cxx-command",
        default=os.environ.get("CXX", "c++") + f" -std=c++20 -I{PROJECT_ROOT / 'include'}",
        help="compiler command including the standard and include flags",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help="the baseline file, baseline.json by default",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="directory for generated programs and traces, such as build/contracts",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument(
        "--scenario", help="report: scenario.container.size, such as chain.static.32"
    )
    parser.add_argument("--top", type=int, default=30)
    args = parser.parse_args()

    toolchain = Toolchain(args.cxx_command)
    if args.command == "report":
        if toolchain.family != "clang":
            print("report requires Clang", file=sys.stderr)
            return 2
        report(toolchain, args.work_dir, args.scenario, args.top)
        return 0

    measured = measure(toolchain, args.work_dir, args.jobs)

    if args.command == "measure":
        print(json.dumps({toolchain.key: measured}, indent=2, sort_keys=True))
        return 0

    baselines = load_baselines(args.baseline)
    if args.command == "update":
        baselines[toolchain.key] = measured
        write_baselines(args.baseline, baselines)
        print(f"recorded the baseline for {toolchain.key} in {args.baseline}")
        return 0

    if toolchain.key not in baselines:
        print(f"no baseline for {toolchain.key}; measured values:")
        print(json.dumps({toolchain.key: measured}, indent=2, sort_keys=True))
        return 0

    regressions, improvements = compare(baselines[toolchain.key], measured)
    for line in improvements:
        print(f"improved: {line}")
    if improvements:
        print(f"run `probe.py update` to record the improvements for {toolchain.key}")
    for line in regressions:
        print(f"regressed: {line}", file=sys.stderr)
    if regressions:
        return 1
    print(f"contract baseline for {toolchain.key} holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
