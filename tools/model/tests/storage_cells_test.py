#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

import pytest

from tools.model.emit.common.validation import validate, validate_catalog
from tools.model.emit.families.storage import (
    build_rows,
    render,
    render_header,
)
from tools.model.grammar.catalog import (
    FORM_BY_NAME,
    FORMS,
    SCOPE_BY_NAME,
    SCOPES,
)
from tools.model.semantics.forms import (
    LVALUE,
    POINTER,
    RVALUE,
    VALUE,
    exposure,
)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


def test_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert len(rows) == len(SCOPES) * len(FORMS)
    keys = {(row["scope_name"], row["form_name"]) for row in rows}
    assert len(keys) == len(rows)


def test_exposures_are_derived_for_every_scope_form_pair() -> None:
    for scope in SCOPES:
        for form in FORMS:
            assert exposure(scope, form) >= 0
    assert exposure(SCOPE_BY_NAME["shared"], FORM_BY_NAME["plain"]) == (
        VALUE | LVALUE | POINTER
    )
    assert exposure(SCOPE_BY_NAME["unique"], FORM_BY_NAME["alternative"]) == RVALUE
    assert (
        exposure(SCOPE_BY_NAME["shared_cyclical"], FORM_BY_NAME["unique_handle"])
        == 0
    )


def test_intentional_exclusions_are_explicit() -> None:
    rows = {
        (row["scope_name"], row["form_name"]): row for row in build_rows()
    }
    for scope in SCOPES:
        assert rows[(scope.name, "array")]["enabled"] is False
    for form in (
        "unique_handle",
        "unique_array_handle",
        "optional",
    ):
        assert rows[("shared_cyclical", form)]["enabled"] is False
    for form in ("alternative", "unique_handle", "optional"):
        assert rows[("unique", form)]["enabled"] is True
        assert rows[("unique", form)]["value"] is False


def test_scope_policies_are_named() -> None:
    scopes = {scope.name: scope for scope in SCOPES}
    assert scopes["unique"].consumes is True
    assert scopes["unique"].guard == "always"
    assert scopes["shared"].retains_unresolved_source is True
    assert scopes["external"].guard == "none"
    assert scopes["shared_cyclical"].stable is False
    assert scopes["shared_cyclical"].allows_cycles is True
    assert scopes["shared"].allows_cycles is False


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "storage_cells.cpp")
    assert content == render(tmp_path / "storage_cells.cpp")
    for row in build_rows():
        assert f"{row['scope_name']}_{row['form_name']}" in content
    assert content.count("TEST(storage_cells, ") == len(build_rows()) + 1


def test_generation_is_byte_stable(tmp_path: Path) -> None:
    path = tmp_path / "storage_cells.cpp"
    first = render(path)
    path.write_text(first, encoding="utf-8")
    assert render(path) == first


def test_scope_header_names_every_scope(tmp_path: Path) -> None:
    content = render_header(tmp_path / "scopes.gen.h")
    for scope in SCOPES:
        assert f"struct {scope.name};" in content
        assert f"storage_scope_policy<{scope.name}>" in content
    assert "materialization_guard::while_unresolved" in content
    assert "storage_exposure exposures[]" in content
    assert "shared_cyclical_cell_lists[]" in content


def test_catalog_forms_share_one_wrapper_declaration() -> None:
    from tools.model.grammar import cpp
    from tools.model.grammar.shapes import WRAPPER_BY_NAME

    for form in FORMS:
        if form.wrapper_name is not None:
            assert form.wrapper is WRAPPER_BY_NAME[form.wrapper_name]
        storage = form.storage_type("tracked")
        assert storage.kind is cpp.CppKind.TEMPLATE
        assert storage.name == "dingo::storage"
        assert form.cpp_type("int")


def test_recipe_derives_external_instances() -> None:
    from tools.model.emit.common.recipe import external_instance

    for form in FORMS:
        if form.name == "array":
            continue
        instance = external_instance(form)
        assert instance is not None
        if form.external_moves:
            assert instance.argument == "std::move(external_instance)"
        else:
            assert instance.argument == "external_instance"


def test_recipe_adds_a_registration_factory_only_when_declared() -> None:
    from tools.model.emit.common.recipe import registration

    for form in FORMS:
        runtime, _ = registration(form, "shared", "runtime")
        assert ("dingo::factory" in runtime) == (
            form.factory() is not None and form.registration_factory
        )


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
