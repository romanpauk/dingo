"""dingo for nbench: checkouts and compilers are the variants.

    uv run --with nbench@git+https://github.com/romanpauk/nbench \\
        nbench run -C benchmark/nbench_conf.py -v this=. -v main=git:main
    ... -s 'footprint|costs' --baseline latest   what changed since the last run

A case is one measured thing, named by its parameters; a metric is a
quantity of it:

  footprint   a scenario compiled at -O0 with the variant's headers
              (spec/footprint.py): code, constants, data and unwind bytes and
              functions of the object; the compiler's instructions, peak
              memory and CPU time
  costs       differences of footprint scenarios: a container, a first and a
              further registration, a request type; code bytes, compiler
              instructions and memory
  compiler    where the compiler spent its work in a scenario: a phase (gcc),
              an event or the instantiations of an area of the library
              (clang); time, memory collected, count
  probes      the code-generation probes, optimized: code bytes of each
  operations  what a container runs: an operation of a container and shape
              (register first, resolve T<0>, the first resolution and the
              mean of 1000 after it), counted by the program: instructions
  containers  the container benchmarks (build/benchmark/dingo_benchmark):
              time per operation

Every variant runs in each compiler of the matrix (this[g++], this[clang++])
and is compared with the reference of its own compiler. git:REF is the
commit's tree, extracted into the cache; the container benchmarks run from
the working tree's build, under "." only. Sizes and counts are exact, so any
change of them is reported; instructions within 1% (2 for what the
containers run), memory within 3% and times within 20%.
"""

import re
from pathlib import Path

from nbench import (Config, Derive, GoogleBenchmark, Metric, Python, Variant)
from spec import footprint
from spec.generator import driver

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build" / "nbench" / "footprint"
SCENARIOS = footprint.scenarios()
# A program's results are kept by what it is (compiler and preprocessed
# source), so a scenario of a tree is compiled once, for every suite.
driver.open_cache(WORK / "programs")


def checkout(name, value):
    """-v NAME=. is the working tree and its build; git:REF the commit's tree
    ({tree}, which its worktree step makes); any other VALUE a tree."""
    if value in ("", "."):
        return Variant(name, tree=str(ROOT), build=str(ROOT / "build"))
    return Variant(name, tree=value)


def work(variant):
    path = WORK / "work" / re.sub(r"\W", "_", variant.name)
    path.mkdir(parents=True, exist_ok=True)
    return path


def flags(variant, optimized):
    base = ["-std=c++17", f"-I{variant.values['tree']}/include"]
    return base + (optimized or ["-O0"])


def compiled(scenario, variant):
    """driver.measure of a scenario, or an error."""
    source, added = SCENARIOS[scenario]
    found = driver.measure(variant.values["compiler"], flags(variant, added),
                           work(variant), scenario.replace("/", "__"), source)
    if found.get("returncode"):
        return {"error": driver.first_error(found.get("stderr", ""))}
    return found


def measure(scenario, variant):
    """The object of a scenario and what the compiler took."""
    found = compiled(scenario, variant)
    if "error" in found:
        return found
    sections = found.get("object", {}).get("sections", {})
    return {**{k: sections[k] for k in ("code", "constants", "data", "unwind")
               if k in sections},
            "functions": found.get("object", {}).get("functions"),
            **{k: found[k] for k in ("instructions", "memory", "cpu")
               if k in found}}


def compiler_work(scenario, variant):
    """Where the compiler spent its work: a case per phase, event or area."""
    found = compiled(scenario, variant)
    if "error" in found:
        return found
    cases = {}
    for kind, items in found.get("drilldown", {}).items():
        for name, values in items.items():
            row = {}
            if "seconds" in values:
                row["time_ms"] = values["seconds"] * 1000
            if "milliseconds" in values:
                row["time_ms"] = values["milliseconds"]
            if "collected" in values:
                row["collected_kb"] = values["collected"]
            if "count" in values:
                row["count"] = values["count"]
            cases[f"{scenario}/{kind}/{name}"] = row
    return {"cases": cases}


def probes(scenario, variant):
    """The code bytes of each code-generation probe."""
    found = compiled(scenario, variant)
    if "error" in found:
        return found
    return {"cases": {f"probe/{name}": {"code": size} for name, size in
                      found.get("object", {}).get("named", {}).items()}}


def operations(case, variant):
    """The instructions of each operation of two registrations of a shape in
    a container, counted by the program, optimized: a case each."""
    container, shape = case.split("/")
    records, stderr, crash = driver.build_and_run(
        variant.values["compiler"], flags(variant, footprint.OPTIMIZED),
        work(variant), f"run__{container}__{shape}",
        footprint.performance_program(container, shape))
    if stderr or crash:
        return {"error": driver.first_error(stderr) if stderr else crash}
    return {"cases": {f"{case}/{r['operation']}":
                      {"instructions": r["instructions"]} for r in records}}


def costs(results, variant):
    """footprint.costs of the measured scenarios."""
    measured = {case: {"object": {"sections": {"code": m.get("code")}},
                       "instructions": m.get("instructions"),
                       "memory": m.get("memory")}
                for case, m in results.get("footprint", {}).items()}
    return footprint.costs(measured)


def cost_params(case):
    """runtime/unique: first registration -> container, shape, part."""
    if ":" not in case:
        container, part = case.split(" ", 1)
        return {"container": container, "part": part}
    where, part = case.split(": ", 1)
    container, shape = where.split("/")
    return {"container": container, "shape": shape, "part": part}


def benchmark_params(name):
    """What a container benchmark is, from its name."""
    head, _, rest = name.split("/")[0].partition("<")
    found = {"benchmark": head}
    found["container"] = ("fruit" if "fruit" in head else
                          "basic" if head.startswith("basic_") else
                          "static" if "static_container" in rest else
                          "runtime")
    if "container_traits<" in rest:
        found["cache"] = "on" if "container_traits<true>" in rest else "off"
    if "std::string" in rest or head.endswith("_string"):
        found["type"] = "string"
    elif rest.endswith("int>") or head.endswith("_int"):
        found["type"] = "int"
    if "arena" in rest:
        found["allocator"] = "arena"
    parts = name.split("/")
    if len(parts) > 1 and parts[1].isdigit():
        found["count"] = parts[1]
    return found


SCENARIO = r"^(?P<scenario>headers/\w+|runtime/empty|codegen|(?P<container>runtime|static)/(?P<shape>\w+)/(?P<registrations>\d+)/(?P<requests>\d+))"
OPERATIONS = [f"{c}/{s}" for c in ("runtime", "static") for s in footprint.SHAPES]
BYTES = dict(unit="B", noise=0, section="memory")

config = Config(
    name="dingo",
    variant=checkout,
    reference="this",
    matrix={"compiler": ["g++", "clang++"]},
    repetitions=1,
    suites=[
        Python("footprint", list(SCENARIOS), measure, title="Footprint",
               primary="code", params=SCENARIO,
               description="Each scenario compiled at -O0 with the "
                           "variant's headers (spec/footprint.py).",
               metrics=[Metric("code", "Code", **BYTES),
                        Metric("constants", "Constants", **BYTES),
                        Metric("data", "Data", **BYTES),
                        Metric("unwind", "Unwind tables", **BYTES),
                        Metric("functions", "Functions", noise=0,
                               section="memory"),
                        Metric("instructions", "Compiler instructions",
                               noise=0.01, section="cpu"),
                        Metric("memory", "Compiler peak memory", "KiB",
                               noise=0.03, section="memory"),
                        Metric("cpu", "Compiler CPU", "s", noise=0.2,
                               section="time")]),
        Python("compiler", list(SCENARIOS), compiler_work,
               title="Compiler work", primary="time_ms",
               params=SCENARIO + r"/(?P<kind>phases|events|instantiations)/(?P<name>.+)$",
               description="Where the compiler spent its work in each "
                           "scenario: gcc's phases, clang's events and the "
                           "instantiations of each area of the library.",
               metrics=[Metric("time_ms", "Time", "ms", noise=0.2,
                               abs_tol=10, section="time"),
                        Metric("collected_kb", "Memory collected", "kB",
                               noise=0.03, section="memory"),
                        Metric("count", "Count", noise=0, section="cpu")]),
        Python("probes", ["codegen"], probes, title="Code-generation probes",
               primary="code", params=r"^probe/(?P<probe>.+)$",
               metrics=[Metric("code", "Code", **BYTES)]),
        Python("operations", OPERATIONS, operations,
               title="Operations", primary="instructions",
               requires=driver.counts_instructions,
               params=r"^(?P<container>\w+)/(?P<shape>\w+)/(?P<operation>.+)$",
               description="The instructions of each operation of two "
                           "registrations of a shape, counted by the "
                           "program, optimized.",
               metrics=[Metric("instructions", "Instructions",
                               "per operation", noise=0, abs_tol=2,
                               section="cpu")]),
        GoogleBenchmark("containers", "{build}/benchmark/dingo_benchmark",
                        title="Container benchmarks", repetitions=3,
                        params=benchmark_params,
                        variants=lambda v: v.cell["compiler"] == "g++"),
    ],
    derive=[Derive("costs", costs, title="Costs", primary="code",
                   params=cost_params,
                   description="Differences of the footprint scenarios: a "
                               "container, a first and a further "
                               "registration and a request type.",
                   metrics=[Metric("code", "Code", **BYTES),
                            Metric("instructions", "Compiler instructions",
                                   noise=0.01, section="cpu"),
                            Metric("memory", "Compiler peak memory", "KiB",
                                   noise=0.03, section="memory")])],
)
