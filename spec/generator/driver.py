"""Compiles and runs probe programs, and reads their outcomes.

A unit is a program of probes. It names the cell of each probe, and renders
the program of a list of its probes from a template. The template
marks the lines of each probe, `// probe N` before them and `// probes end`
after the last, so that a compile error is attributed to its probe: the
compiler names the lines of the program an error was instantiated from.

What a program does is kept, once open_cache names a directory: what it
printed, or how it failed to compile, under a key of its compiler, its flags,
its name and its source as the preprocessor makes it, as ccache does. A
program is compiled again only when one of them changes: a header changed,
added or removed changes the source the preprocessor makes of it, where the
program reads it. The key holds no path of the checkout, so that what one
checkout kept another finds. Each run notes the results it used, and
close_cache removes the others after a run of every program of a check.
"""

import concurrent.futures
import hashlib
from compression import zstd
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import threading

import jinja2

from . import drilldown, objects
from .dimensions import Outcome
from .model import spell_request, spell_shape, spell_stored

# The headers of the probes, which their programs include as model/<header>.
HEADERS = Path(__file__).parents[2] / "test" / "model"

TEMPLATES = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).with_name("templates")),
    undefined=jinja2.StrictUndefined,
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)
TEMPLATES.globals.update(spell_request=spell_request, spell_shape=spell_shape,
                         spell_stored=spell_stored)

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
# the probes run as many compilers as there are processors. It is a gate, not
# a guide: a program the compiler runs out of memory on is not split, its
# probes fail, and the size of programs or the library is to be changed.
MEMORY_LIMIT = 1024
# The probes a program holds, and the registrations they may be of.
GROUP_PROBES = 64
GROUP_REGISTRATIONS = 16
EXHAUSTED = re.compile(r"out of memory|memory exhausted|"
                       r"Cannot allocate memory|bad_alloc")
OUT_OF_MEMORY = f"error: the compiler ran out of its {MEMORY_LIMIT} MiB"

# The flag that compiles a program without making code of it: the probes a
# compile rejects need nothing more, and it takes less time and memory.
COMPILE_ONLY = "-fsyntax-only"


def compiles_only(flags):
    """Whether a program is compiled without code, and so not run."""
    return COMPILE_ONLY in flags


def default_jobs():
    """How many compilers run at once unless given: one per processor, as
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


# The directory the results of programs are kept in, or None; the identity of
# each compiler this run used; the results the next run needs, those this run
# used or stored; and the programs this run compiled.
CACHE = None
COMPILER_IDENTITIES = {}
CACHE_LOCK = threading.Lock()
NEEDED = set()
COMPILED = []

# How hard a kept result is compressed: past it, zstd makes it little smaller
# and far slower.
ZSTD_LEVEL = 9

# The project, which no key holds the path of.
ROOT = Path(__file__).resolve().parents[2]


def open_cache(directory):
    """Keeps the results of programs in a directory."""
    global CACHE
    directory.mkdir(parents=True, exist_ok=True)
    CACHE = directory


def compiler_identity(compiler):
    """What distinguishes a compiler: the file it runs and what it reports of
    itself."""
    with CACHE_LOCK:
        if compiler in COMPILER_IDENTITIES:
            return COMPILER_IDENTITIES[compiler]
    executable = os.path.realpath(shutil.which(compiler) or compiler)
    version = subprocess.run([compiler, "--version"], capture_output=True,
                             text=True).stdout
    identity = [hashlib.sha256(Path(executable).read_bytes()).hexdigest(),
                version]
    with CACHE_LOCK:
        COMPILER_IDENTITIES[compiler] = identity
    return identity


def portable_text(text):
    return text.replace(str(ROOT), "$ROOT")


def portable_flag(flag):
    """A flag as a key holds it: an include path absolute, then of no path
    of the checkout, so that a relative and an absolute one are the same."""
    for prefix in ("-I", "-isystem", "-iquote"):
        if flag.startswith(prefix) and len(flag) > len(prefix):
            return prefix + portable_text(os.path.abspath(flag[len(prefix):]))
    return portable_text(flag)


def program_key(compiler, flags, directory, name, cpp):
    """The key of a program written to cpp: of its compiler, its flags, its
    name and directory, and the source the preprocessor makes of it, with no
    line markers and no path of the checkout; None when it cannot be
    preprocessed."""
    with COMPILES:
        run = subprocess.run(
            compiler_command(compiler, *[flag for flag in flags
                                         if flag != COMPILE_ONLY],
                             "-E", "-P", str(cpp)),
            capture_output=True)
    if run.returncode != 0:
        return None
    return hashlib.sha256(json.dumps(
        [compiler_identity(compiler), MEMORY_LIMIT,
         [portable_flag(flag) for flag in flags],
         portable_text(os.path.abspath(directory)), name,
         hashlib.sha256(run.stdout).hexdigest()]).encode()).hexdigest()


def load_result(key):
    """The result kept of a program, or None."""
    try:
        with zstd.open(CACHE / key[:2] / f"{key}.zst", "rt") as entry:
            result = json.load(entry)
    except (OSError, ValueError, zstd.ZstdError):
        return None
    with CACHE_LOCK:
        NEEDED.add(key)
    return result


def store_result(key, result):
    """Keeps the result of a program, written whole or not at all; not when
    a file the precompiled header read changed since it was compiled, as the
    compiler took that file as the header was then, which the key of the
    program is not of."""
    if not precompiled_header_current():
        return
    path = CACHE / key[:2] / f"{key}.zst"
    path.parent.mkdir(exist_ok=True)
    written = path.with_suffix(f".{threading.get_ident()}")
    with zstd.open(written, "wt", level=ZSTD_LEVEL) as out:
        json.dump(result, out)
    written.replace(path)
    with CACHE_LOCK:
        NEEDED.add(key)


def close_cache(prune):
    """Prints how many programs this run compiled and how many results it
    used as kept; when it prunes, as after a run of every program of a
    check, removes the results it did not use, so that what is kept is what
    the next run may use."""
    if CACHE is None:
        return
    kept = len(NEEDED - set(COMPILED))
    print(f"programs: {len(COMPILED)} compiled, {kept} kept")
    if prune:
        for path in CACHE.glob("??/*.zst"):
            if path.stem not in NEEDED:
                path.unlink()


def make_program(compiler, flags, directory, name, source):
    """Compiles a program and runs it, unless compiled only; returns
    (returncode, stderr, stdout, status): how the compile ended and what it
    reported, and what the program printed and how it exited. Every result
    is kept: a compile of the same source in the same memory, and a program
    of it, end alike."""
    cpp = directory / f"{name}.cpp"
    exe = directory / name
    cpp.write_text(source)
    key = (program_key(compiler, flags, directory, name, cpp)
           if CACHE is not None else None)
    kept = load_result(key) if key is not None else None
    if kept is not None:
        return tuple(kept)
    with CACHE_LOCK:
        COMPILED.append(key)
    ensure_precompiled_header()
    output = [] if compiles_only(flags) else ["-o", str(exe)]
    with COMPILES:
        returncode, stderr = run_compiler(
            compiler_command(compiler, *flags, str(cpp), *output))
    if returncode != 0 or compiles_only(flags):
        result = (returncode, stderr, "", 0)
        if key is not None:
            store_result(key, result)
        return result
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    exe.unlink()
    result = (0, stderr, run.stdout, run.returncode)
    if key is not None:
        store_result(key, result)
    return result


def program_headers(scopes, containers=()):
    """The headers a program includes after the header of its family: of each
    scope and each container its cells use, so that it reads no other; a
    static container with model/static.h."""
    found = {f"<dingo/storage/{scope.value}.h>" for scope in scopes}
    for container in containers:
        found.add("<dingo/runtime_container.h>"
                  if container.value == "runtime" else '"model/static.h"')
    return sorted(found)


def render(template, **context):
    return TEMPLATES.get_template(template).render(**context)


def line_owners(source, probes):
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
    """Runs a compile; returns (returncode, stderr)."""
    with tempfile.TemporaryFile(mode="w+") as errors:
        process = subprocess.run(command, stdout=subprocess.DEVNULL,
                                 stderr=errors, text=True)
        errors.seek(0)
        return process.returncode, errors.read()


def run_program(compiler, flags, directory, name, source):
    """Compiles and runs one program; returns (records, stderr, crash): the
    records it printed, the compiler output when it did not compile, and how
    it exited when it crashed."""
    returncode, stderr, stdout, status = make_program(
        compiler, flags, directory, name, source)
    if returncode != 0:
        if EXHAUSTED.search(stderr):
            # What it reported before is not all it would, and names no probe.
            return None, OUT_OF_MEMORY, None
        return None, stderr, None
    if compiles_only(flags):
        return [], None, None
    try:
        records = [json.loads(line) for line in stdout.splitlines() if line]
    except json.JSONDecodeError:
        # A record the program could not print is a crash of its probe.
        return [], None, f"{CRASH} a malformed record"
    if status != 0:
        return records, None, f"{CRASH} {status}"
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
    (records, stderr, crash, owners)."""
    source = unit.render(probes)
    records, stderr, crash = run_program(compiler, flags, directory, name,
                                           source)
    return records, stderr, crash, line_owners(source, probes)


def fitting(probes):
    """The parts of probes a program holds each."""
    return split(probes, GROUP_PROBES)


def out_of_memory(unit, probes):
    """The probes of a program the compiler ran out of memory on: each
    fails, as the limit is not to be worked around."""
    return [failure(unit, probe, OUT_OF_MEMORY) for probe in probes]


def make_parts(make, compiler, flags, directory, unit, name, parts):
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
    again. Probes whose errors cannot be distinguished, or that crash, are made
    one at a time, concurrently, and so are the rest when a compile
    distinguishes few of them: a failure they share is reported for one at a
    time. Probes that do not fit in one program are made in parts that do."""
    parts = fitting(probes)
    if len(parts) > 1:
        return make_parts(probe_group, compiler, flags, directory, unit, name,
                        parts)
    outcomes = []
    while probes:
        records, stderr, crash, owners = compile_probes(
            compiler, flags, directory, unit, name, probes)
        if stderr is None and crash is None:
            return outcomes + records
        if stderr == OUT_OF_MEMORY:
            return outcomes + out_of_memory(unit, probes)
        failed = failed_probes(stderr, name, owners) if stderr else None
        if failed is None:
            break
        outcomes += [failure(unit, probe, error)
                     for probe, error in failed.items()]
        told_apart = len(failed) * 4 >= len(probes)
        probes = [probe for probe in probes if probe not in failed]
        if not told_apart:
            break
    if len(probes) > 2:
        # Halves, each made as a group: the half without a failure compiles
        # at once, and the failures of the other are distinguished in fewer
        # probes.
        return outcomes + make_parts(probe_group, compiler, flags, directory,
                                   unit, name, split(probes,
                                                     -(-len(probes) // 2)))
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
    their probes are distinguished one compile after another, and the rest in
    one program again. Probes that do not fit in one program are made in
    parts that do."""
    if not probes:
        return []
    parts = fitting(probes)
    if len(parts) > 1:
        return make_parts(probe_shared, compiler, flags, directory, unit, name,
                        parts)
    records, stderr, crash, owners = compile_probes(
        compiler, flags, directory, unit, name, probes)
    if stderr is None and crash is None:
        return records
    if stderr == OUT_OF_MEMORY:
        return out_of_memory(unit, probes)
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
        # Nothing could be distinguished: every probe is a suspect.
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


def probe_expecting_rejection(compiler, flags, directory, unit, name,
                              probes):
    """Makes probes the specification marks as rejected by the library as they
    compile, as probe_shared makes them, in programs that are only compiled:
    each probe a compile rejects is recorded with its first error. A probe no
    compile rejects is made again in programs that are run, so that what it
    does is recorded."""
    if not probes:
        return []
    outcomes = probe_shared(compiler, flags + [COMPILE_ONLY], directory, unit,
                            name, probes)
    recorded = {outcome["cell"] for outcome in outcomes}
    rest = [probe for probe in probes if unit.cell(probe) not in recorded]
    return outcomes + probe_shared(compiler, flags, directory, unit,
                                   f"{name}_run", rest)


def error_limit_flags(compiler):
    """The flags that let the compiler report every error of a program:
    clang stops at 20, GCC does not stop."""
    result = subprocess.run([compiler, "--version"], capture_output=True,
                            text=True)
    return ["-ferror-limit=0"] if "clang" in result.stdout else []


# The compile of the precompiled header of this run, made before the first
# program is compiled, or None once made.
PRECOMPILE_COMMAND = None
PRECOMPILE_LOCK = threading.Lock()


def precompile_header(compiler, flags, work, header):
    """Prepares model/<header>, which every program of a family includes
    first, to be precompiled, and returns the include path that finds it
    first. It is compiled before the first program is, so that a run that
    compiles none does not compile it either; the preprocessor reads the
    header itself. The headers are copied, so that the one a program
    includes is the one precompiled; a compiler that cannot precompile it
    reads the copy."""
    global PRECOMPILE_COMMAND
    directory = work / "pch"
    (directory / "model").mkdir(parents=True, exist_ok=True)
    for path in HEADERS.glob("*.h"):
        (directory / "model" / path.name).write_text(path.read_text())
    copy = directory / "model" / header
    gch = Path(str(copy) + ".gch")
    if gch.exists():
        gch.unlink()
    PRECOMPILE_COMMAND = compiler_command(compiler, *flags, "-x", "c++-header",
                                  str(copy), "-o", str(gch))
    return [f"-I{directory}"]


# The size and the time of change of each file the precompiled header of
# this run read, as it was compiled.
PRECOMPILED_HEADER_FILES = {}


def file_stamp(path):
    try:
        found = os.stat(path)
    except OSError:
        return None
    return found.st_size, found.st_mtime_ns


def ensure_precompiled_header():
    """Compiles the precompiled header, once, before a program is, and notes
    the files it read."""
    global PRECOMPILE_COMMAND
    with PRECOMPILE_LOCK:
        if PRECOMPILE_COMMAND is not None:
            depfile = Path(PRECOMPILE_COMMAND[-1] + ".d")
            subprocess.run([*PRECOMPILE_COMMAND, "-MD", "-MF", str(depfile)],
                           capture_output=True)
            if depfile.exists():
                text = depfile.read_text().replace("\\\n", " ")
                depfile.unlink()
                for name in text.partition(": ")[2].split():
                    PRECOMPILED_HEADER_FILES[name] = file_stamp(name)
            PRECOMPILE_COMMAND = None


def precompiled_header_current():
    """Whether every file the precompiled header read is as it was."""
    return all(file_stamp(name) == found
               for name, found in PRECOMPILED_HEADER_FILES.items())


# Whether the system counts the instructions of a process for its user, by
# perf, found once; None until it is queried.
COUNTS_INSTRUCTIONS = None


def counts_instructions():
    """Whether perf counts the instructions of a process this user runs."""
    global COUNTS_INSTRUCTIONS
    if COUNTS_INSTRUCTIONS is None:
        try:
            counted = subprocess.run(
                ["perf", "stat", "-x,", "-e", "instructions:u", "--", "true"],
                capture_output=True, text=True, timeout=60)
            COUNTS_INSTRUCTIONS = (counted.returncode == 0 and
                                   ",instructions" in counted.stderr and
                                   "<not" not in counted.stderr)
        except (OSError, subprocess.SubprocessError):
            COUNTS_INSTRUCTIONS = False
    return COUNTS_INSTRUCTIONS


def measured_compile(command, directory, name):
    """Runs a compile; returns (returncode, stderr, measured): the processor
    seconds and the peak memory in KiB of the compiler and what it runs, and
    the instructions they ran, where the system reports them."""
    errors = directory / f"{name}.stderr"
    counted = directory / f"{name}.counted"
    prefix = (["perf", "stat", "-x,", "-e", "instructions:u", "-o",
               str(counted), "--"] if counts_instructions() else [])
    measured = {}
    with open(errors, "w") as error_file:
        process = subprocess.Popen(prefix + command,
                                   stdout=subprocess.DEVNULL,
                                   stderr=error_file)
        if hasattr(os, "wait4"):
            _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
            # macOS reports the peak memory in bytes, Linux in KiB.
            memory = usage.ru_maxrss
            if os.uname().sysname == "Darwin":
                memory //= 1024
            measured["cpu"] = round(usage.ru_utime + usage.ru_stime, 2)
            measured["memory"] = memory
        else:
            process.wait()
    stderr = errors.read_text(errors="replace")
    errors.unlink()
    if counted.exists():
        for line in counted.read_text().splitlines():
            fields = line.split(",")
            if len(fields) > 2 and fields[2].startswith("instructions"):
                if fields[0].isdigit():
                    measured["instructions"] = int(fields[0])
        counted.unlink()
    return process.returncode, stderr, measured


def compiler_family(compiler):
    """The compiler a command runs, as what distinguishes its work: clang or
    gcc, by what it reports of itself; None for another."""
    version = compiler_identity(compiler)[1]
    if "clang" in version:
        return "clang"
    if "GCC" in version or "Free Software Foundation" in version:
        return "gcc"
    return None


def compiler_drilldown(compiler, flags, cpp, obj):
    """Where the compiler spends its work on a program, as it reports it, of
    a compile of its own, which the report would slow: GCC's phases, clang's
    events; None for another compiler."""
    family = compiler_family(compiler)
    if family == "gcc":
        returncode, stderr = run_compiler(compiler_command(
            compiler, *flags, "-ftime-report", "-c", str(cpp), "-o", str(obj)))
        obj.unlink(missing_ok=True)
        return ({"phases": drilldown.gcc_report(stderr)}
                if returncode == 0 else None)
    if family == "clang":
        returncode, _ = run_compiler(compiler_command(
            compiler, *flags, "-ftime-trace", "-ftime-trace-granularity=0",
            "-c", str(cpp), "-o", str(obj)))
        trace = obj.with_suffix(".json")
        found = (drilldown.clang_trace(trace)
                 if returncode == 0 and trace.exists() else None)
        obj.unlink(missing_ok=True)
        trace.unlink(missing_ok=True)
        return found
    return None


def measure(compiler, flags, directory, name, source):
    """Compiles a program to an object and measures it: how the compile
    ended and what it reported, what the compiler took, the sections of the
    object, its functions and the sizes of those of no mangled name, and
    where the compiler spent its work. What is found is kept as the result of
    the program: the same program and the same compiler take the same
    work."""
    cpp = directory / f"{name}.cpp"
    obj = directory / f"{name}.o"
    cpp.write_text(source)
    key = (program_key(compiler, flags, directory, name, cpp)
           if CACHE is not None else None)
    kept = load_result(key) if key is not None else None
    if kept is not None:
        return kept
    with CACHE_LOCK:
        COMPILED.append(key)
    with COMPILES:
        returncode, stderr, measured = measured_compile(
            compiler_command(compiler, *flags, "-c", str(cpp), "-o", str(obj)),
            directory, name)
        result = {"returncode": returncode, "stderr": stderr[-4000:],
                  **measured}
        if returncode == 0:
            found = objects.read(obj)
            obj.unlink(missing_ok=True)
            if found is not None:
                result["object"] = {
                    "sections": found["sections"],
                    "functions": len(found["functions"]),
                    "named": {function: size for function, size
                              in found["functions"].items()
                              if not function.startswith("_Z")}}
            result["drilldown"] = compiler_drilldown(compiler, flags, cpp, obj)
    if key is not None:
        store_result(key, result)
    return result
