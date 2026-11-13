"""The conversion family: what the conversion layer of every binding answers
every request of the grammar, decided as the code compiles, compared with the
specification. A unit is a registration of the runtime container, selected
plainly: the request lists of a binding are those of both containers, and
selection does not change them."""

from dataclasses import dataclass

from spec import semantics
from spec.vocabulary import (Container, Entry, InterfaceSet, Leaf, Position,
                             Scope, Selection, Shape, StoredForm)

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

    def published_prefix(self, bound):
        return f"{self.base}/published/{bound.name}/"

    def cells(self):
        return {self.prefix(request) + request.name: request
                for request in self.requests()}

    def expectation(self, request):
        """Whether the specification, with the limitations of the library,
        resolves a request at all, in a call, and for the caller, in a
        resolve; None when it says nothing of the request. It does not of a
        storage that cannot be made, whose request lists are of its types
        alone, nor of the other alternative of an alternative type, which a
        probe's never holds: that is a copyable type of its own."""
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
                *args, position, entry, limits=semantics.LIMITATIONS,
                interfaces=self.interfaces_declared)
        return (resolves(Position.DIRECT, Entry.INVOKE),
                resolves(Position.DIRECT, Entry.RESOLVE))


    def converted(self, request):
        """The fewest copies and moves of the leaf the conversion of a
        request makes, as the specification says; None when it says nothing
        of them, as of the other alternative, which holds no leaf."""
        bound = request.interface.bound
        if bound.element == "other":
            return None
        return semantics.converted(self.scope, self.form, self.leaf, bound,
                                   request.shape, request.category,
                                   semantics.LIMITATIONS,
                                   self.interfaces_declared)


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
    """A request the conversion layer publishes that the specification
    resolves nowhere, and one it does not publish that the specification
    resolves for the caller, are unexpected. A published request has to be
    resolved somewhere; one the caller resolves, as a value or through a scope
    that keeps it, has to be published: what a request resolves into the
    context of a call is placed there, not published. The conversion of a
    published request that copies or moves the leaf more often than the
    specification needs to is excess."""
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
        if (outcome in (Outcome.PUBLISHED, Outcome.COMPOSED, Outcome.DERIVED,
                        Outcome.ADOPTED,
                        Outcome.PLACED) and not anywhere):
            cell.update(outcome=Outcome.UNEXPECTED.value,
                        error=f"{outcome.value}, which the specification "
                        "resolves nowhere")
        elif (outcome in (Outcome.UNPUBLISHED, Outcome.UNCONVERTIBLE,
                          Outcome.PLACED) and for_caller):
            cell.update(outcome=Outcome.UNEXPECTED.value,
                        error=f"{outcome.value}, which the specification "
                        "resolves for the caller")
        elif outcome is Outcome.PUBLISHED and "copies" in cell:
            excess(cell, unit.converted(request))
    for unit in units:
        empty_safe(unit, cells)
    return cells


def empty_safe(unit, cells):
    """A request a binding answers through a step that does not fail when a
    layer is empty, a pointer into the layer or the optional it holds, has
    to be published in one of its categories: one a request derives goes
    through an element that may be absent. The first category of each that
    the specification resolves is unexpected when none is."""
    by_cell = {cell["cell"]: cell for cell in cells}
    requests = {(request.interface.name, request.shape, request.category):
                request for request in unit.requests()}
    for interface in {request.interface for request in requests.values()}:
        if interface.bound.element == "other":
            continue
        for shape, categories in semantics.empty_safe_sources(
                unit.scope, unit.form, unit.leaf, interface.bound,
                unit.interfaces_declared):
            asked = [requests[key] for key in
                     ((interface.name, shape, category)
                      for category in categories) if key in requests]
            answered = [by_cell.get(unit.prefix(request) + request.name)
                        for request in asked]
            if any(cell is not None and
                   cell["outcome"] == Outcome.PUBLISHED.value
                   for cell in answered):
                continue
            for request, cell in zip(asked, answered):
                expected = unit.expectation(request)
                if cell is None or not expected or not any(expected):
                    continue
                cell.update(outcome=Outcome.UNEXPECTED.value,
                            error=f"{cell['outcome']}, through an element "
                            "that may be absent where a layer it goes "
                            "through is empty")
                break


def excess(cell, fewest):
    """Records a published cell whose conversion copies or moves the leaf
    more often than the specification needs to as excess."""
    if fewest is None:
        return
    copies, moves = fewest
    if cell["copies"] > copies or cell["moves"] > moves:
        cell.update(outcome=Outcome.EXCESS.value,
                    error=f"copies and moves the leaf {cell['copies']} and "
                    f"{cell['moves']} times, where it needs {copies} and "
                    f"{moves}")
