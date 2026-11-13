"""The model both families of probes share: the interfaces the library names,
the requests of the grammar over them, and a probe of a request."""

from dataclasses import dataclass, field

from spec.vocabulary import WRAPPERS, Category, Shape, StoredForm

# The type each stored form stores around a leaf L that is not a composition
# of wrappers.
STORED = {
    StoredForm.PLAIN: "L",
    StoredForm.REFERENCE: "L &",
    StoredForm.POINTER: "L *",
    StoredForm.BOUNDED_ARRAY: "L[2]",
    StoredForm.UNIQUE_HANDLE: "std::unique_ptr<L>",
    StoredForm.UNIQUE_ARRAY_HANDLE: "std::unique_ptr<L[]>",
    StoredForm.SHARED_HANDLE: "std::shared_ptr<L>",
    StoredForm.SHARED_ARRAY_HANDLE: "std::shared_ptr<L[]>",
    StoredForm.SHARED_BOUNDED_ARRAY_HANDLE: "std::shared_ptr<L[2]>",
    StoredForm.OPTIONAL: "std::optional<L>",
    StoredForm.ALTERNATIVE: "std::variant<L, other>",
}

# The alias templates of test/model/probe.h that spell an array shape.
ARRAY_ALIASES = {
    Shape.SHARED_ARRAY: "sp_array",
    Shape.UNIQUE_ARRAY: "up_array",
    Shape.SHARED_BOUNDED_ARRAY: "sp_bounded_array",
}


def spell_stored(form):
    """The type a stored form stores around a leaf L."""
    if form in STORED:
        return STORED[form]
    spelled = "L"
    for name in reversed(form.wrappers):
        spelled = WRAPPERS[name][1].format(spelled)
    return spelled


def spell_shape(shape, inner):
    """A shape around the type inner, in the aliases of probe.h."""
    if shape in ARRAY_ALIASES:
        return f"shape::{ARRAY_ALIASES[shape]}<{inner}>"
    for name in reversed(shape.wrappers):
        inner = f"shape::{WRAPPERS[name][2]}<{inner}>"
    return inner


@dataclass(frozen=True, order=True)
class Interface:
    """A type requests are made of: its name in a cell, the type as the
    compiler spells it, whether it is abstract or an array, and the interface
    of the specification it is, when it is one a registration binds."""
    name: str
    spelled: str
    abstract: bool = False
    array: bool = False
    bound: object = field(default=None, compare=False)


def interface(spelled, abstract=False, array=False):
    """An interface of the probe's types, named without their namespace."""
    return Interface(spelled.replace("dingo::model::", ""), spelled, abstract,
                     array)


@dataclass(frozen=True, order=True)
class Request:
    """A shape of the index-th interface of a unit, in a category."""
    index: int
    interface: Interface
    shape: Shape
    category: Category

    @property
    def name(self):
        return self.category.value.replace("X", self.shape.value)

    @property
    def invalid(self):
        """Why the request is not valid C++, or None."""
        by_value = self.category.by_value
        if self.interface.abstract:
            if self.shape is Shape.OBJECT and by_value:
                return "an abstract type cannot be held by value"
            if self.shape.holds_optional:
                return "an optional cannot hold an abstract type"
            if self.shape.is_array:
                return "an array cannot hold an abstract type"
        if self.interface.array:
            if self.shape is Shape.OBJECT and by_value:
                return "an array cannot be returned by value"
            if self.shape.holds_optional:
                return "an optional cannot hold an array"
            if self.shape.holds_unique:
                return "a unique_ptr deletes an array as an object"
        return None


# The shapes every unit requests: the interface, a wrapper of it, and an
# array of it.
SHAPES = tuple(shape for shape in Shape if shape.depth <= 1)

# The shapes of two wrappers, which some units request too.
COMPOSED_SHAPES = tuple(shape for shape in Shape if shape.depth == 2)


def grammar(interfaces, shapes=SHAPES):
    """Every shape of every interface, in every category."""
    return [Request(index, bound, shape, category)
            for index, bound in enumerate(interfaces)
            for shape in shapes for category in Category]


@dataclass(frozen=True, order=True)
class Probe:
    """A request made with an operation of a family."""
    operation: object
    request: Request

    @property
    def request_key(self):
        """The request a failed instantiation may be shared by."""
        return (self.request.interface.name, self.request.name)
