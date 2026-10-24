"""Content of the lookup filters section."""

from spec.schema import FilterDimension, LookupFilter

lookup_filters: tuple[LookupFilter, ...] = (
    LookupFilter(
        id="binding_filter",
        description=(
            "A binding is named by a request when it serves the interface of "
            "the request and carries a key of the same domain that compares "
            "equal to the key of the request: no key for no key, the same tag, "
            "or an equal value. It serves its declared interfaces, or without "
            "one the leaves of the stored type tree: the leaf its wrappers hold, and "
            "through a variadic wrapper the leaf of each alternative, whichever "
            "alternative its object holds. A binding carries a key of each key "
            "type it is registered with, and for an interface the value "
            "registered for it, or otherwise the value registered for every "
            "interface."
        ),
        dimensions=(
            FilterDimension.INTERFACE,
            FilterDimension.KEY_KIND,
            FilterDimension.KEY_VALUE,
        ),
    ),
)
