"""Content of the C++ scope tags section."""

from spec.schema import CppScopeTag

cpp_scope_tags: tuple[CppScopeTag, ...] = (
    CppScopeTag(
        id="cpp_external",
        scope="external",
        spelling="dingo::external",
        header="dingo/storage/external.h",
    ),
    CppScopeTag(
        id="cpp_shared",
        scope="shared",
        spelling="dingo::shared",
        header="dingo/storage/shared.h",
    ),
    CppScopeTag(
        id="cpp_shared_cyclical",
        scope="shared_cyclical",
        spelling="dingo::shared_cyclical",
        header="dingo/storage/shared_cyclical.h",
    ),
    CppScopeTag(
        id="cpp_unique",
        scope="unique",
        spelling="dingo::unique",
        header="dingo/storage/unique.h",
    ),
)
