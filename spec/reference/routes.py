"""The reference implementation of registration and route derivation.

It interprets the storage forms, scope descriptors, wrapper semantics, and route
rules of the spec over abstract type_trees, and computes, for a binding and a
request, whether the binding can be registered, whether the request finds the
binding and is served, and what serving it observably does: how many leaves
are constructed and copied, where the leaf of the result lives, and whether a
repeated request yields the same leaf. It is the oracle the C++
implementations are compared against; see test/routes/conform.py.

It names no wrapper semantics: a rule is a form of a kind, and what a wrapper is, is
the properties of its kind, so a kind of the user is interpreted as one of the
library is. Wrappers of a mapped semantics carry an opaque wrapper family. The
spec names no concrete wrapper; the family only tells two wrappers of the same
kind apart. A family of ANY stands for a wrapper a rule creates, which a wrapper
of any family that provides the wrapper semantics can serve.
"""

from dataclasses import dataclass, field, replace

from spec.content.wrapper_semantics import wrapper_semantics
from spec.content.rules import rules_of
from spec.schema import (
    Access,
    ExposureRule,
    PropertyClass,
    Property,
    PatternForm,
    Predicate,
    RequestCategory,
    Operation,
    Slot,
    Spec,
    ValueCategory,
    Via,
)

LVALUE = ValueCategory.LVALUE
RVALUE = ValueCategory.RVALUE
PRVALUE = ValueCategory.PRVALUE

ANY = "*"
# The binding of a variadic wrapper pattern to the argument it matched.
ARGUMENT = "@argument"
# The alternative an object of a variadic wrapper holds: its first, which the
# default factory makes, and which the instances of the probes hold. A route
# through another alternative is not served, which the next implementation
# finds when it walks the route: the binding is named by every alternative,
# so the request is found and refused as ALTERNATIVE_NOT_HELD.
HELD_ALTERNATIVE = 0
ALTERNATIVE_NOT_HELD = "the object holds another alternative than the route takes"
# The wrapper semantics of the library, which name the family of a wrapper type tree
# made without a model.
KINDS = {kind.id: kind for kind in wrapper_semantics}
# The class of the handles a binding of an interface may serve it by: a
# nullable wrapper that refers to its element rather than holding it.
INTERFACE_HANDLE = PropertyClass(has=(Property.NULLABLE,), lacks=(Property.IN_PLACE,))


def is_mapped(kinds: dict, kind: str) -> bool:
    return Property.MAPPED in kinds[kind].traits


@dataclass(frozen=True, slots=True)
class Leaf:
    """A leaf type and the properties route rules depend on.

    `bases` names the interfaces the leaf implements, and `converts` the ones
    it declares a user conversion to.
    """

    name: str
    copyable: bool = True
    constructible: bool = True
    bases: tuple[str, ...] = ()
    converts: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class TypeTree:
    """A leaf (`wrapper` is None) or a wrapper semantics applied to argument type trees."""

    wrapper: str | None = None
    args: tuple["TypeTree", ...] = ()
    leaf: Leaf | None = None
    extent: int | None = None
    family: str | None = None

    def size(self) -> int:
        return 1 + sum(arg.size() for arg in self.args)


def leaf(value: Leaf) -> TypeTree:
    return TypeTree(leaf=value)


def wrapper(kind: str, *args: TypeTree, extent: int | None = None, family: str | None = None) -> TypeTree:
    if family is None and is_mapped(KINDS, kind):
        family = "std"
    return TypeTree(wrapper=kind, args=args, extent=extent, family=family)


class RequestForm:
    LVALUE_REFERENCE = "lvalue_reference"
    RVALUE_REFERENCE = "rvalue_reference"
    VALUE = "value"


@dataclass(frozen=True, slots=True)
class Request:
    kind: str  # a RequestKind
    type_tree: TypeTree


@dataclass(frozen=True, slots=True)
class Outcome:
    """What registering a binding and serving one request does.

    `identity` is where the leaf of the result lives on the first and on a
    repeated request: "existing" for an object that existed before the
    request, "new" for one the request created, or None when the result is a
    leaf value. A request that does not name the binding is not `found`.
    """

    registered: bool
    reason: str = ""
    found: bool = False
    served: bool = False
    rules: tuple[str, ...] = ()
    copies: int = 0
    constructions: tuple[int, int] = (0, 0)
    identity: tuple[str | None, str | None] = (None, None)
    stable: bool = False


@dataclass
class Derivation:
    """A derived reachable type and how it was reached."""

    type_tree: TypeTree
    category: ValueCategory
    rules: tuple[str, ...] = ()
    # For each rule, the argument of a variadic wrapper it reached, else 0.
    arguments: tuple[int, ...] = ()
    copied: TypeTree | None = None  # the type tree the copy rule copied
    consumed: bool = False
    # The routes to the same reachable type through other alternatives of a variadic
    # wrapper: each takes the route of the reachable type up to the step `branch` of it,
    # there another alternative, and reaches the reachable type on its own. A walk
    # continues from the object of the step along the one the object holds.
    siblings: list = field(default_factory=list)
    branch: int | None = None


@dataclass
class Model:
    """The spec, with the wrapper semantics of a user beside those of the library,
    which the model derives the routes of as it does those of the library."""

    spec: Spec
    kinds: tuple = ()
    # The conditions of routes the model ignores, as (route id, condition):
    # a predicate a route requires, "exposure" for the request category it
    # needs a scope to expose, or "handle_copies" for the copy rule's need of
    # a sharing handle in a scope that copies handles. A test relaxes the
    # rules so to find the cases a condition decides.
    relaxed: frozenset = frozenset()
    semantics_by_id: dict = field(init=False)
    scopes: dict = field(init=False)
    rules: tuple = field(init=False)

    def __post_init__(self):
        kinds = (*self.spec.wrapper_semantics, *self.kinds)
        self.semantics_by_id = {kind.id: kind for kind in kinds}
        self.scopes = {scope.id: scope for scope in self.spec.scopes}
        self.rules = rules_of(kinds) if self.kinds else self.spec.rules
        self.rules_by_id = {route.id: route for route in self.rules}

    def route(self, route_id: str):
        return self.rules_by_id[route_id]


# Type tree properties


def has_trait(model: "Model", type_tree: TypeTree, trait: Property) -> bool:
    return type_tree.wrapper is not None and trait in model.semantics_by_id[type_tree.wrapper].traits


def has_form(model: "Model", type_tree: TypeTree, form: Operation) -> bool:
    return type_tree.wrapper is not None and any(
        op.form is form for op in model.semantics_by_id[type_tree.wrapper].forms
    )


def has_extent(model: "Model", type_tree: TypeTree) -> bool:
    return type_tree.wrapper is not None and model.semantics_by_id[type_tree.wrapper].has_extent


def is_array(model: "Model", type_tree: TypeTree) -> bool:
    """Whether the outer wrapper holds a number of elements."""
    return has_extent(model, type_tree) or has_trait(model, type_tree, Property.UNSIZED)


def copyable(model: "Model", type_tree: TypeTree) -> bool:
    if type_tree.wrapper is None:
        return type_tree.leaf.copyable
    if has_trait(model, type_tree, Property.COPYABLE):
        return True
    if has_trait(model, type_tree, Property.IN_PLACE) and not has_trait(model, type_tree, Property.IMMOVABLE):
        return all(copyable(model, arg) for arg in type_tree.args)
    return False


def constructible(model: "Model", type_tree: TypeTree) -> bool:
    """Whether the default factory can construct the type tree."""
    if type_tree.wrapper is None:
        return type_tree.leaf.constructible
    if not has_trait(model, type_tree, Property.CONSTRUCTIBLE):
        return False
    return constructible(model, type_tree.args[0])


def constructed_leaves(model: "Model", type_tree: TypeTree) -> int:
    """The leaves the default factory constructs for the type tree."""
    if type_tree.wrapper is None:
        return 1
    if has_extent(model, type_tree):
        return type_tree.extent * constructed_leaves(model, type_tree.args[0])
    return constructed_leaves(model, type_tree.args[0])


def leaves_held_by_value(model: "Model", type_tree: TypeTree) -> int:
    """The leaves a copy of the type tree copies: those a wrapper holds in place."""
    if type_tree.wrapper is None:
        return 1
    if not has_trait(model, type_tree, Property.IN_PLACE):
        return 0
    if has_extent(model, type_tree):
        return type_tree.extent * leaves_held_by_value(model, type_tree.args[0])
    return leaves_held_by_value(model, type_tree.args[0])


def addressable(model: "Model", type_tree: TypeTree) -> bool:
    """Whether a type tree is an object that can be referred to and pointed to:
    anything but an address, a wrapper of a kind of the address form."""
    return not has_form(model, type_tree, Operation.ADDRESS)


def innermost_leaf(type_tree: TypeTree) -> Leaf:
    while type_tree.wrapper is not None:
        type_tree = type_tree.args[0]
    return type_tree.leaf


def holds_in_array(model: "Model", type_tree: TypeTree) -> bool:
    if is_array(model, type_tree):
        return True
    return any(holds_in_array(model, arg) for arg in type_tree.args)


def type_trees_match(derived: TypeTree, requested: TypeTree) -> bool:
    """Whether a derived type_tree serves a requested one; ANY matches any family."""
    if derived.wrapper != requested.wrapper or derived.extent != requested.extent:
        return False
    if derived.wrapper is None:
        return derived.leaf == requested.leaf
    if ANY not in (derived.family, requested.family) and derived.family != requested.family:
        return False
    return len(derived.args) == len(requested.args) and all(
        type_trees_match(d, r) for d, r in zip(derived.args, requested.args)
    )


# Patterns


def match(model: "Model", pattern, type_tree: TypeTree, bindings: dict) -> list[dict]:
    """Every way the pattern matches the type tree, as variable bindings."""
    if pattern.form is PatternForm.VARIABLE:
        bound = bindings.get(pattern.name)
        if bound is not None and bound != type_tree:
            return []
        return [{**bindings, pattern.name: type_tree}]
    if pattern.form is not PatternForm.SEMANTICS or type_tree.wrapper != pattern.name:
        return []
    if model.semantics_by_id[pattern.name].variadic:
        # The variadic wrapper pattern of one argument matches any argument,
        # which the bindings record.
        return [
            m
            for index, arg in enumerate(type_tree.args)
            for m in match(model, pattern.args[0], arg, {**bindings, ARGUMENT: index})
        ]
    if len(pattern.args) != len(type_tree.args):
        return []
    results = [bindings]
    for sub, arg in zip(pattern.args, type_tree.args):
        results = [m for b in results for m in match(model, sub, arg, b)]
    return results


def instantiate(model: "Model", pattern, bindings: dict, family: str | None) -> TypeTree:
    if pattern.form is PatternForm.VARIABLE:
        return bindings[pattern.name]
    args = tuple(instantiate(model, arg, bindings, None) for arg in pattern.args)
    kind = pattern.name
    return TypeTree(wrapper=kind, args=args, family=(family or ANY) if is_mapped(model.semantics_by_id, kind) else None)


def upcast_leaf(type_tree: TypeTree, base: Leaf) -> TypeTree:
    if type_tree.wrapper is None:
        return leaf(base)
    return replace(type_tree, args=(upcast_leaf(type_tree.args[0], base), *type_tree.args[1:]))


# Registration


def property_class_holds(model: Model, property_class, type_tree: TypeTree) -> bool:
    """Whether the outer wrapper of a type tree is of a wrapper class."""
    if type_tree.wrapper is None:
        return False
    kind = model.semantics_by_id[type_tree.wrapper]
    traits = set(kind.traits)
    return (
        all(trait in traits for trait in property_class.has)
        and not any(trait in traits for trait in property_class.lacks)
        and property_class.variadic in (None, kind.variadic)
        and property_class.extent in (None, kind.has_extent)
    )


def storage_form(model: Model, type_tree: TypeTree) -> str:
    """The most specific storage form the type tree is of, by the properties of
    the kinds of its outer wrappers."""
    best, depth = "plain", 0
    for form in model.spec.storage_forms:
        if form.outer is None or not property_class_holds(model, form.outer, type_tree):
            continue
        if form.inner is None:
            if depth < 1:
                best, depth = form.id, 1
        elif len(type_tree.args) == 1 and property_class_holds(model, form.inner, type_tree.args[0]):
            best, depth = form.id, 2
    return best


def pattern_size(pattern) -> int:
    return 1 + sum(pattern_size(arg) for arg in pattern.args)


def exposed_categories(model: Model, scope_id: str, form: str) -> tuple:
    for exposure in model.scopes[scope_id].exposures:
        if exposure.form == form:
            return exposure.categories
    return ()


def interface_probes(model: Model, interfaces: tuple[Leaf, ...]) -> list[Request]:
    """The requests that show a binding serves its interfaces: by reference,
    and by value of a handle of an interface of any kind."""
    probes = []
    for interface in interfaces:
        base = leaf(interface)
        probes.append(Request(RequestForm.LVALUE_REFERENCE, base))
        for kind in model.semantics_by_id.values():
            handle = TypeTree(wrapper=kind.id, args=(base,))
            if property_class_holds(model, INTERFACE_HANDLE, handle):
                family = ANY if is_mapped(model.semantics_by_id, kind.id) else None
                probes.append(Request(RequestForm.VALUE, replace(handle, family=family)))
    return probes


def register(
    model: Model,
    scope_id: str,
    stored: TypeTree,
    interfaces: tuple[Leaf, ...] = (),
    factory: str | None = None,
) -> tuple[bool, str]:
    """Whether a binding can be registered, and why not. `factory` is the id
    of the factory kind the binding declares, or None for the detected
    constructor."""
    if factory is not None and factory not in {kind.id for kind in model.spec.factory_kinds}:
        raise ValueError(f"unknown factory kind {factory!r}")
    scope = model.scopes[scope_id]
    form = storage_form(model, stored)
    if not exposed_categories(model, scope_id, form):
        return False, f"{scope_id} does not store the {form} form"
    if not scope.nested_wrappers and stored.wrapper is not None:
        if any(arg.wrapper is not None for arg in stored.args):
            return False, f"{scope_id} does not store nested wrappers"
    if "instance" not in scope.needs and factory is None and not constructible(model, stored):
        return False, "the stored type tree has no default factory"
    if interfaces:
        if holds_in_array(model, stored):
            return False, "an array element cannot be exposed as an interface"
        if not any(
            serve(model, scope_id, stored, probe, interfaces) is not None
            for probe in interface_probes(model, interfaces)
        ):
            return False, "no declared interface can be served"
    return True, ""


# Derivation


def creates(model: Model, rule) -> bool:
    """Whether a route creates a wrapper of its kind, from an element or from a
    wrapper of the kind it leads from: wrapping, adopting, or taking an address.
    A creating form applies at the request, which names the kind it creates."""
    if rule.form is None:
        return False
    template = next(t for t in model.spec.operations if t.form is rule.form)
    to_semantics = template.to_type_tree.form is PatternForm.SEMANTICS and template.to_type_tree.name == "K"
    from_semantics = (
        template.from_type_tree.form is PatternForm.SEMANTICS and template.from_type_tree.name == "K"
    )
    return to_semantics and not from_semantics


def derive(
    model: Model,
    scope_id: str,
    stored: TypeTree,
    interfaces: tuple[Leaf, ...],
    bound: int,
) -> list[Derivation]:
    """The reachable types a binding derives by the routes that do not create a wrapper."""
    scope = model.scopes[scope_id]
    form = storage_form(model, stored)
    exposed = set(exposed_categories(model, scope_id, form))
    start = Derivation(stored, scope.materializes, consumed=scope.materializes is RVALUE)
    primaries = {(stored, scope.materializes): start}
    queue = [start]
    results = [start]
    while queue:
        reachable_type = queue.pop(0)
        for rule in model.rules:
            if creates(model, rule):
                continue
            if rule.from_type_tree.category not in (None, reachable_type.category):
                continue
            if exposure_refuses(model, rule, exposed):
                continue
            for bindings in match(model, rule.from_type_tree, reachable_type.type_tree, {}):
                if not requirements_hold(model, scope, rule, bindings):
                    continue
                for type_tree in derived_type_trees(model, rule, bindings, reachable_type.type_tree, interfaces):
                    category = rule.to_type_tree.category or reachable_type.category
                    if type_tree.size() > bound:
                        continue
                    derived = Derivation(
                        type_tree,
                        category,
                        rules=(*reachable_type.rules, rule.id),
                        arguments=(*reachable_type.arguments, bindings.get(ARGUMENT, 0)),
                        copied=bindings.get("X") if rule.id == "copy" else reachable_type.copied,
                        consumed=reachable_type.consumed or rule.access is Access.CONSUME,
                    )
                    primary = primaries.get((type_tree, category))
                    if primary is not None:
                        add_sibling(model, primary, derived)
                        continue
                    primaries[(type_tree, category)] = derived
                    results.append(derived)
                    queue.append(derived)
    return results


def variadic_route(model: Model, rule_id: str) -> bool:
    """Whether a route reaches into a wrapper of a variadic kind."""
    source = model.route(rule_id).from_type_tree
    return source.form is PatternForm.SEMANTICS and model.semantics_by_id[source.name].variadic


def completion_class(model: Model, reachable_type: Derivation) -> str:
    """How a request completes a reachable type: by copying, decaying, or neither."""
    if not reachable_type.rules:
        return "none"
    last = model.route(reachable_type.rules[-1])
    if last.id == "copy":
        return "copy"
    return "decay" if last.form is Operation.DECAY else "other"


def add_sibling(model: Model, primary: Derivation, derived: Derivation) -> None:
    """Keeps `derived` as a sibling of `primary`, the reachable_type of its type_tree and
    category derived first, when their routes part at a step into a variadic
    wrapper, by its alternatives, and are completed alike; one per step and
    alternative."""
    steps = zip(zip(primary.rules, primary.arguments), zip(derived.rules, derived.arguments))
    for position, (taken, other) in enumerate(steps):
        if taken == other:
            continue
        if taken[0] != other[0] or not variadic_route(model, taken[0]):
            return
        if completion_class(model, primary) != completion_class(model, derived):
            return
        for sibling in primary.siblings:
            if sibling.branch == position and sibling.arguments[position] == other[1]:
                return
        derived.branch = position
        primary.siblings.append(derived)
        return


def walked(model: Model, reachable_type: Derivation) -> Derivation | None:
    """The route a walk of the reachable_type takes by the alternatives the objects of
    its variadic wrappers hold: the reachable_type's, or at the first step into an object
    that holds another alternative, the sibling that takes it; or None when
    none does, or the route reaches an object holding another later on."""
    for position, (rule_id, argument) in enumerate(zip(reachable_type.rules, reachable_type.arguments)):
        if argument == HELD_ALTERNATIVE or not variadic_route(model, rule_id):
            continue
        sibling = next(
            (
                s
                for s in reachable_type.siblings
                if s.branch == position and s.arguments[position] == HELD_ALTERNATIVE
            ),
            None,
        )
        if sibling is None or not holds_alternatives(model, sibling):
            return None
        return sibling
    return reachable_type


def derived_type_trees(model: "Model", rule, bindings: dict, source: TypeTree, interfaces: tuple[Leaf, ...]):
    keeps_family = (
        rule.from_type_tree.form is PatternForm.SEMANTICS and rule.to_type_tree.form is PatternForm.SEMANTICS
    )
    type_tree = instantiate(model, rule.to_type_tree, bindings, source.family if keeps_family else None)
    if rule.via is None:
        yield type_tree
        return
    stored_leaf = bindings["L"].leaf
    targets = stored_leaf.bases if rule.via is Via.UPCAST else stored_leaf.converts
    for interface in interfaces:
        if interface.name in targets:
            yield upcast_leaf(type_tree, interface)


def exposure_refuses(model: Model, rule, exposed: set) -> bool:
    """Whether the scope does not expose the request category a route needs."""
    if (rule.id, "exposure") in model.relaxed:
        return False
    return rule.exposure is not None and rule.exposure not in exposed


def requirements_hold(model: Model, scope, rule, bindings: dict) -> bool:
    for requirement in rule.requires:
        if (rule.id, requirement.predicate.value) in model.relaxed:
            continue
        subject = bindings[requirement.subject]
        if requirement.predicate is Predicate.COPYABLE and not copyable(model, subject):
            return False
        if requirement.predicate is Predicate.ARRAY and not (
            has_extent(model, subject) and has_trait(model, subject, Property.IMMOVABLE)
        ):
            return False
        if requirement.predicate is Predicate.ADDRESSABLE and not addressable(model, subject):
            return False
        if requirement.predicate is Predicate.LEAF and subject.wrapper is not None:
            return False
    if (
        rule.id == "copy"
        and scope.exposure_rule is ExposureRule.HANDLE_COPIES
        and (rule.id, "handle_copies") not in model.relaxed
    ):
        if not has_trait(model, bindings["X"], Property.SHARING):
            return False
    return True


def accepts(model: "Model", request: Request, reachable_type: Derivation, exposed: set) -> bool:
    if not type_trees_match(reachable_type.type_tree, request.type_tree):
        return False
    if request.kind == RequestForm.LVALUE_REFERENCE:
        return (
            reachable_type.category is LVALUE
            and RequestCategory.LVALUE in exposed
            and addressable(model, reachable_type.type_tree)
        )
    if has_trait(model, reachable_type.type_tree, Property.IMMOVABLE):
        return False
    if reachable_type.category is RVALUE:
        return RequestCategory.RVALUE in exposed
    return request.kind == RequestForm.VALUE and reachable_type.category is PRVALUE


def named_leaves(model: "Model", type_tree: TypeTree) -> tuple[Leaf, ...]:
    """The leaves a stored type tree names its binding by: the leaf its wrappers
    hold, and through a wrapper of a variadic kind, the leaf of each of its
    alternatives, whichever the object holds."""
    if type_tree.wrapper is None:
        return (type_tree.leaf,)
    if not model.semantics_by_id[type_tree.wrapper].variadic:
        return named_leaves(model, type_tree.args[0])
    leaves = []
    for arg in type_tree.args:
        leaves.extend(found for found in named_leaves(model, arg) if found not in leaves)
    return tuple(leaves)


def finds(
    model: "Model", stored: TypeTree, request: Request, interfaces: tuple[Leaf, ...]
) -> bool:
    """Whether the request names the binding: its leaf is a declared interface,
    or a leaf the stored type tree names when no interface is declared."""
    requested = innermost_leaf(request.type_tree)
    if interfaces:
        return requested in interfaces
    return requested in named_leaves(model, stored)


def derivation_bound(stored: TypeTree) -> int:
    """Routes derive type trees at most one wrapper larger than the stored type tree.

    The bound depends on the binding only, so a binding can publish every reachable type
    it serves before any request is known.
    """
    return stored.size() + 1


# The wrappers a request creates at most, each by a creating form of its kind.
MAX_CREATED_WRAPPERS = 2


def created(model: "Model", scope, reachable_types, goal: TypeTree, exposed: set, bound: int, depth: int):
    """The reachable types a request of the type tree `goal` creates from the reachable types of a
    binding, by the creating forms of the kind of its outer wrapper, and of the
    wrappers it creates under it."""
    if goal.wrapper is None or depth == 0 or not goal.args:
        return []
    results = []
    for rule in model.rules:
        if not creates(model, rule) or rule.to_type_tree.name != goal.wrapper:
            continue
        if exposure_refuses(model, rule, exposed):
            continue
        # The element the form creates the wrapper of, as the request names it,
        # which a binding reachable type or a wrapper the request creates under it is.
        inner_goal = instantiate(model, rule.from_type_tree, {"X": goal.args[0], "A": goal.args[0]}, None)
        sources = [*reachable_types, *created(model, scope, reachable_types, inner_goal, exposed, bound, depth - 1)]
        for reachable_type in sources:
            if rule.from_type_tree.category not in (None, reachable_type.category):
                continue
            for bindings in match(model, rule.from_type_tree, reachable_type.type_tree, {}):
                if not requirements_hold(model, scope, rule, bindings):
                    continue
                for type_tree in derived_type_trees(model, rule, bindings, reachable_type.type_tree, ()):
                    if type_tree.size() > bound:
                        continue
                    results.append(
                        Derivation(
                            type_tree,
                            rule.to_type_tree.category or reachable_type.category,
                            rules=(*reachable_type.rules, rule.id),
                            arguments=(*reachable_type.arguments, 0),
                            siblings=[
                                replace(
                                    sibling,
                                    type_tree=type_tree,
                                    category=rule.to_type_tree.category or reachable_type.category,
                                    rules=(*sibling.rules, rule.id),
                                    arguments=(*sibling.arguments, 0),
                                    consumed=sibling.consumed or rule.access is Access.CONSUME,
                                    siblings=[],
                                )
                                for sibling in reachable_type.siblings
                            ],
                            copied=reachable_type.copied,
                            consumed=reachable_type.consumed or rule.access is Access.CONSUME,
                        )
                    )
    return results


def serve(model: "Model", scope_id, stored, request, interfaces) -> Derivation | None:
    exposed = set(exposed_categories(model, scope_id, storage_form(model, stored)))
    bound = derivation_bound(stored)
    reachable_types = derive(model, scope_id, stored, interfaces, bound)
    scope = model.scopes[scope_id]
    made = created(model, scope, reachable_types, request.type_tree, exposed, bound, MAX_CREATED_WRAPPERS)
    candidates = [reachable_type for reachable_type in (*reachable_types, *made) if accepts(model, request, reachable_type, exposed)]
    if not candidates:
        return None
    # Prefer the derivation with the fewest rules, then without a copy, then the
    # one whose rules come first in the order of the rules, which orders the
    # forms and not the kinds, then the one derived first.
    order = {}
    first = {}
    for index, rule in enumerate(model.rules):
        key = rule.form if rule.form is not None else rule.id
        order[rule.id] = first.setdefault(key, index)
    return min(
        candidates,
        key=lambda t: (len(t.rules), t.copied is not None, [order[r] for r in t.rules]),
    )


def holds_alternatives(model: Model, reachable_type: Derivation) -> bool:
    """Whether the objects of the variadic wrappers a route reaches into hold
    the alternatives it takes."""
    return all(
        argument == HELD_ALTERNATIVE or not variadic_route(model, rule_id)
        for rule_id, argument in zip(reachable_type.rules, reachable_type.arguments)
    )


def leaf_location(model: "Model", reachable_type: Derivation) -> str:
    if reachable_type.consumed:
        return "new"
    if reachable_type.copied is not None and leaves_held_by_value(model, reachable_type.copied) > 0:
        return "new"
    return "stored"


def resolve(
    model: Model,
    scope_id: str,
    stored: TypeTree,
    request: Request,
    interfaces: tuple[Leaf, ...] = (),
) -> Outcome:
    registered, reason = register(model, scope_id, stored, interfaces)
    if not registered:
        return Outcome(registered=False, reason=reason)
    if not finds(model, stored, request, interfaces):
        return Outcome(registered=True)
    reachable_type = serve(model, scope_id, stored, request, interfaces)
    if reachable_type is None:
        return Outcome(registered=True, found=True)
    reachable_type = walked(model, reachable_type)
    if reachable_type is None:
        return Outcome(registered=True, found=True, reason=ALTERNATIVE_NOT_HELD)

    scope = model.scopes[scope_id]
    lazily_constructed = scope.slot is not Slot.EXTERNAL_REF
    per_request = scope.source_access.value == "consume"
    built = constructed_leaves(model, stored) if lazily_constructed else 0
    constructions = (built, built if per_request else 0)

    location = leaf_location(model, reachable_type)
    if request.kind == RequestForm.VALUE and request.type_tree.wrapper is None:
        identity = (None, None)
    elif location == "new":
        identity = ("new", "new")
    else:
        identity = ("new" if lazily_constructed else "existing", "existing")
    stable = location == "stored" and identity != (None, None)
    copies = leaves_held_by_value(model, reachable_type.copied) if reachable_type.copied is not None else 0
    return Outcome(
        registered=True,
        found=True,
        served=True,
        rules=reachable_type.rules,
        copies=copies,
        constructions=constructions,
        identity=identity,
        stable=stable,
    )
