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
        id="named_type_id",
        kind=VariantKind.COMPILER_VARIANT,
        justification=(
            "Compares type identities by type name in constant evaluation, for a\n"
            "compiler that rejects comparing the addresses of distinct objects in\n"
            "a constant expression, as GCC does under its null and nonnull\n"
            "sanitizers, a compiler defect. The library detects the defect, and\n"
            "addresses still identify types at runtime."
        ),
    ),
    VariantPoint(
        id="constructor_detection_msvc",
        kind=VariantKind.COMPILER_VARIANT,
        justification=(
            "Detects constructors with the msvc backend, for MSVC, which\n"
            "instantiates a recursive primary template before a zero-arity\n"
            "specialization, applies no brace elision while probing a\n"
            "std::array, and instantiates the exception specifications of\n"
            "competing conversions. The backend probes each arity with a class,\n"
            "takes the arity of a std::array from its size, and recovers a\n"
            "signature by one probe per argument and conversion category. It is\n"
            "standard C++ and is tested on every compiler; MSVC selects it."
        ),
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
