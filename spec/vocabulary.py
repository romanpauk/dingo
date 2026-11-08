"""The vocabulary of the specification: the concepts of the library it speaks
of, an enum each. The value of a member is its name where the specification
is written out, such as in a cell of a probe."""

from enum import Enum


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
    # Two layers: a handle of an optional, and an optional of a handle.
    SHARED_OPTIONAL = "shared_optional"
    UNIQUE_OPTIONAL = "unique_optional"
    OPTIONAL_SHARED_HANDLE = "optional_shared_handle"
    OPTIONAL_UNIQUE_HANDLE = "optional_unique_handle"

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


class Shape(Dimension):
    """What a request asks of a bound interface I."""
    OBJECT = "I"
    SHARED_PTR = "std::shared_ptr<I>"
    UNIQUE_PTR = "std::unique_ptr<I>"
    OPTIONAL = "std::optional<I>"
    SHARED_ARRAY = "std::shared_ptr<I[]>"
    UNIQUE_ARRAY = "std::unique_ptr<I[]>"
    SHARED_BOUNDED_ARRAY = "std::shared_ptr<I[2]>"
    # The shapes of the two-layer stored forms, which only a registration of
    # the form requests.
    SHARED_OPTIONAL = "std::shared_ptr<std::optional<I>>"
    UNIQUE_OPTIONAL = "std::unique_ptr<std::optional<I>>"
    OPTIONAL_SHARED_PTR = "std::optional<std::shared_ptr<I>>"
    OPTIONAL_UNIQUE_PTR = "std::optional<std::unique_ptr<I>>"

    @property
    def is_nested(self):
        return any(self in shapes for shapes in NESTED_SHAPES.values())

    @property
    def holds_optional(self):
        """Whether an optional holds the interface itself."""
        return self in (Shape.OPTIONAL, Shape.SHARED_OPTIONAL,
                        Shape.UNIQUE_OPTIONAL)

    @property
    def is_array(self):
        return self in (Shape.SHARED_ARRAY, Shape.UNIQUE_ARRAY,
                        Shape.SHARED_BOUNDED_ARRAY)

    @property
    def converts(self):
        """Whether a value of the shape converts from anything its element
        can be made of, by a converting constructor."""
        return self is Shape.OPTIONAL


# The shapes a two-layer stored form is requested as: its own, and that of
# the shared handle a unique handle of it converts to.
NESTED_SHAPES = {
    StoredForm.SHARED_OPTIONAL: (Shape.SHARED_OPTIONAL,),
    StoredForm.UNIQUE_OPTIONAL: (Shape.UNIQUE_OPTIONAL, Shape.SHARED_OPTIONAL),
    StoredForm.OPTIONAL_SHARED_HANDLE: (Shape.OPTIONAL_SHARED_PTR,),
    StoredForm.OPTIONAL_UNIQUE_HANDLE: (Shape.OPTIONAL_UNIQUE_PTR,),
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
