"""The main family: every request of the grammar over the interfaces a
registration binds, made through every entry point, in every position."""

from dataclasses import dataclass

from . import cover, driver
from .dimensions import (Binding, Container, Entry, Keeps, Leaf, Position,
                         Scope, StoredForm)
from .model import Probe, grammar, interface

# The leaves a registration stores.
LEAVES = (Leaf.COPYABLE, Leaf.MOVE_ONLY)

# The interface a binding names, by the leaf it stores, when the library
# rejects the registration and so does not say which it binds, and whether
# that interface is abstract.
NAMED = {
    Binding.CONCRETE: (None, False),
    Binding.INTERFACE: ("leaf_interface", True),
    Binding.ANNOTATED: (None, False),
    Binding.KEYED: (None, False),
}
LEAF_NAMES = {Leaf.COPYABLE: "leaf", Leaf.MOVE_ONLY: "move_only_leaf"}


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
    binding: Binding
    # The interfaces the registration binds, as the library says.
    bound: tuple = ()

    @property
    def name(self):
        return "__".join((self.container, self.scope, self.form, self.leaf,
                          self.binding))

    @property
    def survey_key(self):
        return "__".join((self.scope, self.form, self.leaf, self.binding))

    @property
    def dimensions(self):
        return (self.scope, self.form, self.leaf, self.binding)

    def interfaces(self):
        """The interfaces the registration binds; when the library rejects
        it, the one it names."""
        if self.bound:
            return self.bound
        named, abstract = NAMED[self.binding]
        return (interface(f"dingo::model::{named or LEAF_NAMES[self.leaf]}",
                          abstract),)

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
    return [Registration(container, scope, form, leaf, binding)
            for container in Container for scope in Scope
            for form in StoredForm for leaf in LEAVES for binding in Binding]


def covering_pairs():
    """The registrations, as (scope, form, leaf, binding), that together hold
    every pair of values of two of these dimensions."""
    return cover.covering_pairs(Scope, StoredForm, LEAVES, Binding)


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
