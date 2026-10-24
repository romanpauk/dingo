"""Content of the scope descriptors section.

The descriptors record the target scope semantics. They start from the storage
scope policies of the current implementation,
include/dingo/storage/{shared,unique,external,shared_cyclical}.h, and every
difference from those policies is a declared behavior change, see
spec/content/behavior_changes.py.
"""

from spec.schema import (
    Exposure,
    ExposureRule,
    RecursionGuard,
    RequestCategory,
    Scope,
    Slot,
    SourceAccess,
    ValueCategory,
)

VALUE = RequestCategory.VALUE
LVALUE = RequestCategory.LVALUE
RVALUE = RequestCategory.RVALUE
POINTER = RequestCategory.POINTER

STORED_FORMS = (
    "alternative",
    "bounded_array",
    "optional",
    "plain",
    "pointer",
    "shared_array_handle",
    "shared_handle",
    "unbounded_array",
    "unique_array_handle",
    "unique_handle",
)


def exposures(categories_by_kind):
    return tuple(
        Exposure(storage_kind=form, categories=categories)
        for form, categories in sorted(categories_by_kind.items())
    )


BORROWED = (VALUE, LVALUE, POINTER)

scopes: tuple[Scope, ...] = (
    Scope(
        id="external",
        description=(
            "Refers to an instance supplied by the caller at registration; "
            "requests borrow it."
        ),
        slot=Slot.EXTERNAL_REF,
        materializes=ValueCategory.LVALUE,
        stable=True,
        source_access=SourceAccess.BORROW,
        guard=RecursionGuard.NONE,
        exposures=exposures(
            {form: BORROWED for form in (*STORED_FORMS, "reference")}
        ),
        needs=("instance",),
    ),
    Scope(
        id="shared",
        description=(
            "Constructs one instance on the first request and keeps it for the "
            "lifetime of the container; requests borrow it."
        ),
        slot=Slot.LAZY,
        materializes=ValueCategory.LVALUE,
        stable=True,
        source_access=SourceAccess.BORROW,
        guard=RecursionGuard.WHILE_UNRESOLVED,
        retains_unresolved_source=True,
        exposures=exposures({form: BORROWED for form in STORED_FORMS}),
    ),
    Scope(
        id="shared_cyclical",
        description=(
            "Shares one instance like shared, but publishes it before its "
            "dependencies resolve, so instances can depend on each other in a "
            "cycle. The instance is never copied, a sharing handle to it is."
        ),
        slot=Slot.TWO_PHASE,
        materializes=ValueCategory.LVALUE,
        stable=False,
        source_access=SourceAccess.BORROW,
        guard=RecursionGuard.NONE,
        exposure_rule=ExposureRule.HANDLE_COPIES,
        exposures=exposures(
            {
                "alternative": (LVALUE, POINTER),
                "plain": (LVALUE, POINTER),
                "shared_handle": BORROWED,
            }
        ),
        nested_wrappers=False,
        phases=("publish", "complete"),
    ),
    Scope(
        id="unique",
        description=(
            "Constructs a new instance for every request; requests consume it."
        ),
        slot=Slot.NONE,
        materializes=ValueCategory.RVALUE,
        stable=False,
        source_access=SourceAccess.CONSUME,
        guard=RecursionGuard.ALWAYS,
        exposures=exposures(
            {
                "alternative": (RVALUE,),
                "bounded_array": (RVALUE, POINTER),
                "optional": (RVALUE,),
                "plain": (RVALUE,),
                "pointer": (RVALUE, POINTER),
                "shared_array_handle": (RVALUE,),
                "shared_handle": (RVALUE,),
                "unbounded_array": (RVALUE, POINTER),
                "unique_array_handle": (RVALUE,),
                "unique_handle": (RVALUE,),
            }
        ),
    ),
)
