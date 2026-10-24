"""Content of the conversion rules wrapper semantics section.

A stored or requested type tree is a leaf wrapped in zero or more wrappers. The leaf
itself is not a wrapper: a type tree without wrappers is the leaf value.

Wrapper semantics are classes of handles defined by their traits and the route
forms they have. These are the kinds of the library, declarations as a kind of
the user is: the spec names no concrete wrapper type, and a concrete wrapper,
a standard one or a user's, maps to a wrapper semantics through the wrapper-traits
extension point of the C++ library.
"""

from spec.schema import SemanticsOperation, WrapperSemantics, Property, Operation


def form(rule_id, op, description, target=None):
    return SemanticsOperation(id=rule_id, form=op, description=description, target=target)

wrapper_semantics: tuple[WrapperSemantics, ...] = (
    WrapperSemantics(
        id="alt",
        description="A value holding exactly one of several alternative type trees.",
        parameters=1,
        variadic=True,
        traits=(
            Property.OWNING,
            Property.IN_PLACE,
            Property.CONSTRUCTIBLE,
            Property.MAPPED,
        ),
        forms=(
            form("select_alternative", Operation.BORROW, "Borrows the held alternative."),
            form(
                "take_alternative",
                Operation.TAKE,
                "Moves the held alternative out of a consumed alternative.",
            ),
        ),
    ),
    WrapperSemantics(
        id="array",
        description="A fixed number of elements of one type tree, stored inline.",
        parameters=1,
        has_extent=True,
        traits=(
            Property.OWNING,
            Property.IN_PLACE,
            Property.CONSTRUCTIBLE,
            Property.IMMOVABLE,
        ),
        forms=(
            form(
                "decay_array",
                Operation.DECAY,
                "Takes the address of the first element of a borrowed array.",
                target="ptr",
            ),
            form(
                "release_array",
                Operation.RELEASE,
                "Hands out the first element of a consumed array, whose ownership "
                "passes to the caller.",
                target="ptr",
            ),
        ),
    ),
    WrapperSemantics(
        id="array_unbounded",
        description=(
            "Elements of one type_tree whose number is known only at runtime; only "
            "an owning or sharing handle can hold it."
        ),
        parameters=1,
        traits=(Property.UNSIZED, Property.IN_PLACE, Property.IMMOVABLE),
        forms=(
            form(
                "decay_unbounded_array",
                Operation.DECAY,
                "Takes the address of the first element of a borrowed array of "
                "runtime size.",
                target="ptr",
            ),
        ),
    ),
    WrapperSemantics(
        id="opt",
        description="A value that may hold one element of a type tree or nothing.",
        parameters=1,
        traits=(
            Property.OWNING,
            Property.NULLABLE,
            Property.BORROWABLE,
            Property.IN_PLACE,
            Property.CONSTRUCTIBLE,
            Property.MAPPED,
        ),
        forms=(
            form("borrow_optional", Operation.BORROW, "Borrows the element of an optional."),
            form(
                "take_optional",
                Operation.TAKE,
                "Moves the element out of a consumed optional.",
            ),
            form(
                "wrap_optional",
                Operation.WRAP,
                "Moves a consumed object into a new optional.",
            ),
        ),
    ),
    WrapperSemantics(
        id="own",
        description="A handle that is the only owner of one element of a type tree.",
        parameters=1,
        traits=(
            Property.OWNING,
            Property.NULLABLE,
            Property.BORROWABLE,
            Property.CONSTRUCTIBLE,
            Property.MAPPED,
        ),
        forms=(
            form(
                "borrow_owned",
                Operation.BORROW,
                "Borrows the element a sole-owner handle owns.",
            ),
            form(
                "share_owned",
                Operation.SHARE,
                "Transfers the element of a consumed sole-owner handle to a sharing "
                "handle.",
                target="share",
            ),
            form(
                "adopt_pointer_owned",
                Operation.ADOPT,
                "Adopts the element of a consumed pointer into a sole-owner handle.",
                target="ptr",
            ),
            form(
                "upcast_owned",
                Operation.UPCAST_CONSUMED,
                "Converts a consumed sole-owner handle of a leaf to one of an "
                "interface.",
            ),
        ),
    ),
    WrapperSemantics(
        id="ptr",
        description="A non-owning address of one element of a type tree.",
        parameters=1,
        traits=(
            Property.NULLABLE,
            Property.BORROWABLE,
            Property.CONSTRUCTIBLE,
            Property.COPYABLE,
        ),
        forms=(
            form("deref_pointer", Operation.BORROW, "Borrows the element a pointer points to."),
            form(
                "address_of",
                Operation.ADDRESS,
                "Takes the address of a borrowed object.",
            ),
            form(
                "release_whole_array",
                Operation.RELEASE_WHOLE,
                "Hands out the address of a consumed array, whose ownership passes "
                "to the caller.",
            ),
            form(
                "upcast_pointer",
                Operation.UPCAST_COPIED,
                "Converts a pointer to a leaf to a pointer to one of its interfaces.",
            ),
            form(
                "upcast_consumed_pointer",
                Operation.UPCAST_CONSUMED,
                "Converts a consumed pointer to a leaf to one to one of its "
                "interfaces.",
            ),
        ),
    ),
    WrapperSemantics(
        id="ref",
        description="A non-owning, non-null alias of one element of a type tree.",
        parameters=1,
        traits=(Property.BORROWABLE,),
        forms=(
            form(
                "deref_reference",
                Operation.BORROW,
                "Borrows the element a reference aliases.",
            ),
        ),
    ),
    WrapperSemantics(
        id="share",
        description="A handle that shares the ownership of one element of a type tree.",
        parameters=1,
        traits=(
            Property.OWNING,
            Property.SHARING,
            Property.NULLABLE,
            Property.BORROWABLE,
            Property.CONSTRUCTIBLE,
            Property.COPYABLE,
            Property.MAPPED,
        ),
        forms=(
            form(
                "borrow_shared",
                Operation.BORROW,
                "Borrows the element a sharing handle shares.",
            ),
            form(
                "adopt_pointer_shared",
                Operation.ADOPT,
                "Adopts the element of a consumed pointer into a sharing handle.",
                target="ptr",
            ),
            form(
                "upcast_shared",
                Operation.UPCAST_CONSUMED,
                "Converts a consumed sharing handle of a leaf to one of an "
                "interface.",
            ),
            form(
                "upcast_copied_shared",
                Operation.UPCAST_COPIED,
                "Converts a copied sharing handle of a leaf to one of an interface.",
            ),
            form(
                "retain_shared_upcast",
                Operation.RETAIN,
                "Keeps a sharing handle of an interface converted from a borrowed "
                "sharing handle of the leaf, so it can be referred to.",
            ),
        ),
    ),
)
