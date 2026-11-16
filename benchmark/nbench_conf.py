"""dingo for nbench: checkouts and compilers are the variants; the footprint
scenarios, the instructions the containers run, and the container
benchmarks are the suites.

    uv run --with nbench@git+https://github.com/romanpauk/nbench \\
        nbench run -C benchmark/nbench_conf.py -v this=. -v main=git:main
    ... -s footprint --baseline latest       what changed since the last run

Every variant runs in each compiler of the matrix (this[g++], this[clang++],
...) and is compared with the reference of its own compiler. git:REF is the
commit's tree, extracted into the cache. The container benchmarks run from the
working tree's build (build/benchmark/dingo_benchmark), under "." only. The footprint
compiles each scenario with the headers of the variant's tree, as
spec/footprint.py does (docs/development.md#footprint); sizes and counts are
exact, so any change of them is reported, instructions within 1% (2 for what
the containers run), memory within 3% and times within 20%.
"""

import re
from pathlib import Path

from nbench import Config, Derive, GoogleBenchmark, Metric, Python, Variant
from spec import footprint
from spec.generator import driver

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build" / "nbench" / "footprint"
SCENARIOS = footprint.scenarios()
# A program's results are kept by what it is (compiler and preprocessed
# source), so a tree measured once is not compiled again.
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


def measure(case, variant):
    """A scenario compiled to an object: its sections and functions, what
    the compiler took, and where it spent its work."""
    source, added = SCENARIOS[case]
    found = driver.measure(variant.values["compiler"], flags(variant, added),
                           work(variant), case.replace("/", "__"), source)
    if found.get("returncode"):
        return {"error": driver.first_error(found.get("stderr", ""))}
    return {k: v for k, v in found.items() if k not in ("returncode",)}


RAN = [f"{c}/{s}" for c in ("runtime", "static") for s in footprint.SHAPES]


def ran(case, variant):
    """The instructions of each operation of a container and shape, counted
    by the program itself, optimized."""
    container, shape = case.split("/")
    records, stderr, crash = driver.build_and_run(
        variant.values["compiler"], flags(variant, footprint.OPTIMIZED),
        work(variant), f"run__{container}__{shape}",
        footprint.performance_program(container, shape))
    if stderr or crash:
        return {"error": driver.first_error(stderr) if stderr else crash}
    return {"instructions": {r["operation"]: r["instructions"]
                             for r in records}}


def unflatten(metrics):
    nested = {}
    for key, value in metrics.items():
        *path, leaf = key.split("/")
        place = nested
        for part in path:
            place = place.setdefault(part, {})
        place[leaf] = value
    return nested


def costs(results, variant):
    """footprint.costs of the measured scenarios: a container, a first and
    a further registration and a request type, in code bytes, compiler
    instructions and memory."""
    measured = {case: unflatten(m)
                for case, m in results.get("footprint", {}).items()}
    return footprint.costs(measured)


config = Config(
    name="dingo",
    variant=checkout,
    reference="this",
    matrix={"compiler": ["g++", "clang++"]},
    repetitions=1,
    suites=[
        Python("footprint", list(SCENARIOS), measure,
               title="Footprint", primary="object/sections/code",
               description="Each scenario compiled to an object with the "
                           "variant's headers (spec/footprint.py)."),
        Python("ran", RAN, ran, title="Instructions run",
               primary=None, requires=driver.counts_instructions,
               description="The instructions of each operation of two "
                           "registrations, counted by the program."),
        GoogleBenchmark("containers", "{build}/benchmark/dingo_benchmark",
                        title="Container benchmarks", repetitions=3,
                        variants=lambda v: v.cell["compiler"] == "g++"),
    ],
    metrics=[
        Metric("object/*", "object", "B", noise=0, section="memory"),
        Metric("instructions/*", "instructions run", noise=0, abs_tol=2,
               section="cpu"),
        Metric("instructions", "Compiler instructions", noise=0.01,
               section="cpu"),
        Metric("memory", "Compiler peak memory", "KiB", noise=0.03,
               section="memory"),
        Metric("cpu", "Compiler CPU", "s", noise=0.2, section="time"),
        Metric("drilldown/*/milliseconds", noise=0.2, section="other"),
        Metric("drilldown/*/seconds", noise=0.2, section="other"),
        Metric("drilldown/*", noise=0, section="other"),
        Metric("code", "Code", "B", noise=0, section="memory"),
    ],
    derive=[Derive("costs", costs, title="Costs",
                   primary="code",
                   description="Differences of the footprint scenarios: "
                               "code bytes, compiler instructions and memory "
                               "(KiB) of a container, a registration and a "
                               "request type.")],
)
