"""Content of the storage forms section.

A storage form classifies a stored shape by the properties of the kinds of its
outer layers, so a wrapper semantics of the user is of a form as a kind of the
library is. The scopes publish request lists per form, see
spec/content/scopes.py. The forms are those of the current implementation's
storage_form, described in include/dingo/storage/storage_scope_policy.h, plus
the reference form that only external storage holds.
"""

from spec.schema import LayerClass, LayerTrait, StorageForm

OWNING = LayerTrait.OWNING
SHARING = LayerTrait.SHARING
NULLABLE = LayerTrait.NULLABLE
BORROWABLE = LayerTrait.BORROWABLE
IN_PLACE = LayerTrait.IN_PLACE
UNSIZED = LayerTrait.UNSIZED

UNIQUE = LayerClass(has=(OWNING,), lacks=(SHARING, IN_PLACE, UNSIZED))
SHARED = LayerClass(has=(SHARING,))
ARRAY = LayerClass(has=(UNSIZED,))

storage_forms: tuple[StorageForm, ...] = (
    StorageForm(
        id="alternative",
        description="An alternative of several shapes.",
        outer=LayerClass(variadic=True),
    ),
    StorageForm(
        id="bounded_array",
        description="An inline array of a fixed number of elements.",
        outer=LayerClass(extent=True),
    ),
    StorageForm(
        id="optional",
        description="An optional element.",
        outer=LayerClass(has=(IN_PLACE, NULLABLE)),
    ),
    StorageForm(
        id="plain",
        description="The leaf value itself, or a shape no other form matches.",
    ),
    StorageForm(
        id="pointer",
        description="A non-owning address of an element.",
        outer=LayerClass(has=(NULLABLE,), lacks=(OWNING,)),
    ),
    StorageForm(
        id="reference",
        description="An alias of an element owned elsewhere.",
        outer=LayerClass(has=(BORROWABLE,), lacks=(OWNING, NULLABLE)),
    ),
    StorageForm(
        id="shared_array_handle",
        description="A sharing handle of an array of runtime size.",
        outer=SHARED,
        inner=ARRAY,
    ),
    StorageForm(
        id="shared_handle",
        description="A sharing handle of one element.",
        outer=SHARED,
    ),
    StorageForm(
        id="unbounded_array",
        description="An array of runtime size, held by the storage itself.",
        outer=ARRAY,
    ),
    StorageForm(
        id="unique_array_handle",
        description="A sole-owner handle of an array of runtime size.",
        outer=UNIQUE,
        inner=ARRAY,
    ),
    StorageForm(
        id="unique_handle",
        description="A sole-owner handle of one element.",
        outer=UNIQUE,
    ),
)
