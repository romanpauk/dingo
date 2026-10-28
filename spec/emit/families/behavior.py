#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate behavior classes from the scope/form model.

The generator owns the product: every supported scope/form from the route
ledger becomes a behavior class in each applicable container front-end. The
primary request, relation, request class, and construction count are derived
from the semantic form kind, stored wrapper properties, and scope; a form adds
its registration facts and capabilities, not a hand-written expectation per
form.

Behavior gaps are forms the ledger publishes but that have no resolvable
behavior case yet. They carry a reason and are checked by the coverage
contract, so a supported form can never silently lose its behavior test.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.grammar.catalog import (
    FORM_BY_NAME,
    FORMS as CATALOG_FORMS,
    SCOPES as CATALOG_SCOPES,
    RUNTIME_ONLY_SCOPES,
    SCOPE_BY_NAME,
    FormConstructor,
    StorageForm,
)
from spec.semantics.storage_forms import iter_supported_forms
from spec.semantics.gaps import form_gaps, name_gaps, validate as validate_gaps
from spec.emit.common.recipe import external_instance, registration
from spec.semantics.routes.ledger import supported_categories
from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath
from spec.emit.common.validation import validate_catalog
from spec.grammar import cpp
from spec.grammar.shapes import WRAPPER_BY_NAME
from spec.grammar.traits import COPYABLE, MOVE_ONLY, TRAITS, LeafTraits


SCOPES = tuple(scope.name for scope in CATALOG_SCOPES)
FORMS = tuple(form for form in CATALOG_FORMS if form.name != "array")
CONTAINERS = ("runtime", "static")


@dataclass(frozen=True)
class Expectation:
    """The resolvable request, its identity relation, and its shape.

    Construction counts are storage-shape facts and stay explicit. Copy and
    move counts are derived from the request class and the scope, so a storage form
    cannot silently carry a count that no longer matches its shape.
    """

    request: str
    relation: str
    constructions: int
    request_class: str
    leaf: str = ""
    array_element: str = ""
    move_out: bool = False
    delete_result: str = ""


# The minimum copy and move count for two resolutions of each request class.
# The class describes the request shape, not the storage form.
REQUEST_COSTS = {
    # A value copied out of a borrowed source: one copy per resolution.
    "borrow_value": (2, 0),
    # A value moved out of a consumed source: one move per resolution.
    "consume_value": (0, 2),
    # An rvalue reference moved into a caller object: one library move and one
    # caller move per resolution.
    "consume_rvalue": (0, 4),
    # References, owning handles, and arrays transfer no tracked object.
    "reference": (0, 0),
    "handle": (0, 0),
    "array": (0, 0),
}


def _elements(kind: str) -> int:
    return 2 if kind in {"array", "array_handle"} else 1


def _constructions(scope: str, kind: str) -> int:
    if scope == "external":
        return 0
    if scope == "unique":
        return 2 * _elements(kind)
    return _elements(kind)


def derive_expectation(
    form: StorageForm,
    scope: str,
    traits: LeafTraits = COPYABLE,
) -> Expectation:
    """Derive the primary behavior request from the selected route shape.

    The request class, relation, constructions, and optional leaf checks all
    follow from the form's semantic kind, its stored wrapper properties, and the
    scope. Construction counts are a form fact: external storage constructs
    nothing, unique storage constructs two resolutions, and the other scopes
    construct one. Array-shaped forms hold two tracked elements.

    A copyable leaf uses the value request; a move-only leaf uses the first
    non-copying route (a borrow, a raw pointer, or the stored wrapper
    reference), so the expected copy count is zero.
    """
    kind = form.kind
    wrapper = form.wrapper
    leaf = cpp.leaf(traits.cpp)
    constructions = _constructions(scope, kind)
    consumes = SCOPE_BY_NAME[scope].consumes
    sharing = wrapper is not None and wrapper.has("sharing")

    if kind == "value":
        if scope == "shared_cyclical":
            return Expectation(
                f"{traits.cpp} &", "&first == &second", constructions, "reference"
            )
        if consumes:
            return Expectation(
                traits.cpp,
                "first.id != second.id",
                constructions,
                "consume_value",
            )
        if not traits.copyable:
            return Expectation(
                f"{traits.cpp} &",
                "&first == &second",
                constructions,
                "reference",
                leaf="external" if scope == "external" else "",
            )
        return Expectation(
            traits.cpp,
            "first.id == second.id",
            constructions,
            "borrow_value",
            leaf="external" if scope == "external" else "id",
        )

    if kind in {"variant", "optional"}:
        if consumes:
            if kind == "variant":
                return Expectation(
                    cpp.template("std::variant", leaf, cpp.leaf("long")).render(),
                    "std::get<0>(first).id != std::get<0>(second).id",
                    constructions,
                    "consume_value",
                )
            return Expectation(
                cpp.template("std::optional", leaf).render(),
                "first->id != second->id",
                constructions,
                "consume_value",
            )
        if not traits.copyable:
            return Expectation(
                cpp.reference(form.cpp(traits.cpp)).render(),
                "&first == &second",
                constructions,
                "reference",
            )
        return Expectation(
            traits.cpp,
            "first.id == second.id",
            constructions,
            "borrow_value",
            leaf="id",
        )

    if kind == "pointer":
        if consumes:
            return Expectation(
                cpp.pointer(leaf).render(),
                "first->id != second->id",
                constructions,
                "handle",
                delete_result="delete",
            )
        if not traits.copyable:
            return Expectation(
                cpp.pointer(leaf).render(),
                "first == second",
                constructions,
                "reference",
            )
        return Expectation(
            traits.cpp,
            "first.id == second.id",
            constructions,
            "borrow_value",
            leaf="id",
        )

    if kind == "array":
        if scope == "shared_cyclical":
            return Expectation(
                cpp.array_pointer(leaf, 2).render(),
                "first == second",
                constructions,
                "array",
                array_element="(*first)[0].id != (*first)[1].id",
            )
        return Expectation(
            cpp.pointer(leaf).render(),
            "first[0].id != second[0].id"
            if consumes
            else "first[0].id == second[0].id",
            constructions,
            "array",
            array_element="first[0].id != first[1].id",
            delete_result="delete[]" if consumes else "",
        )

    if kind == "handle":
        if sharing:
            return Expectation(
                cpp.template("std::shared_ptr", leaf).render(),
                "first == second" if scope != "unique" else "first != second",
                constructions,
                "handle",
                leaf="" if consumes else "pointer",
            )
        return Expectation(
            cpp.template("std::unique_ptr", leaf).render(),
            "first->id != second->id",
            constructions,
            "handle",
        )

    if kind == "array_handle":
        if sharing:
            return Expectation(
                cpp.shared_array(leaf).render(),
                "first == second" if scope != "unique" else "first != second",
                constructions,
                "array",
            )
        if consumes:
            return Expectation(
                cpp.unique_array(leaf).render(),
                "first[0].id != second[0].id",
                constructions,
                "array",
            )
        return Expectation(
            cpp.reference(cpp.unique_array(leaf)).render(),
            "&first == &second",
            constructions,
            "reference",
        )

    raise ValueError(f"no behavior expectation for {form.name}/{scope}")


# Additional request shapes for a storage form that the primary expectation does not
# exercise: the unique rvalue route and the shared wrapper-value route.
def derive_extra_expectations(
    form: StorageForm,
    scope: str,
    primary: Expectation,
    traits: LeafTraits = COPYABLE,
) -> tuple[tuple[str, Expectation], ...]:
    """Additional route shapes worth executing for a storage form.

    These are not alternate expectations: they exercise a second published
    route. The unique plain form also moves its rvalue route; the shared
    alternative form also copies the variant wrapper value.
    """
    kind = form.kind
    extras = []
    if kind == "value" and scope == "unique" and primary.request_class == "consume_value":
        extras.append(
            (
                "rvalue",
                Expectation(
                    f"{traits.cpp} &&",
                    "first.id != second.id",
                    primary.constructions,
                    "consume_rvalue",
                    move_out=True,
                ),
            )
        )
    if kind == "variant" and scope == "shared" and primary.request_class == "borrow_value":
        extras.append(
            (
                "wrapper",
                Expectation(
                    cpp.template(
                        "std::variant", cpp.leaf(traits.cpp), cpp.leaf("long")
                    ).render(),
                    "std::get<0>(first).id == std::get<0>(second).id",
                    primary.constructions,
                    "borrow_value",
                ),
            )
        )
    return tuple(extras)


@dataclass(frozen=True)
class InterfaceCase:
    """A storage exposed through one or more interfaces."""

    name: str
    scope: str
    storage: str
    factory: str | None
    interfaces: tuple[str, ...]
    request: str
    relation: str
    constructions: int
    request_class: str
    form_name: str = ""
    containers: tuple[str, ...] = CONTAINERS
    other_request: str = ""
    other_relation: str = ""
    # An annotated reference request returns a reference from a value-shaped
    # wrapper, so the generated test must bind by reference explicitly.
    bind_reference: bool = False


@dataclass(frozen=True)
class InterfaceShape:
    """A storage exposed through one or more interfaces."""

    name: str
    form_name: str
    scope: str
    storage: str
    factory: str | None
    kind: str
    interfaces: tuple[str, ...]
    constructions: int


# Interface storage shapes are derived from the behavior forms. The concrete
# storage/factory facts come from the form; the interface fixture and copyable
# implementation are the only test-specific data.
INTERFACE_GAPS = name_gaps("interface")


def _interface_shape(
    form_name: str,
    *,
    twin: bool = False,
    copyable: bool = False,
    traits: LeafTraits = COPYABLE,
) -> InterfaceShape:
    form = next(form for form in FORMS if form.name == form_name)
    wrapper = form.wrapper
    sharing = wrapper is not None and wrapper.has("sharing")
    sole_owner = wrapper is not None and wrapper.has("sole_owner")
    if copyable:
        return InterfaceShape(
            name="shared_copyable",
            form_name=form_name,
            scope="shared",
            storage=cpp.storage(
                cpp.template(
                    "std::shared_ptr", cpp.leaf("copyable_implementation")
                )
            ).render(),
            factory="make_copyable_implementation",
            kind="copyable",
            interfaces=("copyable_interface",),
            constructions=1,
        )
    if sharing:
        prefix = "shared"
        scope = "shared"
        kind = "shared_handle"
        constructions = 1
    elif sole_owner:
        prefix = "unique"
        scope = "unique"
        kind = "unique_handle"
        constructions = 2
    else:
        prefix = "shared_plain"
        scope = "shared"
        kind = "plain"
        constructions = 1
    if twin:
        prefix = f"{prefix}_second"
        constructions = 3 if sole_owner else constructions
    interfaces = (
        ("tracked_interface", "tracked_interface2")
        if twin
        else ("tracked_interface",)
    )
    return InterfaceShape(
        name=prefix,
        form_name=form_name,
        scope=scope,
        storage=form.storage(traits.cpp),
        factory=form.factory(traits.cpp),
        kind=kind,
        interfaces=interfaces,
        constructions=constructions,
    )


def derive_interface_shapes(
    traits: LeafTraits = COPYABLE,
) -> tuple[InterfaceShape, ...]:
    shapes = [
        _interface_shape("shared_handle", traits=traits),
        _interface_shape("plain", traits=traits),
        _interface_shape("unique_handle", traits=traits),
        _interface_shape("shared_handle", twin=True, traits=traits),
        _interface_shape("unique_handle", twin=True, traits=traits),
    ]
    if traits.copyable:
        shapes.append(
            _interface_shape("shared_handle", copyable=True, traits=traits)
        )
    return tuple(shapes)


INTERFACE_SHAPES = derive_interface_shapes()


def _interface_forms(shape: InterfaceShape) -> tuple[str, ...]:
    if shape.kind == "shared_handle":
        if len(shape.interfaces) == 1:
            return ("ref", "ptr", "handle")
        return ("ref", "handle")
    if shape.kind == "plain":
        return ("ref",)
    if shape.kind == "unique_handle":
        if len(shape.interfaces) == 1:
            return ("unique_handle", "shared_handle")
        return ("unique_handle",)
    if shape.kind == "copyable":
        return ("value",)
    raise ValueError(f"no interface forms for {shape.name}")


def _interface_request(form: str, primary: str, scope: str) -> tuple[str, str, str]:
    if form == "ref":
        return f"{primary} &", "&first == &second", "reference"
    if form == "ptr":
        return f"{primary} *", "first == second", "reference"
    if form in {"handle", "shared_handle"}:
        relation = "first == second" if scope != "unique" else "first != second"
        request = WRAPPER_BY_NAME["shared_ptr"].constructor(cpp.leaf(primary)).render()
        return request, relation, "handle"
    if form == "unique_handle":
        request = WRAPPER_BY_NAME["unique_ptr"].constructor(cpp.leaf(primary)).render()
        return (
            request,
            "first->value() != second->value()",
            "handle",
        )
    if form == "value":
        return primary, "first.value.id == second.value.id", "borrow_value"
    raise ValueError(f"no interface request for form {form}")


def derive_interface_cases(
    traits: LeafTraits = COPYABLE,
) -> tuple[InterfaceCase, ...]:
    cases = []
    for shape in derive_interface_shapes(traits):
        twin = len(shape.interfaces) > 1
        other_interface = shape.interfaces[0] if twin else ""
        for form in _interface_forms(shape):
            primary = (
                shape.interfaces[-1]
                if twin and form in {"handle", "shared_handle", "unique_handle"}
                else shape.interfaces[0]
            )
            request, relation, request_class = _interface_request(
                form, primary, shape.scope
            )
            other_request = ""
            other_relation = ""
            if twin:
                other_request, _, _ = _interface_request(
                    form, other_interface, shape.scope
                )
                if form == "ref":
                    other_relation = "first.value() == other.value()"
                elif form == "unique_handle":
                    other_relation = "first->value() != other->value()"
                else:
                    other_relation = "first->value() == other->value()"
            suffix = "handle" if form == "unique_handle" else form
            cases.append(
                InterfaceCase(
                    name=f"{shape.name}_{suffix}",
                    scope=shape.scope,
                    storage=shape.storage,
                    factory=shape.factory,
                    interfaces=shape.interfaces,
                    request=request,
                    relation=relation,
                    constructions=shape.constructions,
                    request_class=request_class,
                    form_name=shape.form_name,
                    other_request=other_request,
                    other_relation=other_relation,
                )
            )
    return tuple(cases)


INTERFACE_CASES = derive_interface_cases()


def _interface_rows(
    traits: LeafTraits = COPYABLE,
) -> list[dict[str, object]]:
    rows = []
    for case in derive_interface_cases(traits):
        copies, moves = REQUEST_COSTS[case.request_class]
        for container in case.containers:
            registration, static_source = _registration(
                case.form_name,
                case.scope,
                container,
                case.interfaces,
                storage=case.storage,
                factory=case.factory,
                leaf=traits.cpp,
            )
            rows.append(
                _row(
                    f"{case.scope}_interface_{case.name}_{container}",
                    case.scope,
                    case.name,
                    container,
                    storage=case.storage,
                    registration=registration,
                    static_source=static_source,
                    request=case.request,
                    relation=case.relation,
                    constructions=case.constructions,
                    request_class=case.request_class,
                    copies=copies,
                    moves=moves,
                    other_request=case.other_request,
                    other_relation=case.other_relation,
                    bind_reference=case.bind_reference,
                    interface=True,
                )
            )
    return rows


@dataclass(frozen=True)
class AnnotationRecipe:
    """An annotated exposure derived from a covered storage shape."""

    name: str
    scope: str
    storage: str
    factory: str | None
    leaf: str
    request: str
    relation: str
    constructions: int
    request_class: str
    bind_reference: bool = False
    form_name: str = ""
    external_instance: str = ""
    external_argument: str = ""
    registration_moves: int = 0


@dataclass(frozen=True)
class AnnotationShape:
    """A storage shape exposed through an annotated interface."""

    name: str
    scope: str
    storage: str
    factory: str | None
    leaf: str
    forms: tuple[str, ...]
    form_name: str = ""
    append_form: bool = True
    external_instance: str = ""
    external_argument: str = ""
    registration_moves: int = 0


ANNOTATION_GAPS = name_gaps("annotation")


def _shared_annotation_shape(
    form_name: str,
    name: str,
    *,
    leaf: str = "tracked",
    forms: tuple[str, ...] = ("ref",),
    traits: LeafTraits = COPYABLE,
) -> AnnotationShape:
    form = next(form for form in FORMS if form.name == form_name)
    return AnnotationShape(
        name=name,
        scope="shared",
        storage=form.storage(traits.cpp),
        factory=form.factory(traits.cpp),
        leaf=leaf,
        forms=forms,
        form_name=form_name,
    )


def _external_annotation_shape(
    name: str,
    storage: str,
    declaration: str,
    argument: str,
    moves: int = 0,
    *,
    form_name: str = "plain",
    traits: LeafTraits = COPYABLE,
) -> AnnotationShape:
    return AnnotationShape(
        name=name,
        scope="external",
        storage=storage,
        factory=None,
        leaf=traits.cpp,
        forms=("ref",),
        form_name=form_name,
        append_form=False,
        external_instance=declaration,
        external_argument=argument,
        registration_moves=moves,
    )


def derive_annotation_shapes(
    traits: LeafTraits = COPYABLE,
) -> tuple[AnnotationShape, ...]:
    leaf = traits.cpp
    value_forms = ("ref", "value") if traits.copyable else ("ref",)
    shapes = [
        _shared_annotation_shape(
            "plain",
            "shared_annotated",
            leaf=leaf,
            forms=value_forms,
            traits=traits,
        ),
        _shared_annotation_shape(
            "shared_handle",
            "shared_annotated_shared_ptr",
            leaf=leaf,
            traits=traits,
        ),
        _shared_annotation_shape(
            "optional",
            "shared_annotated_optional",
            leaf=leaf,
            traits=traits,
        ),
        _shared_annotation_shape(
            "pointer",
            "shared_annotated_pointer",
            leaf=leaf,
            traits=traits,
        ),
        _shared_annotation_shape(
            "shared_handle",
            "shared_annotated",
            leaf="tracked_interface",
            forms=(
                "interface_ref",
                "interface_ptr",
                "interface_handle",
                "interface_handle_ref",
            ),
            traits=traits,
        ),
        _external_annotation_shape(
            "external_annotated_ref",
            cpp.storage(cpp.reference(cpp.leaf(leaf))).render(),
            f"{leaf} external_instance",
            "external_instance",
            traits=traits,
        ),
        _external_annotation_shape(
            "external_annotated_ptr",
            cpp.storage(cpp.pointer(cpp.leaf(leaf))).render(),
            f"{leaf} external_instance",
            "std::addressof(external_instance)",
            form_name="pointer",
            traits=traits,
        ),
        _external_annotation_shape(
            "external_annotated_shared_ptr",
            cpp.storage(
                cpp.template("std::shared_ptr", cpp.leaf(leaf))
            ).render(),
            f"auto external_instance = make_shared_{leaf}()",
            "external_instance",
            form_name="shared_handle",
            traits=traits,
        ),
    ]
    if traits.copyable:
        shapes.insert(
            5,
            _external_annotation_shape(
                "external_annotated_value",
                cpp.storage(cpp.leaf(leaf)).render(),
                f"{leaf} external_instance",
                "std::move(external_instance)",
                moves=1,
                traits=traits,
            ),
        )
    return tuple(shapes)


ANNOTATION_SHAPES = derive_annotation_shapes()


ANNOTATION_REQUESTS = {
    "ref": (lambda leaf: cpp.reference(cpp.leaf(leaf)), "&first == &second", "reference", True),
    "interface_ref": (lambda leaf: cpp.reference(cpp.leaf(leaf)), "&first == &second", "reference", True),
    "value": (lambda leaf: cpp.leaf(leaf), "first.id == second.id", "borrow_value", False),
    "interface_ptr": (lambda leaf: cpp.pointer(cpp.leaf(leaf)), "first == second", "reference", False),
    "interface_handle": (
        lambda leaf: cpp.template("std::shared_ptr", cpp.leaf(leaf)),
        "first == second",
        "handle",
        False,
    ),
    "interface_handle_ref": (
        lambda leaf: cpp.reference(cpp.template("std::shared_ptr", cpp.leaf(leaf))),
        "&first == &second",
        "reference",
        True,
    ),
}


def _annotation_form(
    shape: AnnotationShape, form: str
) -> tuple[str, str, str, bool]:
    try:
        template, relation, request_class, bind_reference = (
            ANNOTATION_REQUESTS[form]
        )
    except KeyError:
        raise ValueError(f"no annotation request for form {form}") from None
    return (
        cpp.annotated(template(shape.leaf), "tag_a").render(),
        relation,
        request_class,
        bind_reference,
    )


def derive_annotation_recipes(
    traits: LeafTraits = COPYABLE,
) -> tuple[AnnotationRecipe, ...]:
    recipes = []
    for shape in derive_annotation_shapes(traits):
        for form in shape.forms:
            request, relation, request_class, bind_reference = _annotation_form(
                shape, form
            )
            name = f"{shape.name}_{form}" if shape.append_form else shape.name
            recipes.append(
                AnnotationRecipe(
                    name=name,
                    scope=shape.scope,
                    storage=shape.storage,
                    factory=shape.factory,
                    leaf=shape.leaf,
                    request=request,
                    relation=relation,
                    constructions=0 if shape.scope == "external" else 1,
                    request_class=request_class,
                    bind_reference=bind_reference,
                    form_name=shape.form_name,
                    external_instance=shape.external_instance,
                    external_argument=shape.external_argument,
                    registration_moves=shape.registration_moves,
                )
            )
    return tuple(recipes)


ANNOTATION_RECIPES = derive_annotation_recipes()


def _annotation_rows(
    traits: LeafTraits = COPYABLE,
) -> list[dict[str, object]]:
    rows = []
    for recipe in derive_annotation_recipes(traits):
        copies, moves = REQUEST_COSTS[recipe.request_class]
        moves += recipe.registration_moves
        containers = (
            ("runtime",) if recipe.scope == "external" else CONTAINERS
        )
        interfaces = (cpp.annotated(cpp.leaf(recipe.leaf), "tag_a").render(),)
        for container in containers:
            if recipe.scope == "external":
                registration, static_source = _registration(
                    recipe.form_name,
                    recipe.scope,
                    container,
                    interfaces,
                    storage=recipe.storage,
                    external_argument=recipe.external_argument,
                    leaf=traits.cpp,
                )
            else:
                registration, static_source = _registration(
                    recipe.form_name,
                    recipe.scope,
                    container,
                    interfaces,
                    storage=recipe.storage,
                    factory=recipe.factory,
                    leaf=traits.cpp,
                )
            rows.append(
                _row(
                    f"{recipe.scope}_annotation_{recipe.name}_{container}",
                    recipe.scope,
                    recipe.name,
                    container,
                    storage=recipe.storage,
                    instance=recipe.external_instance,
                    registration=registration,
                    static_source=static_source,
                    request=recipe.request,
                    relation=recipe.relation,
                    constructions=recipe.constructions,
                    request_class=recipe.request_class,
                    copies=copies,
                    moves=moves,
                    bind_reference=recipe.bind_reference,
                    interface=True,
                    annotation=True,
                )
            )
    return rows


# Supported forms the route model publishes but that have no resolvable
# behavior case yet. Each one must state why, so the contract distinguishes a
# missing test from a shape the library cannot actually serve.
BEHAVIOR_GAPS = form_gaps("behavior")


def _registration(
    form_name: str,
    scope: str,
    container: str,
    interfaces: tuple[str, ...] = (),
    *,
    storage: str | None = None,
    factory: str | None = None,
    external_argument: str = "",
    leaf: str = "tracked",
) -> tuple[str, str]:
    return registration(
        FORM_BY_NAME[form_name],
        scope,
        container,
        leaf=leaf,
        interfaces=interfaces,
        storage=storage,
        factory=factory,
        external_argument=external_argument,
    )


def _row(
    name: str,
    scope: str,
    form: str,
    container: str,
    *,
    scope_type: str | None = None,
    storage: str = "",
    instance: str = "",
    registration: str = "",
    static_source: str = "",
    request: str = "",
    relation: str = "",
    constructions: int = 0,
    request_class: str = "",
    copies: int | None = None,
    moves: int | None = None,
    leaf: str = "",
    array_element: str = "",
    move_out: bool = False,
    delete_result: str = "",
    other_request: str = "",
    other_relation: str = "",
    bind_reference: bool = False,
    interface: bool = False,
    annotation: bool = False,
) -> dict[str, object]:
    return {
        "name": name,
        "scope": scope,
        "form": form,
        "container": container,
        "scope_type": scope_type
        or cpp.template("dingo::scope", cpp.text(f"dingo::{scope}")).render(),
        "storage": storage,
        "instance": instance,
        "registration": registration,
        "static_source": static_source,
        "request": request,
        "relation": relation,
        "constructions": constructions,
        "request_class": request_class,
        "copies": copies,
        "moves": moves,
        "leaf": leaf,
        "array_element": array_element,
        "move_out": move_out,
        "delete_result": delete_result,
        "other_request": other_request,
        "other_relation": other_relation,
        "bind_reference": bind_reference,
        "interface": interface,
        "annotation": annotation,
    }


def build_rows(
    traits: LeafTraits = COPYABLE,
) -> list[dict[str, object]]:
    rows = []
    for scope, form in iter_supported_forms(form_major=True):
        scope_name = scope.name
        if (scope_name, form.name) in BEHAVIOR_GAPS:
            continue
        expectation = derive_expectation(form, scope_name, traits)
        external_move = (
            not traits.copyable
            and form.external_constructor is not FormConstructor.REFERENCE
            and form.kind != "array"
        )
        instance = (
            external_instance(form, traits.cpp, move=external_move)
            if scope_name == "external"
            else None
        )
        extras = derive_extra_expectations(
            form, scope_name, expectation, traits
        )
        registration_moves = (
            instance.moves if instance is not None else form.external_moves
        )
        for container in CONTAINERS:
            if scope_name in RUNTIME_ONLY_SCOPES and container == "static":
                continue
            registration_line, static_source = _registration(
                form.name,
                scope_name,
                container,
                external_argument=(
                    instance.argument if instance is not None else ""
                ),
                leaf=traits.cpp,
            )
            copies, moves = REQUEST_COSTS[expectation.request_class]
            if scope_name == "external":
                moves += registration_moves
            common = {
                "scope": scope_name,
                "form": form.name,
                "container": container,
                "storage": form.storage(traits.cpp),
                "instance": instance.declaration if instance else "",
                "registration": registration_line,
                "static_source": static_source,
            }
            rows.append(
                _row(
                    f"{scope_name}_{form.name}_{container}",
                    request=expectation.request,
                    relation=expectation.relation,
                    constructions=expectation.constructions,
                    request_class=expectation.request_class,
                    copies=copies,
                    moves=moves,
                    leaf=expectation.leaf,
                    array_element=expectation.array_element,
                    move_out=expectation.move_out,
                    delete_result=expectation.delete_result,
                    **common,
                )
            )
            for case, extra in extras:
                extra_copies, extra_moves = REQUEST_COSTS[extra.request_class]
                if scope_name == "external":
                    extra_moves += registration_moves
                rows.append(
                    _row(
                        f"{scope_name}_{form.name}_{case}_{container}",
                        request=extra.request,
                        relation=extra.relation,
                        constructions=extra.constructions,
                        request_class=extra.request_class,
                        copies=extra_copies,
                        moves=extra_moves,
                        leaf=extra.leaf,
                        array_element=extra.array_element,
                        move_out=extra.move_out,
                        delete_result=extra.delete_result,
                        **common,
                    )
                )
    rows.extend(_interface_rows(traits))
    rows.extend(_annotation_rows(traits))
    return rows


def covered_forms(traits: LeafTraits = COPYABLE) -> set[tuple[str, str]]:
    rows = build_rows(traits)
    return {
        (str(row["scope"]), str(row["form"]))
        for row in rows
        if not row.get("interface")
    }


def supported_forms() -> set[tuple[str, str]]:
    return {
        (scope.name, form.name)
        for scope, form in iter_supported_forms()
    }


def _require(condition: object, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _check_unique(space: str, names) -> None:
    names = list(names)
    _require(len(names) == len(set(names)), f"{space} contains duplicate names")


def _check_case(space: str, name: str, request_class: str, scope: str) -> None:
    _require(
        request_class in REQUEST_COSTS,
        f"{space} {name} has unknown request class {request_class!r}",
    )
    _require(scope in SCOPES, f"{space} {name} has unknown scope {scope!r}")


def validate(traits: LeafTraits = COPYABLE) -> None:
    rows = build_rows(traits)
    _require(rows, "behavior catalog must not be empty")
    _check_unique("behavior catalog", (row["name"] for row in rows))
    expected = supported_forms()
    accounted = covered_forms() | set(BEHAVIOR_GAPS)
    _require(
        not expected - accounted,
        f"supported forms without a behavior case: {expected - accounted}",
    )
    _require(
        not accounted - expected,
        f"behavior cases for unsupported forms: {accounted - expected}",
    )
    validate_gaps()
    for form in FORMS:
        for scope in SCOPES:
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            if not supported_categories(scope, form.name):
                continue
            expectation = derive_expectation(form, scope, traits)
            _check_case("behavior form", f"{scope}/{form.name}", expectation.request_class, scope)
            for case, extra in derive_extra_expectations(
                form, scope, expectation, traits
            ):
                _check_case(
                    "behavior form",
                    f"{scope}/{form.name}/{case}",
                    extra.request_class,
                    scope,
                )
    _require(ANNOTATION_RECIPES, "annotation behavior catalog must not be empty")
    _check_unique("annotation catalog", (recipe.name for recipe in ANNOTATION_RECIPES))
    for recipe in ANNOTATION_RECIPES:
        _check_case("annotation case", recipe.name, recipe.request_class, recipe.scope)
        if recipe.scope == "external":
            _require(
                recipe.external_instance,
                f"external annotation case {recipe.name} needs an instance",
            )
    _require(
        all(form.external_moves >= 0 for form in FORMS),
        "behavior form has negative registration moves",
    )
    _require(INTERFACE_CASES, "interface behavior catalog must not be empty")
    _check_unique("interface catalog", (case.name for case in INTERFACE_CASES))
    for case in INTERFACE_CASES:
        _check_case("interface case", case.name, case.request_class, case.scope)
        _require(
            bool(case.other_request) == bool(case.other_relation),
            f"interface case {case.name} must declare both the secondary "
            "request and its relation",
        )


@dataclass(frozen=True)
class BehaviorShard:
    container: str
    scope: str
    form: str
    traits: str
    path: str
    suite: str
    namespace: str
    leaf: str
    move_only: bool
    name_suffix: str
    emit_annotation_tests: bool
    rows: tuple[dict[str, object], ...]


def build_shards() -> list[BehaviorShard]:
    grouped: dict[tuple[str, str, str, str], list[dict[str, object]]] = {}
    for traits in TRAITS:
        for row in build_rows(traits):
            key = (
                str(row["container"]),
                str(row["scope"]),
                str(row["form"]),
                traits.name,
            )
            grouped.setdefault(key, []).append(row)
    shards = []
    annotation_emitted: set[str] = set()
    for (container, scope, form, traits_name), rows in sorted(grouped.items()):
        prefix = CasePath(
            phase="resolution",
            container=container,
            scope=scope,
            storage=form,
            category="behavior",
            traits=traits_name,
        )
        traits = next(item for item in TRAITS if item.name == traits_name)
        suite = f"{prefix.suite}_{traits_name}"
        shards.append(
            BehaviorShard(
                container=container,
                scope=scope,
                form=form,
                traits=traits_name,
                path=f"{prefix.file[:-4]}/{traits_name}.cpp",
                suite=suite,
                namespace=suite,
                leaf=traits.cpp,
                move_only=not traits.copyable,
                name_suffix="" if traits.copyable else f"_{traits.name}",
                emit_annotation_tests=(
                    traits_name not in annotation_emitted
                    and container == "runtime"
                ),
                rows=tuple(rows),
            )
        )
        if shards[-1].emit_annotation_tests:
            annotation_emitted.add(traits_name)
    return shards


def render(shard: BehaviorShard) -> str:
    return render_template(
        "behavior_cases.cpp.j2",
        rows=shard.rows,
        namespace=shard.namespace,
        suite=shard.suite,
        leaf=shard.leaf,
        move_only=shard.move_only,
        name_suffix=shard.name_suffix,
        emit_annotation_tests=shard.emit_annotation_tests,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    validate(COPYABLE)
    validate(MOVE_ONLY)
    for shard in build_shards():
        emit(
            arguments.out_dir / shard.path,
            render(shard),
            check=arguments.check,
        )


if __name__ == "__main__":
    main()
