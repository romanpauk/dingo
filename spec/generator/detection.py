"""The constructor detection family: classes generated around every request
of the grammar over a supported leaf, what detection says about them, and
what constructing them does."""

from dataclasses import dataclass
from typing import Optional

from spec.vocabulary import (Category, Container, Leaf, Scope, Selection,
                             Shape, StoredForm)

from . import cover, driver

from .dimensions import Construction, Detection, Kind, Making, Outcome
from .model import Probe, grammar, interface

# The interface a parameter is a shape of, by the leaf it is resolved from.
INTERFACES = {
    Leaf.COPYABLE: interface("dingo::model::leaf"),
    Leaf.MOVE_ONLY: interface("dingo::model::move_only_leaf"),
    Leaf.INTERFACE: interface("dingo::model::leaf_interface", abstract=True),
}

# The registrations a leaf is resolved from.
PROVIDERS = (
    (Scope.UNIQUE, StoredForm.PLAIN),
    (Scope.SHARED, StoredForm.PLAIN),
    (Scope.SHARED, StoredForm.SHARED_HANDLE),
    (Scope.UNIQUE, StoredForm.UNIQUE_HANDLE),
)

# How a class is made with each constructor. A factory, which an explicit
# constructor and signature detection are, is taken only by construct and by
# a registration; shape detection is what the containers do without one.
MAKINGS = {
    Construction.EXPLICIT: (Making.CONSTRUCT, Making.REGISTERED),
    Construction.SHAPE: (Making.RESOLVE, Making.CONSTRUCT, Making.INVOKE,
                         Making.REGISTERED),
    Construction.SIGNATURE: (Making.CONSTRUCT, Making.REGISTERED),
}


@dataclass(frozen=True, order=True)
class Detect:
    """What detection says about a class."""
    detection: Detection

    @property
    def name(self):
        return f"detect_{self.detection.value}"


@dataclass(frozen=True, order=True)
class Make:
    """A class made with a constructor."""
    construction: Construction
    making: Making

    @property
    def name(self):
        return f"{self.construction.value}_{self.making.value}"

    @property
    def reference(self):
        """The making with the explicit constructor a detected one has to
        agree with: a registration with a registration, a class returned to
        the caller with construct. A parameter of a call has none: it is
        returned by value as construct returns it, but may refer to what the
        request makes, as a class a shared scope keeps may; a detected one is
        checked only for defects."""
        if self.making is Making.INVOKE:
            return None
        making = (Making.REGISTERED if self.making is Making.REGISTERED
                  else Making.CONSTRUCT)
        return Make(Construction.EXPLICIT, making)


@dataclass(frozen=True)
class Unit:
    """The probes of one kind of class around the requests of one leaf and
    selection: what detection says, when it has no container, or how the
    class is made in a container over a registration of the leaf."""
    leaf: Leaf
    selection: Selection
    kind: Kind
    container: Optional[Container] = None
    provider: Optional[tuple] = None

    @property
    def interface(self):
        return INTERFACES[self.leaf]

    @property
    def provider_name(self):
        if self.provider is None:
            return "-"
        scope, form = self.provider
        return f"{scope.value}:{form.value}"

    @property
    def container_name(self):
        return self.container.value if self.container else "-"

    @property
    def name(self):
        return "__".join(("detection", self.container_name,
                          self.provider_name.replace(":", "_"), self.leaf,
                          self.selection, self.kind))

    def operations(self):
        if self.container is None:
            return [Detect(detection) for detection in Detection]
        return [Make(construction, making)
                for construction, makings in MAKINGS.items()
                for making in makings]

    def probes(self):
        requests = grammar([self.interface])
        return [(Probe(operation, request), request.invalid)
                for operation in self.operations() for request in requests]

    def cell(self, probe):
        return "/".join(("detection", self.container_name,
                         self.provider_name, self.leaf, self.selection,
                         self.kind, probe.operation.name,
                         probe.request.name))

    def render(self, probes):
        return driver.render("detection.cpp.j2", unit=self, probes=probes)


def units(strength=None):
    """Every unit; with a strength, the units that make classes over the
    combinations of leaf, selection, kind and provider that hold every
    combination of values of `strength` of them, in both containers. What
    detection says is probed for every class."""
    covered = cover.covering(strength, list(INTERFACES), Selection, Kind,
                             PROVIDERS) if strength else None
    found = []
    for leaf in INTERFACES:
        for selection in Selection:
            for kind in Kind:
                found.append(Unit(leaf, selection, kind))
                found += [Unit(leaf, selection, kind, container, provider)
                          for container in Container
                          for provider in PROVIDERS
                          if covered is None or
                          (leaf, selection, kind, provider) in covered]
    return found


def probe_unit(compiler, flags, directory, unit):
    outcomes = []
    probes = []
    for probe, invalid in unit.probes():
        if invalid:
            outcomes.append(driver.invalid(unit, probe, invalid))
        else:
            probes.append(probe)
    return outcomes + driver.probe_shared(compiler, flags, directory, unit,
                                          unit.name, probes)


@dataclass(frozen=True)
class Limitation:
    """A known limitation of detection: the parameters it holds for, the
    detections that report a class wrongly, the detected constructions that
    make the class differently from its explicit constructor, and why. A
    parameter is a shape in a category, of a request plain or selected."""
    reason: str
    shapes: frozenset
    categories: frozenset
    selections: frozenset
    misdetected_by: frozenset
    made_differently_by: frozenset

    def holds(self, selection, shape, category):
        return (selection in self.selections and shape in self.shapes and
                category in self.categories)


LIMITATIONS = (
    Limitation(
        "a value whose type converts from anything its element can be made "
        "of, such as std::optional: its converting constructor and the "
        "detection argument's conversion to the type cannot be ranked, so no "
        "constructor is detected",
        frozenset(shape for shape in Shape if shape.converts),
        frozenset({Category.VALUE, Category.CONST,
                   Category.RVALUE_REFERENCE}),
        frozenset({Selection.PLAIN}),
        frozenset({Detection.SHAPE, Detection.SIGNATURE}),
        frozenset({Construction.SHAPE, Construction.SIGNATURE})),
    Limitation(
        "an rvalue-reference parameter: detection cannot tell it from a value "
        "one, so the argument is a value and the dependency it requests is "
        "the value, not an rvalue; signature detection recovers the value",
        frozenset(Shape),
        frozenset({Category.RVALUE_REFERENCE}),
        frozenset({Selection.PLAIN}),
        frozenset({Detection.SIGNATURE}),
        frozenset({Construction.SHAPE, Construction.SIGNATURE})),
)

def limitations(unit, probe):
    """The known limitations a probe is made with: those whose detection
    reports its class wrongly, for a detection, and those whose construction
    makes it differently, for a making."""
    request, operation = probe.request, probe.operation
    found = [known for known in LIMITATIONS
             if known.holds(unit.selection, request.shape, request.category)]
    if isinstance(operation, Detect):
        return [known for known in found
                if operation.detection in known.misdetected_by]
    return [known for known in found
            if operation.construction in known.made_differently_by]


def probes_of(units):
    """Each valid probe of the units, by its cell."""
    return {unit.cell(probe): (unit, probe) for unit in units
            for probe, invalid in unit.probes() if not invalid}


def apply_limitations(cells, units):
    """Records a misdetection a known limitation says as limited, and a
    detection a known limitation says is misdetected as a stale
    limitation."""
    probes = probes_of(units)
    for cell in cells:
        unit, probe = probes.get(cell["cell"], (None, None))
        if probe is None or not isinstance(probe.operation, Detect):
            continue
        known = limitations(unit, probe)
        if not known:
            continue
        reasons = "; ".join(limitation.reason for limitation in known)
        if cell["outcome"] == Outcome.MISDETECTED:
            cell["outcome"] = Outcome.LIMITED.value
            cell["error"] = f"{reasons}; {cell['error']}"
        elif cell["outcome"] == Outcome.DETECTED:
            cell["outcome"] = Outcome.STALE_LIMITATION.value
            cell["error"] = f"no longer limited: {reasons}"
    return cells


def disagreements(cells, units):
    """The makings with a detected constructor whose outcome is not that of
    the explicit one."""
    outcomes = {cell["cell"]: cell["outcome"] for cell in cells}
    found = []
    for cell, (unit, probe) in probes_of(units).items():
        operation = probe.operation
        # A class a known limitation makes differently is not made as the
        # explicit constructor makes it.
        if (not isinstance(operation, Make) or
                operation.construction is Construction.EXPLICIT or
                operation.reference is None or limitations(unit, probe) or
                cell not in outcomes):
            continue
        explicit = unit.cell(Probe(operation.reference, probe.request))
        if explicit in outcomes and outcomes[explicit] != outcomes[cell]:
            found.append(f"{cell}: {outcomes[cell]}, explicit "
                         f"{outcomes[explicit]}")
    return found
