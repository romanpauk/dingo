"""Builds the model baseline: the outcome of every cell, asked from the library.

A cell is container x scope x stored form x leaf x binding x request. The
library answers each cell; nothing here decides what should work:

- invalid: the request cannot be spelled in C++, such as an abstract type
  held by value
- not_registrable: the library rejects the registration (binding_model::valid),
  so the probe does not register it
- rejected: the library rejects the request or the registration at compile
  time with a static_assert; its message is kept
- does_not_compile: the request fails to compile otherwise, which is a defect;
  the first error is kept
- crashed: the probe of the request did not exit, which is a defect
- violated: the request resolved, and the result is wrong: it does not reach
  the registered leaf, a kept instance is not kept, a unique one is, or the
  probe leaks
- resolved: the request resolved and the result is right
- type_not_found, type_not_convertible, type_ambiguous, exception: the
  exception the request threw

The containers must agree: a request that one resolves the other resolves,
unless one of them does not accept the registration. The probe fails when a
cell is a defect (does_not_compile, crashed, violated) or the containers
disagree.

Every registration gets its own translation unit, which makes each request
with each operation, so a failure is confined to its cells and the operations
share what they instantiate. The compiler reports every request that fails to
compile at once, by its line in the unit, so those are recorded and the others
are compiled again; a unit whose errors cannot be attributed, or that crashes,
is split into one unit per request.
"""

import argparse
import concurrent.futures
from dataclasses import dataclass, replace
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

CONTAINERS = ("runtime", "static")
SCOPES = ("unique", "shared", "external", "shared_cyclical")

# The leaf every cell stores, by the operations it supports.
LEAVES = {
    "copyable": "leaf",
    "move_only": "move_only_leaf",
}

# A stored type of each storage form, around the leaf L.
STORED = (
    ("plain", "L"),
    ("reference", "L &"),
    ("pointer", "L *"),
    ("bounded_array", "L[2]"),
    ("unique_handle", "std::unique_ptr<L>"),
    ("unique_array_handle", "std::unique_ptr<L[]>"),
    ("shared_handle", "std::shared_ptr<L>"),
    ("shared_array_handle", "std::shared_ptr<L[]>"),
    ("optional", "std::optional<L>"),
    ("alternative", "std::variant<L, other>"),
)

# The shapes a bound interface I is requested in: I itself or a handle of it,
# each in every form. Each is spelled for the report and, through the alias
# templates of probe.h, for C++.
BASES = (
    ("I", "{}"),
    ("std::shared_ptr<I>", "shape::sp<{}>"),
    ("std::unique_ptr<I>", "shape::up<{}>"),
    ("std::optional<I>", "shape::opt<{}>"),
    ("std::shared_ptr<I[]>", "shape::sp_array<{}>"),
    ("std::unique_ptr<I[]>", "shape::up_array<{}>"),
)

# The forms of a shape X, and whether a request in the form holds X by value;
# a resolved rvalue reference is returned by value.
FORMS = (
    ("X", "{}", True),
    ("const X", "shape::constant<{}>", True),
    ("X &", "shape::lref<{}>", False),
    ("const X &", "shape::clref<{}>", False),
    ("X &&", "shape::rref<{}>", True),
    ("X *", "shape::ptr<{}>", False),
    ("const X *", "shape::cptr<{}>", False),
)


@dataclass(frozen=True)
class Interface:
    """An interface a registration binds: its name in the report, its C++
    spelling, and whether it is abstract or an array."""
    name: str
    spelled: str
    abstract: bool = False
    array: bool = False


def interface(spelled, abstract=False, array=False):
    name = spelled.replace("dingo::model::", "").replace(" [", "[")
    return Interface(name, spelled, abstract, array)


def invalid_request(bound, base, by_value):
    """Why a request of a shape of an interface is not valid C++, or None."""
    if bound.abstract:
        if base == "I" and by_value:
            return "an abstract type cannot be held by value"
        if base == "std::optional<I>":
            return "an optional cannot hold an abstract type"
        if base.endswith("[]>"):
            return "an array cannot hold an abstract type"
    if bound.array:
        if base == "I" and by_value:
            return "an array cannot be returned by value"
        if base == "std::optional<I>":
            return "an optional cannot hold an array"
        if base == "std::unique_ptr<I>":
            return "a unique_ptr deletes an array as an object"
    return None


# How a binding is registered:
# - interface: the interfaces<> of the registration, or None
# - target: the interface it binds when the library rejects it, and so does
#   not say
# - abstract: whether the target is abstract
BINDINGS = {
    "concrete": dict(interface=None, target="L", abstract=False),
    "interface": dict(interface="leaf_interface", target="leaf_interface",
                      abstract=True),
    "annotated": dict(interface="dingo::annotated<L, tag>", target="L",
                      abstract=False),
    "keyed": dict(interface="L", target="L", abstract=False),
}

KEY = "dingo::key_type<std::size_t, 1>"

# The entry points of a container a request is made through: resolve<>,
# invoke() of a callable that takes it, and construct<>.
ENTRIES = ("resolve", "invoke", "construct")

# Where the request stands, and the scope of the class whose constructor
# takes it:
# - direct: the request itself
# - consumer: the dependency of an unregistered class, which the container
#   constructs
# - shared, cyclical: the dependency of a class registered in the shared or
#   shared_cyclical scope, which keeps it and what it receives; the class is
#   requested by reference
POSITIONS = {
    "direct": None,
    "consumer": None,
    "shared": "shared",
    "cyclical": "shared_cyclical",
}

# An operation is a position and an entry point, such as shared_invoke; a
# direct one is named by its entry point.
OPERATIONS = tuple(entry if position == "direct" else f"{position}_{entry}"
                   for position in POSITIONS for entry in ENTRIES)


def operation_parts(operation):
    """The position and the entry point of an operation."""
    if operation in ENTRIES:
        return "direct", operation
    position, entry = operation.split("_", 1)
    return position, entry


@dataclass(frozen=True)
class Probe:
    """A request of a bound interface, made with an operation: the interface
    and the request as the report names them, and the request in C++."""
    operation: str
    interface: str
    request: str
    spelled: str


@dataclass(frozen=True)
class Registration:
    container: str
    scope: str
    form: str
    leaf: str
    binding: str
    # The interfaces the registration binds, as the library says.
    bound: tuple = ()

    @property
    def name(self):
        return "__".join((self.container, self.scope, self.form, self.leaf,
                          self.binding))

    @property
    def leaf_type(self):
        return LEAVES[self.leaf]

    @property
    def stored(self):
        return dict(STORED)[self.form].replace("L", self.leaf_type)

    @property
    def traits(self):
        return BINDINGS[self.binding]

    def spell(self, text):
        return text.replace("L", self.leaf_type)

    @property
    def arguments(self):
        arguments = (
            f"dingo::scope<dingo::{self.scope}>, dingo::storage<{self.stored}>")
        if self.form.endswith("array_handle") and self.scope != "external":
            # A dynamic array has no size the default factory can make.
            arguments += (f", dingo::factory<dingo::function<"
                          f"&instance<{self.stored}>::make>>")
        if self.traits["interface"]:
            interface = self.spell(self.traits["interface"])
            arguments += f", dingo::interfaces<{interface}>"
        return arguments

    @property
    def static_arguments(self):
        if self.binding == "keyed":
            return f"{self.arguments}, {KEY}"
        return self.arguments

    @property
    def survey_key(self):
        return "__".join((self.scope, self.form, self.leaf, self.binding))

    def interfaces(self):
        """The interfaces the registration binds; when the library rejects
        it, the one it names."""
        if self.bound:
            return self.bound
        return (interface(self.spell(self.traits["target"]),
                          self.traits["abstract"]),)

    def requests(self):
        """The requests of every bound interface: (the index of the
        interface, the request as the report names it, the request in C++,
        and why it is not valid C++, or None)."""
        found = []
        for index, bound in enumerate(self.interfaces()):
            for base, base_spelled in BASES:
                for form, form_spelled, by_value in FORMS:
                    found.append(
                        (index, form.replace("X", base),
                         form_spelled.format(base_spelled.format(f"I{index}")),
                         invalid_request(bound, base, by_value)))
        return found

    def probes(self):
        """The probes, each with why its request is not valid C++, or None."""
        interfaces = self.interfaces()
        return [(Probe(operation, interfaces[index].name, request, spelled),
                 invalid)
                for operation in OPERATIONS
                for index, request, spelled, invalid in self.requests()]

    def cell(self, probe):
        return "/".join((self.container, self.scope, self.form, self.leaf,
                         self.binding, probe.operation, probe.interface,
                         probe.request))

    def container_type(self):
        base = f"dingo::{self.container}_container_traits"
        keys = ", ".join(f"I{index}" for index in range(len(self.interfaces())))
        traits = (f"keyed_traits<{base}, {keys}>"
                  if self.binding == "keyed" else base)
        if self.container == "runtime":
            return f"dingo::container<{traits}>"
        return (f"dingo::static_container<dingo::bindings<dingo::bind<"
                f"{self.static_arguments}>>, {traits}>")

    def given(self):
        """The instance an external scope is given, or None."""
        if self.scope != "external":
            return None
        if self.form in ("reference", "bounded_array"):
            object_type = self.stored.replace("&", "").strip()
            return f"external_object<{object_type}>()"
        return f"instance<{self.stored}>::make()"

    def register(self):
        """The statement that registers the stored type in container c."""
        if self.container == "static":
            return ""
        args = [self.given()] if self.given() else []
        if self.binding == "keyed":
            args.append("dingo::key_value{std::size_t(1)}")
        return (f"c.template register_type<{self.arguments}>"
                f"({', '.join(args)});")

    def selected(self, request):
        """The request as a dependency names it: with its annotation or key."""
        if self.binding == "annotated":
            return f"dingo::annotated<{request}, tag>"
        if self.binding == "keyed":
            return f"dingo::dependency<{request}, {KEY}>"
        return request

    def resolve(self, probe):
        """The expression that makes the request in container c."""
        position, entry = operation_parts(probe.operation)
        request = probe.spelled
        selected = self.selected(request)
        if position == "direct":
            if entry == "invoke":
                return (f"c.invoke([]({selected} arg) {{ return received<"
                        f"{selected}>(pass_on<{selected}>(arg)); }})")
            if entry == "construct":
                return f"c.template construct<{selected}>()"
            if self.binding == "keyed" and self.container == "runtime":
                return f"c.template resolve<{request}>(std::size_t(1))"
            return f"c.template resolve<{selected}>()"
        made = f"consumer<{selected}>"
        if POSITIONS[position]:
            made += " &"
        if entry == "invoke":
            # invoke returns a value, so a kept consumer is returned by its
            # address, which is not copied.
            if POSITIONS[position]:
                return (f"c.invoke([]({made} arg) {{ "
                        f"return std::addressof(arg); }})")
            return (f"c.invoke([]({made} arg) {{ "
                    f"return {made}(std::move(arg)); }})")
        return f"c.template {entry}<{made}>()"

    def probed_container(self, probe):
        """The container a request is probed in, within run<Container>."""
        scope = POSITIONS[operation_parts(probe.operation)[0]]
        if scope:
            return (f"typename with_consumer<Container, "
                    f"{self.selected(probe.spelled)}, dingo::{scope}>::type")
        return "Container"

    def setup(self, probe):
        """The statements that register what a request needs in c."""
        statements = self.register()
        scope = POSITIONS[operation_parts(probe.operation)[0]]
        if scope:
            statements += (
                f" register_consumer<{self.selected(probe.spelled)}, "
                f"dingo::{scope}>(c);")
        return statements

    def keeps(self, probe):
        """How the scope keeps what the probe resolves."""
        if self.scope == "unique":
            return "nothing"
        if probe.operation == "construct":
            return "addresses"
        return "instance"

    def source(self, probes):
        """The unit that makes the probes, and the probe each of its lines
        belongs to, by line number."""
        static = "true" if self.container == "static" else "false"
        registration = f"dingo::type_registration<{self.static_arguments}>"
        lines = [
            "// Generated by test/model/probe.py. Do not edit.",
            '#include "model/probe.h"',
            "",
            "using namespace dingo::model;",
            "",
            "// The interfaces the registration binds.",
        ]
        lines += [f"using I{index} = {bound.spelled};"
                  for index, bound in enumerate(self.interfaces())]
        lines += [
            "",
            "// The container is a template parameter, so that the calls on it",
            "// are dependent and the branch not taken is discarded.",
            "template <typename Container> void run() {",
            f"  if constexpr (registrable<{registration}, {static}>()) {{",
        ]
        # Every request a bound interface serves has to be probed.
        for index, bound in enumerate(self.bound):
            requests = ", ".join(spelled for at, _, spelled, invalid in
                                 self.requests()
                                 if at == index and invalid is None)
            prefix = "/".join((self.container, self.scope, self.form,
                               self.leaf, self.binding, "published",
                               bound.name, ""))
            lines.append(f'    check_probed<{registration}, I{index}, '
                         f'{requests}>("{prefix}");')
        if self.given():
            # What the scope is given outlives the probes; make it first, so
            # that they do not count it as leaked.
            lines.append(f"    (void){self.given()};")
        owners = {}
        for probe in probes:
            resolve = self.resolve(probe)
            owners.update(
                {len(lines) + offset: probe for offset in range(1, 5)})
            lines += [
                f"    probe<{self.probed_container(probe)}>(",
                f'        "{self.cell(probe)}", keeps::{self.keeps(probe)},',
                f"        [](auto &c) {{ {self.setup(probe)} }},",
                f"        [](auto &c) -> decltype(auto) {{ return {resolve}; }});",
            ]
        lines.append("  } else {")
        lines += [
            f'    record("{self.cell(probe)}", "not_registrable");'
            for probe in probes
        ]
        lines += [
            "  }",
            "}",
            "",
            f"int main() {{ run<{self.container_type()}>(); }}",
            "",
        ]
        return "\n".join(lines), owners


def registrations():
    for container in CONTAINERS:
        for scope in SCOPES:
            for form, _ in STORED:
                for leaf in LEAVES:
                    for binding in BINDINGS:
                        yield Registration(container, scope, form, leaf,
                                           binding)


ERROR = re.compile(r"error: (.*)")
ERROR_LIMIT = "too many errors emitted"
# GCC: "static assertion failed: message"; clang: "static assertion failed
# due to requirement 'condition': message".
ASSERTION = re.compile(
    r"^static assertion failed(?: due to requirement '.*')?:\s*(.*)$")
CRASH = "exited with"


def first_error(output):
    for line in output.splitlines():
        match = ERROR.search(line)
        if match:
            return match.group(1).strip()
    return output.strip().splitlines()[-1] if output.strip() else "unknown"


def build_and_run(compiler, flags, directory, name, source):
    """Compiles and runs one unit; returns (records, stderr, crash): the
    records it printed, the compiler output when it did not compile, and how
    it exited when it crashed."""
    cpp = directory / f"{name}.cpp"
    exe = directory / name
    cpp.write_text(source)
    result = subprocess.run(
        [compiler, *flags, str(cpp), "-o", str(exe)],
        capture_output=True,
        text=True,
    )
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
    the lines of the unit its instantiation context names: GCC prints the
    context before an error, clang after it, as notes. None when an error
    belongs to no probe. Clang stops at its error limit, unless it is lifted;
    the probes after it are left to the next compile."""
    reference = re.compile(re.escape(f"{name}.cpp") + r":(\d+):")
    context_follows = "note: in instantiation of" in stderr
    errors = []  # each error with the unit lines of its context
    pending = []
    for line in stderr.splitlines():
        if ERROR_LIMIT in line:
            # The probes after the limit are left to the next compile.
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


def failure(registration, probe, error):
    assertion = ASSERTION.match(error)
    if error.startswith(CRASH):
        outcome = "crashed"
    elif assertion:
        outcome = "rejected"
        error = assertion.group(1)
    else:
        outcome = "does_not_compile"
    return {
        "cell": registration.cell(probe),
        "outcome": outcome,
        "error": error.strip(),
    }


def probe_alone(compiler, flags, directory, registration, index, probe):
    """Makes one probe in a unit of its own."""
    source, _ = registration.source((probe,))
    records, stderr, crash = build_and_run(
        compiler, flags, directory, f"{registration.name}__{index}", source)
    if stderr is not None:
        return [failure(registration, probe, first_error(stderr))]
    if crash is not None:
        return [failure(registration, probe, crash)]
    return records


def probe_group(compiler, flags, directory, registration, name, probes):
    """Makes probes in one unit, until they compile. The probes a compile
    fails for are recorded with their first error and the rest are compiled
    again; probes whose errors cannot be told apart, or that crash, are made
    one at a time."""
    outcomes = []
    while probes:
        source, owners = registration.source(probes)
        records, stderr, crash = build_and_run(compiler, flags, directory,
                                               name, source)
        if stderr is None and crash is None:
            return outcomes + records
        failed = failed_probes(stderr, name, owners) if stderr else None
        if failed is None:
            break
        outcomes += [failure(registration, probe, error)
                     for probe, error in failed.items()]
        probes = [probe for probe in probes if probe not in failed]
    for index, probe in enumerate(probes):
        outcomes += probe_alone(compiler, flags, directory, registration,
                                index, probe)
    return outcomes


def probe_shared(compiler, flags, directory, registration, name, probes):
    """Makes probes in one unit, where the operations share what they
    instantiate. A failure in an instantiation that several probes share is
    reported for the first of them only. So when the unit fails, the probes it
    reports are recorded, the operations they belong to and the other probes
    of their requests are made in a unit per operation, where their probes
    are told apart one compile after another, and the rest in one unit
    again."""
    if not probes:
        return []
    source, owners = registration.source(probes)
    records, stderr, crash = build_and_run(compiler, flags, directory, name,
                                           source)
    if stderr is None and crash is None:
        return records
    failed = (failed_probes(stderr, name, owners) if stderr else None) or {}
    outcomes = [failure(registration, probe, error)
                for probe, error in failed.items()]
    rest = [probe for probe in probes if probe not in failed]
    if failed:
        operations = {probe.operation for probe in failed}
        requests = {(probe.interface, probe.request) for probe in failed}
        suspects = [probe for probe in rest
                    if probe.operation in operations or
                    (probe.interface, probe.request) in requests]
    else:
        # Nothing could be told apart: every probe is a suspect.
        suspects = rest
    outcomes += probe_shared(compiler, flags, directory, registration,
                             f"{name}_", [probe for probe in rest
                                          if probe not in suspects])
    for operation in OPERATIONS:
        outcomes += probe_group(
            compiler, flags, directory, registration,
            f"{registration.name}__{operation}",
            [probe for probe in suspects if probe.operation == operation])
    return outcomes


def probe_registration(compiler, flags, directory, registration):
    """Makes the probes of a registration, the invalid ones aside."""
    outcomes = []
    probes = []
    for probe, invalid in registration.probes():
        if invalid:
            outcomes.append({
                "cell": registration.cell(probe),
                "outcome": "invalid",
                "error": invalid,
            })
        else:
            probes.append(probe)
    # A program per position, which bounds the memory of its compile.
    for position in POSITIONS:
        outcomes += probe_shared(
            compiler, flags, directory, registration,
            f"{registration.name}__{position}",
            [probe for probe in probes
             if operation_parts(probe.operation)[0] == position])
    return outcomes


MARKS = {
    "resolved": "ok",
    "type_not_convertible": "nc",
    "type_not_found": "nf",
    "type_ambiguous": "amb",
    "rejected": "rej",
    "not_registrable": "-",
    "invalid": "inv",
    "does_not_compile": "DNC",
    "crashed": "CRASH",
    "violated": "BAD",
    "unprobed": "UNPROBED",
    "unreachable": "UNREACHABLE",
    "exception": "exc",
}

DIMENSIONS = ("container", "scope", "form", "leaf", "binding", "operation",
              "interface", "request")

# unprobed: a request a binding publishes and selects that no probe makes;
# unreachable: a request a binding publishes that selects another binding.
DEFECTS = ("does_not_compile", "crashed", "violated", "unprobed",
           "unreachable")


def disagreements(cells):
    """The cells one container resolves and the other does not, though both
    accept the registration."""
    outcomes = {}
    for cell in cells:
        container, rest = cell["cell"].split("/", 1)
        outcomes.setdefault(rest, {})[container] = cell["outcome"]
    found = []
    for rest, by_container in sorted(outcomes.items()):
        if len(by_container) != len(CONTAINERS) or any(
                outcome in ("not_registrable", "invalid")
                for outcome in by_container.values()):
            continue
        if len({outcome == "resolved"
                for outcome in by_container.values()}) > 1:
            found.append((rest, by_container))
    return found


def counts_table(cells, dimension):
    outcomes = [o for o in MARKS if any(c["outcome"] == o for c in cells)]
    totals = {}
    for cell in cells:
        value = cell["cell"].split("/")[DIMENSIONS.index(dimension)]
        row = totals.setdefault(value, {})
        row[cell["outcome"]] = row.get(cell["outcome"], 0) + 1
    lines = [
        f"| {dimension} | " + " | ".join(outcomes) + " |",
        "|---|" + "---:|" * len(outcomes),
    ]
    for value, row in totals.items():
        lines.append(f"| `{value}` | " +
                     " | ".join(str(row.get(o, "")) for o in outcomes) + " |")
    return lines + [""]


def write_report(path, cells):
    """Counts by dimension, a table per container, leaf and binding, and the
    compile-time outcomes with their messages."""
    table = {}
    errors = {}
    for cell in cells:
        table[cell["cell"]] = MARKS.get(cell["outcome"], cell["outcome"])
        if cell["outcome"] in ("does_not_compile", "rejected"):
            errors.setdefault((cell["outcome"], cell["error"]), []).append(
                cell["cell"])
    lines = [
        "# Model baseline",
        "",
        "Generated by test/model/probe.py. Legend: "
        + ", ".join(f"`{mark}` {outcome}" for outcome, mark in MARKS.items()),
        "",
        f"{len(cells)} cells.",
        "",
    ]
    for dimension in DIMENSIONS[:-1]:
        lines += counts_table(cells, dimension)
    patterns = [form.replace("X", base) for base, _ in BASES
                for form, _, _ in FORMS]
    rows = {}
    for cell in cells:
        (container, scope, form, leaf, binding, operation, bound,
         request) = cell["cell"].split("/")
        rows.setdefault((container, leaf, binding, operation), {}).setdefault(
            (scope, form, bound), {})[request] = table[cell["cell"]]
    for container, leaf, binding, operation in (
            (c, l, b, o) for c in CONTAINERS for l in LEAVES for b in BINDINGS
            for o in OPERATIONS):
        lines += [
            f"## {container} container, {leaf} leaf, {binding} binding, "
            f"{operation}", "",
            "| scope / stored / interface | " +
            " | ".join(f"`{r}`" for r in patterns) + " |",
            "|---|" + "---|" * len(patterns),
        ]
        found = rows.get((container, leaf, binding, operation), {})
        for (scope, form, bound), row in found.items():
            lines.append(f"| {scope} / {form} / `{bound}` | " +
                         " | ".join(row.get(r, "?") for r in patterns) + " |")
        lines.append("")
    lines += ["## Container disagreements", ""]
    lines += [
        f"- `{rest}`: " + ", ".join(f"{c} {o}" for c, o in by.items())
        for rest, by in disagreements(cells)
    ] or ["None."]
    lines += ["", "## Compile-time outcomes", ""]
    for (outcome, error), where in sorted(
            errors.items(), key=lambda item: (item[0][0], -len(item[1]))):
        lines.append(f"- {outcome}, {len(where)} cells: `{error[:200]}`")
        lines += [f"  - {cell}" for cell in where[:4]]
        if len(where) > 4:
            lines.append(f"  - ... {len(where) - 4} more")
    path.write_text("\n".join(lines) + "\n")


def error_limit_flags(compiler):
    """The flags that let the compiler report every error of a unit: clang
    stops at 20, GCC does not stop."""
    result = subprocess.run([compiler, "--version"], capture_output=True,
                            text=True)
    return ["-ferror-limit=0"] if "clang" in result.stdout else []


def survey(compiler, flags, work, units):
    """The interfaces each registration binds, by its survey key, as the
    library says; a registration it rejects binds none."""
    keys = {}
    for unit in units:
        keys.setdefault(unit.survey_key, unit)
    lines = ['#include "model/probe.h"', "", "using namespace dingo::model;",
             "", "int main() {"]
    lines += [f'  survey<dingo::type_registration<{unit.static_arguments}>>'
              f'("{key}");' for key, unit in keys.items()]
    lines += ["}", ""]
    records, stderr, crash = build_and_run(compiler, flags, work, "survey",
                                           "\n".join(lines))
    if stderr is not None or crash is not None:
        sys.exit(f"the survey failed: {stderr or crash}")
    bound = {}
    for record in records:
        bound.setdefault(record["registration"], []).append(
            interface(record["interface"], record["abstract"],
                      record["array"]))
    return {key: tuple(found) for key, found in bound.items()}


def precompile_header(compiler, flags, work):
    """Precompiles model/probe.h, which every unit includes, and returns the
    include path that finds it first; no path when the compiler cannot."""
    header = Path(__file__).with_name("probe.h")
    directory = work / "pch"
    (directory / "model").mkdir(parents=True, exist_ok=True)
    copy = directory / "model" / "probe.h"
    copy.write_text(header.read_text())
    result = subprocess.run(
        [compiler, *flags, "-x", "c++-header", str(copy), "-o",
         str(copy) + ".gch"],
        capture_output=True,
    )
    return [f"-I{directory}"] if result.returncode == 0 else []


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", type=Path, help="the baseline JSON")
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument(
        "--flags", default="-std=c++17 -O0 -Iinclude -Itest",
        help="compiler flags, with include paths",
    )
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    parser.add_argument(
        "--only", default="",
        help="probe only the registrations whose name contains this text",
    )
    options = parser.parse_args()
    options.work.mkdir(parents=True, exist_ok=True)
    flags = shlex.split(options.flags) + error_limit_flags(options.compiler)
    flags = precompile_header(options.compiler, flags, options.work) + flags

    units = [unit for unit in registrations() if options.only in unit.name]
    bound = survey(options.compiler, flags, options.work, units)
    units = [replace(unit, bound=bound.get(unit.survey_key, ()))
             for unit in units]
    cells = []
    with concurrent.futures.ThreadPoolExecutor(options.jobs) as executor:
        for outcomes in executor.map(
            lambda unit: probe_registration(
                options.compiler, flags, options.work, unit),
            units,
        ):
            cells += outcomes
    # Every unit of a registration checks what it publishes.
    cells = list({cell["cell"]: cell for cell in cells}.values())
    cells.sort(key=lambda cell: cell["cell"])
    options.output.write_text(json.dumps(cells, indent=1) + "\n")
    write_report(options.output.with_suffix(".md"), cells)

    counts = {}
    for cell in cells:
        counts[cell["outcome"]] = counts.get(cell["outcome"], 0) + 1
    print(f"{len(cells)} cells:", file=sys.stderr)
    for outcome, count in sorted(counts.items(), key=lambda item: -item[1]):
        print(f"  {count:6} {outcome}", file=sys.stderr)
    defects = [cell for cell in cells if cell["outcome"] in DEFECTS]
    disagreeing = disagreements(cells)
    for cell in defects:
        print(f"defect: {cell['cell']}: {cell['outcome']}: "
              f"{cell.get('error', '')}", file=sys.stderr)
    for rest, by_container in disagreeing:
        print(f"disagreement: {rest}: {by_container}", file=sys.stderr)
    print(f"report: {options.output.with_suffix('.md')}", file=sys.stderr)
    return 1 if defects or disagreeing else 0


if __name__ == "__main__":
    sys.exit(main())
