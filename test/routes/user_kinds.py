"""The layer kinds of the user wrappers of the route probes.

The next implementation declares a kind of its own for each user wrapper,
through the public layer_kind_traits, with the properties and forms of the
library kind the wrapper stands in for: a sole-owner handle that shares its
element with the user's sharing handle, a sharing handle, and an optional. The
reference implementation derives their routes as it does those of the
library's kinds, so the route table checks a kind of the user as one of the
library. The C++ declarations are in templates/user_wrapper_traits_next.h; the
current implementation maps the same wrappers to its built-in kinds.
"""

from dataclasses import replace

from spec.content.layer_kinds import layer_kinds

LIBRARY = {kind.id: kind for kind in layer_kinds}


def user_kind(kind_id, like, targets=None):
    """The kind `kind_id` of the properties and forms of the library kind
    `like`, whose forms lead to the kinds `targets` names instead."""
    targets = targets or {}
    kind = LIBRARY[like]
    return replace(
        kind,
        id=kind_id,
        description=f"A kind of the user with the properties and forms of {like}.",
        forms=tuple(
            replace(form, id=f"{form.id}_of_{kind_id}", target=targets.get(form.target, form.target))
            for form in kind.forms
        ),
    )


user_kinds = (
    user_kind("user_own", "own", {"share": "user_share"}),
    user_kind("user_share", "share"),
    user_kind("user_opt", "opt"),
)
