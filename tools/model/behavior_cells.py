#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate behavior classes from the scope/form model.

The generator owns the product: every supported scope/form cell from the route
ledger becomes a behavior class in each applicable container front-end. Per-cell
expectations are data, so a new form or scope is one table entry plus a
registration description instead of a hand-written test.

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
from storage_cells import validate_catalog


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


EXPECTATIONS = {
    ("plain", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, "borrow_value", leaf="id"
    ),
    ("plain", "unique"): Expectation(
        "tracked", "first.id != second.id", 2, "consume_value"
    ),
    ("plain", "external"): Expectation(
        "tracked",
        "first.id == second.id",
        0,
        "borrow_value",
        leaf="external",
    ),
    ("plain", "shared_cyclical"): Expectation(
        "tracked &", "&first == &second", 1, "reference"
    ),
    ("alternative", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, "borrow_value", leaf="id"
    ),
    ("alternative", "unique"): Expectation(
        "variant_t",
        "std::get<0>(first).id != std::get<0>(second).id",
        2,
        "consume_value",
    ),
    ("alternative", "external"): Expectation(
        "tracked", "first.id == second.id", 0, "borrow_value", leaf="id"
    ),
    ("pointer", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, "borrow_value", leaf="id"
    ),
    # A consumed raw pointer transfers ownership: the caller deletes it.
    ("pointer", "unique"): Expectation(
        "tracked *",
        "first->id != second->id",
        2,
        "handle",
        delete_result="delete",
    ),
    ("pointer", "external"): Expectation(
        "tracked", "first.id == second.id", 0, "borrow_value", leaf="id"
    ),
    ("bounded_array", "shared"): Expectation(
        "tracked *",
        "first[0].id == second[0].id",
        2,
        "array",
        array_element="first[0].id != first[1].id",
    ),
    # A consumed raw row pointer transfers ownership: the caller deletes it.
    ("bounded_array", "unique"): Expectation(
        "tracked *",
        "first[0].id != second[0].id",
        4,
        "array",
        array_element="first[0].id != first[1].id",
        delete_result="delete[]",
    ),
    ("bounded_array", "external"): Expectation(
        "tracked *",
        "first[0].id == second[0].id",
        0,
        "array",
        array_element="first[0].id != first[1].id",
    ),
    ("bounded_array", "shared_cyclical"): Expectation(
        "tracked (*)[2]",
        "first == second",
        2,
        "array",
        array_element="(*first)[0].id != (*first)[1].id",
    ),
    ("unique_handle", "unique"): Expectation(
        "std::unique_ptr<tracked>", "first->id != second->id", 2, "handle"
    ),
    ("unique_array_handle", "shared"): Expectation(
        "std::unique_ptr<tracked[]>&", "&first == &second", 2, "reference"
    ),
    ("unique_array_handle", "unique"): Expectation(
        "std::unique_ptr<tracked[]>",
        "first[0].id != second[0].id",
        4,
        "array",
    ),
    ("shared_handle", "shared"): Expectation(
        "std::shared_ptr<tracked>", "first == second", 1, "handle", leaf="pointer"
    ),
    ("shared_handle", "unique"): Expectation(
        "std::shared_ptr<tracked>", "first != second", 2, "handle"
    ),
    ("shared_handle", "external"): Expectation(
        "std::shared_ptr<tracked>", "first == second", 0, "handle", leaf="pointer"
    ),
    ("shared_handle", "shared_cyclical"): Expectation(
        "std::shared_ptr<tracked>", "first == second", 1, "handle", leaf="pointer"
    ),
    ("shared_array_handle", "shared"): Expectation(
        "std::shared_ptr<tracked[]>", "first == second", 2, "array"
    ),
    ("shared_array_handle", "unique"): Expectation(
        "std::shared_ptr<tracked[]>", "first != second", 4, "array"
    ),
    ("shared_array_handle", "external"): Expectation(
        "std::shared_ptr<tracked[]>", "first == second", 0, "array"
    ),
    ("optional", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, "borrow_value", leaf="id"
    ),
    ("optional", "unique"): Expectation(
        "std::optional<tracked>", "first->id != second->id", 2, "consume_value"
    ),
    ("optional", "external"): Expectation(
        "tracked", "first.id == second.id", 0, "borrow_value", leaf="id"
    ),
}

# Additional request shapes for a cell that the primary expectation does not
# exercise: the unique rvalue route and the shared wrapper-value route.
EXTRA_EXPECTATIONS = {
    ("plain", "unique"): (
        (
            "rvalue",
            Expectation(
                "tracked &&",
                "first.id != second.id",
                2,
                "consume_rvalue",
                move_out=True,
            ),
        ),
    ),
    ("alternative", "shared"): (
        (
            "wrapper",
            Expectation(
                "variant_t",
                "std::get<0>(first).id == std::get<0>(second).id",
                1,
                "borrow_value",
            ),
        ),
    ),
}


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


INTERFACE_CASES = (
    InterfaceCase(
        name="shared_ref",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        interfaces=("tracked_interface",),
        request="tracked_interface &",
        relation="&first == &second",
        constructions=1,
        request_class="reference",
    ),
    InterfaceCase(
        name="shared_ptr",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        interfaces=("tracked_interface",),
        request="tracked_interface *",
        relation="first == second",
        constructions=1,
        request_class="reference",
    ),
    InterfaceCase(
        name="shared_handle",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        interfaces=("tracked_interface",),
        request="std::shared_ptr<tracked_interface>",
        relation="first == second",
        constructions=1,
        request_class="handle",
    ),
    InterfaceCase(
        name="shared_plain_ref",
        scope="shared",
        storage="dingo::storage<tracked>",
        factory=None,
        interfaces=("tracked_interface",),
        request="tracked_interface &",
        relation="&first == &second",
        constructions=1,
        request_class="reference",
    ),
    InterfaceCase(
        name="unique_handle",
        scope="unique",
        storage="dingo::storage<std::unique_ptr<tracked>>",
        factory="make_unique_tracked",
        interfaces=("tracked_interface",),
        request="std::unique_ptr<tracked_interface>",
        relation="first->value() != second->value()",
        constructions=2,
        request_class="handle",
    ),
    InterfaceCase(
        name="unique_shared_handle",
        scope="unique",
        storage="dingo::storage<std::unique_ptr<tracked>>",
        factory="make_unique_tracked",
        interfaces=("tracked_interface",),
        request="std::shared_ptr<tracked_interface>",
        relation="first != second",
        constructions=2,
        request_class="handle",
    ),
    InterfaceCase(
        name="shared_second_ref",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        interfaces=("tracked_interface", "tracked_interface2"),
        request="tracked_interface &",
        relation="&first == &second",
        constructions=1,
        request_class="reference",
        other_request="tracked_interface2 &",
        other_relation="first.value() == other.value()",
    ),
    InterfaceCase(
        name="shared_second_handle",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<tracked>>",
        factory="make_shared_tracked",
        interfaces=("tracked_interface", "tracked_interface2"),
        request="std::shared_ptr<tracked_interface2>",
        relation="first == second",
        constructions=1,
        request_class="handle",
        other_request="std::shared_ptr<tracked_interface>",
        other_relation="first->value() == other->value()",
    ),
    InterfaceCase(
        name="shared_copyable_value",
        scope="shared",
        storage="dingo::storage<std::shared_ptr<copyable_implementation>>",
        factory="make_copyable_implementation",
        interfaces=("copyable_interface",),
        request="copyable_interface",
        relation="first.value.id == second.value.id",
        constructions=1,
        request_class="borrow_value",
    ),
    InterfaceCase(
        name="unique_second_handle",
        scope="unique",
        storage="dingo::storage<std::unique_ptr<tracked>>",
        factory="make_unique_tracked",
        interfaces=("tracked_interface", "tracked_interface2"),
        request="std::unique_ptr<tracked_interface2>",
        relation="first->value() != second->value()",
        constructions=3,
        request_class="handle",
        other_request="std::unique_ptr<tracked_interface>",
        other_relation="first->value() != other->value()",
    ),
)


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
                    "interface": True,
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
            expectation = EXPECTATIONS.get((form.name, scope))
            if expectation is None:
                if (scope, form.name) not in BEHAVIOR_GAPS:
                    raise ValueError(
                        f"unsupported behavior cell {scope}/{form.name} has no "
                        "expectation and no declared behavior gap"
                    )
                continue
            instance = EXTERNAL_INSTANCES.get(form.name) if scope == "external" else None
            extras = EXTRA_EXPECTATIONS.get((form.name, scope), ())
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
                        }
                    )
    rows.extend(_interface_rows())
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
    for (form_name, scope), expectation in EXPECTATIONS.items():
        if expectation.request_class not in REQUEST_COSTS:
            raise ValueError(
                f"behavior cell {scope}/{form_name} has unknown request class "
                f"{expectation.request_class!r}"
            )
    for (form_name, scope), extras in EXTRA_EXPECTATIONS.items():
        for case, extra in extras:
            if extra.request_class not in REQUEST_COSTS:
                raise ValueError(
                    f"behavior cell {scope}/{form_name}/{case} has unknown "
                    f"request class {extra.request_class!r}"
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
