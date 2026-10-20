"""Content of the shape algebra layer kinds section.

A stored or requested shape is a leaf wrapped in zero or more layers. The leaf
itself is not a layer: a shape without layers is the leaf value.

Layer kinds are classes of handles defined by their traits and the route
forms they have. These are the kinds of the library, declarations as a kind of
the user is: the spec names no concrete wrapper type, and a concrete wrapper,
a standard one or a user's, maps to a layer kind through the wrapper-traits
extension point of the C++ library.
"""

from spec.schema import KindForm, LayerKind, LayerTrait, RouteForm


def form(rule_id, kind_form, description, target=None):
    return KindForm(id=rule_id, form=kind_form, description=description, target=target)

layer_kinds: tuple[LayerKind, ...] = (
    LayerKind(
        id="alt",
        description="A value holding exactly one of several alternative shapes.",
        parameters=1,
        variadic=True,
        traits=(LayerTrait.OWNING, LayerTrait.IN_PLACE),
        forms=(
            form("select_alternative", RouteForm.BORROW, "Borrows the held alternative."),
            form(
                "take_alternative",
                RouteForm.TAKE,
                "Moves the held alternative out of a consumed alternative.",
            ),
        ),
    ),
    LayerKind(
        id="array",
        description="A fixed number of elements of one shape, stored inline.",
        parameters=1,
        has_extent=True,
        traits=(LayerTrait.OWNING, LayerTrait.IN_PLACE),
        forms=(
            form(
                "decay_array",
                RouteForm.DECAY,
                "Takes the address of the first element of a borrowed array.",
                target="ptr",
            ),
            form(
                "release_array",
                RouteForm.RELEASE,
                "Hands out the first element of a consumed array, whose ownership "
                "passes to the caller.",
                target="ptr",
            ),
        ),
    ),
    LayerKind(
        id="array_unbounded",
        description=(
            "Elements of one shape whose number is known only at runtime; only "
            "an owning or sharing handle can hold it."
        ),
        parameters=1,
        traits=(LayerTrait.UNSIZED,),
        forms=(
            form(
                "decay_unbounded_array",
                RouteForm.DECAY,
                "Takes the address of the first element of a borrowed array of "
                "runtime size.",
                target="ptr",
            ),
        ),
    ),
    LayerKind(
        id="opt",
        description="A value that may hold one element of a shape or nothing.",
        parameters=1,
        traits=(LayerTrait.OWNING, LayerTrait.NULLABLE, LayerTrait.BORROWABLE, LayerTrait.IN_PLACE),
        forms=(
            form("borrow_optional", RouteForm.BORROW, "Borrows the element of an optional."),
            form(
                "take_optional",
                RouteForm.TAKE,
                "Moves the element out of a consumed optional.",
            ),
            form(
                "wrap_optional",
                RouteForm.WRAP,
                "Moves a consumed object into a new optional.",
            ),
        ),
    ),
    LayerKind(
        id="own",
        description="A handle that is the only owner of one element of a shape.",
        parameters=1,
        traits=(LayerTrait.OWNING, LayerTrait.NULLABLE, LayerTrait.BORROWABLE),
        forms=(
            form(
                "borrow_owned",
                RouteForm.BORROW,
                "Borrows the element a sole-owner handle owns.",
            ),
            form(
                "share_owned",
                RouteForm.SHARE,
                "Transfers the element of a consumed sole-owner handle to a sharing "
                "handle.",
                target="share",
            ),
            form(
                "adopt_pointer_owned",
                RouteForm.ADOPT,
                "Adopts the element of a consumed pointer into a sole-owner handle.",
                target="ptr",
            ),
            form(
                "upcast_owned",
                RouteForm.UPCAST_CONSUMED,
                "Converts a consumed sole-owner handle of a leaf to one of an "
                "interface.",
            ),
        ),
    ),
    LayerKind(
        id="ptr",
        description="A non-owning address of one element of a shape.",
        parameters=1,
        traits=(LayerTrait.NULLABLE, LayerTrait.BORROWABLE),
        forms=(
            form("deref_pointer", RouteForm.BORROW, "Borrows the element a pointer points to."),
            form(
                "address_of",
                RouteForm.ADDRESS,
                "Takes the address of a borrowed object.",
            ),
            form(
                "release_whole_array",
                RouteForm.RELEASE_WHOLE,
                "Hands out the address of a consumed array, whose ownership passes "
                "to the caller.",
            ),
            form(
                "upcast_pointer",
                RouteForm.UPCAST_COPIED,
                "Converts a pointer to a leaf to a pointer to one of its interfaces.",
            ),
            form(
                "upcast_consumed_pointer",
                RouteForm.UPCAST_CONSUMED,
                "Converts a consumed pointer to a leaf to one to one of its "
                "interfaces.",
            ),
        ),
    ),
    LayerKind(
        id="ref",
        description="A non-owning, non-null alias of one element of a shape.",
        parameters=1,
        traits=(LayerTrait.BORROWABLE,),
        forms=(
            form(
                "deref_reference",
                RouteForm.BORROW,
                "Borrows the element a reference aliases.",
            ),
        ),
    ),
    LayerKind(
        id="share",
        description="A handle that shares the ownership of one element of a shape.",
        parameters=1,
        traits=(
            LayerTrait.OWNING,
            LayerTrait.SHARING,
            LayerTrait.NULLABLE,
            LayerTrait.BORROWABLE,
        ),
        forms=(
            form(
                "borrow_shared",
                RouteForm.BORROW,
                "Borrows the element a sharing handle shares.",
            ),
            form(
                "adopt_pointer_shared",
                RouteForm.ADOPT,
                "Adopts the element of a consumed pointer into a sharing handle.",
                target="ptr",
            ),
            form(
                "upcast_shared",
                RouteForm.UPCAST_CONSUMED,
                "Converts a consumed sharing handle of a leaf to one of an "
                "interface.",
            ),
            form(
                "upcast_copied_shared",
                RouteForm.UPCAST_COPIED,
                "Converts a copied sharing handle of a leaf to one of an interface.",
            ),
            form(
                "retain_shared_upcast",
                RouteForm.RETAIN,
                "Keeps a sharing handle of an interface converted from a borrowed "
                "sharing handle of the leaf, so it can be referred to.",
            ),
        ),
    ),
)
