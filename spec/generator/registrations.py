"""The main family: every request of the grammar over the interfaces a
registration binds, made through every entry point, in every position."""

from dataclasses import dataclass

from spec import semantics
from spec.vocabulary import (NESTED_SHAPES, Answer, Category, Container,
                             Entry, InterfaceSet, Leaf, Position, Scope,
                             Selection, Shape, StoredForm)

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

    def render_part(self, part, probes, first, registers=False):
        """The part of a program of the registration: its probes, numbered
        from first, and the registration alone where registers says so."""
        return driver.render("probe_part.cpp.j2", r=self, part=part,
                             probes=probes, first=first, registers=registers)

    def stores(self, probe):
        """Whether a probe requests what the registration stores, as it is:
        by reference from a scope that keeps it, and as an rvalue reference
        a call takes from a scope that makes it, so that the storage alone
        copies and moves the leaf."""
        source = semantics.source(self.scope, self.form,
                                  self.interfaces_declared)
        request = probe.request
        bound = request.interface.bound
        if (probe.operation.position is not Position.DIRECT or
                semantics.SHAPES[request.shape] + bound.layers !=
                source.layers or bound.element != source.element or
                request.shape.const_element):
            return False
        if semantics.SCOPES[self.scope].makes:
            return (probe.operation.entry is Entry.INVOKE and
                    request.category is Category.RVALUE_REFERENCE)
        return (probe.operation.entry is Entry.RESOLVE and
                request.category is Category.LVALUE_REFERENCE)

    def passes(self, probe):
        """Whether a probe requests the leaf a plain registration stores,
        which no conversion copies or moves: what resolving it copies and
        moves is what passing it on in its position does."""
        return (probe.request.shape is Shape.OBJECT and
                probe.request.interface.bound == semantics.LEAF)


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


@dataclass(frozen=True)
class Member:
    """A probe of a registration, in a program of several registrations."""
    registration: Registration
    probe: Probe

    @property
    def operation(self):
        return self.probe.operation

    @property
    def request_key(self):
        return self.probe.request_key


@dataclass(frozen=True)
class Group:
    """The unit of a program of several registrations, each of which makes a
    part of it. Its probes are the members of the registrations; compiling
    the container and the probe machinery once for all of them is what makes
    the program cheaper than one for each."""
    registrations: tuple

    def cell(self, member):
        return member.registration.cell(member.probe)

    def render(self, members, registers=False):
        """The program of members, in their order: a part for each run of
        them of one registration, a part for each registration when the
        registration alone is made. The probes are numbered across the
        program, as the driver tells them apart by number."""
        runs = []
        for member in members:
            if runs and runs[-1][0] == member.registration:
                runs[-1][1].append(member.probe)
            else:
                runs.append((member.registration, [member.probe]))
        if registers:
            runs = [(registration, []) for registration in self.registrations]
        parts = []
        first = 0
        for registration, probes in runs:
            parts.append(registration.render_part(len(parts), probes, first,
                                                  registers))
            first += len(probes)
        return driver.render("probe.cpp.j2", parts=parts)


def registration_outcome(compiler, flags, directory, registration, name,
                         probes, expected):
    """The records of probes the specification expects all to answer as their
    registration does, of one program, or None where the program does not
    answer so: a registration that is not registrable answers each probe so,
    and one the library rejects as it registers it rejects each probe, with
    its error. One program tells it, rather than one for each probe, which
    could not tell the error of the registration they share apart."""
    group = Group((registration,))
    if expected is Answer.NOT_REGISTRABLE:
        records, stderr, crash = driver.build_and_run(
            compiler, flags, directory, f"{name}__registrable",
            group.render([Member(registration, probes[0])]))
        if (stderr is None and crash is None and len(records) == 1 and
                records[0]["outcome"] == Outcome.NOT_REGISTRABLE.value):
            return [{"cell": registration.cell(probe),
                     "outcome": Outcome.NOT_REGISTRABLE.value}
                    for probe in probes]
        return None
    _, stderr, _ = driver.build_and_run(
        compiler, flags + [driver.COMPILE_ONLY], directory,
        f"{name}__registers", group.render([], registers=True))
    if stderr is None or stderr == driver.OUT_OF_MEMORY:
        return None
    error = driver.first_error(stderr)
    return [driver.failure(registration, probe, error) for probe in probes]


def plan_registration(compiler, flags, directory, registration, position,
                      selected=None):
    """The probes of a registration in a position, those selected when told
    which: (outcomes, members, rejected). Outcomes are those of the invalid
    probes, and of all of them when the specification expects each to answer
    as the registration does and one program tells it. The members are to be
    made in programs of several registrations; the rejected ones, which the
    specification expects the library to reject as they compile, apart from
    the others, so that a program of those compiles at once."""
    outcomes = []
    probes = []
    rejected = []
    for probe, invalid in registration.probes(position):
        if selected is not None and not selected(registration, probe):
            continue
        if invalid:
            outcomes.append(driver.invalid(registration, probe, invalid))
        elif registration.expectation(probe)[0] is Answer.REJECTED:
            rejected.append(probe)
        else:
            probes.append(probe)
    expected = {registration.expectation(probe)[0]
                for probe in probes + rejected}
    if len(expected) == 1 and next(iter(expected)) in (
            Answer.NOT_REGISTRABLE, Answer.REJECTED):
        found = registration_outcome(
            compiler, flags, directory, registration,
            f"{registration.name}__{position.value}", probes + rejected,
            next(iter(expected)))
        if found is not None:
            return outcomes + found, [], []
    return (outcomes, [Member(registration, probe) for probe in probes],
            [Member(registration, probe) for probe in rejected])


def packed(members):
    """Members in programs of at most GROUP_PROBES probes of at most
    GROUP_REGISTRATIONS registrations, in their order."""
    programs = []
    for member in members:
        if programs:
            current = programs[-1]
            registrations = {other.registration for other in current}
            if len(current) < driver.GROUP_PROBES and (
                    member.registration in registrations or
                    len(registrations) < driver.GROUP_REGISTRATIONS):
                current.append(member)
                continue
        programs.append([member])
    return programs


def probe_registrations(compiler, flags, directory, tasks, selected, map):
    """Makes the probes of registrations in positions, (registration,
    position) tasks, by map, which runs a function over a list. The probes
    of several registrations are made in one program, as the program of the
    direct position checks what each registration publishes."""
    planned = list(map(lambda task: plan_registration(
        compiler, flags, directory, *task, selected), tasks))
    outcomes = [outcome for found, _, _ in planned for outcome in found]
    members = [member for _, found, _ in planned for member in found]
    # The first error of a rejected probe depends on what else the program
    # instantiates: its probes are made with the others of its registration
    # and position only, as their errors are recorded.
    rejected = [found for _, _, found in planned if found]

    def group_of(part):
        return Group(tuple(dict.fromkeys(
            member.registration for member in part)))

    calls = [lambda index=index, part=part: driver.probe_shared(
        compiler, flags, directory, group_of(part), f"probes_{index}", part)
        for index, part in enumerate(packed(members))]
    calls += [lambda index=index, part=part: driver.probe_expecting_rejection(
        compiler, flags, directory, group_of(part), f"rejected_{index}", part)
        for index, part in enumerate(rejected)]
    for found in map(lambda call: call(), calls):
        outcomes += found
    return outcomes


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
