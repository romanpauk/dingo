"""dingo for nbench: checkouts and compilers are the variants.

    cmake --build build -t benchmark      (DINGO_BENCHMARK_ARGUMENTS)
    uv run --with-editable tools/nbench nbench run -C benchmark/nbench_conf.py \\
        -v this=. -v master=build/master
    ... -s 'release|release-costs' --baseline latest   what changed since the last run

A case is one measured thing, named by its parameters; a metric is a
quantity of it. Every suite measures the scenarios of spec/footprint.py, the
same programs of each container (runtime and static) and shape, compiled as
a release build compiles them. Of the compiler:

  release     the code, constants, data and unwind bytes and functions of a
              scenario's object; the compiler's instructions, peak memory and
              CPU time
  release-costs  their differences: a container, a first and a further
              registration, a request type
  callers     the code another place that resolves adds
  headers     the code of a scenario by the header it is defined in
  compiler    where the compiler spent its work in a scenario: a phase (gcc),
              an event or the instantiations of an area of the library
              (clang); time, memory collected, count
  probes      the code-generation probes: code bytes of each

Of what runs:

  run         what a container runs, of nbench/bench.hpp, a runtime
              container of the memory of an arena:
              constructing an empty container, registering, and the first
              resolution of a request type and those after it; time,
              instructions, cycles and cache misses per operation. With it,
              the transactions of benchmark/transaction.cpp and the resolvers
              written by hand of benchmark/reference.cpp

Every variant runs in each compiler of the matrix (this[g++], this[clang++])
and is compared with the reference of its own compiler. git:REF is the
commit's tree, extracted into the cache. Sizes and counts are exact, so any
change of them is reported; instructions within 1%, memory within 3% and
times within 20%.
"""

import hashlib
import json
import re
import subprocess
import threading
from pathlib import Path

from nbench import Config, Derive, Metric, Native, Python, Variant
from spec import footprint
from spec.generator import driver

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build" / "nbench" / "footprint"
SCENARIOS = footprint.scenarios()
# A program's results are kept by what it is (compiler and preprocessed
# source), so a scenario of a tree is compiled once, for every suite. Runs of
# several trees share the cache, so what no run used for two weeks goes.
KEPT_DAYS = 14
driver.open_cache(WORK / "programs")
driver.prune_unused(WORK / "programs", KEPT_DAYS)
# Eight compilers at once, each within the driver's memory limit: within
# 8 GB, as many as the machine the measurements run on holds beside them.
driver.set_jobs(8)


def checkout(name, value):
    """-v NAME=. is the working tree; git:REF the commit's tree ({tree},
    which its worktree step makes); any other VALUE a tree, such as a
    worktree of the main branch."""
    tree = ROOT if value in ("", ".") else Path(value).resolve()
    return Variant(name, tree=str(tree))


def work(variant):
    path = WORK / "work" / re.sub(r"\W", "_", variant.name)
    path.mkdir(parents=True, exist_ok=True)
    return path


def spelling(variant):
    """The flags that spell a scenario in a tree that names its runtime
    container traits as the main branch did, dynamic_container_traits."""
    tree = Path(variant.values["tree"])
    traits = tree / "include" / "dingo" / "runtime" / "container_traits.h"
    if traits.exists() and \
            "struct dynamic_container_traits" in traits.read_text():
        return ["-Druntime_container_traits=dynamic_container_traits"]
    return []


def flags(variant, *more):
    """The flags of a scenario of a variant: a release build's
    (footprint.OPTIMIZED), with its headers."""
    return ["-std=c++17", f"-I{variant.values['tree']}/include",
            *spelling(variant), *footprint.OPTIMIZED, *more]


def compiled(scenario, variant):
    """driver.measure of a scenario, or an error."""
    source, _ = SCENARIOS[scenario]
    found = driver.measure(variant.values["compiler"], flags(variant),
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


def costs(results, variant, suite="release"):
    """footprint.costs of the scenarios a suite measured."""
    measured = {case: {"object": {"sections": {"code": m.get("code")}},
                       "instructions": m.get("instructions"),
                       "memory": m.get("memory"), "cpu": m.get("cpu")}
                for case, m in results.get(suite, {}).items()}
    return footprint.costs(measured)


# The scenarios whose code is attributed: one registration of each container,
# and five of each shape resolved by every request.
ATTRIBUTED = [name for name in footprint.scenario_names()
              if re.fullmatch(r"\w+/shared/1/0", name) or
              re.fullmatch(r"\w+/\w+/5/[1-9]", name)]


def attribution(scenario, variant):
    """The code of a scenario, by the header of the library
    each function is defined in, by the debug information: the function of
    the scenario, probe(), is the caller, which the library inlines into;
    the standard library and the rest are apart. It is kept by what the
    program is, as the results of the driver are."""
    source, _ = SCENARIOS[scenario]
    directory = work(variant)
    name = scenario.replace("/", "__") + "__attributed"
    cpp, obj = directory / f"{name}.cpp", directory / f"{name}.o"
    cpp.write_text(source)
    chosen = flags(variant, "-g")
    key = driver.program_key(variant.values["compiler"], chosen, directory,
                             name, cpp)
    kept = driver.load_result(key) if key is not None else None
    if kept is not None:
        return kept
    compiled = subprocess.run(
        [variant.values["compiler"], *chosen, "-c", str(cpp), "-o", str(obj)],
        capture_output=True, text=True)
    if compiled.returncode != 0:
        return {"error": driver.first_error(compiled.stderr)}
    symbols = subprocess.run(["nm", "-S", "-l", str(obj)], capture_output=True,
                             text=True).stdout
    obj.unlink(missing_ok=True)
    found = {}
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) < 4 or fields[2] not in "tTwW":
            continue
        size = int(fields[1], 16)
        if fields[3].startswith("_Z5probev"):
            where = "caller"
        elif "include/dingo/" in line:
            where = line.rsplit("include/dingo/", 1)[1].split(":")[0]
        elif "/include/c++/" in line:
            where = "std"
        else:
            where = "other"
        found[where] = found.get(where, 0) + size
    result = {"cases": {f"{scenario}/{where}": {"code": size}
                        for where, size in found.items()}}
    if key is not None:
        driver.store_result(key, result)
    return result


def callers(scenario, variant):
    """The code a place that resolves adds: of the scenario with each of its
    requests resolved twice, less the scenario, by the requests. A
    function called once is inlined into its caller, so the size of the
    caller is not what another place pays."""
    source, _ = SCENARIOS[scenario]
    twice = re.sub(r"(?m)^(\s*\(void\)c\.resolve<.*)$", r"\1\n\1", source)
    requests = len(re.findall(r"(?m)^\s*\(void\)c\.resolve<", source))
    if not requests:
        return {"error": "no request"}
    code = []
    for name, text in ((scenario, source), (scenario + "/twice", twice)):
        found = driver.measure(variant.values["compiler"],
                               flags(variant), work(variant),
                               name.replace("/", "__") + "__callers", text)
        if found.get("returncode"):
            return {"error": driver.first_error(found.get("stderr", ""))}
        code.append(found.get("object", {}).get("sections", {}).get("code"))
    return {"code": round((code[1] - code[0]) / requests)}


# The sources of the run suite besides the scenarios, and the one that holds
# the harness's main.
BENCHMARKS = [ROOT / "benchmark" / "transaction.cpp",
              ROOT / "benchmark" / "reference.cpp"]
MAIN = "#include <nbench/bench.hpp>\nNBENCH_MAIN();\n"
# The objects of the run suite, by what the preprocessor makes of their
# source, which every variant and run shares.
OBJECTS = WORK / "objects"
driver.prune_unused(OBJECTS, KEPT_DAYS)
RUNNERS = {}
RUNNERS_LOCK = threading.Lock()
# The compilers that build the programs of the run suite at once, as many
# as of the other suites.
RUN_COMPILES = threading.BoundedSemaphore(8)


def run_flags(variant):
    """The flags of the run suite: a scenario's, with the harness."""
    return flags(variant, f"-I{ROOT}/tools/nbench/include")


def run_object(compiler, run_flags_, directory, name, source):
    """The object of a source of the run suite, kept by its compiler, its
    flags and what the preprocessor makes of it, or the error that compiling
    it ended with."""
    cpp = directory / f"{re.sub(r'[^A-Za-z0-9_]', '_', name)}.cpp"
    cpp.write_text(source)
    with RUN_COMPILES:
        expanded = subprocess.run(driver.compiler_command(
            compiler, *run_flags_, "-E", "-P", str(cpp)), capture_output=True)
    if expanded.returncode != 0:
        return None, driver.first_error(expanded.stderr.decode())
    key = hashlib.sha256(json.dumps(
        [driver.compiler_identity(compiler),
         [flag for flag in run_flags_ if not flag.startswith("-I")],
         hashlib.sha256(expanded.stdout).hexdigest()]).encode()).hexdigest()
    obj = OBJECTS / key[:2] / f"{key}.o"
    if obj.exists():
        obj.touch()
        return obj, None
    obj.parent.mkdir(parents=True, exist_ok=True)
    made = obj.with_suffix(f".{threading.get_ident()}.o")
    with RUN_COMPILES:
        returncode, stderr = driver.run_compiler(driver.compiler_command(
            compiler, *run_flags_, "-c", str(cpp), "-o", str(made)))
    if returncode != 0:
        made.unlink(missing_ok=True)
        return None, driver.first_error(stderr)
    made.replace(obj)
    return obj, None


def runner(variant):
    """The program of the run suite of a variant: the cases of every scenario
    and of BENCHMARKS, linked into one. A source the variant's headers do not
    compile has no case in it; its error is in compile.log beside it."""
    with RUNNERS_LOCK:
        if variant.name in RUNNERS:
            return RUNNERS[variant.name]
    compiler = variant.values["compiler"]
    directory = work(variant) / "run"
    directory.mkdir(exist_ok=True)
    chosen = run_flags(variant)
    sources = {name: footprint.run_source(name)
               for name in footprint.scenario_names()}
    sources = {name: source for name, source in sources.items() if source}
    sources.update({path.stem: path.read_text() for path in BENCHMARKS})
    sources["main"] = MAIN
    built = driver.concurrently(
        [lambda name=name, source=source: run_object(
            compiler, chosen, directory, name, source)
         for name, source in sources.items()])
    failed = {name: error for name, (obj, error) in zip(sources, built)
              if obj is None}
    (directory / "compile.log").write_text(
        "".join(f"{name}: {error}\n" for name, error in failed.items()))
    if "main" in failed:
        raise RuntimeError(f"the harness does not compile: {failed['main']}")
    program = directory / "runner"
    linked = subprocess.run(
        [compiler, "-pthread", "-o", str(program),
         *[str(obj) for obj, _ in built if obj is not None]],
        capture_output=True, text=True)
    if linked.returncode != 0:
        raise RuntimeError("the run suite does not link: " +
                           driver.first_error(linked.stderr))
    with RUNNERS_LOCK:
        RUNNERS[variant.name] = str(program)
    return str(program)


def cost_params(case):
    """runtime/unique: first registration -> container, shape, part."""
    if ":" not in case:
        container, part = case.split(" ", 1)
        return {"container": container, "part": part}
    where, part = case.split(": ", 1)
    container, shape = where.split("/")
    return {"container": container, "shape": shape, "part": part}


RUN_CASE = re.compile(
    r"^(?P<container>\w+)/(?:empty|(?P<shape>\w+)/(?P<registrations>\d+)/"
    r"(?P<requests>\d+))/(?P<operation>.+)$")


def run_params(case):
    """What a case of the run suite is, from its name: of a scenario, its
    container, shape, registrations, requests and operation; of
    benchmark/*.cpp, the file and what it measures."""
    found = RUN_CASE.match(case)
    if found and found["container"] in footprint.CONTAINERS:
        return {k: v for k, v in found.groupdict().items() if v is not None}
    head, _, rest = case.partition("/")
    return {"benchmark": head, "operation": rest}


SCENARIO = r"^(?P<scenario>headers/\w+|\w+/empty|codegen|(?P<container>\w+)/(?P<shape>\w+)/(?P<registrations>\d+)/(?P<requests>\d+))"
BYTES = dict(unit="B", noise=0, section="memory")
COST_METRICS = [Metric("code", "Code", **BYTES),
                Metric("instructions", "Compiler instructions", noise=0.01,
                       section="cpu"),
                Metric("memory", "Compiler peak memory", "KiB", noise=0.03,
                       section="memory"),
                Metric("cpu", "Compiler CPU", "s", noise=0.2, abs_tol=0.05,
                       section="time")]
OBJECT_METRICS = [Metric("code", "Code", **BYTES),
                  Metric("constants", "Constants", **BYTES),
                  Metric("data", "Data", **BYTES),
                  Metric("unwind", "Unwind tables", **BYTES),
                  Metric("functions", "Functions", noise=0, section="memory"),
                  Metric("instructions", "Compiler instructions", noise=0.01,
                         section="cpu"),
                  Metric("memory", "Compiler peak memory", "KiB", noise=0.03,
                         section="memory"),
                  Metric("cpu", "Compiler CPU", "s", noise=0.2,
                         section="time")]

config = Config(
    name="dingo",
    variant=checkout,
    reference="this",
    matrix={"compiler": ["g++", "clang++"]},
    repetitions=1,
    suites=[
        Python("release", [s for s in SCENARIOS if s != "codegen"], measure,
               title="Release footprint", primary="code", params=SCENARIO,
               description="Each scenario of spec/footprint.py compiled with "
                           "the variant's headers, as a release build "
                           "compiles it.",
               metrics=OBJECT_METRICS),
        Python("callers", [s for s in SCENARIOS
                           if s.count("/") == 3 and not s.endswith("/0")],
               callers, title="Code of a place that resolves",
               primary="code", params=SCENARIO,
               description="The code another place that resolves a request "
                           "adds: of each request of a scenario "
                           "resolved twice, less once, by the requests.",
               metrics=[Metric("code", "Code", **BYTES)]),
        Python("headers", ATTRIBUTED, attribution, title="Code by header",
               primary="code",
               params=r"^(?P<scenario>[a-z]+/\w+/\d+/\d+)/(?P<header>.+)$",
               description="The code of a scenario by the header of "
                           "the library each function is defined in; caller "
                           "is the function of the scenario, std the "
                           "standard library.",
               metrics=[Metric("code", "Code", **BYTES)]),
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
        Native("run", runner, title="Run", min_time="0.05s", stable="2%",
               repetitions=1,
               params=run_params,
               description="What each scenario runs, a runtime container "
                           "of the memory of an arena: "
                           "constructing an empty container, registering "
                           "into a new one, and the first resolution of a "
                           "request type of each registration, in a new "
                           "container, and those after it; with the "
                           "transactions of benchmark/transaction.cpp and "
                           "the resolvers written by hand of "
                           "benchmark/reference.cpp.",
               metrics=[Metric("instructions", "Instructions", "/op",
                               noise=0.01, section="cpu")]),
    ],
    derive=[Derive("release-costs", costs, title="Release costs",
                   primary="code", params=cost_params,
                   description="The differences of the release footprint: a "
                               "container, a first and a further "
                               "registration and a request type.",
                   metrics=COST_METRICS)],
)
