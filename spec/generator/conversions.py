"""The conversion family: what the binding of every registration plans for
every request of the grammar, decided as the code compiles, compared with the
specification. A unit is a registration of the runtime container, selected
plainly: both containers plan a request of a binding alike, and selection
does not change it."""

from dataclasses import dataclass

from spec import semantics
from spec.vocabulary import (Category, Container, Entry, InterfaceSet, Leaf,
                             Position, Scope, Selection, Shape, StoredForm)

from . import driver
from .dimensions import Outcome
from .model import grammar
from .registrations import LEAVES, Registration


@dataclass(frozen=True)
class Unit(Registration):
    """A registration whose conversion layer is asked."""

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
        """Whether the specification, under limitations, resolves a request
        at all, in a call, and for the caller, in a resolve; None when it
        says nothing of the request. It does not of a storage that cannot be
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


# What the specification refuses on purpose, of what C++ builds of what a
# binding holds: each is a reason and whether it refuses a route of a unit's
# request.
REFUSALS = (
    ("a new layer does not hold a copy of an instance, nor of a value that "
     "shares nothing",
     lambda unit, request, route: route.how == WRAPS and route.value),
    ("a cyclical scope publishes its instance before it constructs it: a "
     "request refers to its stored object and what that holds in place, and "
     "copies only a stored handle",
     lambda unit, request, route: unit.scope is Scope.SHARED_CYCLICAL and (
         route.depth >= 2 if route.how == REFERS
         else route.depth >= 1 or route.value)),
    ("an rvalue reference is answered by a value moved out of what the "
     "request owns, which an immovable leaf, or an optional of one, cannot "
     "be",
     lambda unit, request, route: (
         request.category is Category.RVALUE_REFERENCE and
         unit.leaf is Leaf.IMMOVABLE and not request.shape.is_array and
         set(request.shape.wrappers) <= {"optional"})),
)


def refused(unit, request, mask):
    """Why the specification refuses every route C++ builds a request by,
    or None when it refuses none or only some."""
    reasons = []
    for route in routes(mask):
        found = [reason for reason, refuses in REFUSALS
                 if refuses(unit, request, route)]
        if not found:
            return None
        reasons += [reason for reason in found if reason not in reasons]
    return "; ".join(reasons) if reasons else None


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
    source = driver.render("conversion.cpp.j2", units=batch, none=None)
    records, stderr, crash = driver.build_and_run(compiler, flags, directory,
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


def apply_semantics(cells, units):
    """A request a binding plans that the specification resolves nowhere,
    and one it does not plan for as long as the container that the
    specification resolves for the caller, are unexpected: what a request
    resolves into the context of a call is placed there, not published. So
    is a request C++ itself builds of what the binding holds, which the
    specification without the limitations of the library resolves nowhere,
    unless it refuses it on purpose; and one it refuses on purpose that it
    resolves."""
    requests = {cell: (unit, request) for unit in units
                for cell, request in unit.cells().items()}
    for cell in cells:
        if cell["cell"] not in requests:
            continue
        unit, request = requests[cell["cell"]]
        expected = unit.expectation(request)
        if expected is None:
            continue
        anywhere, for_caller = expected
        outcome = Outcome(cell["outcome"])
        if (outcome in (Outcome.PUBLISHED, Outcome.PLACED) and
                not anywhere):
            cell.update(outcome=Outcome.UNEXPECTED.value,
                        error=f"{outcome.value}, which the specification "
                        "resolves nowhere")
        elif (outcome in (Outcome.UNPUBLISHED, Outcome.PLACED) and
              for_caller):
            cell.update(outcome=Outcome.UNEXPECTED.value,
                        error=f"{outcome.value}, which the specification "
                        "resolves for the caller")
        elif cell.get("native"):
            error = native_error(unit, request, cell["native"], anywhere)
            if error is not None:
                cell.update(outcome=Outcome.UNEXPECTED.value, error=error)
    return cells


def native_error(unit, request, mask, anywhere):
    """What is unexpected of a request C++ builds of what the binding holds,
    or None: that the specification resolves it nowhere although it refuses
    it on no purpose, or resolves it although it refuses it on one."""
    refusal = refused(unit, request, mask)
    if not anywhere:
        anywhere = unit.expectation(request, frozenset())[0]
    if not anywhere and refusal is None:
        return ("C++ builds it of what the binding holds, which the "
                "specification resolves nowhere")
    if anywhere and refusal is not None:
        return f"resolved, which the specification is said to refuse: {refusal}"
    return None
