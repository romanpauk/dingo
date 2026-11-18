"""Measures what the library costs a compiler and the code it makes it emit.

Each scenario is a program of the library alone: its headers, a container,
registrations and requests of a few shapes, and the code of resolutions the
optimizer compiles. Each is compiled to an object, and measured:

- the code, the constants and the unwind tables of the object, and its
  functions
- the peak memory and the processor time of the compiler, and the
  instructions it ran, where the system counts them
- where the compiler spent its work, as it reports it: GCC the time and the
  collected memory of each of its phases, clang the events of its work and
  the templates it instantiated by the area of the library they are of

Differences of the scenarios give the costs of the parts of the library: of a
file that includes it, of a container, of a registration and of a request
type, in each container.

Each scenario is also a set of cases of nbench/bench.hpp (run_source), which
the run suite of benchmark/nbench_conf.py measures: constructing a container,
registering, and the first resolution of a request type and those after it.

The measurements are kept in the cache of the programs, by what each program
is, and the last of each scenario for each compiler beside them: a run reports
what changed since the one before, and the cache of one run, such as that of
a main branch, is the baseline of another.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import sys

# The root, which the specification is a package of.
sys.path.insert(0, str(Path(__file__).parents[1]))

from spec.generator import driver  # noqa: E402

ROOT = Path(__file__).parents[1]

# The flags that compile the code the library compiles to, without the
# hardening a distribution turns on by default, which adds checks no
# scenario measures.
OPTIMIZED = ["-O2", "-fno-stack-protector", "-fcf-protection=none",
             "-fno-stack-clash-protection", "-U_FORTIFY_SOURCE",
             "-D_FORTIFY_SOURCE=0", "-U_GLIBCXX_ASSERTIONS"]

# The resolutions whose optimized code is measured.
CODEGEN = ROOT / "test" / "lit" / "codegen-static-probes.cpp"

LEAF = """template <int I> struct T {
  DINGO_CONSTRUCTOR(T()) {}
  int value = I;
};
template <int N> struct I {
  virtual ~I() = default;
  virtual int get() const = 0;
};
template <int N> struct U : I<N> {
  DINGO_CONSTRUCTOR(U()) {}
  int get() const override { return N; }
};
"""

HEADERS = """#include <dingo/registration/constructor.h>
#include <dingo/{container}.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <memory>
#include <vector>
"""


class Shape:
    """Registrations of one shape and their requests, of a registration of
    the index i.

    binds      the arguments of each bind<> (static) or register_type<>
               (runtime) a registration makes
    requests   the types its requests name
    includes   the headers it includes besides HEADERS
    types      what it declares besides LEAF
    argument   the argument of a runtime registration, or none
    keyed      whether a registration is under the key i: a static binding
               names it, a runtime registration and request pass it
    lookup     the lookups the traits of the container define, or none
    containers the containers that hold it"""

    def __init__(self, binds, requests, includes="", types="", argument="",
                 keyed=False, lookup="",
                 containers=("runtime", "arena", "static")):
        self.binds, self.requests = binds, requests
        self.includes, self.types = includes, types
        self.argument, self.keyed, self.lookup = argument, keyed, lookup
        self.containers = containers


SHAPES = {
    "shared": Shape(["dingo::scope<dingo::shared>, "
                     "dingo::storage<std::shared_ptr<T<{i}>>>"],
                    ["T<{i}> &", "std::shared_ptr<T<{i}>>",
                     "std::shared_ptr<T<{i}>> &"]),
    "unique": Shape(["dingo::scope<dingo::unique>, dingo::storage<T<{i}>>"],
                    ["T<{i}>", "std::unique_ptr<T<{i}>>"]),
    "interface": Shape(["dingo::scope<dingo::shared>, "
                        "dingo::storage<std::shared_ptr<U<{i}>>>, "
                        "dingo::interfaces<I<{i}>>"],
                       ["I<{i}> &", "std::shared_ptr<I<{i}>>",
                        "std::shared_ptr<I<{i}>> &"]),
    # A value that owns memory, which a unique request moves out of what the
    # container made, and destroys.
    "string": Shape(["dingo::scope<dingo::unique>, dingo::storage<S<{i}>>"],
                    ["S<{i}>", "std::unique_ptr<S<{i}>>"],
                    includes="#include <string>\n",
                    types="template <int I> struct S {\n"
                          "  DINGO_CONSTRUCTOR(S()) {}\n"
                          "  std::string value = \"longer than a string "
                          "keeps in place\";\n"
                          "};\n"),
    # An object the code of the program owns. A static container holds no
    # object made before it.
    "external": Shape(["dingo::scope<dingo::external>, "
                       "dingo::storage<T<{i}> &>"],
                      ["T<{i}> &", "T<{i}>"],
                      includes="#include <dingo/storage/external.h>\n",
                      types="template <int I> T<I> owned;\n",
                      argument="owned<{i}>",
                      containers=("runtime", "arena")),
    # A shared object constructed of another shared object.
    "dependency": Shape(["dingo::scope<dingo::shared>, dingo::storage<T<{i}>>",
                         "dingo::scope<dingo::shared>, dingo::storage<D<{i}>>"],
                        ["D<{i}> &", "D<{i}> *"],
                        types="template <int I> struct D {\n"
                              "  DINGO_CONSTRUCTOR(D(T<I> &leaf)) : leaf(leaf) "
                              "{}\n"
                              "  T<I> &leaf;\n"
                              "};\n"),
    # Implementations of one interface under keys, which a request selects.
    "indexed": Shape(["dingo::scope<dingo::shared>, "
                      "dingo::storage<std::shared_ptr<Q<{i}>>>, "
                      "dingo::interfaces<P>"],
                     ["P &", "std::shared_ptr<P>", "std::shared_ptr<P> &"],
                     includes="#include <cstddef>\n",
                     types="struct P {\n"
                           "  virtual ~P() = default;\n"
                           "  virtual int get() const = 0;\n"
                           "};\n"
                           "template <int N> struct Q : P {\n"
                           "  DINGO_CONSTRUCTOR(Q()) {}\n"
                           "  int get() const override { return N; }\n"
                           "};\n",
                     keyed=True,
                     lookup="dingo::lookups<dingo::associative<std::size_t, "
                            "P>>"),
}

# The containers: the header of each, and whether it is the runtime
# container, which the program registers into, or the static one, whose
# bindings are its type. The arena container is the runtime one, of the
# memory of an arena the program gives it.
CONTAINERS = {"runtime": "runtime_container", "arena": "runtime_container",
              "static": "static_container"}
# The bytes of the arena of a container of an arena container.
ARENA = 16384


def numbered(text, index):
    """A spelling of a registration of the index i, of the index."""
    return text.replace("{i}", str(index))


def family(container):
    return "static" if container == "static" else "runtime"


class Program:
    """A scenario as code: what it declares, how a function makes its
    container c, what registers into it, and each request of each
    registration, as an expression."""

    def __init__(self, container, shape=None, registrations=0):
        found = SHAPES[shape] if shape else Shape([], [])
        self.container = container
        includes = [HEADERS.format(container=CONTAINERS[container])]
        if container == "arena":
            includes.append("#include <dingo/memory/arena_allocator.h>\n")
        includes.append(found.includes)
        self.includes = "".join(includes)
        lines = [LEAF, found.types]
        traits = f"dingo::{family(container)}_container_traits"
        if found.lookup:
            lines.append(f"struct traits : {traits} {{\n"
                         f"  using lookup_definition_type = {found.lookup};\n"
                         "};\n")
            traits = "traits"
        if container == "static":
            key = (", dingo::key_type<std::size_t, {i}>" if found.keyed
                   else "")
            binds = ", ".join(numbered(f"dingo::bind<{bind}{key}>", index)
                              for index in range(registrations)
                              for bind in found.binds)
            lines.append(f"using bound = dingo::bindings<{binds}>;")
            self.type = (f"dingo::static_container<bound, {traits}>"
                         if found.lookup else
                         "dingo::static_container<bound>")
        elif container == "arena":
            self.type = (f"dingo::container<{traits}, "
                         "dingo::arena_allocator<char>>")
        else:
            self.type = ("dingo::container<traits>" if found.lookup else
                         "dingo::container<>")
        self.types = "\n".join(line for line in lines if line)
        argument = ("dingo::key_value{std::size_t({i})}" if found.keyed
                    else found.argument)
        self.registers = [] if container == "static" else [
            numbered(f"c.register_type<{bind}>({argument})", index)
            for index in range(registrations) for bind in found.binds]
        self.requests = []
        for request in found.requests:
            if found.keyed and container == "static":
                spelled = [f"c.resolve<dingo::dependency<{request}, "
                           f"dingo::key_type<std::size_t, {index}>>>()"
                           for index in range(registrations)]
            elif found.keyed:
                spelled = [f"c.resolve<{request}>(std::size_t({index}))"
                           for index in range(registrations)]
            else:
                spelled = [f"c.resolve<{numbered(request, index)}>()"
                           for index in range(registrations)]
            self.requests.append((request.replace("{i}", "i"), spelled))

    def made(self, indent):
        """The lines that make the container c in a function."""
        if self.container != "arena":
            return [f"{indent}{self.type} c;"]
        return [f"{indent}unsigned char buffer[{ARENA}];",
                f"{indent}dingo::arena<> arena(buffer);",
                f"{indent}dingo::arena_allocator<char> allocator(arena);",
                f"{indent}{self.type} c(allocator);"]

    def compiled(self, requested):
        """The program the compile suites measure: a function that makes the
        container, registers into it and resolves the first requested of the
        requests of each registration."""
        lines = [self.includes + self.types, "void probe() {",
                 *self.made("  ")]
        lines += [f"  {register};" for register in self.registers]
        count = len(self.requests[0][1]) if self.requests else 0
        for index in range(count):
            for _, spelled in self.requests[:requested]:
                lines.append(f"  (void){spelled[index]};")
        lines.append("}")
        return "\n".join(lines) + "\n"

    def run(self, scenario, requested):
        """The cases the run suite measures, of nbench/bench.hpp, in a
        namespace of the scenario: constructing an empty container;
        registering into a new container; the first resolution of a request
        of each registration, in a new container; and the resolutions after
        it. Those of a new container are of a batch of containers made, and
        destroyed, apart from what is measured, a fixed number of times: the
        instructions of each are the same, and a time of a new container is
        of its first use, which repeating does not make steadier."""
        space = "scenario_" + re.sub(r"\W", "_", scenario)
        lines = ["#include <nbench/bench.hpp>", self.includes,
                 f"namespace {space} {{", self.types, "struct instance {"]
        if self.container == "arena":
            lines += [f"  unsigned char buffer[{ARENA}];",
                      "  dingo::arena<> arena{buffer};",
                      "  dingo::arena_allocator<char> allocator{arena};",
                      f"  {self.type} c{{allocator}};"]
        else:
            lines.append(f"  {self.type} c;")
        lines += ["  void populate() {",
                  *[f"    {register};" for register in self.registers],
                  "  }", "};", "constexpr int batch = 64;",
                  "constexpr nbench::iterations batches{16};",
                  "using batch_type = std::unique_ptr<instance>[batch];"]
        cases = []
        if not self.registers and not self.requests:
            lines += ["void construct(nbench::State &s) {",
                      "  while (s.keep_running()) {",
                      "    instance made;", "    nbench::keep(made);",
                      "  }", "}"]
            cases.append(("construct", "construct"))
        elif self.registers and not requested:
            lines += ["void registers(nbench::State &s) {",
                      "  batch_type made;",
                      "  while (s.keep_running()) {",
                      "    s.pause();",
                      "    for (auto &each : made) {",
                      "      each = std::make_unique<instance>();", "    }",
                      "    s.resume();",
                      "    for (auto &each : made) {",
                      "      each->populate();", "      nbench::keep(each->c);",
                      "    }",
                      "    s.pause();",
                      "    for (auto &each : made) {", "      each.reset();",
                      "    }",
                      "    s.resume();", "  }",
                      f"  s.operations(batch * {len(self.registers)});", "}"]
            cases.append(("registers", "register"))
        for number, (request, spelled) in enumerate(self.requests[:requested]):
            resolved = [f"nbench::keep({each.replace('c.', 'each->c.', 1)});"
                        for each in spelled]
            lines += [f"void first_{number}(nbench::State &s) {{",
                      "  batch_type made;",
                      "  while (s.keep_running()) {",
                      "    s.pause();",
                      "    for (auto &each : made) {",
                      "      each = std::make_unique<instance>();",
                      "      each->populate();", "    }",
                      "    s.resume();",
                      "    for (auto &each : made) {",
                      *[f"      {line}" for line in resolved], "    }",
                      "    s.pause();",
                      "    for (auto &each : made) {", "      each.reset();",
                      "    }",
                      "    s.resume();", "  }",
                      f"  s.operations(batch * {len(spelled)});", "}"]
            warm = [f"nbench::keep({each.replace('c.', 'made.c.', 1)});"
                    for each in spelled]
            lines += [f"void again_{number}(nbench::State &s) {{",
                      "  instance made;", "  made.populate();",
                      *[f"  {line}" for line in warm],
                      "  while (s.keep_running()) {",
                      *[f"    {line}" for line in warm], "  }",
                      f"  s.operations({len(spelled)});", "}"]
            cases += [(f"first_{number}", f"resolve first {request}"),
                      (f"again_{number}", f"resolve {request}")]
        for function, name in cases:
            fixed = ", batches" if function != "construct" and \
                not function.startswith("again") else ""
            lines.append(f'NBENCH({function}, "{scenario}/{name}"{fixed});')
        lines.append("}")
        return "\n".join(lines) + "\n"


def scenario_names():
    """Every scenario: the headers of each family, an empty container of each
    container, and the registrations of each shape a container holds, 1 or
    5, resolved by none of the requests of the shape or by each."""
    names = ["headers/runtime", "headers/static"]
    for container in CONTAINERS:
        names.append(f"{container}/empty")
        for shape, found in SHAPES.items():
            if container in found.containers:
                names += [f"{container}/{shape}/{registrations}/{requested}"
                          for registrations in (1, 5)
                          for requested in (0, len(found.requests))]
    return names


def program(scenario):
    """The program of a scenario and the number of its requests of each
    registration, or None for the headers alone."""
    parts = scenario.split("/")
    if parts[0] == "headers":
        return None, 0
    if parts[1] == "empty":
        return Program(parts[0]), 0
    container, shape, registrations, requested = parts
    return Program(container, shape, int(registrations)), int(requested)


def scenarios():
    """The scenarios by name: their source, and the flags they add."""
    found = {"codegen": (CODEGEN.read_text(), OPTIMIZED)}
    for name in scenario_names():
        made, requested = program(name)
        if made is None:
            container = name.split("/")[1]
            found[name] = (HEADERS.format(container=CONTAINERS[container]), [])
        else:
            found[name] = (made.compiled(requested), [])
    return found


def run_source(scenario):
    """The source of the cases the run suite measures of a scenario, or None:
    of an empty container, and of five registrations; not of the headers
    alone, nor of one registration, which runs as one of five does."""
    made, requested = program(scenario)
    if made is None or re.search(r"/1/\d+$", scenario):
        return None
    return made.run(scenario, requested)


def metric(measured, *path):
    """A metric of a measurement, or None."""
    for key in path:
        if not isinstance(measured, dict) or key not in measured:
            return None
        measured = measured[key]
    return measured


def costs(measured):
    """The costs of the parts of the library, of the differences of the
    scenarios: code bytes, compiler instructions, peak memory and processor
    seconds."""
    found = {}

    def difference(what, later, earlier, count=1):
        for name, path in (("code", ("object", "sections", "code")),
                           ("instructions", ("instructions",)),
                           ("memory", ("memory",)), ("cpu", ("cpu",))):
            a = metric(measured.get(earlier), *path)
            b = metric(measured.get(later), *path)
            if a is not None and b is not None:
                found.setdefault(what, {})[name] = round(
                    (b - a) / count, 2 if name == "cpu" else None)

    for container in CONTAINERS:
        difference(f"{container} container", f"{container}/empty",
                   f"headers/{family(container)}")
        for shape, found_shape in SHAPES.items():
            if container not in found_shape.containers:
                continue
            base = f"{container}/{shape}"
            requests = len(found_shape.requests)
            difference(f"{base}: first registration", f"{base}/1/0",
                       f"{container}/empty")
            difference(f"{base}: further registration", f"{base}/5/0",
                       f"{base}/1/0", 4)
            difference(f"{base}: request type", f"{base}/5/{requests}",
                       f"{base}/5/0", 5 * requests)
    return found


# The changes a metric shows between runs that are not of the program: of
# a count or a size, none; of what the processor counts, a little; of a
# time, much.
def noise(path):
    leaf = path[-1]
    if leaf in ("seconds", "milliseconds", "cpu"):
        return 0.2
    if leaf == "instructions":
        return 0.01
    if leaf == "memory":
        return 0.03
    return 0.0


def flatten(value, path=()):
    if isinstance(value, dict):
        for key, inner in value.items():
            yield from flatten(inner, (*path, key))
    elif isinstance(value, (int, float)) and not isinstance(value, bool):
        yield path, value


def changes(before, now):
    """The metrics that changed past their noise: (path, before, now)."""
    old = dict(flatten(before))
    found = []
    for path, value in flatten(now):
        if path not in old or path[-1] == "returncode":
            continue
        previous = old[path]
        if previous == value:
            continue
        if path[1] == "drilldown" and abs(value - previous) < 5:
            continue
        relative = abs(value - previous) / max(abs(previous), 1)
        if relative > noise(path):
            found.append((path, previous, value))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", type=Path,
                        help="the measurements and the costs as JSON")
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--flags", default="-std=c++17 -O0 -Iinclude",
                        help="compiler flags, with include paths")
    parser.add_argument("--jobs", type=int, default=driver.default_jobs())
    parser.add_argument("--only", default="",
                        help="measure only the scenarios whose name matches "
                        "this regular expression")
    options = parser.parse_args()
    options.work.mkdir(parents=True, exist_ok=True)
    driver.open_cache(options.work / "programs")
    driver.set_jobs(options.jobs)
    flags = shlex.split(options.flags)
    chosen = {name: scenario for name, scenario in scenarios().items()
              if re.search(options.only, name)}

    def measure(name):
        source, added = chosen[name]
        base = [flag for flag in flags
                if not (added and re.fullmatch(r"-O\d", flag))]
        return driver.measure(options.compiler, base + added, options.work,
                              name.replace("/", "__"), source)

    measured = dict(zip(chosen, driver.concurrently(
        [lambda name=name: measure(name) for name in chosen])))
    failed = [name for name, found in measured.items()
              if found["returncode"] != 0]
    found = {"measured": measured, "costs": costs(measured)}
    options.output.write_text(json.dumps(found, indent=1, sort_keys=True))

    # The last measurement of every scenario for this compiler, which the
    # next run compares with.
    identity = hashlib.sha256(json.dumps(driver.compiler_identity(
        options.compiler)).encode()).hexdigest()[:16]
    latest = options.work / "programs" / "latest" / f"{identity}.json"
    before = (json.loads(latest.read_text()) if latest.exists() else {})
    print(f"{len(measured)} scenarios, {len(failed)} do not compile")
    for name in failed:
        print(f"  {name}: " + driver.first_error(measured[name]["stderr"]))
    print("costs (code bytes, instructions, compiler memory in KiB):")
    for what, value in found["costs"].items():
        print(f"  {what:40} {value.get('code', '-'):>8} "
              f"{value.get('instructions', '-'):>14} "
              f"{value.get('memory', '-'):>8}")
    if before:
        changed = changes(before, found)
        print(f"{len(changed)} metrics changed since the last run:")
        for path, previous, value in changed[:60]:
            print(f"  {'/'.join(map(str, path))}: {previous} -> {value}")
    if not options.only:
        latest.parent.mkdir(parents=True, exist_ok=True)
        latest.write_text(json.dumps(found, sort_keys=True))
    driver.close_cache(prune=not options.only)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
