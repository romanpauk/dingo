"""The emptiness family: what a registration whose stored object holds no
leaf gives a request the specification says is defined when a layer is
empty. A unit is a registration of either container, its stored object
empty in its outermost layer, or in the layer that holds."""

from dataclasses import dataclass, replace

from spec import semantics
from spec.vocabulary import (Container, Entry, InterfaceSet, Leaf, Position,
                             Scope, Selection)

from . import driver
from .conversions import Unit as ConversionUnit
from .dimensions import Outcome

# The scopes an empty stored object is given to or made by: a cyclical one
# constructs its leaf in place.
SCOPES = (Scope.SHARED, Scope.EXTERNAL, Scope.UNIQUE)
INTERFACE_SETS = (InterfaceSet.DEFAULT, InterfaceSet.INTERFACE)


@dataclass(frozen=True)
class Unit(ConversionUnit):
    """A registration whose stored object holds no leaf: its outermost layer
    is empty, or, inner, holds an empty one."""
    inner: bool = False
    # The requests probed, when only some of them are.
    only: tuple = None

    @property
    def base(self):
        return "/".join((self.container.value, self.scope.value,
                         self.form.value, self.leaf.value,
                         self.interfaces_declared.value,
                         "inner" if self.inner else "outer"))

    def defined(self, request):
        """Whether the specification resolves a request in a call as defined
        when a layer is empty."""
        bound = request.interface.bound
        if bound.element == "other":
            return False
        answer = semantics.fewest(
            self.scope, self.form, self.leaf, bound, request.shape,
            request.category, Position.DIRECT, Entry.INVOKE,
            semantics.LIMITATIONS, interfaces=self.interfaces_declared,
            defined=True)
        return answer is not None and answer[1]

    def probed(self):
        """The requests probed: those defined when a layer is empty."""
        if self.only is not None:
            return list(self.only)
        return [request for request in self.requests()
                if self.defined(request)]

    def halves(self):
        """The unit as two, each probing half of its requests."""
        probed = self.probed()
        middle = len(probed) // 2
        return [replace(self, only=tuple(probed[:middle])),
                replace(self, only=tuple(probed[middle:]))]


def empty_layers(form, inner):
    """Whether a stored form has the layer a unit leaves empty: the
    outermost one, or the one it holds; no array is."""
    layers = semantics.FORMS[form]
    if any(semantics.LAYERS[name].decays_to for name in layers):
        return False
    depth = 2 if inner else 1
    return (len(layers) >= depth and
            semantics.LAYERS[layers[depth - 1]].may_be_empty)


def units():
    """Every registration of an empty stored object the library accepts."""
    return [Unit(container, scope, form, Leaf.COPYABLE, interfaces,
                 Selection.PLAIN, inner)
            for container in Container for scope in SCOPES
            for form in semantics.FORMS for interfaces in INTERFACE_SETS
            for inner in (False, True)
            if empty_layers(form, inner) and
            not (container is Container.STATIC and scope is Scope.EXTERNAL)
            and semantics.registrable(container, scope, form, Leaf.COPYABLE,
                                      interfaces)]


def batch_name(index):
    return f"emptiness_{index}"


def compile_units(compiler, flags, directory, name, batch):
    """The records of a batch of units, compiled into one program. A batch
    that does not compile or run is compiled a unit at a time, and a unit
    half of its requests at a time, down to a request, which records the
    first error or how it crashed."""
    source = driver.render("emptiness.cpp.j2", units=batch)
    records, stderr, crash = driver.build_and_run(compiler, flags, directory,
                                                  name, source)
    if stderr is None and crash is None:
        return records
    if len(batch) > 1:
        parts = batch
    elif len(batch[0].probed()) > 1:
        parts = batch[0].halves()
    else:
        parts = []
    if parts:
        found = driver.concurrently([
            lambda index=index, unit=unit: compile_units(
                compiler, flags, directory, f"{name}_{index}", [unit])
            for index, unit in enumerate(parts)])
        return [record for records in found for record in records]
    error = (driver.first_error(stderr) if stderr is not None else crash)
    outcome = (Outcome.DOES_NOT_COMPILE if stderr is not None
               else Outcome.CRASHED)
    return [{"cell": batch[0].prefix(request) + request.name,
             "outcome": outcome.value, "error": error}
            for request in batch[0].probed()]


def apply_semantics(cells):
    """A probed request that does not give what holds no leaf is
    unexpected."""
    for cell in cells:
        if cell["outcome"] not in ("defined", Outcome.DOES_NOT_COMPILE.value,
                                   Outcome.CRASHED.value,
                                   Outcome.VIOLATED.value):
            cell.update(outcome=Outcome.UNEXPECTED.value,
                        error=f"{cell['outcome']}, where the specification "
                        "gives what an empty layer gives: "
                        f"{cell.get('error') or ''}")
    return cells
