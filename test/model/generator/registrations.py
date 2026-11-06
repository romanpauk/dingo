"""The main family: every request of the grammar over the interfaces a
registration binds, made through every entry point, in every position."""

from dataclasses import dataclass

from . import cover, driver
from .dimensions import (Container, Entry, InterfaceSet, Keeps, Leaf,
                         Position, Scope, Selection, StoredForm)
from .model import Probe, grammar, interface

# The leaves a registration stores.
LEAVES = (Leaf.COPYABLE, Leaf.MOVE_ONLY, Leaf.IMMOVABLE, Leaf.FACTORY)

# The interfaces a registration names, around its leaf L, when the library
# rejects it and so does not say which it binds; whether each is abstract.
NAMED = {
    InterfaceSet.DEFAULT: (("L", False),),
    InterfaceSet.INTERFACE: (("leaf_interface", True),),
    InterfaceSet.BOTH: (("L", False), ("leaf_interface", True)),
}
LEAF_NAMES = {Leaf.COPYABLE: "leaf", Leaf.MOVE_ONLY: "move_only_leaf",
              Leaf.IMMOVABLE: "immovable_leaf", Leaf.FACTORY: "factory_leaf"}


@dataclass(frozen=True, order=True)
class Operation:
    """A position of a request and the entry point it is made through; a
    direct one is named by its entry point."""
    position: Position
    entry: Entry

    @property
    def name(self):
        if self.position is Position.DIRECT:
            return self.entry.value
        return f"{self.position.value}_{self.entry.value}"


OPERATIONS = tuple(Operation(position, entry) for position in Position
                   for entry in Entry)


@dataclass(frozen=True)
class Registration:
    container: Container
    scope: Scope
    form: StoredForm
    leaf: Leaf
    interfaces_declared: InterfaceSet
    selection: Selection
    # The interfaces the registration binds, as the library says.
    bound: tuple = ()

    @property
    def binding(self):
        """The interfaces declared and how a request selects them, as a cell
        names them."""
        return f"{self.interfaces_declared.value}/{self.selection.value}"

    @property
    def name(self):
        return "__".join((self.container, self.scope, self.form, self.leaf,
                          self.interfaces_declared, self.selection))

    @property
    def survey_key(self):
        return "__".join((self.scope, self.form, self.leaf,
                          self.interfaces_declared, self.selection))

    @property
    def dimensions(self):
        return (self.scope, self.form, self.leaf, self.interfaces_declared,
                self.selection)

    def interfaces(self):
        """The interfaces the registration binds; when the library rejects
        it, the one it names."""
        if self.bound:
            return self.bound
        return tuple(
            interface(f"dingo::model::{named.replace('L', LEAF_NAMES[self.leaf])}",
                      abstract)
            for named, abstract in NAMED[self.interfaces_declared])

    def probes(self, position=None):
        """The probes, in a position or in all of them, each with why its
        request is not valid C++, or None."""
        requests = grammar(self.interfaces())
        return [(Probe(operation, request), request.invalid)
                for operation in OPERATIONS
                if position is None or operation.position is position
                for request in requests]

    def cell(self, probe):
        return "/".join((self.container, self.scope, self.form, self.leaf,
                         self.binding, probe.operation.name,
                         probe.request.interface.name, probe.request.name))

    def published_prefix(self, bound):
        """The cell of a request the registration publishes for bound, without
        the request."""
        return "/".join((self.container, self.scope, self.form, self.leaf,
                         self.binding, "published", bound.name, ""))

    def checks(self):
        """For each interface the library says the registration binds, the
        valid requests of the grammar: every request it publishes for the
        interface has to be one of them."""
        requests = grammar(self.bound)
        return [(index, bound, [request for request in requests
                                if request.index == index and
                                request.invalid is None])
                for index, bound in enumerate(self.bound)]

    def keeps(self, probe):
        if self.scope is Scope.UNIQUE:
            return Keeps.NOTHING
        if probe.operation == Operation(Position.DIRECT, Entry.CONSTRUCT):
            return Keeps.ADDRESSES
        return Keeps.INSTANCE

    def render(self, probes):
        return driver.render("probe.cpp.j2", r=self, probes=probes)


def registrations():
    return [Registration(container, scope, form, leaf, interfaces, selection)
            for container in Container for scope in Scope
            for form in StoredForm for leaf in LEAVES
            for interfaces in InterfaceSet for selection in Selection]


def covering(strength):
    """The registrations, as (scope, form, leaf, interfaces, selection), that
    together hold every combination of values of `strength` of these
    dimensions."""
    return cover.covering(strength, Scope, StoredForm, LEAVES, InterfaceSet,
                          Selection)


def probe_registration(compiler, flags, directory, registration, position):
    """Makes the probes of a registration in a position, the invalid ones
    aside, in a program of their own, which bounds the memory of a
    compile."""
    outcomes = []
    probes = []
    for probe, invalid in registration.probes(position):
        if invalid:
            outcomes.append(driver.invalid(registration, probe, invalid))
        else:
            probes.append(probe)
    return outcomes + driver.probe_shared(
        compiler, flags, directory, registration,
        f"{registration.name}__{position.value}", probes)
