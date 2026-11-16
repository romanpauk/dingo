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

The measurements are kept in the cache of the programs, by what each program
is, and the last of each scenario for each compiler beside them: a run tells
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

# The shapes a registration and its requests are of: what it registers, and
# what each request asks for, of a registration of the index i.
SHAPES = {
    "shared": ("dingo::scope<dingo::shared>, "
               "dingo::storage<std::shared_ptr<T<{i}>>>",
               ["T<{i}> &", "std::shared_ptr<T<{i}>>"]),
    "unique": ("dingo::scope<dingo::unique>, dingo::storage<T<{i}>>",
               ["T<{i}>", "std::unique_ptr<T<{i}>>"]),
    "interface": ("dingo::scope<dingo::shared>, "
                  "dingo::storage<std::shared_ptr<U<{i}>>>, "
                  "dingo::interfaces<I<{i}>>",
                  ["I<{i}> &", "std::shared_ptr<I<{i}>>"]),
}


def runtime_program(shape, registrations, requested):
    """A runtime container of registrations of a shape, each requested by
    the first requested of its requests."""
    registered, requests = SHAPES[shape]
    lines = [HEADERS.format(container="runtime_container"), LEAF,
             "void probe() {", "  dingo::container<> c;"]
    for index in range(registrations):
        lines.append(f"  c.register_type<{registered.format(i=index)}>();")
    for index in range(registrations):
        for request in requests[:requested]:
            lines.append(f"  (void)c.resolve<{request.format(i=index)}>();")
    lines.append("}")
    return "\n".join(lines) + "\n"


def static_program(shape, registrations, requested):
    """A static container of registrations of a shape, each requested by the
    first requested of its requests."""
    registered, requests = SHAPES[shape]
    binds = ", ".join(f"dingo::bind<{registered.format(i=index)}>"
                      for index in range(registrations))
    lines = [HEADERS.format(container="static_container"), LEAF,
             f"using bound = dingo::bindings<{binds}>;",
             "void probe() {", "  dingo::static_container<bound> c;"]
    for index in range(registrations):
        for request in requests[:requested]:
            lines.append(f"  (void)c.resolve<{request.format(i=index)}>();")
    lines.append("}")
    return "\n".join(lines) + "\n"


def scenarios():
    """The scenarios by name: their source, and the flags they add."""
    found = {
        "headers/runtime": (HEADERS.format(container="runtime_container"), []),
        "headers/static": (HEADERS.format(container="static_container"), []),
        "runtime/empty": (HEADERS.format(container="runtime_container") +
                          "int probe() { dingo::container<> c; return 0; }\n",
                          []),
        "codegen": (CODEGEN.read_text(), OPTIMIZED),
    }
    for shape in SHAPES:
        for registrations in (1, 5):
            for requested in (0, 2):
                found[f"runtime/{shape}/{registrations}/{requested}"] = (
                    runtime_program(shape, registrations, requested), [])
                found[f"static/{shape}/{registrations}/{requested}"] = (
                    static_program(shape, registrations, requested), [])
    return found


def metric(measured, *path):
    """A metric of a measurement, or None."""
    for key in path:
        if not isinstance(measured, dict) or key not in measured:
            return None
        measured = measured[key]
    return measured


def costs(measured):
    """The costs of the parts of the library, of the differences of the
    scenarios: code bytes and compiler instructions."""
    found = {}

    def difference(what, later, earlier, count=1):
        for name, path in (("code", ("object", "sections", "code")),
                           ("instructions", ("instructions",)),
                           ("memory", ("memory",))):
            a = metric(measured.get(earlier), *path)
            b = metric(measured.get(later), *path)
            if a is not None and b is not None:
                found.setdefault(what, {})[name] = round((b - a) / count)

    difference("runtime container", "runtime/empty", "headers/runtime")
    for container in ("runtime", "static"):
        for shape in SHAPES:
            base = f"{container}/{shape}"
            difference(f"{base}: first registration", f"{base}/1/0",
                       "runtime/empty" if container == "runtime"
                       else "headers/static")
            difference(f"{base}: further registration", f"{base}/5/0",
                       f"{base}/1/0", 4)
            difference(f"{base}: request type", f"{base}/5/2", f"{base}/5/0",
                       10)
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
        print(f"  {name}: {driver.first_error(measured[name]['stderr'])}")
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
