"""The model both families of probes share: the interfaces the library names,
the requests of the grammar over them, and a probe of a request."""

from dataclasses import dataclass

from .dimensions import Category, Shape


@dataclass(frozen=True, order=True)
class Interface:
    """A type requests are made of: its name in a cell, the type as the
    compiler spells it, and whether it is abstract or an array."""
    name: str
    spelled: str
    abstract: bool = False
    array: bool = False


def interface(spelled, abstract=False, array=False):
    """An interface named by the compiler's spelling of it."""
    name = spelled.replace("dingo::model::", "").replace(" [", "[")
    return Interface(name, spelled, abstract, array)


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
            if self.shape is Shape.OPTIONAL:
                return "an optional cannot hold an abstract type"
            if self.shape.is_array:
                return "an array cannot hold an abstract type"
        if self.interface.array:
            if self.shape is Shape.OBJECT and by_value:
                return "an array cannot be returned by value"
            if self.shape is Shape.OPTIONAL:
                return "an optional cannot hold an array"
            if self.shape is Shape.UNIQUE_PTR:
                return "a unique_ptr deletes an array as an object"
        return None


def grammar(interfaces):
    """Every shape of every interface, in every category."""
    return [Request(index, bound, shape, category)
            for index, bound in enumerate(interfaces)
            for shape in Shape for category in Category]


@dataclass(frozen=True, order=True)
class Probe:
    """A request made with an operation of a family."""
    operation: object
    request: Request

    @property
    def request_key(self):
        """The request a failed instantiation may be shared by."""
        return (self.request.interface.name, self.request.name)
