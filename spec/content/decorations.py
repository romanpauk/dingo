"""Content of the decorations section: what a request adds to the type it
names."""

from spec.schema import Decoration

decorations: tuple[Decoration, ...] = (
    Decoration(
        id="keyed",
        description=(
            "Names the bindings of the interface that carry the key of the request."
        ),
    ),
    Decoration(
        id="annotated",
        description=(
            "Names the interface together with an annotation type, a distinct "
            "interface of its own, without a key."
        ),
    ),
    Decoration(
        id="collection",
        description=(
            "Asks for every binding the request names, each resolved as the "
            "element type of a collection."
        ),
    ),
)
