"""The reference implementation of registration and route derivation.

It interprets the storage forms, scope descriptors, layer kinds, and route
rules of the spec over abstract shapes, and computes, for a binding and a
request, whether the binding can be registered, whether the request finds the
binding and is served, and what serving it observably does: how many leaves
are constructed and copied, where the leaf of the result lives, and whether a
repeated request yields the same leaf. It is the oracle the C++
implementations are compared against; see test/routes/conform.py.

Layers of the wrapper kinds carry an opaque wrapper family. The spec names no
concrete wrapper; the family only tells two wrappers of the same kind apart. A
family of ANY stands for a layer a rule creates, which a wrapper of any family
that provides the layer kind can serve.
"""

from dataclasses import dataclass, field, replace

from spec.schema import (
    Access,
    ExposureRule,
    LayerTrait,
    PatternForm,
    Predicate,
    RequestCategory,
    Slot,
    Spec,
    ValueCategory,
    Via,
)

LVALUE = ValueCategory.LVALUE
RVALUE = ValueCategory.RVALUE
PRVALUE = ValueCategory.PRVALUE

ANY = "*"
# Layer kinds provided by wrappers, as opposed to the language.
WRAPPER_KINDS = ("alt", "opt", "own", "share")


@dataclass(frozen=True, slots=True)
class Leaf:
    """A leaf type and the properties route rules depend on.

    `bases` names the interfaces the leaf implements.
    """

    name: str
    copyable: bool = True
    constructible: bool = True
    bases: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class Shape:
    """A leaf (`layer` is None) or a layer kind applied to argument shapes."""

    layer: str | None = None
    args: tuple["Shape", ...] = ()
    leaf: Leaf | None = None
    extent: int | None = None
    family: str | None = None

    def size(self) -> int:
        return 1 + sum(arg.size() for arg in self.args)


def leaf(value: Leaf) -> Shape:
    return Shape(leaf=value)


def layer(kind: str, *args: Shape, extent: int | None = None, family: str | None = None) -> Shape:
    if family is None and kind in WRAPPER_KINDS:
        family = "std"
    return Shape(layer=kind, args=args, extent=extent, family=family)


class RequestKind:
    LVALUE_REFERENCE = "lvalue_reference"
    RVALUE_REFERENCE = "rvalue_reference"
    VALUE = "value"


@dataclass(frozen=True, slots=True)
class Request:
    kind: str  # a RequestKind
    shape: Shape


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
    """A derived term and how it was reached."""

    shape: Shape
    category: ValueCategory
    rules: tuple[str, ...] = ()
    copied: Shape | None = None  # the shape the copy rule copied
    consumed: bool = False


@dataclass
class Model:
    spec: Spec
    layers: dict = field(init=False)
    scopes: dict = field(init=False)

    def __post_init__(self):
        self.layers = {kind.id: kind for kind in self.spec.layer_kinds}
        self.scopes = {scope.id: scope for scope in self.spec.scopes}


# Shape properties


def copyable(shape: Shape) -> bool:
    if shape.layer is None:
        return shape.leaf.copyable
    if shape.layer in ("own", "array", "array_unbounded", "ref"):
        return False
    if shape.layer in ("ptr", "share"):
        return True
    return all(copyable(arg) for arg in shape.args)


def constructible(shape: Shape) -> bool:
    """Whether the default factory can construct the shape."""
    if shape.layer is None:
        return shape.leaf.constructible
    if shape.layer in ("ref", "array_unbounded"):
        return False
    return constructible(shape.args[0])


def constructed_leaves(shape: Shape) -> int:
    """The leaves the default factory constructs for the shape."""
    if shape.layer is None:
        return 1
    if shape.layer == "array":
        return shape.extent * constructed_leaves(shape.args[0])
    return constructed_leaves(shape.args[0])


def leaves_held_by_value(shape: Shape) -> int:
    """The leaves a copy of the shape copies."""
    if shape.layer is None:
        return 1
    if shape.layer in ("ptr", "share", "own", "ref"):
        return 0
    if shape.layer == "array":
        return shape.extent * leaves_held_by_value(shape.args[0])
    return leaves_held_by_value(shape.args[0])


def addressable(shape: Shape) -> bool:
    """Whether a shape is an object that can be referred to and pointed to."""
    return shape.layer != "ptr"


def innermost_leaf(shape: Shape) -> Leaf:
    while shape.layer is not None:
        shape = shape.args[0]
    return shape.leaf


def holds_in_array(shape: Shape) -> bool:
    if shape.layer in ("array", "array_unbounded"):
        return True
    return any(holds_in_array(arg) for arg in shape.args)


def has_trait(model: Model, shape: Shape, trait: LayerTrait) -> bool:
    return shape.layer is not None and trait in model.layers[shape.layer].traits


def shapes_match(derived: Shape, requested: Shape) -> bool:
    """Whether a derived shape serves a requested one; ANY matches any family."""
    if derived.layer != requested.layer or derived.extent != requested.extent:
        return False
    if derived.layer is None:
        return derived.leaf == requested.leaf
    if ANY not in (derived.family, requested.family) and derived.family != requested.family:
        return False
    return len(derived.args) == len(requested.args) and all(
        shapes_match(d, r) for d, r in zip(derived.args, requested.args)
    )


# Patterns


def match(pattern, shape: Shape, bindings: dict) -> list[dict]:
    """Every way the pattern matches the shape, as variable bindings."""
    if pattern.form is PatternForm.VARIABLE:
        bound = bindings.get(pattern.name)
        if bound is not None and bound != shape:
            return []
        return [{**bindings, pattern.name: shape}]
    if pattern.form is not PatternForm.LAYER or shape.layer != pattern.name:
        return []
    if pattern.name == "alt":
        # The variadic layer pattern of one argument matches any argument.
        return [m for arg in shape.args for m in match(pattern.args[0], arg, bindings)]
    if len(pattern.args) != len(shape.args):
        return []
    results = [bindings]
    for sub, arg in zip(pattern.args, shape.args):
        results = [m for b in results for m in match(sub, arg, b)]
    return results


def instantiate(pattern, bindings: dict, family: str | None) -> Shape:
    if pattern.form is PatternForm.VARIABLE:
        return bindings[pattern.name]
    args = tuple(instantiate(arg, bindings, None) for arg in pattern.args)
    kind = pattern.name
    return Shape(layer=kind, args=args, family=(family or ANY) if kind in WRAPPER_KINDS else None)


def upcast_leaf(shape: Shape, base: Leaf) -> Shape:
    if shape.layer is None:
        return leaf(base)
    return replace(shape, args=(upcast_leaf(shape.args[0], base), *shape.args[1:]))


# Registration


def storage_form(model: Model, shape: Shape) -> str:
    """The most specific storage form whose pattern matches the shape."""
    best, depth = "plain", 0
    for form in model.spec.storage_forms:
        if form.pattern.form is PatternForm.VARIABLE:
            continue
        if match(form.pattern, shape, {}):
            size = pattern_size(form.pattern)
            if size > depth:
                best, depth = form.id, size
    return best


def pattern_size(pattern) -> int:
    return 1 + sum(pattern_size(arg) for arg in pattern.args)


def exposed_categories(model: Model, scope_id: str, form: str) -> tuple:
    for exposure in model.scopes[scope_id].exposures:
        if exposure.form == form:
            return exposure.categories
    return ()


def interface_probes(interfaces: tuple[Leaf, ...]) -> list[Request]:
    """The requests that show a binding serves its interfaces."""
    probes = []
    for interface in interfaces:
        base = leaf(interface)
        probes += [
            Request(RequestKind.LVALUE_REFERENCE, base),
            Request(RequestKind.VALUE, layer("ptr", base)),
            Request(RequestKind.VALUE, Shape(layer="own", args=(base,), family=ANY)),
            Request(RequestKind.VALUE, Shape(layer="share", args=(base,), family=ANY)),
        ]
    return probes


def register(
    model: Model,
    scope_id: str,
    stored: Shape,
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
    if not scope.nested_layers and stored.layer is not None:
        if any(arg.layer is not None for arg in stored.args):
            return False, f"{scope_id} does not store nested layers"
    if "instance" not in scope.needs and factory is None and not constructible(stored):
        return False, "the stored shape has no default factory"
    if interfaces:
        if holds_in_array(stored):
            return False, "an array element cannot be exposed as an interface"
        if not any(
            serve(model, scope_id, stored, probe, interfaces) is not None
            for probe in interface_probes(interfaces)
        ):
            return False, "no declared interface can be served"
    return True, ""


# Derivation


def derive(
    model: Model,
    scope_id: str,
    stored: Shape,
    interfaces: tuple[Leaf, ...],
    bound: int,
) -> list[Derivation]:
    scope = model.scopes[scope_id]
    form = storage_form(model, stored)
    exposed = set(exposed_categories(model, scope_id, form))
    start = Derivation(stored, scope.materializes, consumed=scope.materializes is RVALUE)
    seen = {(stored, scope.materializes)}
    queue = [start]
    results = [start]
    while queue:
        term = queue.pop(0)
        for rule in model.spec.routes:
            if rule.from_shape.category not in (None, term.category):
                continue
            if rule.exposure is not None and rule.exposure not in exposed:
                continue
            for bindings in match(rule.from_shape, term.shape, {}):
                if not requirements_hold(model, scope, rule, bindings):
                    continue
                for shape in derived_shapes(rule, bindings, term.shape, interfaces):
                    category = rule.to_shape.category or term.category
                    if shape.size() > bound or (shape, category) in seen:
                        continue
                    seen.add((shape, category))
                    derived = Derivation(
                        shape,
                        category,
                        rules=(*term.rules, rule.id),
                        copied=bindings.get("X") if rule.id == "copy" else term.copied,
                        consumed=term.consumed or rule.access is Access.CONSUME,
                    )
                    results.append(derived)
                    queue.append(derived)
    return results


def derived_shapes(rule, bindings: dict, source: Shape, interfaces: tuple[Leaf, ...]):
    keeps_family = (
        rule.from_shape.form is PatternForm.LAYER and rule.to_shape.form is PatternForm.LAYER
    )
    shape = instantiate(rule.to_shape, bindings, source.family if keeps_family else None)
    if rule.via is not Via.UPCAST:
        yield shape
        return
    stored_leaf = bindings["L"].leaf
    for interface in interfaces:
        if interface.name in stored_leaf.bases:
            yield upcast_leaf(shape, interface)


def requirements_hold(model: Model, scope, rule, bindings: dict) -> bool:
    for requirement in rule.requires:
        subject = bindings[requirement.subject]
        if requirement.predicate is Predicate.COPYABLE and not copyable(subject):
            return False
        if requirement.predicate is Predicate.ARRAY and subject.layer != "array":
            return False
        if requirement.predicate is Predicate.ADDRESSABLE and not addressable(subject):
            return False
        if requirement.predicate is Predicate.LEAF and subject.layer is not None:
            return False
    if rule.id == "copy" and scope.exposure_rule is ExposureRule.HANDLE_COPIES:
        if not has_trait(model, bindings["X"], LayerTrait.SHARING):
            return False
    return True


def accepts(request: Request, term: Derivation, exposed: set) -> bool:
    if not shapes_match(term.shape, request.shape):
        return False
    if request.kind == RequestKind.LVALUE_REFERENCE:
        return (
            term.category is LVALUE
            and RequestCategory.LVALUE in exposed
            and addressable(term.shape)
        )
    if term.shape.layer in ("array", "array_unbounded"):
        return False
    if term.category is RVALUE:
        return RequestCategory.RVALUE in exposed
    return request.kind == RequestKind.VALUE and term.category is PRVALUE


def finds(stored: Shape, request: Request, interfaces: tuple[Leaf, ...]) -> bool:
    """Whether the request names the binding: its leaf is a declared interface,
    or the stored leaf when no interface is declared."""
    requested = innermost_leaf(request.shape)
    if interfaces:
        return requested in interfaces
    return requested == innermost_leaf(stored)


def derivation_bound(stored: Shape) -> int:
    """Routes derive shapes at most one layer larger than the stored shape.

    The bound depends on the binding only, so a binding can publish every term
    it serves before any request is known.
    """
    return stored.size() + 1


def serve(model, scope_id, stored, request, interfaces) -> Derivation | None:
    exposed = set(exposed_categories(model, scope_id, storage_form(model, stored)))
    bound = derivation_bound(stored)
    candidates = [
        term
        for term in derive(model, scope_id, stored, interfaces, bound)
        if accepts(request, term, exposed)
    ]
    if not candidates:
        return None
    # Prefer the derivation with the fewest rules, then without a copy, then the
    # one whose rules come first in the spec.
    order = {rule.id: index for index, rule in enumerate(model.spec.routes)}
    return min(
        candidates,
        key=lambda t: (len(t.rules), t.copied is not None, [order[r] for r in t.rules]),
    )


def leaf_location(term: Derivation) -> str:
    if term.consumed:
        return "new"
    if term.copied is not None and leaves_held_by_value(term.copied) > 0:
        return "new"
    return "stored"


def resolve(
    model: Model,
    scope_id: str,
    stored: Shape,
    request: Request,
    interfaces: tuple[Leaf, ...] = (),
) -> Outcome:
    registered, reason = register(model, scope_id, stored, interfaces)
    if not registered:
        return Outcome(registered=False, reason=reason)
    if not finds(stored, request, interfaces):
        return Outcome(registered=True)
    term = serve(model, scope_id, stored, request, interfaces)
    if term is None:
        return Outcome(registered=True, found=True)

    scope = model.scopes[scope_id]
    lazily_constructed = scope.slot is not Slot.EXTERNAL_REF
    per_request = scope.source_access.value == "consume"
    built = constructed_leaves(stored) if lazily_constructed else 0
    constructions = (built, built if per_request else 0)

    location = leaf_location(term)
    if request.kind == RequestKind.VALUE and request.shape.layer is None:
        identity = (None, None)
    elif location == "new":
        identity = ("new", "new")
    else:
        identity = ("new" if lazily_constructed else "existing", "existing")
    stable = location == "stored" and identity != (None, None)
    copies = leaves_held_by_value(term.copied) if term.copied is not None else 0
    return Outcome(
        registered=True,
        found=True,
        served=True,
        rules=term.rules,
        copies=copies,
        constructions=constructions,
        identity=identity,
        stable=stable,
    )
