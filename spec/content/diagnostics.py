"""Content of the diagnostics section.

The messages are those of the current implementation. A message names types
as the C++ library spells them, and `path` is the path of the types whose
objects are being constructed, outermost first, separated by " -> ".
"""

from spec.schema import Diagnostic

diagnostics: tuple[Diagnostic, ...] = (
    Diagnostic(
        id="type_not_found",
        condition="No binding is named by the request.",
        exception="type_not_found_exception",
        message="type not found: {request}",
        placeholders=("request",),
        fallback_carrier="type_not_found_carrier",
    ),
    Diagnostic(
        id="type_not_found_required_by",
        condition=(
            "No binding is named by the request of a dependency of an object "
            "being constructed."
        ),
        exception="type_not_found_exception",
        message="type not found: {request} (required by {path})",
        placeholders=("request", "path"),
        fallback_carrier="type_not_found_carrier",
    ),
    Diagnostic(
        id="type_ambiguous",
        condition="More than one binding is named by the request.",
        exception="type_ambiguous_exception",
        message="type resolution is ambiguous: {request}",
        placeholders=("request",),
        fallback_carrier="type_ambiguous_carrier",
    ),
    Diagnostic(
        id="type_ambiguous_required_by",
        condition=(
            "More than one binding is named by the request of a dependency of "
            "an object being constructed, whose type is `type`."
        ),
        exception="type_ambiguous_exception",
        message="type resolution is ambiguous: {request} (required by {type})",
        placeholders=("request", "type"),
        fallback_carrier="type_ambiguous_carrier",
    ),
    Diagnostic(
        id="type_not_convertible",
        condition="The binding the request names serves no term it accepts.",
        exception="type_not_convertible_exception",
        message="type is not convertible to {request} from {stored}",
        placeholders=("request", "stored"),
        fallback_carrier="type_not_convertible_carrier",
    ),
    Diagnostic(
        id="type_not_convertible_required_by",
        condition=(
            "The binding the request of a dependency of an object being "
            "constructed names serves no term it accepts."
        ),
        exception="type_not_convertible_exception",
        message="type is not convertible to {request} from {stored} (required by {type})",
        placeholders=("request", "stored", "type"),
        fallback_carrier="type_not_convertible_carrier",
    ),
    Diagnostic(
        id="type_recursion",
        condition=(
            "An object is requested while it is being constructed, by a scope "
            "that guards the recursion."
        ),
        exception="type_recursion_exception",
        message="recursive dependency detected: {path}",
        placeholders=("path",),
        fallback_carrier="type_recursion_carrier",
    ),
)
