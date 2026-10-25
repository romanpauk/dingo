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

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from route_ledger import supported_categories
from route_recipes import form_kind
from storage_cells import validate_catalog
from wrapper_semantics import FORM_WRAPPERS


SCOPES = ("shared", "unique", "external", "shared_cyclical")
CONTAINERS = ("runtime", "static")
RUNTIME_ONLY_SCOPES = frozenset({"external"})


@dataclass(frozen=True)
class Form:
    """How one storage form registers and how its leaf is reached."""

    name: str
    storage: str
    factory: str | None = None
    # External storage must receive its instance at registration time.
    instance: str | None = None
    instance_storage: str | None = None
    instance_argument: str | None = None
    # A move of a contained value is performed when external storage takes
    # ownership of the registered wrapper.
    registration_moves: int = 0
    interfaces: tuple[str, ...] = ()


FORMS = (
    Form(name="plain", storage="dingo::storage<tracked>"),
    Form(name="alternative", storage="dingo::storage<variant_t>", registration_moves=1),
    Form(name="pointer", storage="dingo::storage<tracked *>"),
    Form(name="bounded_array", storage="dingo::storage<tracked[2]>"),
    Form(
        name="unique_handle",
        storage="dingo::storage<std::unique_ptr<tracked>>",
        factory="make_unique_tracked",
    ),
    Form(
        name="unique_array_handle",
        storage="dingo::storage<std::unique_ptr<tracked[]>>",
        factory="make_unique_array_tracked",
    ),
    Form(
        name="shared_handle",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
    ),
    Form(
        name="shared_array_handle",
        storage="dingo::storage<std::shared_ptr<tracked[]>>",
        factory="make_shared_array_tracked",
    ),
    Form(
        name="optional",
        storage="dingo::storage<std::optional<tracked>>",
        factory="make_optional_tracked",
        registration_moves=1,
    ),
)

EXTERNAL_INSTANCES = {
    "plain": (
        "tracked external_instance",
        "dingo::storage<tracked &>",
        "external_instance",
    ),
    "alternative": (
        "auto external_instance = make_variant_tracked()",
        "dingo::storage<variant_t>",
        "std::move(external_instance)",
    ),
    "pointer": (
        "auto external_instance = make_pointer_tracked()",
        "dingo::storage<tracked *>",
        "external_instance",
    ),
    "bounded_array": (
        "tracked external_instance[2]",
        "dingo::storage<tracked[2]>",
        "external_instance",
    ),
    "unique_handle": (
        "auto external_instance = make_unique_tracked()",
        "dingo::storage<std::unique_ptr<tracked>>",
        "std::move(external_instance)",
    ),
    "unique_array_handle": (
        "auto external_instance = make_unique_array_tracked()",
        "dingo::storage<std::unique_ptr<tracked[]>>",
        "std::move(external_instance)",
    ),
    "shared_handle": (
        "auto external_instance = make_shared_tracked()",
        "dingo::storage<std::shared_ptr<tracked>>",
        "external_instance",
    ),
    "shared_array_handle": (
        "auto external_instance = make_shared_array_tracked()",
        "dingo::storage<std::shared_ptr<tracked[]>>",
        "external_instance",
    ),
    "optional": (
        "auto external_instance = make_optional_tracked()",
        "dingo::storage<std::optional<tracked>>",
        "std::move(external_instance)",
    ),
}


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


def derive_expectation(form: Form, scope: str) -> Expectation:
    """Derive the primary behavior request from the selected route shape.

    The request class, relation, constructions, and optional leaf checks all
    follow from the form's semantic kind, its stored wrapper properties, and the
    scope. Construction counts are a form fact: external storage constructs
    nothing, unique storage constructs two resolutions, and the other scopes
    construct one. Array-shaped forms hold two tracked elements.
    """
    kind = form_kind(form.name)
    stored = FORM_WRAPPERS.get(form.name)
    constructions = _constructions(scope, kind)
    elements = _elements(kind)

    if kind == "value":
        if scope == "shared_cyclical":
            return Expectation(
                "tracked &", "&first == &second", constructions, "reference"
            )
        if scope == "unique":
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

    if kind == "variant":
        if scope == "unique":
            return Expectation(
                "variant_t",
                "std::get<0>(first).id != std::get<0>(second).id",
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
        if scope == "unique":
            return Expectation(
                "tracked *",
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
                "tracked (*)[2]",
                "first == second",
                constructions,
                "array",
                array_element="(*first)[0].id != (*first)[1].id",
            )
        return Expectation(
            "tracked *",
            "first[0].id == second[0].id"
            if scope != "unique"
            else "first[0].id != second[0].id",
            constructions,
            "array",
            array_element="first[0].id != first[1].id",
            delete_result="delete[]" if scope == "unique" else "",
        )

    if kind == "handle":
        sharing = stored is not None and stored.has("sharing")
        if sharing:
            relation = "first == second" if scope != "unique" else "first != second"
            return Expectation(
                "std::shared_ptr<tracked>",
                relation,
                constructions,
                "handle",
                leaf="" if scope == "unique" else "pointer",
            )
        return Expectation(
            "std::unique_ptr<tracked>",
            "first->id != second->id",
            constructions,
            "handle",
        )

    if kind == "array_handle":
        sharing = stored is not None and stored.has("sharing")
        if sharing:
            relation = "first == second" if scope != "unique" else "first != second"
            return Expectation(
                "std::shared_ptr<tracked[]>",
                relation,
                constructions,
                "array",
            )
        if scope == "unique":
            return Expectation(
                "std::unique_ptr<tracked[]>",
                "first[0].id != second[0].id",
                constructions,
                "array",
            )
        return Expectation(
            "std::unique_ptr<tracked[]>&",
            "&first == &second",
            constructions,
            "reference",
        )

    if kind == "optional":
        if scope == "unique":
            return Expectation(
                "std::optional<tracked>",
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

    raise ValueError(f"no behavior expectation for {form.name}/{scope}")


# Additional request shapes for a cell that the primary expectation does not
# exercise: the unique rvalue route and the shared wrapper-value route.
def derive_extra_expectations(
    form: Form, scope: str, primary: Expectation
) -> tuple[tuple[str, Expectation], ...]:
    """Additional route shapes worth executing for a cell.

    These are not alternate expectations: they exercise a second published
    route. The unique plain cell also moves its rvalue route; the shared
    alternative cell also copies the variant wrapper value.
    """
    kind = form_kind(form.name)
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
                    "variant_t",
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
    scope: str
    storage: str
    factory: str | None
    kind: str
    interfaces: tuple[str, ...]
    constructions: int


INTERFACE_SHAPES = (
    InterfaceShape(
        name="shared",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        kind="shared_handle",
        interfaces=("tracked_interface",),
        constructions=1,
    ),
    InterfaceShape(
        name="shared_plain",
        scope="shared",
        storage="dingo::storage<tracked>",
        factory=None,
        kind="plain",
        interfaces=("tracked_interface",),
        constructions=1,
    ),
    InterfaceShape(
        name="unique",
        scope="unique",
        storage="dingo::storage<std::unique_ptr<tracked>>",
        factory="make_unique_tracked",
        kind="unique_handle",
        interfaces=("tracked_interface",),
        constructions=2,
    ),
    InterfaceShape(
        name="shared_second",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        kind="shared_handle",
        interfaces=("tracked_interface", "tracked_interface2"),
        constructions=1,
    ),
    InterfaceShape(
        name="unique_second",
        scope="unique",
        storage="dingo::storage<std::unique_ptr<tracked>>",
        factory="make_unique_tracked",
        kind="unique_handle",
        interfaces=("tracked_interface", "tracked_interface2"),
        constructions=3,
    ),
    InterfaceShape(
        name="shared_copyable",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<copyable_implementation>>",
        factory="make_copyable_implementation",
        kind="copyable",
        interfaces=("copyable_interface",),
        constructions=1,
    ),
)


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
        return f"std::shared_ptr<{primary}>", relation, "handle"
    if form == "unique_handle":
        return (
            f"std::unique_ptr<{primary}>",
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
                case.storage,
                case.factory,
                case.scope,
                container,
                case.interfaces,
            )
            rows.append(
                {
                    "name": f"{case.scope}_interface_{case.name}_{container}",
                    "scope": case.scope,
                    "form": case.name,
                    "container": container,
                    "scope_type": f"dingo::scope<dingo::{case.scope}>",
                    "storage": case.storage,
                    "instance": "",
                    "registration": registration,
                    "static_source": static_source,
                    "request": case.request,
                    "relation": case.relation,
                    "constructions": case.constructions,
                    "request_class": case.request_class,
                    "copies": copies,
                    "moves": moves,
                    "leaf": "",
                    "array_element": "",
                    "move_out": False,
                    "delete_result": "",
                    "other_request": case.other_request,
                    "other_relation": case.other_relation,
                    "bind_reference": case.bind_reference,
                    "interface": True,
                }
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
    append_form: bool = True
    external_instance: str = ""
    external_argument: str = ""
    registration_moves: int = 0


ANNOTATION_SHAPES = (
    AnnotationShape(
        name="shared_annotated",
        scope="shared",
        storage="dingo::storage<tracked>",
        factory=None,
        leaf="tracked",
        forms=("ref", "value"),
    ),
    AnnotationShape(
        name="shared_annotated_shared_ptr",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        leaf="tracked",
        forms=("ref",),
    ),
    AnnotationShape(
        name="shared_annotated_optional",
        scope="shared",
        storage="dingo::storage<std::optional<tracked>>",
        factory="make_optional_tracked",
        leaf="tracked",
        forms=("ref",),
    ),
    AnnotationShape(
        name="shared_annotated_pointer",
        scope="shared",
        storage="dingo::storage<tracked *>",
        factory="make_pointer_tracked",
        leaf="tracked",
        forms=("ref",),
    ),
    AnnotationShape(
        name="shared_annotated",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        leaf="tracked_interface",
        forms=("interface_ref", "interface_ptr", "interface_handle",
               "interface_handle_ref"),
    ),
    AnnotationShape(
        name="external_annotated_value",
        scope="external",
        storage="dingo::storage<tracked>",
        factory=None,
        leaf="tracked",
        forms=("ref",),
        append_form=False,
        external_instance="tracked external_instance",
        external_argument="std::move(external_instance)",
        registration_moves=1,
    ),
    AnnotationShape(
        name="external_annotated_ref",
        scope="external",
        storage="dingo::storage<tracked &>",
        factory=None,
        leaf="tracked",
        forms=("ref",),
        append_form=False,
        external_instance="tracked external_instance",
        external_argument="external_instance",
    ),
    AnnotationShape(
        name="external_annotated_ptr",
        scope="external",
        storage="dingo::storage<tracked *>",
        factory=None,
        leaf="tracked",
        forms=("ref",),
        append_form=False,
        external_instance="tracked external_instance",
        external_argument="std::addressof(external_instance)",
    ),
    AnnotationShape(
        name="external_annotated_shared_ptr",
        scope="external",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory=None,
        leaf="tracked",
        forms=("ref",),
        append_form=False,
        external_instance="auto external_instance = make_shared_tracked()",
        external_argument="external_instance",
    ),
)


def _annotation_form(
    shape: AnnotationShape, form: str
) -> tuple[str, str, str, bool]:
    if form == "ref" or form == "interface_ref":
        return (
            f"dingo::annotated<{shape.leaf} &, tag_a>",
            "&first == &second",
            "reference",
            True,
        )
    if form == "value":
        return (
            f"dingo::annotated<{shape.leaf}, tag_a>",
            "first.id == second.id",
            "borrow_value",
            False,
        )
    if form == "interface_ptr":
        return (
            f"dingo::annotated<{shape.leaf} *, tag_a>",
            "first == second",
            "reference",
            False,
        )
    if form == "interface_handle":
        return (
            f"dingo::annotated<std::shared_ptr<{shape.leaf}>, tag_a>",
            "first == second",
            "handle",
            False,
        )
    if form == "interface_handle_ref":
        return (
            f"dingo::annotated<std::shared_ptr<{shape.leaf}> &, tag_a>",
            "&first == &second",
            "reference",
            True,
        )
    raise ValueError(f"no annotation request for form {form}")


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
        interfaces = (f"dingo::annotated<{recipe.leaf}, tag_a>",)
        for container in containers:
            if recipe.scope == "external":
                registration = (
                    "container.register_type<dingo::scope<dingo::external>, "
                    f"{recipe.storage}, dingo::interfaces<"
                    f"dingo::annotated<{recipe.leaf}, tag_a>>>("
                    f"{recipe.external_argument});"
                )
                static_source = ""
            else:
                registration, static_source = _registration(
                    recipe.storage,
                    recipe.factory,
                    recipe.scope,
                    container,
                    interfaces,
                )
            rows.append(
                {
                    "name": f"{recipe.scope}_annotation_{recipe.name}_{container}",
                    "scope": recipe.scope,
                    "form": recipe.name,
                    "container": container,
                    "scope_type": f"dingo::scope<dingo::{recipe.scope}>",
                    "storage": recipe.storage,
                    "instance": recipe.external_instance,
                    "registration": registration,
                    "static_source": static_source,
                    "request": recipe.request,
                    "relation": recipe.relation,
                    "constructions": recipe.constructions,
                    "request_class": recipe.request_class,
                    "copies": copies,
                    "moves": moves,
                    "leaf": "",
                    "array_element": "",
                    "move_out": False,
                    "delete_result": "",
                    "other_request": "",
                    "other_relation": "",
                    "bind_reference": recipe.bind_reference,
                    "interface": True,
                    "annotation": True,
                }
            )
    return rows


# Supported cells the route model publishes but that have no resolvable
# behavior case yet. Each one must state why, so the contract distinguishes a
# missing test from a shape the library cannot actually serve.
BEHAVIOR_GAPS = {
    ("shared", "unique_handle"): (
        "the published unique-handle value route is not convertible"
    ),
    ("external", "unique_handle"): (
        "external storage publishes no resolvable unique-handle route"
    ),
    ("external", "unique_array_handle"): (
        "external storage publishes no resolvable array-handle route"
    ),
    ("shared_cyclical", "alternative"): (
        "the cyclical alternative cell needs the graph scenario family"
    ),
    ("shared_cyclical", "pointer"): (
        "the cyclical pointer cell needs the graph scenario family"
    ),
}


def _form_by_storage(storage: str) -> Form | None:
    return next((form for form in FORMS if form.storage == storage), None)


def _registration(
    storage: str,
    factory_name: str | None,
    scope: str,
    container: str,
    interfaces: tuple[str, ...] = (),
) -> tuple[str, str]:
    scope_type = f"dingo::scope<dingo::{scope}>"
    form = _form_by_storage(storage)
    instance = EXTERNAL_INSTANCES.get(form.name) if form is not None else None
    if scope == "external" and instance is not None:
        instance_storage = instance[1]
        argument = instance[2]
        return (
            f"container.register_type<{scope_type}, {instance_storage}>({argument});",
            "",
        )
    # A cyclical shared handle constructs its leaf in place, so its factory is
    # the leaf constructor rather than a handle factory.
    interface_args = (
        f", dingo::interfaces<{', '.join(interfaces)}>" if interfaces else ""
    )
    use_factory = factory_name is not None and not (
        scope == "shared_cyclical" and factory_name == "make_shared_tracked"
    )
    factory_args = (
        f", dingo::factory<dingo::function<&{factory_name}>>"
        if use_factory
        else ""
    )
    if container == "runtime":
        return (
            f"container.register_type<{scope_type}, {storage}{interface_args}"
            f"{factory_args}>();",
            "",
        )
    source = (
        f"dingo::bindings<dingo::bind<{scope_type}, {storage}"
        f"{interface_args}{factory_args}>>"
    )
    return "", f"using source = {source};"


def build_rows() -> list[dict[str, object]]:
    rows = []
    for form in FORMS:
        for scope in SCOPES:
            if not supported_categories(scope, form.name):
                continue
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            expectation = derive_expectation(form, scope)
            instance = EXTERNAL_INSTANCES.get(form.name) if scope == "external" else None
            extras = derive_extra_expectations(form, scope, expectation)
            for container in CONTAINERS:
                if scope in RUNTIME_ONLY_SCOPES and container == "static":
                    continue
                registration, static_source = _registration(
                    form.storage, form.factory, scope, container, form.interfaces
                )
                copies, moves = REQUEST_COSTS[expectation.request_class]
                if scope == "external":
                    moves += form.registration_moves
                rows.append(
                    {
                        "name": f"{scope}_{form.name}_{container}",
                        "scope": scope,
                        "form": form.name,
                        "container": container,
                        "scope_type": f"dingo::scope<dingo::{scope}>",
                        "storage": form.storage,
                        "instance": instance[0] if instance else "",
                        "registration": registration,
                        "static_source": static_source,
                        "request": expectation.request,
                        "relation": expectation.relation,
                        "constructions": expectation.constructions,
                        "request_class": expectation.request_class,
                        "copies": copies,
                        "moves": moves,
                        "leaf": expectation.leaf,
                        "array_element": expectation.array_element,
                        "move_out": expectation.move_out,
                        "delete_result": expectation.delete_result,
                        "other_request": "",
                        "other_relation": "",
                        "bind_reference": False,
                    }
                )
                for case, extra in extras:
                    extra_copies, extra_moves = REQUEST_COSTS[
                        extra.request_class
                    ]
                    if scope == "external":
                        extra_moves += form.registration_moves
                    rows.append(
                        {
                            "name": f"{scope}_{form.name}_{case}_{container}",
                            "scope": scope,
                            "form": form.name,
                            "container": container,
                            "scope_type": f"dingo::scope<dingo::{scope}>",
                            "storage": form.storage,
                            "instance": instance[0] if instance else "",
                            "registration": registration,
                            "static_source": static_source,
                            "request": extra.request,
                            "relation": extra.relation,
                            "constructions": extra.constructions,
                            "request_class": extra.request_class,
                            "copies": extra_copies,
                            "moves": extra_moves,
                            "leaf": extra.leaf,
                            "array_element": extra.array_element,
                            "move_out": extra.move_out,
                            "delete_result": extra.delete_result,
                            "other_request": "",
                            "other_relation": "",
                            "bind_reference": False,
                        }
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
    cells = set()
    for form in FORMS:
        for scope in SCOPES:
            if supported_categories(scope, form.name):
                cells.add((scope, form.name))
    return cells


def validate() -> None:
    rows = build_rows()
    if not rows:
        raise ValueError("behavior catalog must not be empty")
    if len({row["name"] for row in rows}) != len(rows):
        raise ValueError("behavior catalog contains duplicate cells")
    expected = supported_cells()
    accounted = covered_cells() | set(BEHAVIOR_GAPS)
    missing = expected - accounted
    stale = accounted - expected
    if missing:
        raise ValueError(f"supported cells without a behavior case: {missing}")
    if stale:
        raise ValueError(f"behavior cases for unsupported cells: {stale}")
    for cell, reason in BEHAVIOR_GAPS.items():
        if not reason:
            raise ValueError(f"behavior gap {cell} has no reason")
    for form in FORMS:
        for scope in SCOPES:
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            if not supported_categories(scope, form.name):
                continue
            expectation = derive_expectation(form, scope)
            if expectation.request_class not in REQUEST_COSTS:
                raise ValueError(
                    f"behavior cell {scope}/{form.name} has unknown request "
                    f"class {expectation.request_class!r}"
                )
    for form in FORMS:
        for scope in SCOPES:
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            if not supported_categories(scope, form.name):
                continue
            primary = derive_expectation(form, scope)
            for case, extra in derive_extra_expectations(form, scope, primary):
                if extra.request_class not in REQUEST_COSTS:
                    raise ValueError(
                        f"behavior cell {scope}/{form.name}/{case} has unknown "
                        f"request class {extra.request_class!r}"
                    )
    if not ANNOTATION_RECIPES:
        raise ValueError("annotation behavior catalog must not be empty")
    annotation_names = [recipe.name for recipe in ANNOTATION_RECIPES]
    if len(annotation_names) != len(set(annotation_names)):
        raise ValueError("annotation catalog contains duplicate names")
    for recipe in ANNOTATION_RECIPES:
        if recipe.request_class not in REQUEST_COSTS:
            raise ValueError(
                f"annotation case {recipe.name} has unknown request class "
                f"{recipe.request_class!r}"
            )
        if recipe.scope not in SCOPES:
            raise ValueError(
                f"annotation case {recipe.name} has unknown scope {recipe.scope!r}"
            )
        if recipe.scope == "external" and not recipe.external_instance:
            raise ValueError(
                f"external annotation case {recipe.name} needs an instance"
            )
    for form in FORMS:
        if form.registration_moves < 0:
            raise ValueError(
                f"behavior form {form.name} has negative registration moves"
            )
    if not INTERFACE_CASES:
        raise ValueError("interface behavior catalog must not be empty")
    interface_names = [case.name for case in INTERFACE_CASES]
    if len(interface_names) != len(set(interface_names)):
        raise ValueError("interface behavior catalog contains duplicate names")
    for case in INTERFACE_CASES:
        if case.request_class not in REQUEST_COSTS:
            raise ValueError(
                f"interface case {case.name} has unknown request class "
                f"{case.request_class!r}"
            )
        if case.scope not in SCOPES:
            raise ValueError(
                f"interface case {case.name} has unknown scope {case.scope!r}"
            )
        if bool(case.other_request) != bool(case.other_relation):
            raise ValueError(
                f"interface case {case.name} must declare both the secondary "
                "request and its relation"
            )


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("behavior_cells.cpp.j2")
    return template.render(rows=build_rows())


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


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
    content = render(arguments.out)
    if arguments.check:
        if not arguments.out.exists() or arguments.out.read_text(
            encoding="utf-8"
        ) != content:
            raise SystemExit(f"{arguments.out} is out of date")
        return
    write_text_if_changed(arguments.out, content)


if __name__ == "__main__":
    main()
