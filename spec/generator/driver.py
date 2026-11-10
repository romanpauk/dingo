"""Compiles and runs probe programs, and reads the library's answers.

A unit is a program of probes. It has a name, the cell of each probe, and
renders the program of a list of its probes from a template. The template
marks the lines of each probe, `// probe N` before them and `// probes end`
after the last, so that a compile error is told apart by probe: the
compiler names the lines of the program an error was instantiated from.
"""

import concurrent.futures
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import threading

import jinja2

from .dimensions import Outcome
from .model import spell_shape, spell_stored

# The headers of the probes, which their programs include as model/<header>.
HEADERS = Path(__file__).parents[2] / "test" / "model"

TEMPLATES = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).with_name("templates")),
    undefined=jinja2.StrictUndefined,
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)
TEMPLATES.globals.update(spell_shape=spell_shape, spell_stored=spell_stored)

PROBE_MARKER = re.compile(r"^\s*// probe (\d+)$")
END_MARKER = "// probes end"

ERROR = re.compile(r"error: (.*)")
ERROR_LIMIT = "too many errors emitted"
# GCC: "static assertion failed: message"; clang: "static assertion failed
# due to requirement 'condition': message".
ASSERTION = re.compile(
    r"^static assertion failed(?: due to requirement '.*')?:\s*(.*)$")
CRASH = "exited with"

# The memory a compiler may take, in MiB, as the size of its address space:
# the probes run as many compilers as there are processors, and a program of
# many probes would otherwise take several GiB. A program holds as many
# probes as fit in it, and one the compiler runs out of memory on anyway is
# split into programs that do.
MEMORY_LIMIT = 1024
# The share of the limit a program is sized to take, as the peak resident
# memory of its compiler: its address space is larger.
MEMORY_TARGET = 0.75
# The probes of a class a program holds before a compile of them tells how
# much memory they take, and the fewest a compile that tells it holds.
INITIAL_PROBES = 32
MEASURED_PROBES = 8
EXHAUSTED = re.compile(r"out of memory|memory exhausted|"
                       r"Cannot allocate memory|bad_alloc")
OUT_OF_MEMORY = f"error: the compiler ran out of its {MEMORY_LIMIT} MiB"


def default_jobs():
    """How many compilers run at once unless told: one per processor, as
    many as half the memory available holds at their limit."""
    available = None
    try:
        with open("/proc/meminfo") as meminfo:
            for line in meminfo:
                if line.startswith("MemAvailable:"):
                    available = int(line.split()[1]) // 1024
    except OSError:
        pass
    jobs = os.cpu_count() or 1
    if available is not None:
        jobs = min(jobs, available // 2 // MEMORY_LIMIT)
    return max(jobs, 1)


# The compilers running at once, across every program a probe makes: the
# programs a failed one is split into are compiled concurrently, and this
# bounds them all.
COMPILES = threading.BoundedSemaphore(default_jobs())


def set_jobs(jobs):
    """Sets how many compilers run at once."""
    global COMPILES
    COMPILES = threading.BoundedSemaphore(jobs)


class Budget:
    """How many probes of a class fit in one program, learned from the peak
    memory of its compiles: what a program takes without its probes, the
    least any compile took, and the most a probe of the class took above
    that. A compile that ran out of memory took at least the limit."""

    def __init__(self):
        self.lock = threading.Lock()
        self.base = None
        self.observed = {}

    def record(self, key, probes, peak):
        with self.lock:
            self.base = peak if self.base is None else min(self.base, peak)
            if probes >= MEASURED_PROBES or peak >= MEMORY_LIMIT:
                self.observed.setdefault(key, []).append((probes, peak))

    def probes(self, key):
        with self.lock:
            observed = self.observed.get(key)
            if not observed:
                return INITIAL_PROBES
            per_probe = max((peak - self.base) / probes
                            for probes, peak in observed)
            available = MEMORY_LIMIT * MEMORY_TARGET - self.base
        if per_probe <= 0:
            return max(probes for probes, _ in observed)
        return max(1, int(available / per_probe))


BUDGET = Budget()


def memory_class(unit, probes):
    """The class the memory of the probes is told by: a unit may name one,
    otherwise it is the kind of unit."""
    of = getattr(unit, "memory_class", None)
    return of(probes[0]) if of else type(unit).__name__


def split(probes, size):
    """The probes in as few parts of at most size as there are, of sizes
    that differ by one at most."""
    count = -(-len(probes) // size)
    return [probes[index * len(probes) // count:
                   (index + 1) * len(probes) // count]
            for index in range(count)]


def concurrently(calls):
    """The results of calls, made concurrently, in their order. They wait for
    a compiler of their own, so a call waiting on others holds none."""
    if len(calls) <= 1:
        return [call() for call in calls]
    with concurrent.futures.ThreadPoolExecutor(len(calls)) as executor:
        return list(executor.map(lambda call: call(), calls))


def compiler_command(compiler, *arguments):
    """The command that runs the compiler within its memory limit."""
    return ["sh", "-c", f'ulimit -v {MEMORY_LIMIT * 1024} && exec "$0" "$@"',
            compiler, *arguments]


def render(template, **context):
    return TEMPLATES.get_template(template).render(**context)


def owners_of(source, probes):
    """The probe each line of a program belongs to, by line number."""
    owners = {}
    owner = None
    for number, line in enumerate(source.splitlines(), start=1):
        marker = PROBE_MARKER.match(line)
        if marker:
            owner = probes[int(marker.group(1))]
        elif line.strip() == END_MARKER:
            owner = None
        elif owner is not None:
            owners[number] = owner
    return owners


def first_error(output):
    for line in output.splitlines():
        match = ERROR.search(line)
        if match:
            return match.group(1).strip()
    return output.strip().splitlines()[-1] if output.strip() else "unknown"


def run_compiler(command):
    """Runs a compile; returns (returncode, stderr, peak): the peak resident
    memory of the compiler in MiB, or None where it cannot be told."""
    with tempfile.TemporaryFile(mode="w+") as errors:
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                   stderr=errors, text=True)
        peak = None
        if hasattr(os, "wait4"):
            # The usage of a process holds the most that any process it
            # waited for took: the compiler proper, which the driver waits
            # for.
            _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
            peak = usage.ru_maxrss / 1024
        else:
            process.wait()
        errors.seek(0)
        return process.returncode, errors.read(), peak


def build_and_run(compiler, flags, directory, name, source, compiled=None):
    """Compiles and runs one program; returns (records, stderr, crash): the
    records it printed, the compiler output when it did not compile, and how
    it exited when it crashed. compiled is told the peak memory of the
    compile, in MiB, when it can be told."""
    cpp = directory / f"{name}.cpp"
    exe = directory / name
    cpp.write_text(source)
    with COMPILES:
        returncode, stderr, peak = run_compiler(
            compiler_command(compiler, *flags, str(cpp), "-o", str(exe)))
    exhausted = returncode != 0 and EXHAUSTED.search(stderr)
    if compiled is not None and (peak is not None or exhausted):
        compiled(MEMORY_LIMIT if exhausted else peak)
    if returncode != 0:
        if exhausted:
            # What it reported before is not all it would, and names no probe.
            return None, OUT_OF_MEMORY, None
        return None, stderr, None
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    exe.unlink()
    try:
        records = [json.loads(line) for line in run.stdout.splitlines()
                   if line]
    except json.JSONDecodeError:
        # A record the program could not print is a crash of its probe.
        return [], None, f"{CRASH} a malformed record"
    if run.returncode != 0:
        return records, None, f"{CRASH} {run.returncode}"
    return records, None, None


def failed_probes(stderr, name, owners):
    """The first error of each probe a compiler output reports, found by
    the lines of the program its instantiation context names: GCC prints the
    context before an error, clang after it, as notes. None when an error
    belongs to no probe. Clang stops at its error limit, unless it is lifted;
    the probes after it are left to the next compile."""
    reference = re.compile(re.escape(f"{name}.cpp") + r":(\d+):")
    context_follows = "note: in instantiation of" in stderr
    errors = []  # each error with the program lines of its context
    pending = []
    for line in stderr.splitlines():
        if ERROR_LIMIT in line:
            break
        match = ERROR.search(line)
        if match:
            if context_follows:
                errors.append((match.group(1).strip(), []))
            else:
                errors.append((match.group(1).strip(), pending))
                pending = []
        lines = [int(number) for number in reference.findall(line)]
        if context_follows and errors:
            errors[-1][1].extend(lines)
        elif not match:
            pending += lines
    failed = {}
    owner = None
    for error, lines in errors:
        found = next((owners[number] for number in lines if number in owners),
                     None)
        # Clang does not repeat the context of an error in the instantiation
        # of the error before it.
        if found is None and not context_follows:
            return None
        owner = found or owner
        if owner is None:
            return None
        failed.setdefault(owner, error)
    return failed or None


def failure(unit, probe, error):
    """The record of a probe that did not compile or crashed."""
    assertion = ASSERTION.match(error)
    if error.startswith(CRASH):
        outcome = Outcome.CRASHED
    elif assertion:
        outcome = Outcome.REJECTED
        error = assertion.group(1)
    else:
        outcome = Outcome.DOES_NOT_COMPILE
    return {"cell": unit.cell(probe), "outcome": outcome.value,
            "error": error.strip()}


def invalid(unit, probe, reason):
    return {"cell": unit.cell(probe), "outcome": Outcome.INVALID.value,
            "error": reason}


def compile_probes(compiler, flags, directory, unit, name, probes):
    """Renders, compiles and runs probes in one program; returns
    (records, stderr, crash, owners). The memory the compile takes is
    recorded for the class of the probes."""
    source = unit.render(probes)
    key = memory_class(unit, probes)
    records, stderr, crash = build_and_run(
        compiler, flags, directory, name, source,
        lambda peak: BUDGET.record(key, len(probes), peak))
    return records, stderr, crash, owners_of(source, probes)


def fitting(probes, unit, exhausted=False):
    """The parts of probes that fit in a program each, by what the probes of
    their class take; fewer of them in each than in one that ran out of
    memory."""
    size = BUDGET.probes(memory_class(unit, probes))
    if exhausted:
        size = min(size, len(probes) // 2)
    return split(probes, max(size, 1))


def in_parts(make, compiler, flags, directory, unit, name, parts):
    """The outcomes of making each part of probes in programs of its own,
    concurrently, named after the one they are parts of."""
    return [outcome for found in concurrently([
        lambda index=index, part=part: make(
            compiler, flags, directory, unit, f"{name}_{index}", part)
        for index, part in enumerate(parts)]) for outcome in found]


def probe_alone(compiler, flags, directory, unit, name, probe):
    """Makes one probe in a program of its own, of the given name."""
    records, stderr, crash, _ = compile_probes(
        compiler, flags, directory, unit, name, [probe])
    if stderr is not None:
        return [failure(unit, probe, first_error(stderr))]
    if crash is not None:
        return [failure(unit, probe, crash)]
    return records


def probe_group(compiler, flags, directory, unit, name, probes):
    """Makes probes in one program, until they compile. The probes a compile
    fails for are recorded with their first error and the rest are compiled
    again. Probes whose errors cannot be told apart, or that crash, are made
    one at a time, concurrently, and so are the rest when a compile tells
    apart few of them: a failure they share is reported for one at a time.
    Probes that do not fit in one program are made in parts that do."""
    parts = fitting(probes, unit)
    if len(parts) > 1:
        return in_parts(probe_group, compiler, flags, directory, unit, name,
                        parts)
    outcomes = []
    while probes:
        records, stderr, crash, owners = compile_probes(
            compiler, flags, directory, unit, name, probes)
        if stderr is None and crash is None:
            return outcomes + records
        if stderr == OUT_OF_MEMORY and len(probes) > 1:
            return outcomes + in_parts(
                probe_group, compiler, flags, directory, unit, name,
                fitting(probes, unit, exhausted=True))
        failed = failed_probes(stderr, name, owners) if stderr else None
        if failed is None:
            break
        outcomes += [failure(unit, probe, error)
                     for probe, error in failed.items()]
        told_apart = len(failed) * 4 >= len(probes)
        probes = [probe for probe in probes if probe not in failed]
        if not told_apart:
            break
    for alone in concurrently([
            lambda index=index, probe=probe: probe_alone(
                compiler, flags, directory, unit, f"{name}__{index}", probe)
            for index, probe in enumerate(probes)]):
        outcomes += alone
    return outcomes


def probe_shared(compiler, flags, directory, unit, name, probes):
    """Makes probes in one program, where the operations share what they
    instantiate. A failure in an instantiation that several probes share is
    reported for the first of them only. So when the program fails, the
    probes it reports are recorded, the operations they belong to and the
    other probes of their requests are made in a program per operation, where
    their probes are told apart one compile after another, and the rest in
    one program again. Probes that do not fit in one program are made in
    parts that do."""
    if not probes:
        return []
    parts = fitting(probes, unit)
    if len(parts) > 1:
        return in_parts(probe_shared, compiler, flags, directory, unit, name,
                        parts)
    records, stderr, crash, owners = compile_probes(
        compiler, flags, directory, unit, name, probes)
    if stderr is None and crash is None:
        return records
    if stderr == OUT_OF_MEMORY and len(probes) > 1:
        return in_parts(probe_shared, compiler, flags, directory, unit, name,
                        fitting(probes, unit, exhausted=True))
    failed = (failed_probes(stderr, name, owners) if stderr else None) or {}
    outcomes = [failure(unit, probe, error)
                for probe, error in failed.items()]
    rest = [probe for probe in probes if probe not in failed]
    if failed:
        operations = {probe.operation for probe in failed}
        requests = {probe.request_key for probe in failed}
        suspects = [probe for probe in rest
                    if probe.operation in operations or
                    probe.request_key in requests]
    else:
        # Nothing could be told apart: every probe is a suspect.
        suspects = rest
    # The programs are named after this one: a unit may be probed by several
    # programs at once, which must not write one file. They are independent,
    # and compiled concurrently.
    calls = [lambda: probe_shared(compiler, flags, directory, unit,
                                  f"{name}_", [probe for probe in rest
                                               if probe not in suspects])]
    calls += [lambda operation=operation: probe_group(
        compiler, flags, directory, unit, f"{name}__{operation.name}",
        [probe for probe in suspects if probe.operation == operation])
        for operation in sorted({probe.operation for probe in suspects})]
    for found in concurrently(calls):
        outcomes += found
    return outcomes


def error_limit_flags(compiler):
    """The flags that let the compiler report every error of a program:
    clang stops at 20, GCC does not stop."""
    result = subprocess.run([compiler, "--version"], capture_output=True,
                            text=True)
    return ["-ferror-limit=0"] if "clang" in result.stdout else []


def precompile_header(compiler, flags, work, header):
    """Precompiles model/<header>, which every program of a family includes
    first, and returns the include path that finds it first; no path when the
    compiler cannot. The headers are copied, so that the one a program
    includes is the one precompiled."""
    directory = work / "pch"
    (directory / "model").mkdir(parents=True, exist_ok=True)
    for path in HEADERS.glob("*.h"):
        (directory / "model" / path.name).write_text(path.read_text())
    copy = directory / "model" / header
    result = subprocess.run(
        compiler_command(compiler, *flags, "-x", "c++-header", str(copy),
                         "-o", str(copy) + ".gch"), capture_output=True)
    return [f"-I{directory}"] if result.returncode == 0 else []
