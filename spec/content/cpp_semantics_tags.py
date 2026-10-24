"""Content of the C++ semantics tags section.

The public tag of each wrapper semantics of the library. Its wrapper_semantics,
which the generator emits, declare the semantics as the semantics of a user's
wrappers are declared.
"""

from spec.schema import CppSemanticsTag


def tag(semantics, spelling):
    return CppSemanticsTag(id=f"cpp_{semantics}", semantics=semantics, spelling=spelling)


cpp_semantics_tags: tuple[CppSemanticsTag, ...] = (
    tag("alt", "dingo::semantics::variant_value"),
    tag("array", "dingo::semantics::fixed_array"),
    tag("array_unbounded", "dingo::semantics::dynamic_array"),
    tag("opt", "dingo::semantics::optional_value"),
    tag("own", "dingo::semantics::sole_owner"),
    tag("ptr", "dingo::semantics::pointer"),
    tag("ref", "dingo::semantics::reference"),
    tag("share", "dingo::semantics::shared_owner"),
)
