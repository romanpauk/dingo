"""The semantics of resolution: what a registration should be able to satisfy,
derived from what its scope keeps, what its stored type is made of and what
a request asks for. The probe compares the library with it.

A type is a stack of layers around a leaf: std::shared_ptr<L> is the layer
shared_ptr around L. Every layer, leaf and scope is declared by its
properties, and the rules name properties, never a stored form. A request
resolves when its target can be derived from the source of the scope by the
steps below. The semantics also say which registrations the library accepts,
the interfaces they bind, and how a request that does not resolve fails.

Known gaps of the library are not part of the rules: each is a GAP, a filter
over a step, a reference or a storage the rules allow, which the expectations
apply on top of them.
"""

from dataclasses import dataclass, replace
from enum import Enum
import functools
import itertools

from .vocabulary import (SHAPE_DEPTH, WRAPPERS, Answer, Category, Container, Entry,
                         InterfaceSet, Leaf, Position, Scope, Selection,
                         Shape, StoredForm)


class Owner(Enum):
    """What a layer owns of what it holds."""
    NONE = "none"        # a raw pointer refers to it
    SOLE = "sole"        # a unique_ptr or an optional owns it alone
    SHARED = "shared"    # a shared_ptr shares it


@dataclass(frozen=True)
class Layer:
    """A layer around an element."""
    name: str
    owner: Owner
    # Whether a copy of the layer holds the same element; a copy of an
    # optional holds a copy of it.
    shares_element: bool
    # Whether a copy of the layer can be made at all.
    copyable: bool
    # Whether a layer of an element can be made from a value of it, moved
    # into a new object the layer holds.
    made_from_value: bool
    # The layers an owned value of this one converts to, taking over or
    # sharing its element.
    converts_to: tuple = ()
    # Whether it is made only of a bare element, as an alternative type is
    # made of one of its alternatives.
    holds_element_only: bool = False
    # Whether the object can be moved at all; an array cannot.
    relocatable: bool = True
    # Whether its element can be taken from it; the first element of an
    # array is not the array.
    gives_element: bool = True
    # The layer an array decays to: a pointer to its first element.
    decays_to: str = None
    # Whether it is an array of a known bound, made of elements
    # value-initialized.
    bounded: bool = False
    # For a raw pointer: the layers a handle that takes it over holds around
    # its element; None for a layer that is not a raw pointer.
    adopted_with: tuple = None
    # Whether a handle of it takes over a raw pointer the request owns.
    adopts: bool = False
    # Whether a registration bound only to the abstract interface stores a
    # layer of the leaf as one of the interface.
    rebinds: bool = False
    # Whether a scope can construct the element in storage the layer holds
    # before the layer is made, and so publish it early.
    constructs_in_place: bool = False


LAYERS = {layer.name: layer for layer in (
    Layer("pointer", Owner.NONE, shares_element=True, copyable=True,
          made_from_value=False, adopted_with=(), rebinds=True),
    Layer("shared_ptr", Owner.SHARED, shares_element=True, copyable=True,
          made_from_value=True, adopts=True, rebinds=True,
          constructs_in_place=True),
    Layer("unique_ptr", Owner.SOLE, shares_element=True, copyable=False,
          made_from_value=True, converts_to=("shared_ptr",), adopts=True,
          rebinds=True),
    Layer("optional", Owner.SOLE, shares_element=False, copyable=True,
          made_from_value=True),
    # An alternative type, std::variant<L, other>: its element is the
    # alternative it holds.
    Layer("variant", Owner.SOLE, shares_element=False, copyable=True,
          made_from_value=True, holds_element_only=True,
          constructs_in_place=True),
    # A bounded array of two elements, and an array of unknown bound inside
    # a handle: their element is their first element, reached through the
    # pointer they decay to.
    Layer("array2", Owner.SOLE, shares_element=False, copyable=False,
          made_from_value=False, relocatable=False, gives_element=False,
          decays_to="array_pointer", bounded=True,
          constructs_in_place=True),
    Layer("array", Owner.SOLE, shares_element=False, copyable=False,
          made_from_value=False, relocatable=False, gives_element=False,
          decays_to="array_pointer"),
    # The pointer an array decays to, to its first element: a pointer that
    # owns the array is taken over by a handle of an array.
    Layer("array_pointer", Owner.NONE, shares_element=True, copyable=True,
          made_from_value=False, gives_element=False,
          adopted_with=("array",)),
)}

# The layer a pointer to an object is.
POINTER = "pointer"
ADOPTERS = tuple(name for name, layer in LAYERS.items() if layer.adopts)


@dataclass(frozen=True)
class LeafTraits:
    copyable: bool
    movable: bool
    # Whether it has a default constructor, and whether only a factory the
    # registration names makes it.
    default_constructible: bool = True
    made_by_factory: bool = False


LEAVES = {
    Leaf.COPYABLE: LeafTraits(copyable=True, movable=True),
    Leaf.MOVE_ONLY: LeafTraits(copyable=False, movable=True),
    Leaf.IMMOVABLE: LeafTraits(copyable=False, movable=False),
    Leaf.FACTORY: LeafTraits(copyable=True, movable=True,
                             default_constructible=False,
                             made_by_factory=True),
}


@dataclass(frozen=True)
class ScopeTraits:
    # Whether a resolution makes a new instance the request owns, rather
    # than one the container or the caller keeps.
    makes: bool
    # Whether it is given its instance rather than making it.
    given: bool = False
    # Whether it publishes its instance before constructing it, in place: a
    # cyclical scope, whose instance is not copied.
    publishes_early: bool = False


SCOPES = {
    Scope.UNIQUE: ScopeTraits(makes=True),
    Scope.SHARED: ScopeTraits(makes=False),
    Scope.EXTERNAL: ScopeTraits(makes=False, given=True),
    Scope.SHARED_CYCLICAL: ScopeTraits(makes=False, publishes_early=True),
}

# The stack a stored form puts around its leaf, outermost first.
FORMS = {
    StoredForm.PLAIN: (),
    StoredForm.REFERENCE: (),
    StoredForm.POINTER: ("pointer",),
    StoredForm.SHARED_HANDLE: ("shared_ptr",),
    StoredForm.UNIQUE_HANDLE: ("unique_ptr",),
    StoredForm.OPTIONAL: ("optional",),
    StoredForm.ALTERNATIVE: ("variant",),
    StoredForm.BOUNDED_ARRAY: ("array2",),
    StoredForm.UNIQUE_ARRAY_HANDLE: ("unique_ptr", "array"),
    StoredForm.SHARED_ARRAY_HANDLE: ("shared_ptr", "array"),
    StoredForm.SHARED_BOUNDED_ARRAY_HANDLE: ("shared_ptr", "array2"),
    **{form: tuple(WRAPPERS[name][0] for name in form.wrappers)
       for form in StoredForm if form.wrappers},
}

# The stack a request shape asks for around its interface.
SHAPES = {
    Shape.SHARED_ARRAY: ("shared_ptr", "array"),
    Shape.UNIQUE_ARRAY: ("unique_ptr", "array"),
    Shape.SHARED_BOUNDED_ARRAY: ("shared_ptr", "array2"),
    **{shape: tuple(WRAPPERS[name][0] for name in shape.wrappers)
       for shape in Shape if not shape.is_array},
}

@dataclass(frozen=True, order=True)
class Bound:
    """An interface a registration binds: layers around an element, which is
    its leaf, the abstract interface of the leaf, or the other alternative of
    an alternative type."""
    layers: tuple
    element: str


LEAF = Bound((), "leaf")
INTERFACE = Bound((), "interface")


class Lifetime(Enum):
    """How long an object lives, the longer first."""
    # Kept by the container or by the caller, as long as the container.
    STABLE = 2
    # Made for the request: it lives while the construction or call that
    # requested it runs, in the context of the resolution.
    REQUEST = 1


class Origin(Enum):
    """Whose an object is, and how the request came to it."""
    # Kept by the scope, the caller or a layer the request does not own
    # alone: the request may refer to it, not consume it.
    BORROWED = "borrowed"
    # Made for the request, which owns it and may consume it.
    MADE = "made"
    # A copy the request made of something, which it owns.
    COPIED = "copied"

    @property
    def owned(self):
        return self is not Origin.BORROWED


@dataclass(frozen=True)
class Thing:
    """An object a derivation reaches.

    layers: the layers around the element, outermost first
    element: "leaf", or "interface" when it is the abstract interface
    lifetimes: how long each layer and the element live, outermost first
    origin: whose the outermost object is
    instance: whether it is the instance the scope resolves, or a layer of
      it, rather than a copy of it
    contained: the holder made for the request it is the element of, or
      was copied from the element of, or None
    """
    layers: tuple
    element: str
    lifetimes: tuple
    origin: Origin
    instance: bool
    contained: str = None

    @property
    def lifetime(self):
        return self.lifetimes[0]


@dataclass(frozen=True)
class Context:
    """What a derivation knows of its registration."""
    leaf: LeafTraits
    scope: ScopeTraits
    # Whether the leaf is requested as its abstract interface.
    abstract: bool
    # The limitations the derivation applies.
    limits: frozenset = frozenset()


# How a scope comes to its source.


def makes_storage(form, context):
    """Whether the scope can make what it stores, or is given it. A bounded
    array is made of elements value-initialized, which a leaf without a
    default constructor cannot be; a scope that constructs its instance in
    place cannot take a holder of it from a factory."""
    layers = FORMS[form]
    if context.scope.given:
        return True
    outer = LAYERS[layers[0]] if layers else None
    if outer and outer.bounded and not context.leaf.default_constructible:
        return False
    return not (context.scope.publishes_early and
                context.leaf.made_by_factory and outer and
                outer.shares_element)


def source(scope, form, interfaces=InterfaceSet.DEFAULT):
    """What a resolution of the scope starts from: the stored object. A
    pointer or a handle of the leaf a registration binds only to its abstract
    interface is stored as one of the interface."""
    layers = FORMS[form]
    traits = SCOPES[scope]
    lifetime = Lifetime.REQUEST if traits.makes else Lifetime.STABLE
    rebound = (interfaces is InterfaceSet.INTERFACE and len(layers) == 1 and
               LAYERS[layers[0]].rebinds)
    return Thing(layers, "interface" if rebound else "leaf",
                 (lifetime,) * (len(layers) + 1),
                 Origin.MADE if traits.makes else Origin.BORROWED,
                 instance=True)


# What can be done with a thing.


def copies_element(thing, context):
    """Whether the element of a thing may be copied: a leaf that can be,
    unless it is the instance of a scope that publishes it before it is
    constructed."""
    if (context.scope.publishes_early and thing.instance and
            thing.origin is not Origin.COPIED):
        return False
    return context.leaf.copyable


def copied_layers(layers):
    """How many of the layers a copy or a move of a stack makes anew: those
    down to the first that shares its element, which the new one shares; all
    of them and the element, when none does."""
    for depth, name in enumerate(layers, 1):
        if LAYERS[name].shares_element:
            return depth
    return len(layers) + 1


def stack_can(layers, layer_can, element_can):
    """Whether what a copy or a move makes anew of a stack can be: each
    layer down to the first that shares its element, and the element when
    none does."""
    depth = copied_layers(layers)
    return (all(layer_can(LAYERS[name]) for name in layers[:depth]) and
            (depth <= len(layers) or element_can))


def can_copy(thing, context):
    return stack_can(thing.layers, lambda layer: layer.copyable,
                     copies_element(thing, context))


def can_move(thing, context):
    """Whether a thing can be moved: a pointer or a handle can, an array
    cannot, and an optional or an alternative type can when what it holds
    can."""
    return stack_can(thing.layers, lambda layer: layer.relocatable,
                     context.leaf.movable)


# The deepest stack a derivation makes: a request has at most SHAPE_DEPTH
# wrappers around its interface, which may be a layer of its own, such as an
# alternative type.
DEPTH = SHAPE_DEPTH + 1


def transitions(thing, context):
    """The steps the semantics allow from a thing, each as its kind and the
    thing it reaches."""
    layers = thing.layers
    if layers:
        layer = LAYERS[layers[0]]
        below = layers[1:]
        if layer.decays_to:
            # An array decays to a pointer to its first element, which owns
            # it when the request owns the array. An array a scope makes is
            # handed over as a pointer that owns it, to it or to its first
            # element.
            yield "decay", replace(
                thing, layers=(layer.decays_to,) + below,
                lifetimes=(Lifetime.REQUEST,) + thing.lifetimes[1:])
            if thing.origin is Origin.MADE:
                yield "hand_over", replace(
                    thing, layers=(POINTER,) + layers,
                    lifetimes=(Lifetime.REQUEST,) + thing.lifetimes)
        elif layer.adopted_with is not None and thing.origin is Origin.MADE:
            # A raw pointer the request owns, not a copy of one the scope
            # keeps, owns its element: it is handed over as a pointer, or
            # taken over by a handle, of an array when it points to one.
            for handle in ADOPTERS:
                yield "adopt", replace(
                    thing, layers=(handle,) + layer.adopted_with + below,
                    lifetimes=(Lifetime.REQUEST,) *
                    (1 + len(layer.adopted_with)) + thing.lifetimes[1:])
        elif layer.gives_element:
            # The element of a pointer or a handle lives as long as it is
            # held; it is owned by the request only when it owns the layer
            # and the layer owns its element alone. The element of a copy of
            # a layer that shares it is the element itself, not a copy.
            made = thing.lifetimes[1] is Lifetime.REQUEST
            yield "element", replace(
                thing, layers=below, lifetimes=thing.lifetimes[1:],
                origin=(thing.origin if layer.owner is Owner.SOLE
                        else Origin.BORROWED),
                contained=thing.contained or
                (layer.name if made else None))
        # A handle the request owns converts to another, sharing or taking
        # over its element.
        if thing.origin.owned:
            for name in layer.converts_to:
                yield "convert", replace(
                    thing, layers=(name,) + below,
                    lifetimes=(Lifetime.REQUEST,) + thing.lifetimes[1:])
    # A leaf is one of its interface too, as the base it is. A pointer or a
    # handle of it converts to one of the interface: a new pointer or handle,
    # which the binding retains for what it keeps, or which takes over one the
    # request owns when it cannot be copied.
    if thing.element == "leaf" and context.abstract:
        outer = LAYERS[layers[0]] if layers else None
        if outer is None or (len(layers) == 1 and outer.shares_element and
                             outer.copyable):
            yield "upcast", replace(thing, element="interface")
        elif len(layers) == 1 and outer.shares_element and thing.origin.owned:
            yield "upcast", replace(
                thing, element="interface",
                lifetimes=(Lifetime.REQUEST,) + thing.lifetimes[1:])
        elif len(layers) > 1:
            # A copy of the layers above the handle that holds the leaf, the
            # first of them to share it, holds a handle of the interface: a
            # new object, which shares the leaf. A handle of a handle of the
            # leaf is not one of a handle of the interface.
            depth = copied_layers(layers)
            if depth == len(layers) and all(
                    LAYERS[name].copyable for name in layers):
                yield "upcast", replace(
                    thing, element="interface", origin=Origin.COPIED,
                    instance=False,
                    lifetimes=(Lifetime.REQUEST,) * depth +
                    thing.lifetimes[depth:])
    # A copy: a new object the request owns. What a layer that shares its
    # element holds is not copied: the copy still holds the instance.
    if can_copy(thing, context):
        depth = copied_layers(layers)
        yield "copy", replace(
            thing, lifetimes=(Lifetime.REQUEST,) * depth +
            thing.lifetimes[depth:], origin=Origin.COPIED,
            instance=thing.instance and depth <= len(layers))
    # A new layer holding what the request owns, moved into it: a value it
    # made, or a copy that shares what it was copied of, such as a copy of a
    # shared handle. What a scope keeps, or a copy of a value that shares
    # nothing, is not wrapped: a new layer does not hold a copy of an
    # instance, nor share one that no handle owns. A layer holding a copy
    # that shares what the scope or the caller keeps is kept with it, as the
    # copy in it is: the binding retains it.
    shares = thing.origin is Origin.COPIED and thing.instance
    if ((thing.origin is Origin.MADE or shares) and
            can_move(thing, context) and len(layers) < DEPTH):
        depth = copied_layers(layers)
        if shares and thing.lifetimes[depth] is Lifetime.STABLE:
            lifetimes = ((Lifetime.STABLE,) * (1 + depth) +
                         thing.lifetimes[depth:])
        else:
            lifetimes = (Lifetime.REQUEST,) + thing.lifetimes
        for made in LAYERS.values():
            if made.made_from_value and not (made.holds_element_only and
                                             layers):
                yield "wrap", replace(
                    thing, layers=(made.name,) + layers, lifetimes=lifetimes)


# The known gaps of the library, as filters over what the rules allow.


@dataclass(frozen=True)
class Gap:
    """Where a limitation keeps the library from what the semantics allow:
    at a kind of step from one thing to another, or at a reference or a
    pointer to a thing."""
    limitation: "Limitation"
    at: str
    applies: object


class Limitation(Enum):
    """A known gap of the library: a step it does not take, or a request it
    does not satisfy, although the semantics allow it."""
    # A pointer handed over out of a holder made for the request, such as
    # the T * of a std::unique_ptr<T[]> a unique scope makes. The library
    # keeps this one: handing it over would release the holder the
    # registration declared, and give the caller an owning raw pointer
    # instead. The caller requests the holder; a pointer a call or a kept
    # class takes points into a holder kept in the context.
    POINTER_OUT_OF_MADE_HOLDER = "pointer_out_of_made_holder"


GAPS = (
    Gap(Limitation.POINTER_OUT_OF_MADE_HOLDER, "point",
        lambda context, category, thing: thing.contained is not None),
)


def blocked(context, at, *event):
    """Whether a limitation the context applies blocks an event."""
    return any(gap.at == at and gap.limitation in context.limits and
               gap.applies(context, *event) for gap in GAPS)


def steps(thing, context):
    """The steps from a thing the library takes: those the semantics allow
    that no limitation blocks."""
    found = []
    for kind, following in transitions(thing, context):
        if not blocked(context, kind, thing, following):
            found.append((kind, following))
    return found


@functools.lru_cache(maxsize=None)
def reachable(start, context):
    seen = {start}
    frontier = [start]
    while frontier:
        thing = frontier.pop()
        for _, following in steps(thing, context):
            if following not in seen:
                seen.add(following)
                frontier.append(following)
    return frozenset(seen)


# What a request asks of the things.


def required_lifetime(position, entry):
    """How long an object a request refers to has to live, so that no
    reference dangles. A reference that is passed away, returned by a resolve
    or a construct or held by a class one of them constructs and returns, has
    to outlive the resolution: it may refer only to what the container or the
    caller keeps. One a callable takes, or a class constructed for it, is
    used while the call runs; one a class takes that a shared or a cyclical
    scope keeps is kept with it: either may refer to what is made for the
    request too."""
    if position.kept_by is not None or entry is Entry.INVOKE:
        return Lifetime.REQUEST
    return Lifetime.STABLE


REFERENCES = (Category.LVALUE_REFERENCE, Category.CONST_LVALUE_REFERENCE,
              Category.POINTER, Category.CONST_POINTER)
POINTERS = (Category.POINTER, Category.CONST_POINTER)


def satisfies(things, category, lifetime, layers, context):
    """Whether one of the things of a request's element satisfies it."""
    of_shape = [thing for thing in things if thing.layers == layers]
    if category in REFERENCES:
        # A reference or a pointer refers to the instance, or to a layer of
        # it, that lives long enough.
        if any(thing.instance and thing.lifetime.value >= lifetime.value
               for thing in of_shape):
            return True
        if category in POINTERS:
            # A pointer value: one the request owns, or one whose pointee
            # lives long enough.
            def points(thing):
                outer = thing.layers[:1]
                if (not outer or LAYERS[outer[0]].adopted_with is None or
                        thing.layers[1:] != layers or not thing.instance):
                    return False
                if (thing.origin is Origin.MADE and
                        not blocked(context, "point", category, thing)):
                    return True
                return thing.lifetimes[1].value >= lifetime.value
            return any(points(thing) for thing in things)
        return False
    if category is Category.RVALUE_REFERENCE:
        # The object the request moves from: one it made.
        return any(thing.origin is Origin.MADE and can_move(thing, context)
                   for thing in of_shape)
    # A value: an owned object moved, or any object copied.
    return any((thing.origin.owned and can_move(thing, context)) or
               can_copy(thing, context) for thing in of_shape)


def copyable_shape(layers, context):
    """Whether a value of a stack can be copied: a const one cannot be moved
    from, and a selected request holds the value it is passed."""
    return stack_can(layers, lambda layer: layer.copyable,
                     context.leaf.copyable)


def selector_rejects(layers, category, position, entry, selection, context):
    """Whether a request a key or an annotation selects cannot be held by
    value in its selector, which it is passed in: a const value is copied
    into it, another one moved or copied. A direct resolve returns the value
    instead."""
    if (selection is Selection.PLAIN or
            category not in (Category.VALUE, Category.CONST) or
            (position is Position.DIRECT and entry is Entry.RESOLVE)):
        return False
    if category is Category.CONST:
        return not copyable_shape(layers, context)
    return not stack_relocatable(layers, context)


def entry_of(entry, position, selection, category):
    """The entry a request is resolved through. construct<T>() of a
    reference, of an rvalue reference, or of a request a key or an
    annotation selects, resolves it: it names an instance or a binding, not
    a value to make."""
    if position is Position.DIRECT and entry is Entry.CONSTRUCT and (
            selection is not Selection.PLAIN or not category.by_value or
            category is Category.RVALUE_REFERENCE):
        return Entry.RESOLVE
    return entry


def resolves(scope, form, leaf, bound, shape, category, position, entry,
             limits=frozenset(), selection=Selection.PLAIN,
             interfaces=InterfaceSet.DEFAULT):
    """Whether a request of a shape of the bound interface resolves, under
    the limitations given."""
    layers, element = SHAPES[shape] + bound.layers, bound.element
    context = Context(LEAVES[leaf], SCOPES[scope], element == "interface",
                      limits)
    if not makes_storage(form, context):
        return False
    things = [thing for thing in reachable(source(scope, form, interfaces),
                                           context)
              if thing.element == element]
    direct = position is Position.DIRECT
    entry = entry_of(entry, position, selection, category)
    if selector_rejects(layers, category, position, entry, selection,
                        context):
        return False
    if satisfies(things, category, required_lifetime(position, entry), layers,
                 context):
        return True
    # construct<T>() of a value it cannot resolve makes T of the leaf the
    # container resolves by value: a new handle, optional or array of it,
    # the rest of a bounded array value-initialized.
    bounded = any(LAYERS[name].bounded for name in SHAPES[shape])
    if (direct and entry is Entry.CONSTRUCT and element == "leaf" and
            shape is not Shape.OBJECT and
            (context.leaf.default_constructible or not bounded)):
        return resolves(scope, form, leaf, LEAF, Shape.OBJECT,
                        Category.VALUE, Position.DIRECT, Entry.RESOLVE,
                        limits, selection, interfaces)
    return False


LIMITATIONS = frozenset(Limitation)


def expected(scope, form, leaf, bound, shape, category, position, entry,
             selection=Selection.PLAIN, interfaces=InterfaceSet.DEFAULT):
    """What the semantics say of a request, and what the library is expected
    to do: the limitations that keep it from resolving, if it should."""
    args = (scope, form, leaf, bound, shape, category, position, entry)

    def under(limits):
        return resolves(*args, limits=limits, selection=selection,
                        interfaces=interfaces)
    if not under(frozenset()):
        return False, ()
    if under(LIMITATIONS):
        return True, ()
    # The limitations involved: those of the smallest sets whose lifting
    # lets it resolve.
    for size in range(1, len(Limitation) + 1):
        lifting = [set(limits) for limits in
                   itertools.combinations(Limitation, size)
                   if under(LIMITATIONS - set(limits))]
        if lifting:
            return True, tuple(limit for limit in Limitation
                               if any(limit in limits for limits in lifting))
    raise AssertionError("resolves without the limitations, but not when "
                         "all are lifted")


def explain(scope, form, leaf, bound, shape, limits=frozenset(),
            interfaces=InterfaceSet.DEFAULT):
    """The derivations of the things of a request's shape, each as the path
    of steps from the source, a step as its kind and the thing it reaches:
    for reading why the model resolves."""
    context = Context(LEAVES[leaf], SCOPES[scope],
                      bound.element == "interface", limits)
    start = source(scope, form, interfaces)
    parent = {start: None}
    frontier = [start]
    while frontier:
        thing = frontier.pop(0)
        for kind, following in steps(thing, context):
            if following not in parent:
                parent[following] = (kind, thing)
                frontier.append(following)
    paths = []
    for thing in parent:
        if (thing.layers == SHAPES[shape] + bound.layers and
                thing.element == bound.element):
            path = [("target", thing)]
            while parent[thing] is not None:
                kind, previous = parent[thing]
                path[-1] = (kind, path[-1][1])
                path.append(("source", previous))
                thing = previous
            paths.append(list(reversed(path)))
    return paths


# What a registration is: whether the library accepts it, and the interfaces
# it binds.


def registrable(container, scope, form, leaf, interfaces):
    """Whether the library accepts a registration."""
    layers = FORMS[form]
    traits = SCOPES[scope]
    leaf_traits = LEAVES[leaf]
    if traits.given:
        # A static container is given no instance at run time. A scope given
        # a value keeps it, moved or copied in; one given an object, such as
        # a reference, keeps a reference to it.
        if container is Container.STATIC:
            return False
        if form is not StoredForm.REFERENCE and not stack_can(
                layers, lambda layer: layer.relocatable,
                leaf_traits.movable or leaf_traits.copyable):
            return False
    elif form is StoredForm.REFERENCE:
        # Only what the scope is given can be stored as a reference.
        return False
    if traits.publishes_early and not (
            not layers or (len(layers) == 1 and
                           LAYERS[layers[0]].constructs_in_place)):
        # A cyclical scope publishes its instance before it constructs it,
        # in storage it holds.
        return False
    if interfaces is not InterfaceSet.DEFAULT and any(
            LAYERS[name].decays_to or LAYERS[name].holds_element_only
            for name in layers):
        # An array is many elements, and an alternative type may hold the
        # other alternative: neither is one object of the interface.
        return False
    return True


def bound_interfaces(form, interfaces, selection):
    """The interfaces a registration binds. Without interfaces declared it
    binds its leaf, and what holds the leaf as an object of its own: a
    bounded array, and an alternative type with its other alternative. An
    annotated registration declares its leaf."""
    if interfaces is InterfaceSet.INTERFACE:
        return (INTERFACE,)
    if interfaces is InterfaceSet.BOTH:
        return (LEAF, INTERFACE)
    layers = FORMS[form]
    if selection is Selection.ANNOTATED or layers not in (("array2",),
                                                          ("variant",)):
        return (LEAF,)
    if layers == ("variant",):
        return (LEAF, Bound((), "other"), Bound(layers, "leaf"))
    return (LEAF, Bound(layers, "leaf"))


# How the library fails a request it does not resolve.


def stack_relocatable(layers, context):
    """Whether a value of a stack can be moved or copied."""
    return (stack_can(layers, lambda layer: layer.relocatable,
                      context.leaf.movable) or
            copyable_shape(layers, context))


def failure(container, scope, form, leaf, bound, shape, category, position,
            entry, selection=Selection.PLAIN,
            interfaces=InterfaceSet.DEFAULT):
    """How the library fails a request it does not resolve. What the types
    of a request say it cannot be, either container rejects as it compiles:
    a storage that cannot be made, and a value that can be neither moved nor
    copied where one is passed. The static container knows the rest as it
    compiles too, except which alternative an alternative type holds; the
    runtime container finds all of it out as it resolves."""
    layers, element = SHAPES[shape] + bound.layers, bound.element
    context = Context(LEAVES[leaf], SCOPES[scope], element == "interface")
    if not makes_storage(form, context):
        return Answer.REJECTED
    # The other alternative of an alternative type is a type of its own, which
    # can be copied.
    if element == "other":
        context = replace(context, leaf=LEAVES[Leaf.COPYABLE])
    if selector_rejects(layers, category, position,
                        entry_of(entry, position, selection, category),
                        selection, context):
        return Answer.REJECTED
    if (category is Category.RVALUE_REFERENCE and
            position is Position.DIRECT and entry is not Entry.INVOKE and
            element != "interface" and
            not stack_relocatable(layers, context)):
        # resolve<T &&>() returns the value it moves out.
        return Answer.REJECTED
    if container is Container.STATIC:
        # What the storage publishes, a static container resolves as it
        # compiles; the other alternative of an alternative type it publishes
        # too, as it would the leaf, which fails when the alternative type
        # holds the leaf.
        held = element == "other" and resolves(
            scope, form, Leaf.COPYABLE, LEAF, shape, category, position,
            entry, LIMITATIONS, selection, interfaces)
        return Answer.TYPE_NOT_CONVERTIBLE if held else Answer.REJECTED
    return Answer.TYPE_NOT_CONVERTIBLE
