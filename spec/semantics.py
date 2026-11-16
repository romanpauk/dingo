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

import collections
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
    wraps_value: bool
    # The layers an owned value of this one converts to, taking over or
    # sharing its element.
    converts: tuple = ()
    # Whether it is made only of a bare element, as an alternative type is
    # made of one of its alternatives.
    holds_element_only: bool = False
    # Whether the object can be moved at all; an array cannot.
    relocatable: bool = True
    # Whether its element can be taken from it; the first element of an
    # array is not the array.
    gives_element: bool = True
    # The layer an array decays to: a pointer to its first element.
    decay_layer: str = None
    # Whether it is an array of a known bound, made of elements
    # value-initialized.
    bounded: bool = False
    # For a raw pointer: the layers a handle that takes it over holds around
    # its element; None for a layer that is not a raw pointer.
    adopted_layers: tuple = None
    # Whether a handle of it takes over a raw pointer the request owns.
    adopts: bool = False
    # Whether a handle of it takes over one to an array of a known bound,
    # which it deletes as an array: a std::shared_ptr<T[N]> does, a
    # std::unique_ptr<T[N]> deletes it as an object.
    adopts_bounded: bool = False
    # Whether a registration bound only to the abstract interface stores a
    # layer of the leaf as one of the interface.
    rebinds: bool = False
    # Whether a scope can construct the element in storage the layer holds
    # before the layer is made, and so publish it early.
    constructs_in_place: bool = False
    # Whether it may hold no element: a null pointer or handle, an empty
    # optional, or an alternative type that holds the other alternative.
    may_be_empty: bool = False
    # Whether an empty one is a value of its own, which an empty layer
    # around it gives.
    has_empty_value: bool = False
    # Whether it is made of a raw pointer to its element, which it does not
    # own, as an observer handle is.
    observes: bool = False
    # Whether a value of it converts to one of a more qualified element, as
    # a pointer to the element converts: a raw pointer, and a handle whose
    # converting constructor does.
    qualifies: bool = False
    # The layers a value of it that holds an array of a known bound converts
    # to, holding the same array as one of unknown bound, as a
    # std::shared_ptr<T[N]> converts to a std::shared_ptr<T[]>.
    converts_unknown_bound: tuple = ()


LAYERS = {layer.name: layer for layer in (
    Layer("pointer", Owner.NONE, shares_element=True, copyable=True,
          wraps_value=False, adopted_layers=(), rebinds=True,
          may_be_empty=True, qualifies=True),
    Layer("shared_ptr", Owner.SHARED, shares_element=True, copyable=True,
          wraps_value=True, adopts=True, adopts_bounded=True,
          rebinds=True, constructs_in_place=True, may_be_empty=True,
          qualifies=True, converts_unknown_bound=("shared_ptr",)),
    Layer("unique_ptr", Owner.SOLE, shares_element=True, copyable=False,
          wraps_value=True, converts=("shared_ptr",), adopts=True,
          rebinds=True, may_be_empty=True, qualifies=True),
    Layer("optional", Owner.SOLE, shares_element=False, copyable=True,
          wraps_value=True, may_be_empty=True, has_empty_value=True),
    # An alternative type, std::variant<L, other>: its element is the
    # alternative it holds.
    Layer("variant", Owner.SOLE, shares_element=False, copyable=True,
          wraps_value=True, holds_element_only=True,
          constructs_in_place=True, may_be_empty=True),
    # A bounded array of two elements, and an array of unknown bound inside
    # a handle: their element is their first element, reached through the
    # pointer they decay to.
    Layer("array2", Owner.SOLE, shares_element=False, copyable=False,
          wraps_value=False, relocatable=False, gives_element=False,
          decay_layer="array_pointer", bounded=True,
          constructs_in_place=True),
    Layer("array", Owner.SOLE, shares_element=False, copyable=False,
          wraps_value=False, relocatable=False, gives_element=False,
          decay_layer="array_pointer"),
    # The pointer an array decays to, to its first element: a pointer that
    # owns the array is taken over by a handle of an array.
    Layer("array_pointer", Owner.NONE, shares_element=True, copyable=True,
          wraps_value=False, gives_element=False,
          adopted_layers=("array",)),
)}

# The layer a pointer to an object is.
POINTER = "pointer"


def possible(layer):
    """Whether a type can be a layer of these properties. A layer that owns
    nothing, or shares what it owns, holds an element that lives apart from
    it: a copy of it holds the same one. A layer that owns its element alone
    and shares it with a copy cannot be copied: the copy would share what one
    of them owns. A layer that owns nothing is not made of a value and takes
    over no pointer, and only one that takes over a pointer takes over one to
    an array of a known bound. A layer that takes over a pointer holds the
    element apart from it, and so does a layer the leaf is rebound to the interface
    in, and a layer that converts to one of a more qualified element, as a
    pointer to the element does, or to one of an array of unknown bound. An empty value is a value only of a layer
    that may be empty."""
    if layer.owner is not Owner.SOLE and not layer.shares_element:
        return False
    if (layer.owner is Owner.SOLE and layer.shares_element and
            layer.copyable):
        return False
    if layer.owner is Owner.NONE and (layer.wraps_value or layer.adopts):
        return False
    if layer.adopts and not layer.shares_element:
        return False
    if layer.adopts_bounded and not layer.adopts:
        return False
    # A layer the leaf is rebound to the interface in holds it apart, and so
    # does one that converts to one of a more qualified element or of an
    # array of unknown bound.
    if (layer.rebinds or layer.qualifies or
            layer.converts_unknown_bound) and not layer.shares_element:
        return False
    # A layer made of a pointer it does not own owns nothing, and holds none
    # when the pointer is null.
    if layer.observes and (layer.owner is not Owner.NONE or
                           not layer.may_be_empty):
        return False
    return layer.may_be_empty or not layer.has_empty_value


def possible_conversion(source, target):
    """Whether an owned value of a layer can convert to another, taking over
    or sharing its element: only a layer that owns its element alone hands
    it over, and only to a layer that owns an element apart from it."""
    return (source.owner is Owner.SOLE and source.shares_element and
            target.owner is not Owner.NONE and target.shares_element)


@dataclass(frozen=True)
class LeafTraits:
    copyable: bool
    movable: bool
    # Whether it has a default constructor, and whether only a factory the
    # registration names makes it.
    default_constructible: bool = True
    factory_made: bool = False
    # Whether a volatile one can be copied, as a scalar can.
    copies_volatile: bool = False


LEAVES = {
    Leaf.COPYABLE: LeafTraits(copyable=True, movable=True),
    Leaf.MOVE_ONLY: LeafTraits(copyable=False, movable=True),
    Leaf.IMMOVABLE: LeafTraits(copyable=False, movable=False),
    Leaf.FACTORY: LeafTraits(copyable=True, movable=True,
                             default_constructible=False,
                             factory_made=True),
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

def shape_quals(shape, bound):
    """The qualifiers a request of a shape of the bound interface asks of
    each level: none of the object it is of, as its category gives them, and
    of the element of the shape those the shape names, which the element of
    an array of the interface has too, as C++ declares it."""
    depth = len(SHAPES[shape])
    quals = [0] * (depth + len(bound.layers) + 1)
    if shape.const_element:
        quals[depth] = CONST
        for level, name in enumerate(bound.layers, depth):
            if LAYERS[name].decay_layer:
                quals[level + 1] |= quals[level]
    return tuple(quals)


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
class Object:
    """An object a derivation reaches.

    layers: the layers around the element, outermost first
    element: "leaf", or "interface" when it is the abstract interface
    lifetimes: how long each layer and the element live, outermost first
    origin: whose the outermost object is
    instance: whether it is the instance the scope resolves, or a layer of
      it, rather than a copy of it
    contained: the holder made for the request it is the element of, or
      was copied from the element of, or None
    unconstructed: whether it is a wrapper the instance of a scope that
      publishes it before it constructs it holds in place: it is not
      constructed when it is published, so it is only referred to, and
      nothing is derived from it
    absent: whether it may not exist, as the element of a layer that may be
      empty, or what is derived of one: what refers to it or is made of it
      fails when the layer is empty
    full: how many of its outer layers hold their element for sure, as a
      layer the derivation made of a value does
    quals: the qualifiers of each layer and of the element, outermost
      first, as a mask of CONST and VOLATILE: of the outermost as it is
      reached, of the others as the type of the layer above declares them
    owns_pointee: whether the request owns what its outer raw pointer points
      to, which it deletes or hands over: what a pointer a scope makes, or a
      copy of it, points to
    made_layer: whether the derivation made its outer layer of a value or of
      a pointer, which it makes as the layer it needs at once, and so never
      converts
    qualified: whether its outer layer is converted to one of a more
      qualified element, as of all the qualifiers it is converted to at
      once, and so is not converted again
    """
    layers: tuple
    element: str
    lifetimes: tuple
    origin: Origin
    instance: bool
    contained: str = None
    unconstructed: bool = False
    absent: bool = False
    full: int = 0
    quals: tuple = None
    owns_pointee: bool = False
    made_layer: bool = False
    qualified: bool = False

    def __post_init__(self):
        if self.quals is None:
            object.__setattr__(self, "quals", (0,) * (len(self.layers) + 1))

    @property
    def lifetime(self):
        return self.lifetimes[0]


# The qualifiers of an object.
CONST = 1
VOLATILE = 2
# A level of a pointer or a handle converted to one of a more qualified
# element, which may gain any qualifiers a request asks of it, as matches
# says, on top of those it is marked with.
GAIN = 4


def matches(got, wanted):
    """Whether the qualifiers of the levels of an object are those a request
    asks: each level as it is, or of more where it may gain them, which a
    level below the first that may gain only does where every level above it
    is made const, as C++ converts a pointer."""
    for level, (quals, asked) in enumerate(zip(got, wanted)):
        if not quals & GAIN:
            if quals != asked:
                return False
            continue
        if quals & 3 & ~asked:
            return False
        above = [index for index in range(level) if got[index] & GAIN]
        if above and not all(wanted[index] & CONST for index in above):
            return False
    return True


def held_quals(object, layer):
    """The qualifiers of the element of the outer layer of an object: as the
    layer declares them, and those of the layer too when it holds the element
    in place, as an optional or an array does."""
    inherited = 0 if layer.shares_element else object.quals[0]
    return object.quals[1] | inherited


def anew_quals(object):
    """The qualifiers of what a copy or a move of an object makes anew, with
    whether each is the element."""
    depth = copied_layers(object.layers)
    return [(quals, level == len(object.layers))
            for level, quals in enumerate(object.quals[:depth])]

@dataclass(frozen=True)
class Context:
    """What a derivation knows of its registration."""
    leaf: LeafTraits
    scope: ScopeTraits
    # Whether the leaf is requested as its abstract interface.
    abstract: bool
    # The limitations the derivation applies.
    limits: frozenset = frozenset()
    # The names of the layers a derivation may make: a handle that adopts a
    # pointer, or a layer it wraps a value in. The rules name none of them;
    # a program has these and any other its types define.
    universe: tuple = tuple(LAYERS)


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
                context.leaf.factory_made and outer and
                outer.shares_element)


def source(scope, form, interfaces=InterfaceSet.DEFAULT, quals=None):
    """What a resolution of the scope starts from: the stored object, of the
    qualifiers its type declares. A pointer or a handle of the leaf a
    registration binds only to its abstract interface is stored as one of
    the interface."""
    layers = FORMS[form]
    traits = SCOPES[scope]
    lifetime = Lifetime.REQUEST if traits.makes else Lifetime.STABLE
    rebound = (interfaces is InterfaceSet.INTERFACE and len(layers) == 1 and
               LAYERS[layers[0]].rebinds)
    pointer = bool(layers) and LAYERS[layers[0]].adopted_layers is not None
    return Object(layers, "interface" if rebound else "leaf",
                 (lifetime,) * (len(layers) + 1),
                 Origin.MADE if traits.makes else Origin.BORROWED,
                 instance=True, quals=quals,
                 owns_pointee=traits.makes and pointer)


# What can be done with an object.


def copies_element(object, context):
    """Whether the element of an object may be copied: a leaf that can be,
    unless it is the instance of a scope that publishes it before it is
    constructed."""
    if (context.scope.publishes_early and object.instance and
            object.origin is not Origin.COPIED):
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


def slices(object):
    """Whether a copy or a move of an object would slice the leaf: the interface
    without a layer is the base the leaf is, which is referred to, never
    copied, moved or held in place by a new layer."""
    return object.element == "interface" and not object.layers


def can_copy(object, context):
    """Whether an object can be copied: what a copy makes anew can be, and
    none of it is volatile, but a leaf that copies a volatile one."""
    return not slices(object) and stack_can(
        object.layers, lambda layer: layer.copyable,
        copies_element(object, context)) and all(
            not quals & VOLATILE or (element and context.leaf.copies_volatile)
            for quals, element in anew_quals(object))


def can_move(object, context):
    """Whether an object can be moved: a pointer or a handle can, an array
    cannot, and an optional or an alternative type can when what it holds
    can. A const or a volatile object is not moved from."""
    return stack_can(object.layers, lambda layer: layer.relocatable,
                     context.leaf.movable) and all(
                         not quals for quals, _ in anew_quals(object))


# The deepest stack a derivation makes: a request has at most SHAPE_DEPTH
# wrappers around its interface, which may be a layer of its own, such as an
# alternative type.
DEPTH = SHAPE_DEPTH + 1


def transitions(object, context):
    """The steps the semantics allow from an object, each as its kind and the
    object it reaches. A step that makes another outer layer makes one not
    converted to a more qualified element."""
    for kind, following in object_transitions(object, context):
        if following.qualified and kind != "qualify":
            following = replace(following, qualified=False)
        yield kind, following


def object_transitions(object, context):
    layers = object.layers
    if object.unconstructed:
        return
    if layers:
        layer = LAYERS[layers[0]]
        below = layers[1:]
        if layer.adopted_layers == () and not object.owns_pointee:
            # A handle that does not own its element, made of a raw pointer
            # to it that owns nothing either: a new object, which points to
            # what the pointer does.
            for name in (name for name in context.universe
                         if LAYERS[name].observes):
                yield "observe", replace(
                    object, layers=(name,) + below, origin=Origin.MADE,
                    lifetimes=(Lifetime.REQUEST,) + object.lifetimes[1:],
                    quals=(0,) + object.quals[1:], made_layer=True)
        if layer.decay_layer:
            # An array decays to a pointer to its first element, which owns
            # it when the request owns the array. An array a scope makes is
            # handed over as a pointer that owns it, to it or to its first
            # element.
            yield "decay", replace(
                object, layers=(layer.decay_layer,) + below,
                lifetimes=(Lifetime.REQUEST,) + object.lifetimes[1:],
                quals=(0, held_quals(object, layer)) + object.quals[2:],
                owns_pointee=object.origin is Origin.MADE, made_layer=False)
            if object.origin is Origin.MADE:
                yield "hand_over", replace(
                    object, layers=(POINTER,) + layers,
                    lifetimes=(Lifetime.REQUEST,) + object.lifetimes,
                    quals=(0,) + object.quals, owns_pointee=True,
                    made_layer=False)
        elif layer.adopted_layers is not None and object.owns_pointee:
            # A raw pointer the request owns, not a copy of one the scope
            # keeps, owns its element: it is handed over as a pointer, or
            # taken over by a handle, of an array when it points to one.
            held = layer.adopted_layers + below
            for handle in (name for name in context.universe
                           if LAYERS[name].adopts and
                           (not held or not LAYERS[held[0]].bounded or
                            LAYERS[name].adopts_bounded)):
                yield "adopt", replace(
                    object, layers=(handle,) + layer.adopted_layers + below,
                    lifetimes=(Lifetime.REQUEST,) *
                    (1 + len(layer.adopted_layers)) + object.lifetimes[1:],
                    quals=(0,) * (1 + len(layer.adopted_layers)) +
                    object.quals[1:], origin=Origin.MADE, owns_pointee=False,
                    made_layer=True)
        elif layer.gives_element and not object.full and not object.qualified:
            # A layer the derivation wrapped a value in is not taken apart:
            # its element is what it was made of, as it was; nor is one it
            # converted to a more qualified element: its element is that of
            # what it was converted of.
            # The element of a pointer or a handle lives as long as it is
            # held; it is owned by the request only when it owns the layer
            # and the layer owns its element alone. The element of a copy of
            # a layer that shares it is the element itself, not a copy.
            made = object.lifetimes[1] is Lifetime.REQUEST
            element = replace(
                object, layers=below, lifetimes=object.lifetimes[1:],
                origin=(object.origin if layer.owner is Owner.SOLE
                        else Origin.BORROWED),
                contained=object.contained or
                (layer.name if made else None),
                unconstructed=(context.scope.publishes_early and
                               object.instance and bool(below)),
                absent=object.absent or (layer.may_be_empty and
                                        not object.full),
                full=max(object.full - 1, 0),
                quals=(held_quals(object, layer),) + object.quals[2:],
                owns_pointee=False, made_layer=False)
            yield "element", element
            if (layer.may_be_empty and not object.full and
                    layer.adopted_layers is None):
                yield from empty_safe_steps(object, element, context)
        # A handle the request owns converts to another, sharing or taking
        # over its element; not one the derivation made, which it makes as
        # the other at once.
        if (object.origin.owned and not object.quals[0] and
                not object.made_layer and not object.qualified):
            for name in layer.converts:
                yield "convert", replace(
                    object, layers=(name,) + below,
                    lifetimes=(Lifetime.REQUEST,) + object.lifetimes[1:],
                    quals=(0,) + object.quals[1:])
    # A leaf is one of its interface too, as the base it is. A pointer or a
    # handle of it converts to one of the interface: a new pointer or handle,
    # which the binding retains for what it keeps, or which takes over one the
    # request owns when it cannot be copied.
    if object.element == "leaf" and context.abstract:
        outer = LAYERS[layers[0]] if layers else None
        if outer is not None and not outer.gives_element:
            # The pointer to the first element of an array is a pointer to
            # that element as the interface, not to an array of the
            # interface: the request does not own what it points to.
            if len(layers) == 1 and outer.shares_element:
                yield "upcast", replace(object, element="interface",
                                        layers=(POINTER,),
                                        origin=Origin.BORROWED,
                                        owns_pointee=False)
        elif outer is None:
            # The leaf as the base it is: the same object.
            yield "upcast", replace(object, element="interface")
        elif (len(layers) == 1 and outer.shares_element and
              outer.copyable and not object.quals[0] & VOLATILE):
            # A new pointer or handle of the interface.
            yield "upcast", replace(object, element="interface",
                                    quals=(0,) + object.quals[1:])
        elif (len(layers) == 1 and outer.shares_element and
              object.origin.owned and not object.quals[0]):
            yield "upcast", replace(
                object, element="interface",
                lifetimes=(Lifetime.REQUEST,) + object.lifetimes[1:],
                quals=(0,) + object.quals[1:])
        elif len(layers) > 1:
            # A copy of the layers above the handle that holds the leaf, the
            # first of them to share it, holds a handle of the interface: a
            # new object, which shares the leaf. A handle of a handle of the
            # leaf is not one of a handle of the interface.
            depth = copied_layers(layers)
            if depth == len(layers) and all(
                    LAYERS[name].copyable for name in layers) and all(
                        not quals & VOLATILE for quals in object.quals[:depth]):
                yield "upcast", replace(
                    object, element="interface", origin=Origin.COPIED,
                    instance=False,
                    lifetimes=(Lifetime.REQUEST,) * depth +
                    object.lifetimes[depth:],
                    quals=(0,) + object.quals[1:])
    # A copy: a new object the request owns. What a layer that shares its
    # element holds is not copied: the copy still holds the instance. A
    # conversion to a more qualified element is not copied: what it converts
    # is.
    if can_copy(object, context) and not object.qualified:
        depth = copied_layers(layers)
        yield "copy", replace(
            object, lifetimes=(Lifetime.REQUEST,) * depth +
            object.lifetimes[depth:], origin=Origin.COPIED,
            instance=object.instance and depth <= len(layers),
            quals=(0,) + object.quals[1:])
    yield from qualify_steps(object, context)
    yield from unknown_bound_steps(object, context)
    # A new layer holding what the request owns, moved into it: a value it
    # made, or a copy that shares what it was copied of, such as a copy of a
    # shared handle. What a scope keeps, or a copy of a value that shares
    # nothing, is not wrapped: a new layer does not hold a copy of an
    # instance, nor share one that no handle owns. A layer holding a copy
    # that shares what the scope or the caller keeps is kept with it, as the
    # copy in it is: the binding retains it.
    # A raw pointer that owns what it points to is not: it is handed over,
    # or taken over by a handle.
    shares = object.origin is Origin.COPIED and object.instance
    if ((object.origin is Origin.MADE or shares) and
            can_move(object, context) and len(layers) < DEPTH and
            not object.owns_pointee):
        depth = copied_layers(layers)
        if shares and object.lifetimes[depth] is Lifetime.STABLE:
            lifetimes = ((Lifetime.STABLE,) * (1 + depth) +
                         object.lifetimes[depth:])
        else:
            lifetimes = (Lifetime.REQUEST,) + object.lifetimes
        for made in (LAYERS[name] for name in context.universe):
            if made.wraps_value and not (made.holds_element_only and
                                             layers) and not (
                    slices(object) and not made.shares_element):
                yield "wrap", replace(
                    object, layers=(made.name,) + layers, lifetimes=lifetimes,
                    full=object.full + 1, quals=(0,) + object.quals,
                    owns_pointee=False, made_layer=True)


def unknown_bound_steps(object, context):
    """A pointer or a handle of an array of a known bound converted to one of
    an array of unknown bound, which holds the same array: a new object,
    moved out of one the request owns or copied of another. One converted to
    a more qualified element is not converted again, and a pointer the
    request owns is handed over or taken over instead."""
    layers = object.layers
    if (len(layers) < 2 or not LAYERS[layers[1]].bounded or
            object.qualified or object.owns_pointee):
        return
    moved = object.origin.owned and can_move(object, context)
    if not moved and not can_copy(object, context):
        return
    for name in LAYERS[layers[0]].converts_unknown_bound:
        yield "unknown_bound", replace(
            object, layers=(name, "array") + layers[2:],
            lifetimes=(Lifetime.REQUEST,) + object.lifetimes[1:],
            origin=object.origin if moved else Origin.COPIED,
            quals=(0,) + object.quals[1:], made_layer=False)


def qualify_steps(object, context):
    """A pointer or a handle converted to one of a more qualified element, as
    a pointer to the element converts: a new object, which holds the same
    element, moved out of one the request owns or copied of another. A
    pointer the request owns is handed over or taken over instead, never
    converted. Its element may gain any qualifiers, and so may each level
    below it that a raw pointer holds where every level above is a raw
    pointer made const, as matches says: each such level is marked GAIN, of
    the element alone or of every level that may gain."""
    layers = object.layers
    if (not layers or not LAYERS[layers[0]].qualifies or object.qualified or
            object.owns_pointee):
        return
    moved = object.origin.owned and can_move(object, context)
    if not moved and not can_copy(object, context):
        return
    # The levels below the element that may gain: those raw pointers hold.
    deepest = 1
    while (deepest < len(layers) and
           LAYERS[layers[deepest]].adopted_layers == ()):
        deepest += 1
    # A layer made at once is the same object, of the qualifiers it needs.
    life = object.lifetime if made_whole(object) else Lifetime.REQUEST
    for depth in sorted({1, deepest}):
        quals = list(object.quals)
        for level in range(1, depth + 1):
            quals[level] |= GAIN
        quals[0] = 0
        yield "qualify", replace(
            object, lifetimes=(life,) + object.lifetimes[1:],
            origin=object.origin if moved else Origin.COPIED,
            quals=tuple(quals), qualified=True)


def empty_safe_steps(object, element, context):
    """What is derived of the element of a layer that may be empty without
    failing when it is: a pointer to the element, null when the layer is
    empty, and the optional the layer holds, empty when the layer is."""
    below = element.layers
    inner = LAYERS[below[0]] if below else None
    if inner is not None and inner.decay_layer:
        pointer = (inner.decay_layer,) + below[1:]
        quals = (0, held_quals(element, inner)) + element.quals[2:]
    else:
        pointer = (POINTER,) + below
        quals = (0,) + element.quals
    yield "element_pointer", replace(
        element, layers=pointer,
        lifetimes=(object.lifetimes[0],) + element.lifetimes[
            len(below) - len(pointer) + 1:],
        origin=Origin.BORROWED, absent=object.absent, quals=quals,
        owns_pointee=False)
    if inner is not None and inner.has_empty_value and not element.unconstructed:
        if element.origin.owned:
            yield "maybe_element", replace(element, absent=object.absent)
        else:
            for kind, copied in transitions(element, context):
                if kind == "copy":
                    yield "maybe_element_copy", replace(
                        copied, absent=object.absent)


# The known gaps of the library, as filters over what the rules allow.


@dataclass(frozen=True)
class Gap:
    """Where a limitation keeps the library from what the semantics allow:
    at a kind of step from one object to another, or at a reference or a
    pointer to an object."""
    limitation: "Limitation"
    site: str
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
    MADE_HOLDER_HANDOVER = "made_holder_handover"
    # A pointer into a copy a binding converts of what it stores, such as the
    # std::shared_ptr<I> * into the std::optional<std::shared_ptr<I>> a
    # binding of I converts of a kept std::optional<std::shared_ptr<T>>. The
    # library points to the handle it converts of the element instead, which
    # fails when the stored optional is empty.
    CONVERTED_COPY_POINTER = "converted_copy_pointer"
    # A pointer or a handle converted to one more qualified below its
    # element, such as the std::shared_ptr<const L * const> of a
    # std::shared_ptr<L *>. The library converts the qualifiers of the
    # element alone: a level below it would take a conversion for every
    # qualifier of every level.
    QUALIFIED_BELOW_ELEMENT = "qualified_below_element"


GAPS = (
    Gap(Limitation.MADE_HOLDER_HANDOVER, "point",
        lambda context, category, object: object.contained is not None),
    Gap(Limitation.CONVERTED_COPY_POINTER, "element_pointer",
        lambda context, object, following: object.origin is Origin.COPIED),
    Gap(Limitation.QUALIFIED_BELOW_ELEMENT, "qualify",
        lambda context, object, following:
        following.quals[2:] != object.quals[2:]),
)


def blocked(context, site, *event):
    """Whether a limitation the context applies blocks an event."""
    return any(gap.site == site and gap.limitation in context.limits and
               gap.applies(context, *event) for gap in GAPS)


def steps(object, context):
    """The steps from an object the library takes: those the semantics allow
    that no limitation blocks."""
    found = []
    for kind, following in transitions(object, context):
        if not blocked(context, kind, object, following):
            found.append((kind, following))
    return found


# What a derivation does to the leaf. A copy of a stack, or a move of it into a
# new layer, makes its leaf anew when no layer of it shares its element. A
# copy is made where the request holds it, and a value made for the request
# is moved there once, however many layers it is moved into: the fewest a
# resolution needs is a copy when it copies the leaf, else a move when it
# moves it.


def makes_leaf_anew(layers):
    return copied_layers(layers) > len(layers)


def step_effect(kind, object):
    """What a step from an object does to its leaf: "copy", "move" or None."""
    if kind == "maybe_element_copy":
        return "copy" if makes_leaf_anew(object.layers[1:]) else None
    if kind in ("copy", "wrap") and makes_leaf_anew(object.layers):
        return "copy" if kind == "copy" else "move"
    return None


# The cost of a derivation: the copies and the moves of the leaf, the objects
# it keeps as long as the container, and the objects it makes for the
# request. A plan is the cheapest of these in this order, with what may not
# exist when a layer is empty after the leaf.
NO_COST = (0, 0, 0, 0)


def affected(cost, effect):
    """The cost after an effect on the leaf: a copy is made where it is moved
    to."""
    copies, moves = cost[:2]
    if effect == "copy":
        return (1, moves) + cost[2:]
    if effect == "move" and not copies:
        return (copies, 1) + cost[2:]
    return cost


def handed_over(cost, effect, container):
    """The copies and the moves of the leaf after a request is handed what
    satisfies it. A runtime container hands a value over from its context:
    one that holds a leaf moved for the request is moved once more."""
    if (container is Container.RUNTIME and effect == "move" and
            not cost[0]):
        return (cost[0], cost[1] + 1) + cost[2:]
    return affected(cost, effect)


# The steps that make a new object: an upcast makes one of all but the leaf
# itself, which is the base it is.
MAKES_OBJECT = frozenset({"decay", "hand_over", "adopt", "observe",
                          "element_pointer", "maybe_element_copy",
                          "convert", "copy", "wrap", "release", "qualify",
                          "unknown_bound"})


def made_whole(object):
    """Whether the outer layer of an object is made as the layer a request
    needs, of the qualifiers of its element too: one the request made of a
    value or of a pointer. A handle of the interface is one the binding made
    of the leaf and upcast, which a request converts."""
    return object.made_layer and not (object.element == "interface" and
                                     len(object.layers) == 1)


def stepped(cost, kind, object, following):
    """The cost after a step from an object to the object it reaches. A layer
    made of what it holds is made of the qualifiers its element needs at
    once."""
    cost = affected(cost, step_effect(kind, object))
    if kind not in MAKES_OBJECT and not (kind == "upcast" and object.layers):
        return cost
    if kind == "qualify" and made_whole(object):
        return cost
    if following.lifetime is Lifetime.STABLE:
        return cost[:2] + (cost[2] + 1, cost[3])
    return cost[:3] + (cost[3] + 1,)


def dominated(costs, cost):
    """Whether a cost of an object is no better than one it has: of the same
    copies and moves of the leaf, and of no fewer objects. A derivation from
    it costs no less, so a search that drops it ends."""
    return any(other[:2] == cost[:2] and other[2] <= cost[2] and
               other[3] <= cost[3] for other in costs)


def search(starts, context, take):
    """The objects reached from starts, each an object with a cost, by the steps
    take gives, each with the costs of the derivations that reach it that no
    other one of them dominates."""
    found = {}
    frontier = collections.deque()
    for object, cost in starts:
        if not dominated(found.get(object, ()), cost):
            found.setdefault(object, set()).add(cost)
            frontier.append((object, cost))
    while frontier:
        object, cost = frontier.popleft()
        if cost not in found[object]:
            continue
        for kind, following in take(object, context):
            reached = stepped(cost, kind, object, following)
            costs = found.setdefault(following, set())
            if dominated(costs, reached):
                continue
            for other in [other for other in costs
                          if dominated((reached,), other)]:
                costs.discard(other)
            costs.add(reached)
            frontier.append((following, reached))
    return found


@functools.lru_cache(maxsize=16)
def reachable(start, context):
    """The objects derived from a start, each with the costs of the
    derivations that reach it."""
    found = search({(start, NO_COST)}, context, steps)
    return {object: frozenset(costs) for object, costs in found.items()}


# Who takes a step. A binding knows the type it stores, a request only the
# interface it asks for, so a request cannot name what the binding stores. The
# binding takes the steps that need the stored type, or that only go into
# what it stores: it gives the element of a layer, rebinds the leaf to its
# interface, and releases what a pointer it made points to. It takes every
# step from what holds the leaf, when the request asks for the interface: a
# request of the interface cannot name the leaf. What these reach are its
# layer sources, which it publishes. A request takes every other step from
# them: each adds a layer or converts one, so a request finds the layer
# sources it needs from its own shape. A resolution is what a request
# reaches from the layer sources. For the standard layers that is what it
# reaches from the stored object, at the same cost; a layer a type defines
# may reach more by steps a binding and a request take in turn, which no
# container takes.

BINDING_STEPS = frozenset({"element", "element_pointer", "maybe_element",
                           "maybe_element_copy", "upcast"})


def names_leaf(object, context):
    """Whether an object holds the leaf a request of the interface cannot
    name."""
    return context.abstract and object.element == "leaf"


def binding_step(kind, object, following, context, stored):
    """Whether a binding takes a step: one that needs the type it stores, or
    one from what holds the leaf a request cannot name that the upcast to
    the interface needs. That wraps the leaf itself into a layer that
    rebinds, or the stored layers as they are stored, which keeps them as
    defined as they are when empty: what wraps anything else is upcast to
    nothing a request takes."""
    if kind in BINDING_STEPS:
        return True
    # A request converts a handle of the interface to one of a more
    # qualified element, once the binding upcasts it.
    if kind == "qualify" or not names_leaf(object, context):
        return False
    return (kind != "wrap" or
            (not object.layers and LAYERS[following.layers[0]].rebinds) or
            (bool(stored) and object.layers == stored))


def binding_steps(object, context, stored=()):
    """The steps a binding of the stored layers takes from an object, and the
    release of what a pointer it made points to, which a request reaches by
    adopting the pointer and taking its element."""
    found = [(kind, following) for kind, following in steps(object, context)
             if binding_step(kind, object, following, context, stored)]
    for kind, adopted in steps(object, context):
        if kind == "adopt" and LAYERS[adopted.layers[0]].owner is Owner.SOLE:
            found += [("release", released)
                      for step, released in steps(adopted, context)
                      if step == "element"]
    return found


def request_steps(object, context):
    """The steps a request takes from a layer source: every step a binding
    does not take, each of which adds a layer or converts one. A request does
    not wrap the leaf as the bare interface, which only the binding can name:
    the binding wraps the leaf itself."""
    if names_leaf(object, context):
        return []
    return [(kind, following) for kind, following in steps(object, context)
            if kind not in BINDING_STEPS and
            not (kind == "wrap" and slices(object))]


@functools.lru_cache(maxsize=16)
def layer_sources(start, context):
    """What a binding publishes of its stored object, each object with a cost
    that reaches it."""
    found = search({(start, NO_COST)}, context,
                   lambda object, each: binding_steps(object, each,
                                                     start.layers))
    return frozenset((object, cost) for object, costs in found.items()
                     for cost in costs)


@functools.lru_cache(maxsize=16)
def requested(start, context):
    """What requests reach from the layer sources of a stored object, as
    reachable gives what is reached from the stored object itself."""
    found = search(layer_sources(start, context), context, request_steps)
    return {object: frozenset(costs) for object, costs in found.items()}


def shape_candidates(reached):
    """The objects reached that may satisfy a request of an element and its
    layers, by (element, layers): those of the layers, then the raw pointers
    to them, which a pointer request may take. satisfactions of them is that
    of every object reached, of far fewer."""
    found = {}
    for object in reached:
        found.setdefault((object.element, object.layers), []).append(object)
    for object in reached:
        if (object.layers and
                LAYERS[object.layers[0]].adopted_layers is not None):
            found.setdefault((object.element, object.layers[1:]),
                             []).append(object)
    return found


@functools.lru_cache(maxsize=16)
def requested_candidates(start, context):
    """What requests reach from the layer sources of a stored object, and
    its shape_candidates."""
    reached = requested(start, context)
    return reached, shape_candidates(reached)


# What a request asks of the objects.


def required_lifetime(position, entry):
    """How long an object a request refers to has to live, so that no
    reference dangles. A reference that is passed away, returned by a resolve
    or a construct or held by a class one of them constructs and returns, has
    to outlive the resolution: it may refer only to what the container or the
    caller keeps. One a callable takes, or a class constructed for it, is
    used while the call runs; one a class takes that a shared or a cyclical
    scope keeps is kept with it: either may refer to what is made for the
    request too."""
    if position.keeping_scope is not None or entry is Entry.INVOKE:
        return Lifetime.REQUEST
    return Lifetime.STABLE


REFERENCES = (Category.LVALUE_REFERENCE, Category.CONST_LVALUE_REFERENCE,
              Category.POINTER, Category.CONST_POINTER)
POINTERS = (Category.POINTER, Category.CONST_POINTER)


CONST_CATEGORIES = (Category.CONST_LVALUE_REFERENCE, Category.CONST_POINTER)


def converts_quals(got, wanted, layers, outer):
    """Whether a pointer to an object of the qualifiers got, of the layers
    layers, converts to a pointer of the qualifiers wanted, as C++ converts
    one: the object may be of more, of the mask outer, and a level below it
    may be of more when every level above it, from the object down, is a raw
    pointer the request makes const."""
    if got[0] & ~outer:
        return False
    through = True
    for level in range(1, len(got)):
        above = outer if level == 1 else wanted[level - 1]
        through = (through and LAYERS[layers[level - 1]].adopted_layers == () and
                   bool(above & CONST))
        if got[level] & GAIN:
            # A level a conversion marked may gain what it is asked.
            if got[level] & 3 & ~wanted[level]:
                return False
        elif got[level] != wanted[level] and (got[level] & ~wanted[level] or
                                              not through):
            return False
    return True


def satisfactions(objects, category, lifetime, layers, context, quals=None):
    """The objects of a request's element that satisfy it, each with what
    satisfying it does to the leaf, as step_effect says. The request is of the
    qualifiers quals, of the object it is of and of each layer below,
    outermost first; a reference or a pointer may add qualifiers to the
    object it refers to, and none other is told apart."""
    if quals is None:
        quals = (0,) * (len(layers) + 1)
    outer = quals[0] | (CONST if category in CONST_CATEGORIES else 0)
    same_shape = [object for object in objects if object.layers == layers and
                matches(object.quals[1:], quals[1:])]
    if category in REFERENCES:
        # A reference or a pointer refers to the instance, or to a layer of
        # it, that lives long enough. A reference adds qualifiers only to
        # what it refers to; a pointer converts as C++ converts one.
        def qualified(got, held_layers):
            if category in POINTERS:
                return converts_quals(got, quals, held_layers, outer)
            return matches(got[1:], quals[1:]) and not got[0] & ~outer
        referred = [(object, None) for object in objects
                    if object.layers == layers and object.instance and
                    qualified(object.quals, object.layers) and
                    object.lifetime.value >= lifetime.value]
        if category not in POINTERS:
            return referred
        # A pointer value: one whose pointee lives long enough, which
        # satisfies it besides what it refers to, which may be absent where a
        # pointer to it is null; or one whose pointee the request owns, which
        # hands that over, only where nothing is referred to.
        def points(object):
            pointer = object.layers[:1]
            if (not pointer or LAYERS[pointer[0]].adopted_layers is None or
                    object.layers[1:] != layers or not object.instance or
                    not converts_quals(object.quals[1:], quals,
                                       object.layers[1:], outer)):
                return False
            if object.owns_pointee:
                return (not referred and
                        not blocked(context, "point", category, object))
            return object.lifetimes[1].value >= lifetime.value
        return referred + [(object, None) for object in objects if points(object)]
    if category is Category.RVALUE_REFERENCE:
        # The object the request moves from: one it made.
        return [(object, None) for object in same_shape
                if object.origin is Origin.MADE and can_move(object, context)]
    # A value: an owned object moved, or any object copied.
    found = []
    for object in same_shape:
        anew = makes_leaf_anew(object.layers)
        if slices(object) or object.unconstructed:
            continue
        if object.origin.owned and can_move(object, context):
            found.append((object, "move" if anew else None))
        if can_copy(object, context):
            found.append((object, "copy" if anew else None))
    return found


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


def request_entry(entry, position, selection, category):
    """The entry a request is resolved through. construct<T>() of a
    reference, of an rvalue reference, or of a request a key or an
    annotation selects, resolves it: it names an instance or a binding, not
    a value to make."""
    if position is Position.DIRECT and entry is Entry.CONSTRUCT and (
            selection is not Selection.PLAIN or not category.returns_value or
            category is Category.RVALUE_REFERENCE):
        return Entry.RESOLVE
    return entry


def least_cost(answers, defined):
    """The fewest copies and moves of answers, each of them with whether what
    it satisfies the request with may be absent; with defined, as the fewest
    with whether one of so few is defined when a layer is empty. None when
    there is none."""
    if not answers:
        return None
    cost = min(cost[:2] for cost, _ in answers)
    if not defined:
        return cost
    return cost, any(not absent for each, absent in answers
                     if each[:2] == cost)


def converted(scope, form, leaf, bound, shape, category, limits=frozenset(),
              interfaces=InterfaceSet.DEFAULT, defined=False):
    """The fewest copies and moves of the leaf a conversion of what a scope
    stores into a request of a shape of the bound interface makes, as
    (copies, moves): those of the derivations that reach what satisfies the
    request, and of satisfying it, for as long as a call runs. None when
    nothing does. A resolution takes the cost of handing the request over on
    top of it."""
    layers, element = SHAPES[shape] + bound.layers, bound.element
    context = Context(LEAVES[leaf], SCOPES[scope], element == "interface",
                      limits)
    if not makes_storage(form, context):
        return None
    reached, candidates = requested_candidates(
        source(scope, form, interfaces), context)
    objects = candidates.get((element, layers), ())
    return least_cost(
        [(affected(cost, effect), object.absent)
         for object, effect in satisfactions(
             objects, category, Lifetime.REQUEST, layers, context,
             shape_quals(shape, bound))
         for cost in reached[object]], defined)


def fewest(scope, form, leaf, bound, shape, category, position, entry,
           limits=frozenset(), selection=Selection.PLAIN,
           interfaces=InterfaceSet.DEFAULT, container=None, defined=False):
    """The fewest copies and moves of the leaf a request of a shape of the
    bound interface resolves with, as (copies, moves), under the
    limitations given, in a container; None when it does not resolve."""
    if bound.element == "other":
        # The other alternative of an alternative type is published as the
        # leaf would be, of a type of its own that can be copied. The
        # alternative type holds the leaf, so a pointer to the other
        # alternative is null, as one to an element of an empty layer is, and
        # anything else of it fails.
        if shape is not Shape.PLAIN or category not in POINTERS:
            return None
        return fewest(scope, form, Leaf.COPYABLE, LEAF, shape, category,
                      position, entry, limits, selection, interfaces,
                      container, defined)
    layers, element = SHAPES[shape] + bound.layers, bound.element
    context = Context(LEAVES[leaf], SCOPES[scope], element == "interface",
                      limits)
    if not makes_storage(form, context):
        return None
    reached, candidates = requested_candidates(
        source(scope, form, interfaces), context)
    objects = candidates.get((element, layers), ())
    direct = position is Position.DIRECT
    entry = request_entry(entry, position, selection, category)
    if selector_rejects(layers, category, position, entry, selection,
                        context):
        return None
    # resolve<T &&>() and construct<T &&>() return the value moved out of
    # what the request refers to.
    returned = (direct and entry in (Entry.RESOLVE, Entry.CONSTRUCT) and
                category is Category.RVALUE_REFERENCE)
    answers = [(handed_over(cost,
                            "move" if returned and
                            makes_leaf_anew(object.layers) else effect,
                            container), object.absent)
               for object, effect in satisfactions(
                   objects, category, required_lifetime(position, entry),
                   layers, context, shape_quals(shape, bound))
               for cost in reached[object]]
    if answers:
        return least_cost(answers, defined)
    # construct<T>() of a value it cannot resolve makes T of the leaf the
    # container resolves by value: a new handle, optional or array of it,
    # the rest of a bounded array value-initialized. The value is moved into
    # it once it is resolved.
    bounded = any(LAYERS[name].bounded for name in SHAPES[shape])
    if (direct and entry is Entry.CONSTRUCT and element == "leaf" and
            shape is not Shape.PLAIN and
            (context.leaf.default_constructible or not bounded)):
        value = fewest(scope, form, leaf, LEAF, Shape.PLAIN, Category.VALUE,
                       Position.DIRECT, Entry.RESOLVE, limits, selection,
                       interfaces, container, defined)
        if value is None:
            return None
        if defined:
            (copies, moves), is_defined = value
            return (copies, moves + 1), is_defined
        return value[0], value[1] + 1
    return None


def resolves(*args, **options):
    """Whether a request of a shape of the bound interface resolves, under
    the limitations given."""
    return fewest(*args, **options) is not None


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
    """The derivations of the objects of a request's shape, each as the path
    of steps from the source, a step as its kind and the object it reaches:
    for reading why the model resolves."""
    context = Context(LEAVES[leaf], SCOPES[scope],
                      bound.element == "interface", limits)
    start = source(scope, form, interfaces)
    parent = {start: None}
    frontier = collections.deque([start])
    while frontier:
        object = frontier.popleft()
        for kind, following in steps(object, context):
            if following not in parent:
                parent[following] = (kind, object)
                frontier.append(following)
    paths = []
    for object in parent:
        if (object.layers == SHAPES[shape] + bound.layers and
                object.element == bound.element and
                object.quals[1:] == shape_quals(shape, bound)[1:]):
            path = [("target", object)]
            while parent[object] is not None:
                kind, previous = parent[object]
                path[-1] = (kind, path[-1][1])
                path.append(("source", previous))
                object = previous
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
    if traits.publishes_early and layers and not (
            LAYERS[layers[0]].constructs_in_place and
            (len(layers) == 1 or (LAYERS[layers[0]].shares_element and all(
                LAYERS[name].wraps_value for name in layers[1:])))):
        # A cyclical scope publishes its instance before it constructs it,
        # in storage it holds: a handle that shares what it holds may hold a
        # wrapper made of a value, constructed there.
        return False
    if interfaces is not InterfaceSet.DEFAULT and any(
            LAYERS[name].decay_layer or LAYERS[name].holds_element_only
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
                        request_entry(entry, position, selection, category),
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
