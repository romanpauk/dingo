#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate behavior classes from the scope/form model.

The generator owns the product: every supported scope/form cell from the route
ledger becomes a behavior class in each applicable container front-end. The
primary request, relation, request class, and construction count are derived
from the semantic form kind, stored wrapper properties, and scope; a form adds
its registration facts and capabilities, not a hand-written expectation per
cell.

Behavior gaps are cells the ledger publishes but that have no resolvable
behavior case yet. They carry a reason and are checked by the coverage
contract, so a supported cell can never silently lose its behavior test.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.grammar.catalog import (
    FORM_BY_NAME,
    FORMS as CATALOG_FORMS,
    SCOPES as CATALOG_SCOPES,
    RUNTIME_ONLY_SCOPES,
    SCOPE_BY_NAME,
    StorageForm,
)
from tools.model.semantics.cells import iter_supported_cells
from tools.model.semantics.gaps import cell_gaps, name_gaps, validate as validate_gaps
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.semantics.routes.ledger import supported_categories
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar import cpp
from tools.model.grammar.shapes import WRAPPER_BY_NAME


SCOPES = tuple(scope.name for scope in CATALOG_SCOPES)
FORMS = tuple(form for form in CATALOG_FORMS if form.name != "array")
CONTAINERS = ("runtime", "static")
TRACKED = cpp.leaf("tracked")


@dataclass(frozen=True)
class Expectation:
    """The resolvable request, its identity relation, and its shape.

    Construction counts are storage-shape facts and stay explicit. Copy and
    move counts are derived from the request class and the scope, so a cell
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


def derive_expectation(form: StorageForm, scope: str) -> Expectation:
    """Derive the primary behavior request from the selected route shape.

    The request class, relation, constructions, and optional leaf checks all
    follow from the form's semantic kind, its stored wrapper properties, and the
    scope. Construction counts are a form fact: external storage constructs
    nothing, unique storage constructs two resolutions, and the other scopes
    construct one. Array-shaped forms hold two tracked elements.
    """
    kind = form.kind
    wrapper = form.wrapper
    constructions = _constructions(scope, kind)
    consumes = SCOPE_BY_NAME[scope].consumes
    sharing = wrapper is not None and wrapper.has("sharing")

    if kind == "value":
        if scope == "shared_cyclical":
            return Expectation(
                "tracked &", "&first == &second", constructions, "reference"
            )
        if consumes:
            return Expectation(
                "tracked",
                "first.id != second.id",
                constructions,
                "consume_value",
            )
        return Expectation(
            "tracked",
            "first.id == second.id",
            constructions,
            "borrow_value",
            leaf="external" if scope == "external" else "id",
        )

    if kind in {"variant", "optional"}:
        if consumes:
            if kind == "variant":
                return Expectation(
                    cpp.template("std::variant", TRACKED, cpp.leaf("long")).render(),
                    "std::get<0>(first).id != std::get<0>(second).id",
                    constructions,
                    "consume_value",
                )
            return Expectation(
                cpp.template("std::optional", TRACKED).render(),
                "first->id != second->id",
                constructions,
                "consume_value",
            )
        return Expectation(
            "tracked",
            "first.id == second.id",
            constructions,
            "borrow_value",
            leaf="id",
        )

    if kind == "pointer":
        if consumes:
            return Expectation(
                cpp.pointer(TRACKED).render(),
                "first->id != second->id",
                constructions,
                "handle",
                delete_result="delete",
            )
        return Expectation(
            "tracked",
            "first.id == second.id",
            constructions,
            "borrow_value",
            leaf="id",
        )

    if kind == "array":
        if scope == "shared_cyclical":
            return Expectation(
                cpp.array_pointer(TRACKED, 2).render(),
                "first == second",
                constructions,
                "array",
                array_element="(*first)[0].id != (*first)[1].id",
            )
        return Expectation(
            cpp.pointer(TRACKED).render(),
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
                cpp.template("std::shared_ptr", TRACKED).render(),
                "first == second" if scope != "unique" else "first != second",
                constructions,
                "handle",
                leaf="" if consumes else "pointer",
            )
        return Expectation(
            cpp.template("std::unique_ptr", TRACKED).render(),
            "first->id != second->id",
            constructions,
            "handle",
        )

    if kind == "array_handle":
        if sharing:
            return Expectation(
                cpp.shared_array(TRACKED).render(),
                "first == second" if scope != "unique" else "first != second",
                constructions,
                "array",
            )
        if consumes:
            return Expectation(
                cpp.unique_array(TRACKED).render(),
                "first[0].id != second[0].id",
                constructions,
                "array",
            )
        return Expectation(
            cpp.reference(cpp.unique_array(TRACKED)).render(),
            "&first == &second",
            constructions,
            "reference",
        )

    raise ValueError(f"no behavior expectation for {form.name}/{scope}")


# Additional request shapes for a cell that the primary expectation does not
# exercise: the unique rvalue route and the shared wrapper-value route.
def derive_extra_expectations(
    form: StorageForm, scope: str, primary: Expectation
) -> tuple[tuple[str, Expectation], ...]:
    """Additional route shapes worth executing for a cell.

    These are not alternate expectations: they exercise a second published
    route. The unique plain cell also moves its rvalue route; the shared
    alternative cell also copies the variant wrapper value.
    """
    kind = form.kind
    extras = []
    if kind == "value" and scope == "unique" and primary.request_class == "consume_value":
        extras.append(
            (
                "rvalue",
                Expectation(
                    "tracked &&",
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
                    cpp.template("std::variant", TRACKED, cpp.leaf("long")).render(),
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
        storage=form.storage("tracked"),
        factory=form.factory(),
        kind=kind,
        interfaces=interfaces,
        constructions=constructions,
    )


def derive_interface_shapes() -> tuple[InterfaceShape, ...]:
    return (
        _interface_shape("shared_handle"),
        _interface_shape("plain"),
        _interface_shape("unique_handle"),
        _interface_shape("shared_handle", twin=True),
        _interface_shape("unique_handle", twin=True),
        _interface_shape("shared_handle", copyable=True),
    )


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


def derive_interface_cases() -> tuple[InterfaceCase, ...]:
    cases = []
    for shape in INTERFACE_SHAPES:
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


def _interface_rows() -> list[dict[str, object]]:
    rows = []
    for case in INTERFACE_CASES:
        copies, moves = REQUEST_COSTS[case.request_class]
        for container in case.containers:
            registration, static_source = _registration(
                case.form_name,
                case.scope,
                container,
                case.interfaces,
                storage=case.storage,
                factory=case.factory,
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
) -> AnnotationShape:
    form = next(form for form in FORMS if form.name == form_name)
    return AnnotationShape(
        name=name,
        scope="shared",
        storage=form.storage("tracked"),
        factory=form.factory(),
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
) -> AnnotationShape:
    return AnnotationShape(
        name=name,
        scope="external",
        storage=storage,
        factory=None,
        leaf="tracked",
        forms=("ref",),
        form_name=form_name,
        append_form=False,
        external_instance=declaration,
        external_argument=argument,
        registration_moves=moves,
    )


def derive_annotation_shapes() -> tuple[AnnotationShape, ...]:
    return (
        _shared_annotation_shape("plain", "shared_annotated", forms=("ref", "value")),
        _shared_annotation_shape("shared_handle", "shared_annotated_shared_ptr"),
        _shared_annotation_shape("optional", "shared_annotated_optional"),
        _shared_annotation_shape("pointer", "shared_annotated_pointer"),
        _shared_annotation_shape("shared_handle", "shared_annotated", leaf="tracked_interface", forms=("interface_ref", "interface_ptr", "interface_handle", "interface_handle_ref")),
        _external_annotation_shape("external_annotated_value", cpp.storage(cpp.leaf("tracked")).render(), "tracked external_instance", "std::move(external_instance)", moves=1),
        _external_annotation_shape("external_annotated_ref", cpp.storage(cpp.reference(cpp.leaf("tracked"))).render(), "tracked external_instance", "external_instance"),
        _external_annotation_shape("external_annotated_ptr", cpp.storage(cpp.pointer(cpp.leaf("tracked"))).render(), "tracked external_instance", "std::addressof(external_instance)", form_name="pointer"),
        _external_annotation_shape("external_annotated_shared_ptr", cpp.storage(cpp.template("std::shared_ptr", cpp.leaf("tracked"))).render(), "auto external_instance = make_shared_tracked()", "external_instance", form_name="shared_handle"),
    )


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


def derive_annotation_recipes() -> tuple[AnnotationRecipe, ...]:
    recipes = []
    for shape in ANNOTATION_SHAPES:
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


def _annotation_rows() -> list[dict[str, object]]:
    rows = []
    for recipe in ANNOTATION_RECIPES:
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
                )
            else:
                registration, static_source = _registration(
                    recipe.form_name,
                    recipe.scope,
                    container,
                    interfaces,
                    storage=recipe.storage,
                    factory=recipe.factory,
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


# Supported cells the route model publishes but that have no resolvable
# behavior case yet. Each one must state why, so the contract distinguishes a
# missing test from a shape the library cannot actually serve.
BEHAVIOR_GAPS = cell_gaps("behavior")


def _registration(
    form_name: str,
    scope: str,
    container: str,
    interfaces: tuple[str, ...] = (),
    *,
    storage: str | None = None,
    factory: str | None = None,
    external_argument: str = "",
) -> tuple[str, str]:
    return registration(
        FORM_BY_NAME[form_name],
        scope,
        container,
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


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope, form in iter_supported_cells(form_major=True):
        scope_name = scope.name
        if (scope_name, form.name) in BEHAVIOR_GAPS:
            continue
        expectation = derive_expectation(form, scope_name)
        instance = (
            external_instance(form) if scope_name == "external" else None
        )
        extras = derive_extra_expectations(form, scope_name, expectation)
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
            )
            copies, moves = REQUEST_COSTS[expectation.request_class]
            if scope_name == "external":
                moves += form.external_moves
            common = {
                "scope": scope_name,
                "form": form.name,
                "container": container,
                "storage": form.storage("tracked"),
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
                    extra_moves += form.external_moves
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
    rows.extend(_interface_rows())
    rows.extend(_annotation_rows())
    return rows


def covered_cells() -> set[tuple[str, str]]:
    rows = build_rows()
    return {
        (str(row["scope"]), str(row["form"]))
        for row in rows
        if not row.get("interface")
    }


def supported_cells() -> set[tuple[str, str]]:
    return {
        (scope.name, form.name)
        for scope, form in iter_supported_cells()
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


def validate() -> None:
    rows = build_rows()
    _require(rows, "behavior catalog must not be empty")
    _check_unique("behavior catalog", (row["name"] for row in rows))
    expected = supported_cells()
    accounted = covered_cells() | set(BEHAVIOR_GAPS)
    _require(
        not expected - accounted,
        f"supported cells without a behavior case: {expected - accounted}",
    )
    _require(
        not accounted - expected,
        f"behavior cases for unsupported cells: {accounted - expected}",
    )
    validate_gaps()
    for form in FORMS:
        for scope in SCOPES:
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            if not supported_categories(scope, form.name):
                continue
            expectation = derive_expectation(form, scope)
            _check_case("behavior cell", f"{scope}/{form.name}", expectation.request_class, scope)
            for case, extra in derive_extra_expectations(form, scope, expectation):
                _check_case(
                    "behavior cell",
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


def render(output: Path) -> str:
    validate()
    return render_template("behavior_cells.cpp.j2", rows=build_rows())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    emit(arguments.out, render(arguments.out), check=arguments.check)


if __name__ == "__main__":
    main()
