"""Content of the selection sources section.

When the bindings of each selection source are known. A request is decided
when it is compiled when no runtime registration can change its outcome: the
steps up to the one that answers it ask no open source, or the bindings known
when it is compiled already make it ambiguous, as a registration only adds
candidates. A request that is decided and fails is a compile error, of a
static and of a mixed container alike; one that is not is reported when it is
resolved. The registration of a static binding that a static binding rejects
is decided when the container is compiled.
"""

from spec.schema import SelectionSource, SelectionSourceTime, SourceTime

selection_sources: tuple[SelectionSourceTime, ...] = (
    SelectionSourceTime(
        id="local_static",
        description="The static local bindings of the registration whose object is constructed.",
        source=SelectionSource.LOCAL_STATIC,
        time=SourceTime.COMPILE,
    ),
    SelectionSourceTime(
        id="local_runtime",
        description=(
            "The runtime local bindings of the registration whose object is "
            "constructed, which the type of the registration declares."
        ),
        source=SelectionSource.LOCAL_RUNTIME,
        time=SourceTime.COMPILE,
    ),
    SelectionSourceTime(
        id="container_static",
        description="The static bindings of the container, its type declares them.",
        source=SelectionSource.CONTAINER_STATIC,
        time=SourceTime.COMPILE,
    ),
    SelectionSourceTime(
        id="container_runtime",
        description="The runtime bindings of a container that takes runtime registrations.",
        source=SelectionSource.CONTAINER_RUNTIME,
        time=SourceTime.RUNTIME,
    ),
    SelectionSourceTime(
        id="parent",
        description="The selection of the parent, decided when the parent's is.",
        source=SelectionSource.PARENT,
        time=SourceTime.PARENT,
    ),
    SelectionSourceTime(
        id="auto_construct",
        description="Whether the type is auto-constructible, a trait of the type.",
        source=SelectionSource.AUTO_CONSTRUCT,
        time=SourceTime.COMPILE,
    ),
    SelectionSourceTime(
        id="factory",
        description="Whether the factory of a construct request makes its type.",
        source=SelectionSource.FACTORY,
        time=SourceTime.COMPILE,
    ),
    SelectionSourceTime(
        id="elements",
        description="The bindings of the element type the level holds, static and runtime.",
        source=SelectionSource.ELEMENTS,
        time=SourceTime.LEVEL,
    ),
)
