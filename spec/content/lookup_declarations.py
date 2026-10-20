"""Content of the lookup declarations section.

A container declares lookups for the domains of its interfaces and keys. A
declaration decides whether a domain holds one binding or many: a second
binding of a domain declared one is rejected, at registration in a runtime
container and when the container type is formed in a static one. A domain
without a declaration follows the default declaration.
"""

from spec.schema import Cardinality, LookupDeclaration

ONE = Cardinality.ONE
MANY = Cardinality.MANY

lookup_declarations: tuple[LookupDeclaration, ...] = (
    LookupDeclaration(
        id="base",
        description=(
            "Every domain no other declaration covers, holding one binding "
            "unless declared many."
        ),
        key_kinds=("none", "tag", "fixed", "runtime"),
        cardinalities=(ONE, MANY),
        default_cardinality=ONE,
        default=True,
    ),
    LookupDeclaration(
        id="single",
        description="The bindings of an interface without a key, one of them.",
        key_kinds=("none",),
        cardinalities=(ONE,),
        default_cardinality=ONE,
    ),
    LookupDeclaration(
        id="collection",
        description="The bindings of an interface without a key, any number of them.",
        key_kinds=("none",),
        cardinalities=(MANY,),
        default_cardinality=MANY,
    ),
    LookupDeclaration(
        id="typed",
        description="The bindings of an interface by a tag key.",
        key_kinds=("tag",),
        cardinalities=(ONE, MANY),
        default_cardinality=ONE,
    ),
    LookupDeclaration(
        id="associative",
        description="The bindings of an interface by the value of a key type.",
        key_kinds=("fixed", "runtime"),
        cardinalities=(ONE, MANY),
        default_cardinality=ONE,
    ),
)
