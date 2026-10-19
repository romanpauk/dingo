"""Content of the storage forms section.

A storage form classifies a stored shape by its outer layers. The scopes
publish request lists per form, see spec/content/scopes.py. The forms are those
of the current implementation's storage_form, described in
include/dingo/storage/storage_scope_policy.h.
"""

from spec.schema import PatternForm, ShapePattern, StorageForm


def variable(name):
    return ShapePattern(form=PatternForm.VARIABLE, name=name)


def layer(name, *args):
    return ShapePattern(form=PatternForm.LAYER, name=name, args=args)


storage_forms: tuple[StorageForm, ...] = (
    StorageForm(
        id="alternative",
        description="An alternative of several shapes.",
        pattern=layer("alt", variable("T")),
    ),
    StorageForm(
        id="bounded_array",
        description="An inline array of a fixed number of elements.",
        pattern=layer("array", variable("T")),
    ),
    StorageForm(
        id="optional",
        description="An optional element.",
        pattern=layer("opt", variable("T")),
    ),
    StorageForm(
        id="plain",
        description="The leaf value itself, or a shape no other form matches.",
        pattern=variable("T"),
    ),
    StorageForm(
        id="pointer",
        description="A non-owning address of an element.",
        pattern=layer("ptr", variable("T")),
    ),
    StorageForm(
        id="shared_array_handle",
        description="A sharing handle of an array of runtime size.",
        pattern=layer("share", layer("array_unbounded", variable("T"))),
    ),
    StorageForm(
        id="shared_handle",
        description="A sharing handle of one element.",
        pattern=layer("share", variable("T")),
    ),
    StorageForm(
        id="unbounded_array",
        description="An array of runtime size, held by the storage itself.",
        pattern=layer("array_unbounded", variable("T")),
    ),
    StorageForm(
        id="unique_array_handle",
        description="A sole-owner handle of an array of runtime size.",
        pattern=layer("own", layer("array_unbounded", variable("T"))),
    ),
    StorageForm(
        id="unique_handle",
        description="A sole-owner handle of one element.",
        pattern=layer("own", variable("T")),
    ),
)
