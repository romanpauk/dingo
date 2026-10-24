"""Content of the C++ language capabilities section.

The library detects language capabilities, not standard versions. A capability
is detected by a feature-test macro at a minimum value (optionally defined by a
standard header) or by a compiler builtin.
"""

from spec.schema import Capability

cpp_capabilities: tuple[Capability, ...] = (
    Capability(
        id="concepts",
        description="Constrains templates directly in place of SFINAE overload sets.",
        feature_macro="__cpp_concepts",
        minimum=201907,
    ),
    Capability(
        id="consteval",
        description="Forces the route functions to evaluate at compile time, in place of ordinary constexpr functions.",
        feature_macro="__cpp_consteval",
        minimum=201811,
    ),
    Capability(
        id="constexpr_dynamic_alloc",
        description="Allows transient constexpr allocation in the route functions, in place of two-pass fixed-array sizing.",
        feature_macro="__cpp_constexpr_dynamic_alloc",
        minimum=201907,
    ),
    Capability(
        id="copyable_function",
        description="Enables std::copyable_function as a callable factory signature.",
        feature_macro="__cpp_lib_copyable_function",
        minimum=202306,
        header="version",
    ),
    Capability(
        id="deleted_function_reason",
        description="Gives deleted overloads a reason message, in place of silently deleted overloads.",
        feature_macro="__cpp_deleted_function",
        minimum=202403,
    ),
    Capability(
        id="if_consteval",
        description="Branches on constant evaluation with if consteval, in place of std::is_constant_evaluated.",
        feature_macro="__cpp_if_consteval",
        minimum=202106,
    ),
    Capability(
        id="move_only_function",
        description="Enables std::move_only_function as a callable factory signature.",
        feature_macro="__cpp_lib_move_only_function",
        minimum=202110,
        header="version",
    ),
    Capability(
        id="pack_indexing",
        description="Indexes a type pack directly with Ts...[I], in place of the __type_pack_element builtin.",
        feature_macro="__cpp_pack_indexing",
        minimum=202311,
    ),
    Capability(
        id="static_assert_message",
        description="Builds user-generated static_assert messages that show the types involved, in place of fixed literal messages.",
        feature_macro="__cpp_static_assert",
        minimum=202306,
    ),
    Capability(
        id="type_pack_element",
        description="Indexes a type pack with the compiler builtin, in place of the constant-depth fallback.",
        builtin="__type_pack_element",
    ),
)
