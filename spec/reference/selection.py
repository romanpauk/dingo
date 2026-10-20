"""The reference implementation of registration and selection by key.

It interprets the key kinds, lookup declarations, and selection rules of the
spec over a model of containers: a level holds the static and runtime bindings
of one container, a container may have a parent level, and the registration
whose object is being constructed may have local bindings. It computes whether
a binding can be registered and which bindings a singular, a collection, or a
construct request selects, the oracle the C++ implementations are compared
against.
"""

from dataclasses import dataclass, field

from spec.schema import (
    Cardinality,
    Combine,
    SelectionMode,
    SelectionSource,
    Spec,
    Status,
    StepCondition,
)

VALUE_KINDS = ("fixed", "runtime")

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
    name: str
    interfaces: tuple[str, ...]
    key: Key = NO_KEY
    static: bool = False


@dataclass
class Level:
    """The bindings of one container, its lookup declarations, and its parent.

    `declarations` maps (interface, key domain) to the declared cardinality,
    and `bounds` a domain of value keys indexed by a dense array to its bound:
    the key values it stores are 0 up to the bound, excluded.
    """

    bindings: list[Binding] = field(default_factory=list)
    declarations: dict = field(default_factory=dict)
    bounds: dict = field(default_factory=dict)
    parent: "Level | None" = None


@dataclass(frozen=True, slots=True)
class Request:
    interface: str
    key: Key = NO_KEY
    # Whether the request is of an unregistered type the container may
    # construct: a value or a const reference of an auto-constructible leaf.
    constructible: bool = False
    # Whether the factory of a construct request can make its type.
    has_factory: bool = False


@dataclass(frozen=True, slots=True)
class Selection:
    status: Status
    bindings: tuple[Binding, ...] = ()
    auto_constructed: bool = False
    # The object of a construct request is made by its factory.
    made: bool = False


class Model:
    def __init__(self, spec: Spec):
        self.spec = spec
        default = next(d for d in spec.lookup_declarations if d.default)
        self.default_cardinality = default.default_cardinality
        self.rules = {rule.mode: rule for rule in spec.selection_rules}

    def cardinality(self, level: Level, interface: str, key: Key) -> Cardinality:
        return level.declarations.get((interface, key.domain()), self.default_cardinality)

    def register(self, level: Level, binding: Binding) -> tuple[bool, str]:
        """Adds the binding unless its key value is outside the bound of its
        domain, or a domain of cardinality one already holds a binding of its
        key, among the static and runtime bindings of the level alike. A static binding carries no runtime key value, the
        binding-time constraint static_bindings_carry_no_runtime_key."""
        if binding.static and binding.key.kind == "runtime":
            return False, "a static binding carries no runtime key value"
        if binding.key.kind in VALUE_KINDS:
            for interface in binding.interfaces:
                bound = level.bounds.get((interface, binding.key.domain()))
                if bound is not None and not 0 <= binding.key.value < bound:
                    return False, KEY_OUT_OF_RANGE
        for interface in binding.interfaces:
            if self.cardinality(level, interface, binding.key) is not Cardinality.ONE:
                continue
            for existing in level.bindings:
                if interface in existing.interfaces and existing.key.names(binding.key):
                    return False, f"lookup already registered: {interface}"
        level.bindings.append(binding)
        return True, ""

    def candidates(self, bindings, request: Request, static: bool) -> list[Binding]:
        return [
            b
            for b in bindings
            if b.static == static
            and request.interface in b.interfaces
            and b.key.names(request.key)
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
        raise ValueError(source)

    @staticmethod
    def meets(condition, request) -> bool:
        if condition is StepCondition.CONSTRUCTIBLE:
            return request.constructible
        if condition is StepCondition.HAS_FACTORY:
            return request.has_factory
        raise ValueError(condition)

    def gather(self, level, local, request, mode, outermost=True) -> list:
        """The candidates of the first step that names one. A parent is asked
        by the same rule, without the conditional steps, auto-construction and
        the factory, which the outermost level applies once."""
        rule = self.rules[mode]
        found: list = []
        for step in rule.steps:
            if step.when is not None and not (outermost and self.meets(step.when, request)):
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
        if found == ["auto"]:
            return Selection(status, (), auto_constructed=True)
        if found == ["factory"]:
            return Selection(status, (), made=True)
        return Selection(status, tuple(found) if status is Status.OK else ())
