"""The wrapper semantics of the user that the route probes check, as the library's.

Each kind is declared once, here, for both implementations of the algebra:
the reference implementation derives its routes from it, and the C++
declaration the next implementation compiles, its tag and wrapper_semantics,
is generated from it by cpp_declarations().

- The user wrappers user_unique, user_shared, and user_optional of the probes
  are of the kinds user_own, user_share, and user_opt, with the properties and
  forms of the library kinds they stand in for. The current implementation
  maps the same wrappers to its built-in kinds.
- The kind probes, of the next implementation only, wrap a standard type in a
  wrapper of a kind of the user (see templates/kinded_wrappers_next.h): kinds
  with the properties and forms of a library kind, a variant, and an array
  that, unlike one of the language, moves and copies; and kinds that differ
  from a library kind by one form or property, the mutations, which show
  that the next implementation reads the declaration of a kind rather than
  knowing it.
"""

from dataclasses import dataclass, replace

from spec.content.cpp_semantics_tags import cpp_semantics_tags
from spec.content.wrapper_semantics import wrapper_semantics
from spec.schema import Property, Operation

LIBRARY = {kind.id: kind for kind in wrapper_semantics}
LIBRARY_TAGS = {p.semantics: p.spelling for p in cpp_semantics_tags}


@dataclass(frozen=True)
class UserKind:
    """A kind of the user: its declaration, its C++ tag, and the C++ wrapper of
    it the probes name, a template of the element type. `template` is the
    kinded wrapper it is an alias of, with `arguments` after the element, or
    None for a wrapper of its own; `extent` is the number of its elements."""

    kind: object
    tag: str
    wrapper: str
    template: str | None = None
    arguments: str = ""
    extent: int | None = None
    # The expression of an object of the wrapper of {T}, for an external
    # binding.
    instance: str = ""
    # Whether a request creates a wrapper of the kind, wrapping or adopting.
    created: bool = False
    # The kind of the probes a mutation differs from by one form or property.
    mutates: str | None = None


def like(kind_id, library_id, targets=None, without=(), traits=None, lacks=()):
    """The kind `kind_id` of the properties and forms of the library kind
    `library_id`, whose forms lead to the kinds `targets` names instead, less
    the forms `without` and the traits `lacks`; `traits` replaces them."""
    targets = targets or {}
    kind = LIBRARY[library_id]
    return replace(
        kind,
        id=kind_id,
        description=f"A kind of the user like {library_id}.",
        traits=tuple(t for t in (traits or kind.traits) if t not in lacks),
        forms=tuple(
            replace(
                form,
                id=f"{form.id}_of_{kind_id}",
                target=targets.get(form.target, form.target),
            )
            for form in kind.forms
            if form.form not in without
        ),
    )


def kinded(kind, template, arguments="", extent=None, instance="", mutates=None):
    creates = any(f.form in (Operation.WRAP, Operation.ADOPT) for f in kind.forms)
    return UserKind(
        kind=kind,
        tag=f"{kind.id}_kind",
        wrapper=kind.id,
        template=template,
        arguments=arguments,
        extent=extent,
        instance=instance,
        created=creates,
        mutates=mutates,
    )


UNIQUE = "{W}<{T}>(std::make_unique<{T}>())"
SHARED = "{W}<{T}>(std::make_shared<{T}>())"
OPTIONAL = "{W}<{T}>(std::in_place)"

# A movable array: it holds its elements in place, is copied when they are,
# and decays to the address of its first element. It has no release form,
# which only an immovable array the scope allocates has.
ARRAY = replace(
    LIBRARY["array"],
    id="kind_array",
    description="An array of the user, which moves and copies.",
    traits=(
        Property.OWNING,
        Property.IN_PLACE,
        Property.CONSTRUCTIBLE,
        Property.MAPPED,
    ),
    forms=tuple(
        replace(form, id=f"{form.id}_of_kind_array")
        for form in LIBRARY["array"].forms
        if form.form is not Operation.RELEASE
    ),
)

user_kinds = (
    # The user wrappers of the probes.
    UserKind(like("user_own", "own", {"share": "user_share"}), "user_unique_kind", "user_unique"),
    UserKind(like("user_share", "share"), "user_shared_kind", "user_shared"),
    UserKind(like("user_opt", "opt"), "user_optional_kind", "user_optional"),
    # Kinds of the properties and forms of library kinds.
    kinded(like("kind_unique", "own", {"share": "kind_shared"}), "kinded_unique", instance=UNIQUE),
    kinded(like("kind_shared", "share"), "kinded_shared", instance=SHARED),
    kinded(like("kind_optional", "opt"), "kinded_optional", instance=OPTIONAL),
    kinded(
        like("kind_variant", "alt"),
        "kinded_variant",
        arguments="other_leaf",
        instance="{W}<{T}, other_leaf>(std::in_place_index<0>)",
    ),
    kinded(ARRAY, "kinded_array", arguments="2", extent=2, instance="{W}<{T}>{{}}"),
    # The mutations: a library kind less one form or one property.
    kinded(
        like("kind_unique_unshared", "own", without=(Operation.SHARE,)),
        "kinded_unique",
        instance=UNIQUE,
        mutates="kind_unique",
    ),
    kinded(
        like("kind_unique_unadopting", "own", {"share": "kind_shared"}, without=(Operation.ADOPT,)),
        "kinded_unique",
        instance=UNIQUE,
        mutates="kind_unique",
    ),
    kinded(
        like("kind_optional_unwrapping", "opt", without=(Operation.WRAP,)),
        "kinded_optional",
        instance=OPTIONAL,
        mutates="kind_optional",
    ),
    kinded(
        like("kind_optional_unborrowing", "opt", without=(Operation.BORROW,)),
        "kinded_optional",
        instance=OPTIONAL,
        mutates="kind_optional",
    ),
    kinded(
        like(
            "kind_shared_uncopyable",
            "share",
            without=(Operation.RETAIN,),
            lacks=(Property.COPYABLE,),
        ),
        "kinded_shared",
        instance=SHARED,
        mutates="kind_shared",
    ),
    kinded(
        like("kind_shared_unretaining", "share", without=(Operation.RETAIN,)),
        "kinded_shared",
        instance=SHARED,
        mutates="kind_shared",
    ),
)

# The kinds of the reference implementation.
reference_kinds = tuple(user.kind for user in user_kinds)

# The kinds of the kind probes, and of the user wrappers.
kind_probes = tuple(user for user in user_kinds if user.template is not None)
by_wrapper = {user.wrapper: user for user in user_kinds}

# The bits of the C++ wrapper properties, in the order of the spec.
PROPERTY_NAMES = {trait: trait.value for trait in Property}


def tag_of(kind_id):
    for user in user_kinds:
        if user.kind.id == kind_id:
            return user.tag
    return LIBRARY_TAGS[kind_id]


def cpp_declarations():
    """The C++ tags and wrapper_semantics of the kinds of the user, and the
    alias of the kinded wrapper of each kind probe."""
    lines = [
        "// Generated by test/routes/extract.py from test/routes/user_kinds.py.",
        "#pragma once",
        "",
        '#include "kinded_wrappers_next.h"',
        "",
        "#include <cstdint>",
        "",
        "namespace dingo {",
        "",
    ]
    lines += [f"struct {user.tag} {{}};" for user in user_kinds]
    for user in user_kinds:
        kind = user.kind
        properties = [f"dingo::property::{PROPERTY_NAMES[t]}" for t in kind.traits]
        if kind.variadic:
            properties.append("dingo::property::variadic")
        if kind.has_extent:
            properties.append("dingo::property::extent")
        forms = [
            f"dingo::op<dingo::operation::{form.form.value}"
            + (f", {tag_of(form.target)}" if form.target else "")
            + ">"
            for form in kind.forms
        ]
        lines += [
            "",
            f"// {kind.description}",
            f"template <> struct wrapper_semantics<{user.tag}> {{",
            "  static constexpr std::uint16_t properties ="
            f" {' | '.join(properties) or '0'};",
            f"  using operations = dingo::type_list<{', '.join(forms)}>;",
            "};",
        ]
    lines += ["", "} // namespace dingo", ""]
    for user in kind_probes:
        if user.kind.variadic:
            lines.append(
                f"template <typename... Types> using {user.wrapper} = "
                f"{user.template}<dingo::{user.tag}, Types...>;"
            )
        else:
            extra = f", {user.arguments}" if user.arguments else ""
            lines.append(
                f"template <typename T> using {user.wrapper} = "
                f"{user.template}<dingo::{user.tag}, T{extra}>;"
            )
    return "\n".join(lines) + "\n"
