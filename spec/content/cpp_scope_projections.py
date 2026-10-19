"""Content of the C++ scope projections section."""

from spec.schema import CppScopeProjection

cpp_scope_projections: tuple[CppScopeProjection, ...] = (
    CppScopeProjection(
        id="cpp_external",
        scope="external",
        spelling="dingo::external",
        header="dingo/storage/external.h",
    ),
    CppScopeProjection(
        id="cpp_shared",
        scope="shared",
        spelling="dingo::shared",
        header="dingo/storage/shared.h",
    ),
    CppScopeProjection(
        id="cpp_shared_cyclical",
        scope="shared_cyclical",
        spelling="dingo::shared_cyclical",
        header="dingo/storage/shared_cyclical.h",
    ),
    CppScopeProjection(
        id="cpp_unique",
        scope="unique",
        spelling="dingo::unique",
        header="dingo/storage/unique.h",
    ),
)
