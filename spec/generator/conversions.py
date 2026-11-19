"""The conversion family: what the binding of every registration derives for
every request of the grammar, decided as the code compiles, compared with the
specification, and what the runtime container selects of its tables for it,
compared with the derivation. A unit is a registration of the runtime
container, selected plainly: selection does not change what a binding
derives."""

from dataclasses import dataclass, field
import multiprocessing
import re

from spec import semantics
from spec.vocabulary import (Category, Container, Entry, InterfaceSet, Leaf,
                             Position, Scope, Selection, Shape, StoredForm)

from . import driver
from .dimensions import Outcome
from .model import grammar, spell_request
from .registrations import LEAVES, Registration


@dataclass(frozen=True)
class Unit(Registration):
    """A registration whose conversion layer is probed."""

    @property
    def makes(self):
        """Whether the scope makes the stored object for each request."""
        return semantics.SCOPES[self.scope].makes

    @property
    def base(self):
        return "/".join((self.scope, self.form, self.leaf,
                         self.interfaces_declared))

    def grammar(self, interfaces):
        """Every shape of the vocabulary, to its depth."""
        return grammar(interfaces, tuple(Shape))

    def requests(self):
        return [request for request in self.grammar(self.interfaces())
                if request.invalid is None]

    def prefix(self, request):
        """The cell of a request without its name, or of the unit."""
        if request is None:
            return self.base
        return f"{self.base}/{request.interface.name}/"

    def cells(self):
        return {self.prefix(request) + request.name: request
                for request in self.requests()}

    def expectation(self, request, limits=semantics.LIMITATIONS):
        """What the specification defines for a request under limitations,
        as expect_all made it of the limitations of the library, when it did."""
        name = self.prefix(request) + request.name
        if limits == semantics.LIMITATIONS and name in EXPECTATIONS:
            return EXPECTATIONS[name]
        return self.compute_expectation(request, limits)

    def compute_expectation(self, request, limits):
        """Whether the specification, under limitations, resolves a request
        at all, in a call, and for the caller, in a resolve; None when it
        defines nothing of the request. It does not of a storage that cannot be
        made, whose request lists are of its types alone, nor of the other
        alternative of an alternative type, which a probe's never holds: that
        is a copyable type of its own."""
        bound, leaf = request.interface.bound, self.leaf
        context = semantics.Context(semantics.LEAVES[leaf],
                                    semantics.SCOPES[self.scope],
                                    bound.element == "interface")
        if not semantics.makes_storage(self.form, context):
            return None
        if bound.element == "other":
            bound, leaf = semantics.LEAF, Leaf.COPYABLE
        args = (self.scope, self.form, leaf, bound, request.shape,
                request.category)

        def resolves(position, entry):
            return semantics.resolves(
                *args, position, entry, limits=limits,
                interfaces=self.interfaces_declared)
        return (resolves(Position.DIRECT, Entry.INVOKE),
                resolves(Position.DIRECT, Entry.RESOLVE))


# The ways C++ builds a request of an object the binding holds, as
# test/model/conversion.h numbers them.
REFERS, MOVES, COPIES, WRAPS = range(4)


@dataclass(frozen=True)
class Route:
    """A way C++ builds a request: how, of the object at a depth of
    dereferences of the stored one, which shares nothing with its copy or
    not."""
    how: int
    depth: int
    value: bool


def routes(mask):
    """The routes of the mask test/model/conversion.h records."""
    return [Route(bit // 2 // 4, bit // 2 % 4, bool(bit % 2))
            for bit in range(32) if mask >> bit & 1]


# What the specification rejects on purpose, of what C++ builds of what a
# binding holds: each is a reason and whether it rejects a route of a unit's
# request.
REJECTIONS = (
    ("a new layer does not hold a copy of an instance, nor of a value that "
     "shares nothing",
     lambda unit, request, route: route.how == WRAPS and route.value),
    ("a cyclical scope publishes its instance before it constructs it: a "
     "request refers to its stored object and what that holds in place, and "
     "copies only a stored handle",
     lambda unit, request, route: unit.scope is Scope.SHARED_CYCLICAL and (
         route.depth >= 2 if route.how == REFERS
         else route.depth >= 1 or route.value)),
    ("an rvalue reference resolves to a value moved out of what the "
     "request owns, which an immovable leaf, or an optional of one, cannot "
     "be",
     lambda unit, request, route: (
         request.category is Category.RVALUE_REFERENCE and
         unit.leaf is Leaf.IMMOVABLE and not request.shape.is_array and
         set(request.shape.wrappers) <= {"optional"})),
)


def rejection(unit, request, mask):
    """Why the specification rejects every route C++ builds a request by,
    or None when it rejects none or only some."""
    reasons = []
    for route in routes(mask):
        found = [reason for reason, rejects in REJECTIONS
                 if rejects(unit, request, route)]
        if not found:
            return None
        reasons += [reason for reason in found if reason not in reasons]
    return "; ".join(reasons) if reasons else None


# What the specification defines for each cell of this run under the
# limitations of the library, by cell: made by expect_all in processes at once.
EXPECTATIONS = {}


def expect_unit(unit):
    """The expectations of the cells of a unit, in a process of its own."""
    return {name: unit.compute_expectation(request, semantics.LIMITATIONS)
            for name, request in unit.cells().items()}


def expect_all(units, jobs):
    """Makes the expectations of the cells of units, in processes at once:
    a unit at a time, as the specification searches what it stores once for
    all its requests."""
    with multiprocessing.Pool(jobs) as pool:
        for found in pool.imap_unordered(expect_unit, units):
            EXPECTATIONS.update(found)


def units():
    """Every registration of the runtime container the library accepts."""
    return [Unit(Container.RUNTIME, scope, form, leaf, interfaces,
                 Selection.PLAIN)
            for scope in Scope for form in StoredForm for leaf in LEAVES
            for interfaces in InterfaceSet
            if semantics.registrable(Container.RUNTIME, scope, form, leaf,
                                     interfaces)]


def batch_name(index):
    return f"conversion_{index}"


def compile_units(compiler, flags, directory, name, batch):
    """The records of a batch of units, compiled into one program. A batch
    that does not compile is compiled a unit at a time, and a unit that does
    not compile records each of its cells with the first error."""
    source = driver.render(
        "conversion.cpp.j2", units=batch, none=None,
        headers=driver.program_headers({unit.scope for unit in batch}))
    records, stderr, crash = driver.run_program(compiler, flags, directory,
                                                  name, source)
    if stderr is None and crash is None:
        return records
    if len(batch) > 1:
        found = driver.concurrently([
            lambda index=index, unit=unit: compile_units(
                compiler, flags, directory, f"{name}_{index}", [unit])
            for index, unit in enumerate(batch)])
        return [record for records in found for record in records]
    error = (driver.first_error(stderr) if stderr is not None else crash)
    outcome = (Outcome.DOES_NOT_COMPILE if stderr is not None
               else Outcome.CRASHED)
    return [{"cell": cell, "outcome": outcome.value, "error": error}
            for cell in batch[0].cells()]


# The properties of the conversion a container chooses for a request, for
# each lifetime, as test/model/conversion.h prints them: whether it has one,
# the copies and the moves of the leaf and the objects it keeps and makes,
# whether what satisfies the request may be absent, how the request takes it,
# whether its address lives as long as the container, and how the request
# takes the value. Each is compared of the fields it is printed as.
CHOSEN_FIELDS = 11
CHOSEN_PROPERTIES = (("found", (0,)), ("cost", (1, 2)), ("objects", (3, 4)),
                     ("absent", (5,)), ("access", (6,)), ("address", (7,)),
                     ("finish", (8, 9, 10)))
LIFETIMES = ("stable", "transient")


def runtime_differences(cell):
    """The properties, by lifetime, of the conversions the runtime container
    selects for the request of a cell and the static container derives,
    which differ; of a found conversion, its other properties too."""
    derived, selected = cell["derived"], cell["selected"]
    found = []
    for index, lifetime in enumerate(LIFETIMES):
        first = index * CHOSEN_FIELDS
        if derived[first] != selected[first]:
            found.append(f"{lifetime} found")
            continue
        if not derived[first]:
            continue
        found += [f"{lifetime} {name}"
                  for name, fields in CHOSEN_PROPERTIES[1:]
                  if any(derived[first + each] != selected[first + each]
                         for each in fields)]
    return found


def runtime_outcome(cell):
    """How the runtime container selects the request of a cell, compared
    with the derivation: alike; unmodeled, where it models no request and
    the derivation derives it neither; apart, where any property differs; or
    unselected, where its program recorded nothing. None of a cell of no
    comparison."""
    if ("derived" not in cell or
            cell["outcome"] == Outcome.SELECTS_ANOTHER.value):
        return None
    if "selected" not in cell:
        return "unselected"
    if runtime_differences(cell):
        return "apart"
    return "alike" if cell["modeled"] else "unmodeled"


# The request types a part of a runtime program holds at most, and the
# bindings: what the compiler takes of each grows with them.
RUNTIME_REQUESTS = 40
RUNTIME_BINDINGS = 25


@dataclass
class RuntimeProgram:
    """A program of what the runtime container selects for the requests of
    one interface type: the bindings of the interface, each a unit and the
    index of the interface, the requests spelled of an interface I, and the
    pairs of a binding and a request, each with its cell, by index."""
    interface: str
    bindings: dict = field(default_factory=dict)
    requests: dict = field(default_factory=dict)
    pairs: list = field(default_factory=list)


def runtime_programs(units):
    """The runtime programs of the cells of units: one of each interface
    type, as the tables of a request type are of the type alone, and the
    layout of a binding of the binding alone; each is compiled once, in a part
    of its own of the program."""
    found = {}
    for unit in units:
        bounds = unit.interfaces()
        for name, request in unit.cells().items():
            bound = bounds[request.index]
            program = found.setdefault(bound.spelled,
                                       RuntimeProgram(bound.spelled))
            binding = program.bindings.setdefault(
                (unit, request.index), len(program.bindings))
            spelled = re.sub(r"\bI\d+\b", "I", spell_request(request))
            requested = program.requests.setdefault(
                spelled, len(program.requests))
            program.pairs.append((binding, requested, name))
    return list(found.values())


def runtime_name(index):
    return f"runtime_{index}"


def runtime_parts(program):
    """The parts of a runtime program, each with its source: of the
    requests, of the bindings, and the main one of the pairs."""
    requests = list(program.requests)
    bindings = list(program.bindings)
    parts = []
    request_parts = []
    for first in range(0, len(requests), RUNTIME_REQUESTS):
        name = f"requests_{first // RUNTIME_REQUESTS}"
        request_parts.append(name)
        parts.append((name, driver.render(
            "runtime.cpp.j2", part="requests", name=name,
            interface=program.interface, headers=[],
            requests=requests[first:first + RUNTIME_REQUESTS])))
    binding_parts = []
    for first in range(0, len(bindings), RUNTIME_BINDINGS):
        name = f"bindings_{first // RUNTIME_BINDINGS}"
        binding_parts.append(name)
        chunk = bindings[first:first + RUNTIME_BINDINGS]
        parts.append((name, driver.render(
            "runtime.cpp.j2", part="bindings", name=name, bindings=chunk,
            headers=driver.program_headers({unit.scope
                                            for unit, _ in chunk}))))
    pairs = [(f"bindings_{binding // RUNTIME_BINDINGS}"
              f"[{binding % RUNTIME_BINDINGS}]",
              f"requests_{requested // RUNTIME_REQUESTS}"
              f"[{requested % RUNTIME_REQUESTS}]")
             for binding, requested, _ in program.pairs]
    parts.append(("main", driver.render(
        "runtime.cpp.j2", part="main", headers=[],
        requests=request_parts, bindings=binding_parts, pairs=pairs)))
    return parts


def compile_runtime(compiler, flags, directory, name, program):
    """What the runtime container selects for the cells of a runtime
    program, by cell: what it records of each, or the error of a program
    that does not compile or crashes."""
    records, stderr, crash = driver.run_program(
        compiler, flags, directory, name, runtime_parts(program))
    if stderr is not None or crash is not None:
        error = driver.first_error(stderr) if stderr is not None else crash
        return {cell: {"runtime_error": error}
                for _, _, cell in program.pairs}
    return {program.pairs[record["pair"]][2]: {
        "selected": record["selected"], "modeled": record["modeled"]}
        for record in records}


def apply_semantics(cells, units, runtime=True):
    """A request a binding derives that the specification resolves nowhere,
    and one it does not derive for as long as the container that the
    specification resolves for the caller, are unexpected: what a request
    resolves into the context of a call is placed there, not published. So
    is a request C++ itself builds of what the binding holds, which the
    specification without the limitations of the library resolves nowhere,
    unless it rejects it on purpose; and one it rejects on purpose that it
    resolves. So is a request the runtime container selects another
    conversion of than the static container derives, when the runtime
    container is compared."""
    recorded = {cell["cell"]: cell for cell in cells}
    # A unit at a time, as the specification searches what a unit stores
    # once for all its requests.
    for unit in units:
        for name, request in unit.cells().items():
            if name in recorded:
                check_cell(recorded[name], unit, request)
                if runtime:
                    check_runtime(recorded[name])
    return cells


def check_runtime(cell):
    """Marks a cell unexpected where the runtime container selects another
    conversion of its tables than the static container derives, or its
    runtime program recorded nothing of it."""
    outcome = runtime_outcome(cell)
    if outcome == "unselected":
        error = ("what the runtime container selects is not recorded: " +
                 cell.get("runtime_error", "no record"))
    elif outcome == "apart":
        error = ("the runtime container selects otherwise: " +
                 ", ".join(runtime_differences(cell)))
    else:
        return
    if cell["outcome"] == Outcome.UNEXPECTED.value:
        error = f"{cell['error']}; {error}"
    cell.update(outcome=Outcome.UNEXPECTED.value, error=error)


def check_cell(cell, unit, request):
    """Marks a cell unexpected where it is not what the specification
    defines for it."""
    expected = unit.expectation(request)
    if expected is None:
        return
    anywhere, caller_expected = expected
    outcome = Outcome(cell["outcome"])
    if outcome in (Outcome.PUBLISHED, Outcome.PLACED) and not anywhere:
        cell.update(outcome=Outcome.UNEXPECTED.value,
                    error=f"{outcome.value}, which the specification "
                    "resolves nowhere")
    elif outcome in (Outcome.UNPUBLISHED, Outcome.PLACED) and caller_expected:
        cell.update(outcome=Outcome.UNEXPECTED.value,
                    error=f"{outcome.value}, which the specification "
                    "resolves for the caller")
    elif cell.get("cpp_routes"):
        error = cpp_mismatch(unit, request, cell["cpp_routes"], anywhere)
        if error is not None:
            cell.update(outcome=Outcome.UNEXPECTED.value, error=error)


def cpp_mismatch(unit, request, mask, anywhere):
    """What is unexpected of a request C++ builds of what the binding holds,
    or None: that the specification resolves it nowhere although it rejects
    it on no purpose, or resolves it although it rejects it on one."""
    rejected = rejection(unit, request, mask)
    if not anywhere:
        anywhere = unit.expectation(request, frozenset())[0]
    if not anywhere and rejected is None:
        return ("C++ builds it of what the binding holds, which the "
                "specification resolves nowhere")
    if anywhere and rejected is not None:
        return f"resolved, which the specification rejects: {rejected}"
    return None
