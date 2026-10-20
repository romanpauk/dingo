"""Content of the index structures section.

An index structure accelerates the binding filter for the domains a lookup
declaration covers and returns exactly what the filter returns, in the order
of registration for bindings of equal keys; the choice never changes which
bindings a request names.
"""

from spec.schema import IndexStructure

index_structures: tuple[IndexStructure, ...] = (
    IndexStructure(
        id="slot",
        description="The one binding of a domain.",
        filter="binding_filter",
        key_kinds=("none", "tag"),
        declarations=("single", "typed", "base"),
    ),
    IndexStructure(
        id="sequence",
        description="The bindings of a domain in the order of their registration.",
        filter="binding_filter",
        key_kinds=("none", "tag"),
        declarations=("collection", "typed", "base"),
    ),
    IndexStructure(
        id="ordered",
        description="The bindings of a domain ordered by their key values.",
        filter="binding_filter",
        key_kinds=("none", "tag", "fixed", "runtime"),
        declarations=("associative", "base"),
    ),
    IndexStructure(
        id="unordered",
        description="The bindings of a domain hashed by their key values.",
        filter="binding_filter",
        key_kinds=("none", "tag", "fixed", "runtime"),
        declarations=("associative", "base"),
    ),
    IndexStructure(
        id="dense_array",
        description=(
            "The bindings of a domain of integral or enumeration keys by key "
            "value below a declared bound; a key outside it is never stored, "
            "so a registration of one is rejected and a request of one finds "
            "no binding."
        ),
        filter="binding_filter",
        key_kinds=("fixed", "runtime"),
        declarations=("associative",),
    ),
)
