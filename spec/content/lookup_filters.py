"""Content of the lookup filters section."""

from spec.schema import FilterDimension, LookupFilter

lookup_filters: tuple[LookupFilter, ...] = (
    LookupFilter(
        id="binding_filter",
        description=(
            "A binding is named by a request when it serves the interface of "
            "the request, the stored leaf or a declared interface, and carries "
            "a key of the same domain that compares equal to the key of the "
            "request: no key for no key, the same tag, or an equal value."
        ),
        dimensions=(
            FilterDimension.INTERFACE,
            FilterDimension.KEY_KIND,
            FilterDimension.KEY_VALUE,
        ),
    ),
)
