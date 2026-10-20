"""The reference implementation of registration and selection by key.

It interprets the key kinds, lookup declarations, and selection rules of the
spec over a model of containers: a level holds the static and runtime bindings
of one container, a container may have a parent level, and the registration
whose object is being constructed may have local bindings. It computes whether
a binding can be registered and which bindings a singular, a collection, or a
construct request selects, the oracle the C++ implementations are compared
against, and whether each outcome is decided when the container or the
request is compiled, by the times of the selection sources.
"""

from dataclasses import dataclass, field

from spec.schema import (
    BindingTime,
    Cardinality,
    Combine,
    SelectionMode,
    SelectionSource,
    SourceTime,
    Spec,
    Status,
    StepCondition,
)

VALUE_KINDS = ("fixed", "runtime")

OUTERMOST_CONDITIONS = (StepCondition.CONSTRUCTIBLE, StepCondition.HAS_FACTORY)

# The reason a binding is rejected whose key value is outside the bound of the
# dense array that indexes its domain: such a key is never stored.
KEY_OUT_OF_RANGE = "key out of range"


@dataclass(frozen=True, slots=True)
class Key:
    """A key: `kind` is a key kind id; `type` names the tag or the key type."""

    kind: str = "none"
    type: str | None = None
    value: object = None

    def domain(self) -> tuple:
        """Keys of one domain are compared; a fixed and a runtime key of one
        type share it."""
        kind = "value" if self.kind in VALUE_KINDS else self.kind
        return (kind, self.type)

    def names(self, other: "Key") -> bool:
        if self.domain() != other.domain():
            return False
        return self.kind not in VALUE_KINDS or self.value == other.value


NO_KEY = Key()


@dataclass(frozen=True, slots=True)
class Binding:
    """A binding: `key` is the key of every interface, `interface_keys` the
    keys registered for one interface, which replace a key of their domain
    for it, and `aliases` the keys of other key types of every interface."""

    name: str
    interfaces: tuple[str, ...]
    key: Key = NO_KEY
    static: bool = False
    interface_keys: tuple[tuple[str, Key], ...] = ()
    aliases: tuple[Key, ...] = ()

    def keys_for(self, interface: str) -> list[Key]:
        """The keys the binding carries for `interface`, one of each domain."""
        keys = [self.key, *self.aliases]
        for named, key in self.interface_keys:
            if named == interface:
                keys = [key if k.domain() == key.domain() else k for k in keys]
                if all(k.domain() != key.domain() for k in keys):
                    keys.append(key)
        return keys

    def names(self, interface: str, key: Key) -> bool:
        return interface in self.interfaces and any(
            carried.names(key) for carried in self.keys_for(interface)
        )


@dataclass
class Level:
    """The bindings of one container, its lookup declarations, and its parent.

    `declarations` maps (interface, key domain) to the declared cardinality,
    and `bounds` a domain of value keys indexed by a dense array to its bound:
    the key values it stores are 0 up to the bound, excluded. A level that is
    `open` takes runtime registrations; a static container is not.
    """

    bindings: list[Binding] = field(default_factory=list)
    declarations: dict = field(default_factory=dict)
    bounds: dict = field(default_factory=dict)
    parent: "Level | None" = None
    open: bool = True


@dataclass(frozen=True, slots=True)
class Request:
    interface: str
    key: Key = NO_KEY
    # Whether the request is of an unregistered type the container may
    # construct: a value or a const reference of an auto-constructible leaf.
    constructible: bool = False
    # Whether the factory of a construct request can make its type.
    has_factory: bool = False
    # The element interface of a request of a collection type, by value, whose
    # `interface` is the collection type.
    element: str | None = None


@dataclass(frozen=True, slots=True)
class Selection:
    status: Status
    bindings: tuple[Binding, ...] = ()
    auto_constructed: bool = False
    # The object of a construct request is made by its factory.
    made: bool = False
    # A request of a collection type is the collection of the elements its
    # level holds.
    collected: bool = False
    # The outcome is decided when the request is compiled: no runtime
    # registration can change it.
    compiled: bool = False


class Model:
    def __init__(self, spec: Spec):
        self.spec = spec
        default = next(d for d in spec.lookup_declarations if d.default)
        self.default_cardinality = default.default_cardinality
        self.rules = {rule.mode: rule for rule in spec.selection_rules}
        self.times = {entry.source: entry.time for entry in spec.selection_sources}
        self.value_times = {kind.id: kind.value_time for kind in spec.key_kinds}

    def cardinality(self, level: Level, interface: str, key: Key) -> Cardinality:
        return level.declarations.get((interface, key.domain()), self.default_cardinality)

    def rejection(self, level: Level, binding: Binding) -> tuple[str, bool]:
        """Why the level rejects the binding, or "", and whether the rejection
        is decided when the container is compiled: the binding is static, and
        so is the binding that holds its key. A key value outside the bound of
        its domain is rejected, and so is a key a domain of cardinality one
        already holds, among the static and runtime bindings of the level
        alike. A static binding carries no runtime key value, the
        binding-time constraint static_bindings_carry_no_runtime_key."""
        if binding.static and binding.key.kind == "runtime":
            return "a static binding carries no runtime key value", True
        if binding.key.kind in VALUE_KINDS:
            for interface in binding.interfaces:
                bound = level.bounds.get((interface, binding.key.domain()))
                if bound is not None and not 0 <= binding.key.value < bound:
                    return KEY_OUT_OF_RANGE, binding.static
        for interface in binding.interfaces:
            for key in binding.keys_for(interface):
                if self.cardinality(level, interface, key) is not Cardinality.ONE:
                    continue
                for existing in level.bindings:
                    if existing.names(interface, key):
                        return (
                            f"lookup already registered: {interface}",
                            binding.static and existing.static,
                        )
        return "", False

    def register(self, level: Level, binding: Binding) -> tuple[bool, str]:
        """Adds the binding unless the level rejects it, see rejection()."""
        reason, _ = self.rejection(level, binding)
        if reason:
            return False, reason
        level.bindings.append(binding)
        return True, ""

    def candidates(self, bindings, request: Request, static: bool) -> list[Binding]:
        return [
            b
            for b in bindings
            if b.static == static and b.names(request.interface, request.key)
        ]

    def source(self, source, request, level, local, mode) -> list:
        if source is SelectionSource.LOCAL_STATIC:
            return self.candidates(local, request, True)
        if source is SelectionSource.LOCAL_RUNTIME:
            return self.candidates(local, request, False)
        if source is SelectionSource.CONTAINER_STATIC:
            return self.candidates(level.bindings, request, True)
        if source is SelectionSource.CONTAINER_RUNTIME:
            return self.candidates(level.bindings, request, False)
        if source is SelectionSource.PARENT:
            if level.parent is None:
                return []
            return list(self.gather(level.parent, (), request, mode, outermost=False))
        if source is SelectionSource.AUTO_CONSTRUCT:
            return ["auto"]
        if source is SelectionSource.FACTORY:
            return ["factory"]
        if source is SelectionSource.ELEMENTS:
            own = [
                b
                for b in (*local, *level.bindings)
                if b.names(request.element, request.key)
            ]
            return ["elements"] if own else []
        raise ValueError(source)

    @staticmethod
    def meets(condition, request) -> bool:
        if condition is StepCondition.CONSTRUCTIBLE:
            return request.constructible
        if condition is StepCondition.HAS_FACTORY:
            return request.has_factory
        if condition is StepCondition.COLLECTION:
            return request.element is not None
        raise ValueError(condition)

    def gather(self, level, local, request, mode, outermost=True) -> list:
        """The candidates of the first step that names one. A parent is asked
        by the same rule, without the conditional steps, auto-construction and
        the factory, which the outermost level applies once."""
        rule = self.rules[mode]
        found: list = []
        for step in rule.steps:
            if step.when is not None and not self.meets(step.when, request):
                continue
            # Only the outermost level constructs: auto-construction and the
            # factory of a construct request.
            if step.when in OUTERMOST_CONDITIONS and not outermost:
                continue
            step_found = [
                candidate
                for source in step.sources
                for candidate in self.source(source, request, level, local, mode)
            ]
            if rule.combine is Combine.FIRST_NON_EMPTY:
                if step_found:
                    return step_found
            else:
                found += step_found
        return found

    def known(self, source, request, level, local, mode) -> tuple[list, bool]:
        """The candidates of a source known when the request is compiled, and
        whether the source is open: it may name more after."""
        time = self.times[source]
        if time is SourceTime.COMPILE and self.value_times[request.key.kind] is BindingTime.RUNTIME:
            # The value of the key picks the candidates when the request is
            # resolved, among the bindings of its domain the source holds.
            return [], bool(self.domain_candidates(source, request, level, local, mode))
        if time is SourceTime.COMPILE:
            return self.source(source, request, level, local, mode), False
        if time is SourceTime.RUNTIME:
            return [], level.open
        if time is SourceTime.LEVEL:
            own = [
                b
                for b in (*local, *level.bindings)
                if b.static and b.names(request.element, request.key)
            ]
            return (["elements"] if own else []), level.open
        if level.parent is None:
            return [], False
        return self.decision(level.parent, (), request, mode, outermost=False)

    def domain_candidates(self, source, request, level, local, mode) -> list:
        """The candidates of a source for any value of the key of the
        request: its bindings that hold a key of the domain of the key."""
        bindings = {
            SelectionSource.LOCAL_STATIC: [b for b in local if b.static],
            SelectionSource.LOCAL_RUNTIME: [b for b in local if not b.static],
            SelectionSource.CONTAINER_STATIC: [b for b in level.bindings if b.static],
        }.get(source)
        if bindings is None:
            return self.source(source, request, level, local, mode)
        domain = request.key.domain()
        return [
            b
            for b in bindings
            if request.interface in b.interfaces
            and any(k.domain() == domain for k in b.keys_for(request.interface))
        ]

    def decision(self, level, local, request, mode, outermost=True) -> tuple[list, bool]:
        """The candidates known when the request is compiled, by the rule of
        gather(), and whether the outcome is still open. A step that knows two
        candidates is decided, as more only add to them; one that knows one
        is decided unless an open source of it may add another."""
        rule = self.rules[mode]
        found: list = []
        undecided = False
        for step in rule.steps:
            if step.when is not None and not self.meets(step.when, request):
                continue
            if step.when in OUTERMOST_CONDITIONS and not outermost:
                continue
            step_found: list = []
            step_open = False
            for source in step.sources:
                known, open_ = self.known(source, request, level, local, mode)
                step_found += known
                step_open = step_open or open_
            if rule.combine is Combine.FIRST_NON_EMPTY:
                if len(step_found) > 1 or (step_found and not step_open):
                    return step_found, False
                if step_open:
                    return step_found, True
            else:
                found += step_found
                undecided = undecided or step_open
        return found, undecided

    def select(
        self, level: Level, request: Request, local=(), collection=False, construct=False
    ) -> Selection:
        mode = (
            SelectionMode.COLLECTION
            if collection
            else SelectionMode.CONSTRUCT
            if construct
            else SelectionMode.SINGULAR
        )
        found = self.gather(level, tuple(local), request, mode)
        count = (
            Cardinality.EMPTY
            if not found
            else Cardinality.ONE
            if len(found) == 1
            else Cardinality.MANY
        )
        status = next(o.status for o in self.rules[mode].outcomes if o.count is count)
        _, undecided = self.decision(level, tuple(local), request, mode)
        compiled = not undecided
        if found == ["auto"]:
            return Selection(status, (), auto_constructed=True, compiled=compiled)
        if found == ["factory"]:
            return Selection(status, (), made=True, compiled=compiled)
        if found == ["elements"]:
            return Selection(status, (), collected=True, compiled=compiled)
        return Selection(
            status, tuple(found) if status is Status.OK else (), compiled=compiled
        )
