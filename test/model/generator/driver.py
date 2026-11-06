"""Compiles and runs probe programs, and reads the library's answers.

A unit is a program of probes. It has a name, the cell of each probe, and
renders the program of a list of its probes from a template. The template
marks the lines of each probe, `// probe N` before them and `// probes end`
after the last, so that a compile error is told apart by probe: the
compiler names the lines of the program an error was instantiated from.
"""

import json
from pathlib import Path
import re
import subprocess

import jinja2

from .dimensions import Outcome

TEMPLATES = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).with_name("templates")),
    undefined=jinja2.StrictUndefined,
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)

PROBE_MARKER = re.compile(r"^\s*// probe (\d+)$")
END_MARKER = "// probes end"

ERROR = re.compile(r"error: (.*)")
ERROR_LIMIT = "too many errors emitted"
# GCC: "static assertion failed: message"; clang: "static assertion failed
# due to requirement 'condition': message".
ASSERTION = re.compile(
    r"^static assertion failed(?: due to requirement '.*')?:\s*(.*)$")
CRASH = "exited with"


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


def build_and_run(compiler, flags, directory, name, source):
    """Compiles and runs one program; returns (records, stderr, crash): the
    records it printed, the compiler output when it did not compile, and how
    it exited when it crashed."""
    cpp = directory / f"{name}.cpp"
    exe = directory / name
    cpp.write_text(source)
    result = subprocess.run([compiler, *flags, str(cpp), "-o", str(exe)],
                            capture_output=True, text=True)
    if result.returncode != 0:
        return None, result.stderr, None
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    exe.unlink()
    records = [json.loads(line) for line in run.stdout.splitlines() if line]
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
    (records, stderr, crash, owners)."""
    source = unit.render(probes)
    records, stderr, crash = build_and_run(compiler, flags, directory, name,
                                           source)
    return records, stderr, crash, owners_of(source, probes)


def probe_alone(compiler, flags, directory, unit, index, probe):
    """Makes one probe in a program of its own."""
    records, stderr, crash, _ = compile_probes(
        compiler, flags, directory, unit, f"{unit.name}__{index}", [probe])
    if stderr is not None:
        return [failure(unit, probe, first_error(stderr))]
    if crash is not None:
        return [failure(unit, probe, crash)]
    return records


def probe_group(compiler, flags, directory, unit, name, probes):
    """Makes probes in one program, until they compile. The probes a compile
    fails for are recorded with their first error and the rest are compiled
    again; probes whose errors cannot be told apart, or that crash, are made
    one at a time."""
    outcomes = []
    while probes:
        records, stderr, crash, owners = compile_probes(
            compiler, flags, directory, unit, name, probes)
        if stderr is None and crash is None:
            return outcomes + records
        failed = failed_probes(stderr, name, owners) if stderr else None
        if failed is None:
            break
        outcomes += [failure(unit, probe, error)
                     for probe, error in failed.items()]
        probes = [probe for probe in probes if probe not in failed]
    for index, probe in enumerate(probes):
        outcomes += probe_alone(compiler, flags, directory, unit, index,
                                probe)
    return outcomes


def probe_shared(compiler, flags, directory, unit, name, probes):
    """Makes probes in one program, where the operations share what they
    instantiate. A failure in an instantiation that several probes share is
    reported for the first of them only. So when the program fails, the
    probes it reports are recorded, the operations they belong to and the
    other probes of their requests are made in a program per operation, where
    their probes are told apart one compile after another, and the rest in
    one program again."""
    if not probes:
        return []
    records, stderr, crash, owners = compile_probes(
        compiler, flags, directory, unit, name, probes)
    if stderr is None and crash is None:
        return records
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
    outcomes += probe_shared(compiler, flags, directory, unit, f"{name}_",
                             [probe for probe in rest
                              if probe not in suspects])
    for operation in sorted({probe.operation for probe in suspects}):
        outcomes += probe_group(
            compiler, flags, directory, unit,
            f"{unit.name}__{operation.name}",
            [probe for probe in suspects if probe.operation == operation])
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
    source = Path(__file__).parent.parent
    directory = work / "pch"
    (directory / "model").mkdir(parents=True, exist_ok=True)
    for path in source.glob("*.h"):
        (directory / "model" / path.name).write_text(path.read_text())
    copy = directory / "model" / header
    result = subprocess.run(
        [compiler, *flags, "-x", "c++-header", str(copy), "-o",
         str(copy) + ".gch"], capture_output=True)
    return [f"-I{directory}"] if result.returncode == 0 else []
