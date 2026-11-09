"""The main family: every request of the grammar over the interfaces a
registration binds, made through every entry point, in every position."""

from dataclasses import dataclass

from spec import semantics
from spec.vocabulary import (NESTED_SHAPES, Answer, Container, Entry,
                             InterfaceSet, Leaf, Position, Scope, Selection,
                             StoredForm)

from . import cover, driver
from .dimensions import Keeps, Outcome
from .model import COMPOSED_SHAPES, SHAPES, Interface, Probe, grammar

# The leaves a registration stores.
LEAVES = (Leaf.COPYABLE, Leaf.MOVE_ONLY, Leaf.IMMOVABLE, Leaf.FACTORY)

# The types of the probe an element of an interface is, and how a layer of
# one is spelled around what it holds.
ELEMENTS = {"interface": "leaf_interface", "other": "other"}
LEAF_TYPES = {Leaf.COPYABLE: "leaf", Leaf.MOVE_ONLY: "move_only_leaf",
              Leaf.IMMOVABLE: "immovable_leaf", Leaf.FACTORY: "factory_leaf"}
LAYERS = {"array2": "{}[2]", "variant": "std::variant<{}, {}other>"}


def spell(bound, leaf, namespace):
    """An interface of the specification as the probe's types spell it."""
    spelled = namespace + (LEAF_TYPES[leaf] if bound.element == "leaf"
                           else ELEMENTS[bound.element])
    for layer in reversed(bound.layers):
        spelled = LAYERS[layer].format(spelled, namespace)
    return spelled


def interface_of(bound, leaf):
    return Interface(spell(bound, leaf, ""),
                     spell(bound, leaf, "dingo::model::"),
                     abstract=bound == semantics.INTERFACE,
                     array=bound.layers[:1] == ("array2",), bound=bound)


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
        """The interfaces the specification says the registration binds."""
        return tuple(interface_of(bound, self.leaf)
                     for bound in semantics.bound_interfaces(
                         self.form, self.interfaces_declared, self.selection))

    def probes(self, position=None):
        """The probes, in a position or in all of them, each with why its
        request is not valid C++, or None."""
        requests = self.grammar(self.interfaces())
        return [(Probe(operation, request), request.invalid)
                for operation in OPERATIONS
                if position is None or operation.position is position
                for request in requests]

    @property
    def composes(self):
        """Whether the registration is requested as every shape of two
        wrappers too: one of a leaf that can be moved, stored plainly or in
        one wrapper, however it is declared and selected."""
        return (self.form in (StoredForm.PLAIN, StoredForm.SHARED_HANDLE,
                              StoredForm.UNIQUE_HANDLE, StoredForm.OPTIONAL)
                and self.leaf in (Leaf.COPYABLE, Leaf.MOVE_ONLY))

    def grammar(self, interfaces):
        """The requests of the grammar over interfaces: with the shapes of
        the stored form when it has two wrappers, and every shape of two
        wrappers when the registration composes."""
        shapes = SHAPES + NESTED_SHAPES.get(self.form, ())
        if self.composes:
            shapes += tuple(shape for shape in COMPOSED_SHAPES
                            if shape not in shapes)
        return grammar(interfaces, shapes)

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
        """For each interface the registration binds, the valid requests of
        the grammar: every request it publishes for the interface has to be
        one of them."""
        bound = self.interfaces()
        requests = self.grammar(bound)
        return [(index, interface, [request for request in requests
                                    if request.index == index and
                                    request.invalid is None])
                for index, interface in enumerate(bound)]

    def expectation(self, probe):
        """What the specification expects of a probe: the answer, and the
        known limitations of the library it is expected with."""
        if not semantics.registrable(self.container, self.scope, self.form,
                                     self.leaf, self.interfaces_declared):
            return Answer.NOT_REGISTRABLE, ()
        request = (self.scope, self.form, self.leaf,
                   probe.request.interface.bound, probe.request.shape,
                   probe.request.category, probe.operation.position,
                   probe.operation.entry)
        intended, limitations = semantics.expected(
            *request, self.selection, self.interfaces_declared)
        if intended and not limitations:
            return Answer.RESOLVED, ()
        return semantics.failure(self.container, *request, self.selection,
                                 self.interfaces_declared), limitations

    def fewest(self, probe):
        """The fewest copies and moves of the leaf the specification
        resolves a probe with, as (copies, moves)."""
        return semantics.fewest(
            self.scope, self.form, self.leaf, probe.request.interface.bound,
            probe.request.shape, probe.request.category,
            probe.operation.position, probe.operation.entry,
            semantics.LIMITATIONS, self.selection, self.interfaces_declared,
            self.container)

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
    dimensions that a registration the library accepts holds. A registration
    it does not accept resolves nothing, so it covers nothing."""
    return cover.covering(
        strength, Scope, StoredForm, LEAVES, InterfaceSet, Selection,
        valid=lambda registration: semantics.registrable(
            Container.RUNTIME, *registration[:4]))


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


def apply_semantics(cells, units):
    """Compares each cell of the probes of the units with what the
    specification expects of it. A cell a known limitation keeps from
    resolving is recorded as limited, one that resolves although a limitation
    says it does not as a stale limitation, and any other outcome than the
    expected one as unexpected."""
    expected_of = {unit.cell(probe): (unit, probe) for unit in units
                   for probe, invalid in unit.probes() if not invalid}
    for cell in cells:
        if (cell["cell"] not in expected_of or
                Outcome(cell["outcome"]).is_defect):
            continue
        unit, probe = expected_of[cell["cell"]]
        expected, limitations = unit.expectation(probe)
        reasons = ", ".join(limitation.value for limitation in limitations)
        if limitations and cell["outcome"] == Outcome.RESOLVED:
            cell["outcome"] = Outcome.STALE_LIMITATION.value
            cell["error"] = f"no longer limited: {reasons}"
        elif cell["outcome"] != expected:
            error = cell.get("error")
            cell["error"] = (f"{cell['outcome']}, expected {expected.value}" +
                             (f" with {reasons}" if reasons else "") +
                             (f": {error}" if error else ""))
            cell["outcome"] = Outcome.UNEXPECTED.value
        elif limitations:
            cell["outcome"] = Outcome.LIMITED.value
            cell["error"] = f"{expected.value} with {reasons}"
        elif cell["outcome"] == Outcome.RESOLVED:
            excess(cell, unit.fewest(probe))
    return cells


def excess(cell, fewest):
    """Records a resolved cell whose resolutions copy or move the leaf more
    often than the cheapest derivation of the specification as excess."""
    copies, moves = fewest
    made = list(zip(cell.get("copies", ()), cell.get("moves", ())))
    if any(copied > copies or moved > moves for copied, moved in made):
        cell["outcome"] = Outcome.EXCESS.value
        cell["error"] = (
            "copies and moves the leaf " +
            ", then ".join(f"{copied} and {moved} times"
                           for copied, moved in made) +
            f", where it needs {copies} and {moves}")
