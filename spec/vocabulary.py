"""The vocabulary of the specification: the concepts of the library it speaks
of, an enum each. The value of a member is its name where the specification
is written out, such as in a cell of a probe."""

from enum import Enum
import itertools


class Dimension(str, Enum):
    def __str__(self):
        return self.value


class Container(Dimension):
    RUNTIME = "runtime"
    STATIC = "static"


class Scope(Dimension):
    UNIQUE = "unique"
    SHARED = "shared"
    EXTERNAL = "external"
    SHARED_CYCLICAL = "shared_cyclical"


# The wrappers a stored form and a request shape compose, by the name of a
# layer of each in a form: the layer of the semantics it is, its spelling
# around what it holds, and the alias of test/model/probe.h that spells it.
WRAPPERS = {
    "shared": ("shared_ptr", "std::shared_ptr<{}>", "sp"),
    "unique": ("unique_ptr", "std::unique_ptr<{}>", "up"),
    "optional": ("optional", "std::optional<{}>", "opt"),
}


def _spelled(names, inner):
    for name in reversed(names):
        inner = WRAPPERS[name][1].format(inner)
    return inner


def _composed_forms():
    """The stored forms of two wrappers, such as shared_optional for a
    std::shared_ptr<std::optional<L>>."""
    return {f"{outer}_{inner}".upper(): f"{outer}_{inner}"
            for outer, inner in itertools.product(WRAPPERS, repeat=2)}


class StoredForm(Dimension):
    """The type a registration stores, around its leaf."""
    PLAIN = "plain"
    REFERENCE = "reference"
    POINTER = "pointer"
    BOUNDED_ARRAY = "bounded_array"
    UNIQUE_HANDLE = "unique_handle"
    UNIQUE_ARRAY_HANDLE = "unique_array_handle"
    SHARED_HANDLE = "shared_handle"
    SHARED_ARRAY_HANDLE = "shared_array_handle"
    SHARED_BOUNDED_ARRAY_HANDLE = "shared_bounded_array_handle"
    OPTIONAL = "optional"
    ALTERNATIVE = "alternative"
    # Two wrappers, outermost first.
    locals().update(_composed_forms())

    @property
    def wrappers(self):
        """The wrappers of a composed form, outermost first; none of
        another."""
        names = tuple(self.value.split("_"))
        return names if len(names) == 2 and all(
            name in WRAPPERS for name in names) else ()

    @property
    def is_dynamic_array(self):
        """A dynamic array has no size the default factory can make."""
        return self in (StoredForm.UNIQUE_ARRAY_HANDLE,
                        StoredForm.SHARED_ARRAY_HANDLE)

    @property
    def is_object(self):
        """An external scope is given an object of this form, not a value."""
        return self in (StoredForm.REFERENCE, StoredForm.BOUNDED_ARRAY)


class Leaf(Dimension):
    """The type a registration stores, by the operations it supports."""
    COPYABLE = "copyable"
    MOVE_ONLY = "move_only"
    # Can be neither copied nor moved.
    IMMOVABLE = "immovable"
    # Has no default constructor: only its factory makes it.
    FACTORY = "factory"
    # Stores a copyable leaf, requested as its abstract interface.
    INTERFACE = "interface"


class InterfaceSet(Dimension):
    """The interfaces a registration declares: none, so that the library
    binds the ones it derives from the stored type; its abstract interface;
    or both its leaf and its abstract interface."""
    DEFAULT = "default"
    INTERFACE = "interface"
    BOTH = "both"


class Selection(Dimension):
    """How a request names its binding."""
    PLAIN = "plain"
    ANNOTATED = "annotated"
    KEYED = "keyed"


# How many wrappers a request shape composes at most.
SHAPE_DEPTH = 3


def _composed_shapes():
    """The shapes of two or more wrappers around the interface."""
    return {"_".join(names).upper(): _spelled(names, "I")
            for depth in range(2, SHAPE_DEPTH + 1)
            for names in itertools.product(WRAPPERS, repeat=depth)}


class Shape(Dimension):
    """What a request asks of a bound interface I."""
    OBJECT = "I"
    SHARED_PTR = "std::shared_ptr<I>"
    UNIQUE_PTR = "std::unique_ptr<I>"
    OPTIONAL = "std::optional<I>"
    SHARED_ARRAY = "std::shared_ptr<I[]>"
    UNIQUE_ARRAY = "std::unique_ptr<I[]>"
    SHARED_BOUNDED_ARRAY = "std::shared_ptr<I[2]>"
    # Two wrappers or more, outermost first.
    locals().update(_composed_shapes())

    @property
    def wrappers(self):
        """The wrappers the shape composes, outermost first; none of an
        array or of the interface itself."""
        return () if self.is_array else _wrappers_of(self)

    @property
    def depth(self):
        return len(self.wrappers)

    @property
    def holds_optional(self):
        """Whether an optional holds the interface itself."""
        return self.wrappers[-1:] == ("optional",)

    @property
    def holds_unique(self):
        """Whether a unique_ptr holds the interface itself."""
        return self.wrappers[-1:] == ("unique",)

    @property
    def is_array(self):
        return self in (Shape.SHARED_ARRAY, Shape.UNIQUE_ARRAY,
                        Shape.SHARED_BOUNDED_ARRAY)

    @property
    def converts(self):
        """Whether a value of the shape converts from anything its element
        can be made of, by a converting constructor."""
        return self is Shape.OPTIONAL


def _wrappers_of(shape):
    names = []
    rest = shape.value
    while rest != "I":
        name = next(name for name, (_, spelled, _) in WRAPPERS.items()
                    if rest.startswith(spelled.split("{}")[0]))
        names.append(name)
        rest = rest[len(WRAPPERS[name][1].split("{}")[0]):-1]
    return tuple(names)


def shape_of(wrappers):
    """The shape that composes the wrappers, outermost first."""
    return Shape(_spelled(wrappers, "I"))


# The shapes a stored form of two wrappers is requested as: its own, and that
# of the shared handle a unique handle of it converts to.
NESTED_SHAPES = {
    form: (shape_of(form.wrappers),) + (
        (shape_of(("shared",) + form.wrappers[1:]),)
        if form.wrappers[0] == "unique" else ())
    for form in StoredForm if form.wrappers
}


class Category(Dimension):
    """How a request takes a shape X."""
    VALUE = "X"
    CONST = "const X"
    LVALUE_REFERENCE = "X &"
    CONST_LVALUE_REFERENCE = "const X &"
    RVALUE_REFERENCE = "X &&"
    POINTER = "X *"
    CONST_POINTER = "const X *"

    @property
    def by_value(self):
        """Whether a request in the category holds X by value; a resolved
        rvalue reference is returned by value."""
        return self in (Category.VALUE, Category.CONST,
                        Category.RVALUE_REFERENCE)


class Position(Dimension):
    """Where a request stands: itself, the dependency of an unregistered
    class, or the dependency of a class a shared or shared_cyclical scope
    keeps."""
    DIRECT = "direct"
    CONSUMER = "consumer"
    SHARED = "shared"
    CYCLICAL = "cyclical"

    @property
    def kept_by(self):
        """The scope that keeps the class whose dependency the request is."""
        return {Position.SHARED: Scope.SHARED,
                Position.CYCLICAL: Scope.SHARED_CYCLICAL}.get(self)


class Entry(Dimension):
    """The entry point of a container a request is made through."""
    RESOLVE = "resolve"
    INVOKE = "invoke"
    CONSTRUCT = "construct"


class Answer(Dimension):
    """What the library answers a request, as the specification says."""
    # The library does not accept the registration.
    NOT_REGISTRABLE = "not_registrable"
    RESOLVED = "resolved"
    # Rejected as it compiles, with a static_assert.
    REJECTED = "rejected"
    # Failed as it resolves, with the exception a resolution that finds no
    # way throws.
    TYPE_NOT_CONVERTIBLE = "type_not_convertible"
