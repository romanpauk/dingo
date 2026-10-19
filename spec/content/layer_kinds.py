"""Content of the shape algebra layer kinds section.

A stored or requested shape is a leaf wrapped in zero or more layers. The leaf
itself is not a layer: a shape without layers is the leaf value.

Layer kinds are classes of handles defined by their traits. The spec names no
concrete wrapper type: a concrete wrapper, a standard one or a user's, maps to
a layer kind through the wrapper-traits extension point of the C++ library.
"""

from spec.schema import LayerKind, LayerTrait

layer_kinds: tuple[LayerKind, ...] = (
    LayerKind(
        id="alt",
        description="A value holding exactly one of several alternative shapes.",
        parameters=1,
        variadic=True,
        traits=(LayerTrait.OWNING,),
    ),
    LayerKind(
        id="array",
        description="A fixed number of elements of one shape, stored inline.",
        parameters=1,
        has_extent=True,
        traits=(LayerTrait.OWNING,),
    ),
    LayerKind(
        id="array_unbounded",
        description=(
            "Elements of one shape whose number is known only at runtime; only "
            "an owning or sharing handle can hold it."
        ),
        parameters=1,
    ),
    LayerKind(
        id="opt",
        description="A value that may hold one element of a shape or nothing.",
        parameters=1,
        traits=(LayerTrait.OWNING, LayerTrait.NULLABLE, LayerTrait.BORROWABLE),
    ),
    LayerKind(
        id="own",
        description="A handle that is the only owner of one element of a shape.",
        parameters=1,
        traits=(LayerTrait.OWNING, LayerTrait.NULLABLE, LayerTrait.BORROWABLE),
    ),
    LayerKind(
        id="ptr",
        description="A non-owning address of one element of a shape.",
        parameters=1,
        traits=(LayerTrait.NULLABLE, LayerTrait.BORROWABLE),
    ),
    LayerKind(
        id="ref",
        description="A non-owning, non-null alias of one element of a shape.",
        parameters=1,
        traits=(LayerTrait.BORROWABLE,),
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
    ),
)
