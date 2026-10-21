"""Content of the C++ kind projections section.

The public tag of each layer kind of the library. Its layer_kind_traits, which
the generator emits, declare the kind as a user declares a kind of their own.
"""

from spec.schema import CppKindProjection


def projection(kind, spelling):
    return CppKindProjection(id=f"cpp_{kind}", kind=kind, spelling=spelling)


cpp_kind_projections: tuple[CppKindProjection, ...] = (
    projection("alt", "dingo::alternative_kind"),
    projection("array", "dingo::array_kind"),
    projection("array_unbounded", "dingo::unsized_array_kind"),
    projection("opt", "dingo::optional_kind"),
    projection("own", "dingo::unique_kind"),
    projection("ptr", "dingo::pointer_kind"),
    projection("ref", "dingo::reference_kind"),
    projection("share", "dingo::shared_kind"),
)
