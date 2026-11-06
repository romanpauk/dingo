"""The constructor detection family: classes generated around every request
of the grammar over a supported leaf, what detection says about them, and
what constructing them does."""

from dataclasses import dataclass
from typing import Optional

from . import driver
from .dimensions import (Construction, Container, Detection, Kind, Leaf,
                         Making, Scope, Selection, StoredForm)
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
        agree with: a registration with a registration, any other with
        construct."""
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


def units():
    found = []
    for leaf in INTERFACES:
        for selection in Selection:
            for kind in Kind:
                found.append(Unit(leaf, selection, kind))
                found += [Unit(leaf, selection, kind, container, provider)
                          for container in Container
                          for provider in PROVIDERS]
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


def disagreements(cells):
    """The makings with a detected constructor whose outcome is not that of
    the explicit one."""
    outcomes = {cell["cell"]: cell["outcome"] for cell in cells}
    found = []
    for cell, outcome in outcomes.items():
        parts = cell.split("/")
        construction, making = parts[6].split("_", 1)
        if construction not in (Construction.SHAPE, Construction.SIGNATURE):
            continue
        reference = Make(Construction(construction), Making(making)).reference
        explicit = "/".join(parts[:6] + [reference.name] + parts[7:])
        if explicit in outcomes and outcomes[explicit] != outcome:
            found.append(f"{cell}: {outcome}, explicit "
                         f"{outcomes[explicit]}")
    return found
