"""The dimensions of the probes, an enum per axis. The value of a member is
its name in a cell; how it is spelled in C++ is the templates' concern."""

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

    @property
    def is_array(self):
        return self in (Shape.SHARED_ARRAY, Shape.UNIQUE_ARRAY,
                        Shape.SHARED_BOUNDED_ARRAY)

    @property
    def converts(self):
        """Whether a value of the shape converts from anything its element
        can be made of, by a converting constructor."""
        return self is Shape.OPTIONAL


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


class Keeps(Dimension):
    """How the scope of a cell keeps what it resolves."""
    INSTANCE = "instance"
    ADDRESSES = "addresses"
    NOTHING = "nothing"


class Kind(Dimension):
    """A class generated around a parameter type, whose constructor the
    library detects."""
    SINGLE = "single"
    EXPLICIT_SINGLE = "explicit_single"
    AGGREGATE = "aggregate"
    PAIR = "pair"

    @property
    def arity(self):
        return 2 if self is Kind.PAIR else 1


class Detection(Dimension):
    """How the library detects a constructor."""
    SHAPE = "shape"
    SIGNATURE = "signature"


class Construction(Dimension):
    """The constructor a class is constructed with: the one a factory names
    explicitly, or the one detection finds."""
    EXPLICIT = "explicit"
    SHAPE = "shape"
    SIGNATURE = "signature"


class Making(Dimension):
    """How a class is made: by an entry point of a container, or by
    resolving its registration."""
    RESOLVE = "resolve"
    CONSTRUCT = "construct"
    INVOKE = "invoke"
    REGISTERED = "registered"


class Outcome(Dimension):
    """What the library answered for a cell."""
    INVALID = "invalid"
    NOT_REGISTRABLE = "not_registrable"
    REJECTED = "rejected"
    DOES_NOT_COMPILE = "does_not_compile"
    CRASHED = "crashed"
    VIOLATED = "violated"
    RESOLVED = "resolved"
    TYPE_NOT_FOUND = "type_not_found"
    TYPE_NOT_CONVERTIBLE = "type_not_convertible"
    TYPE_AMBIGUOUS = "type_ambiguous"
    EXCEPTION = "exception"
    # A request a binding publishes and selects that no probe makes, and one
    # it publishes that selects another binding.
    UNPROBED = "unprobed"
    UNREACHABLE = "unreachable"
    DETECTED = "detected"
    MISDETECTED = "misdetected"
    # Misdetected as a known limitation says, and a known limitation that
    # no longer occurs.
    LIMITED = "limited"
    STALE_LIMITATION = "stale_limitation"

    @property
    def is_defect(self):
        return self in (Outcome.DOES_NOT_COMPILE, Outcome.CRASHED,
                        Outcome.VIOLATED, Outcome.UNPROBED,
                        Outcome.UNREACHABLE, Outcome.MISDETECTED,
                        Outcome.STALE_LIMITATION)
