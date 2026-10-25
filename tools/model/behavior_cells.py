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


FORMS = (
    Form(name="plain", storage="dingo::storage<tracked>"),
    Form(name="alternative", storage="dingo::storage<variant_t>"),
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
    """The resolvable request and its expected identity and lifetime counts."""

    request: str
    relation: str
    constructions: int
    leaf: str = ""
    copies: int | None = None
    moves: int | None = None
    array_element: str = ""
    move_out: bool = False


EXPECTATIONS = {
    ("plain", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, leaf="id", copies=2, moves=0
    ),
    ("plain", "unique"): Expectation(
        "tracked", "first.id != second.id", 2, copies=0, moves=2
    ),
    ("plain", "external"): Expectation(
        "tracked",
        "first.id == second.id",
        0,
        leaf="external",
        copies=2,
        moves=0,
    ),
    ("plain", "shared_cyclical"): Expectation(
        "tracked &", "&first == &second", 1, copies=0, moves=0
    ),
    ("alternative", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, leaf="id", copies=2, moves=0
    ),
    ("alternative", "unique"): Expectation(
        "variant_t",
        "std::get<0>(first).id != std::get<0>(second).id",
        2,
        copies=0,
        moves=2,
    ),
    ("alternative", "external"): Expectation(
        "tracked",
        "first.id == second.id",
        0,
        leaf="id",
        copies=2,
        moves=1,
    ),
    ("pointer", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, leaf="id", copies=2, moves=0
    ),
    ("pointer", "external"): Expectation(
        "tracked", "first.id == second.id", 0, leaf="id", copies=2, moves=0
    ),
    ("bounded_array", "shared"): Expectation(
        "tracked *",
        "first[0].id == second[0].id",
        2,
        copies=0,
        moves=0,
        array_element="first[0].id != first[1].id",
    ),
    ("bounded_array", "external"): Expectation(
        "tracked *",
        "first[0].id == second[0].id",
        0,
        copies=0,
        moves=0,
        array_element="first[0].id != first[1].id",
    ),
    ("bounded_array", "shared_cyclical"): Expectation(
        "tracked (*)[2]",
        "first == second",
        2,
        copies=0,
        moves=0,
        array_element="(*first)[0].id != (*first)[1].id",
    ),
    ("unique_handle", "unique"): Expectation(
        "std::unique_ptr<tracked>", "first->id != second->id", 2, copies=0, moves=0
    ),
    ("unique_array_handle", "shared"): Expectation(
        "std::unique_ptr<tracked[]>&", "&first == &second", 2, copies=0, moves=0
    ),
    ("unique_array_handle", "unique"): Expectation(
        "std::unique_ptr<tracked[]>",
        "first[0].id != second[0].id",
        4,
        copies=0,
        moves=0,
    ),
    ("shared_handle", "shared"): Expectation(
        "std::shared_ptr<tracked>",
        "first == second",
        1,
        leaf="pointer",
        copies=0,
        moves=0,
    ),
    ("shared_handle", "unique"): Expectation(
        "std::shared_ptr<tracked>", "first != second", 2, copies=0, moves=0
    ),
    ("shared_handle", "external"): Expectation(
        "std::shared_ptr<tracked>",
        "first == second",
        0,
        leaf="pointer",
        copies=0,
        moves=0,
    ),
    ("shared_handle", "shared_cyclical"): Expectation(
        "std::shared_ptr<tracked>",
        "first == second",
        1,
        leaf="pointer",
        copies=0,
        moves=0,
    ),
    ("shared_array_handle", "shared"): Expectation(
        "std::shared_ptr<tracked[]>", "first == second", 2, copies=0, moves=0
    ),
    ("shared_array_handle", "unique"): Expectation(
        "std::shared_ptr<tracked[]>", "first != second", 4, copies=0, moves=0
    ),
    ("shared_array_handle", "external"): Expectation(
        "std::shared_ptr<tracked[]>", "first == second", 0, copies=0, moves=0
    ),
    ("optional", "shared"): Expectation(
        "tracked", "first.id == second.id", 1, leaf="id", copies=2, moves=0
    ),
    ("optional", "unique"): Expectation(
        "std::optional<tracked>", "first->id != second->id", 2, copies=0, moves=2
    ),
    ("optional", "external"): Expectation(
        "tracked", "first.id == second.id", 0, leaf="id", copies=2, moves=1
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
                copies=0,
                moves=4,
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
                copies=2,
                moves=0,
            ),
        ),
    ),
}


# Supported cells the route model publishes but that have no resolvable
# behavior case yet. Each one must state why, so the contract distinguishes a
# missing test from a shape the library cannot actually serve.
BEHAVIOR_GAPS = {
    ("shared", "unique_handle"): (
        "the published unique-handle value route is not convertible"
    ),
    ("unique", "pointer"): (
        "the consumed raw pointer has no ownership contract, so the behavior "
        "case would leak"
    ),
    ("unique", "bounded_array"): (
        "the consumed raw array pointer has no ownership contract, so the "
        "behavior case would leak"
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


def _form(name: str) -> Form:
    return next(form for form in FORMS if form.name == name)


def _registration(form: Form, scope: str, container: str) -> tuple[str, str]:
    storage = form.storage
    scope_type = f"dingo::scope<dingo::{scope}>"
    instance = EXTERNAL_INSTANCES.get(form.name)
    if scope == "external" and instance is not None:
        instance_storage = instance[1]
        argument = instance[2]
        return (
            f"container.register_type<{scope_type}, {instance_storage}>({argument});",
            "",
        )
    # A cyclical shared handle constructs its leaf in place, so its factory is
    # the leaf constructor rather than a handle factory.
    use_factory = form.factory is not None and not (
        scope == "shared_cyclical" and form.name == "shared_handle"
    )
    factory = (
        f", dingo::factory<dingo::function<&{form.factory}>>" if use_factory else ""
    )
    if container == "runtime":
        return (
            f"container.register_type<{scope_type}, {storage}{factory}>();",
            "",
        )
    source = f"dingo::bindings<dingo::bind<{scope_type}, {storage}{factory}>>"
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
                registration, static_source = _registration(form, scope, container)
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
                        "copies": expectation.copies,
                        "moves": expectation.moves,
                        "leaf": expectation.leaf,
                        "array_element": expectation.array_element,
                        "move_out": expectation.move_out,
                    }
                )
                for case, extra in extras:
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
                            "copies": extra.copies,
                            "moves": extra.moves,
                            "leaf": extra.leaf,
                            "array_element": extra.array_element,
                            "move_out": extra.move_out,
                        }
                    )
    return rows


def covered_cells() -> set[tuple[str, str]]:
    rows = build_rows()
    return {(str(row["scope"]), str(row["form"])) for row in rows}


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
