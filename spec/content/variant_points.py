"""Content of the variant points section.

The only places where separate code is allowed. A language tier is selected by
its capability and can be overridden, so every variant the compiler accepts is
tested; see docs/architecture/spec-driven-design.md, Language Tiers And
Compiler Variants.
"""

from spec.schema import VariantKind, VariantPoint

variant_points: tuple[VariantPoint, ...] = (
    VariantPoint(
        id="pack_at_builtin",
        kind=VariantKind.LANGUAGE_TIER,
        justification=(
            "Selects the element of a type pack with the compiler builtin, "
            "without the instantiations of the portable selection."
        ),
        capability="type_pack_element",
    ),
    VariantPoint(
        id="pack_at_indexing",
        kind=VariantKind.LANGUAGE_TIER,
        justification=(
            "Selects the element of a type pack with C++26 pack indexing, "
            "without the instantiations of the portable selection."
        ),
        capability="pack_indexing",
    ),
)
