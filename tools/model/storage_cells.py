#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the exhaustive storage-cell coverage probes.

The model declares the scope policies and the storage forms independently of
the C++ headers. The generator turns the full product into one translation
unit. A missing cell, a changed exposure, or a form that stops publishing a
request list therefore fails to compile instead of silently dropping out of the
matrix.

This is intentionally a coverage generator, not a code multiplier: it emits one
assertion per cell and one consumer to instantiate them.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined


VALUE = 1
LVALUE = 2
RVALUE = 4
POINTER = 8

# Storage forms the library declares but does not implement. The raw dynamic
# array `T[]` has no element count, so no scope can size, construct, or destroy
# it. It stays in the catalog so the form enum and the model stay in sync, but
# no scope exposes it.
UNAVAILABLE_FORMS = frozenset({"array"})


@dataclass(frozen=True)
class Scope:
    name: str
    cpp_type: str
    header: str
    stable: bool
    consumes: bool
    guard: str
    retains_unresolved_source: bool
    allows_cycles: bool
    exposures: dict[str, int]


@dataclass(frozen=True)
class Form:
    name: str
    cpp_type: str
    headers: tuple[str, ...]
    has_value: bool
    has_lvalue: bool
    has_rvalue_reference: bool
    has_pointer: bool


SCOPES = (
    Scope(
        name="shared",
        cpp_type="dingo::shared",
        header="dingo/storage/shared.h",
        stable=True,
        consumes=False,
        guard="while_unresolved",
        retains_unresolved_source=True,
        allows_cycles=False,
        exposures={},
    ),
    Scope(
        name="unique",
        cpp_type="dingo::unique",
        header="dingo/storage/unique.h",
        stable=False,
        consumes=True,
        guard="always",
        retains_unresolved_source=False,
        allows_cycles=False,
        exposures={
            "plain": LVALUE | RVALUE,
            "pointer": RVALUE | POINTER,
            "array": 0,
            "bounded_array": RVALUE | POINTER,
            "alternative": RVALUE,
            "unique_handle": RVALUE,
            "unique_array_handle": RVALUE,
            "shared_handle": RVALUE,
            "shared_array_handle": RVALUE,
            "optional": RVALUE,
        },
    ),
    Scope(
        name="external",
        cpp_type="dingo::external",
        header="dingo/storage/external.h",
        stable=True,
        consumes=False,
        guard="none",
        retains_unresolved_source=False,
        allows_cycles=False,
        exposures={},
    ),
    Scope(
        name="shared_cyclical",
        cpp_type="dingo::shared_cyclical",
        header="dingo/storage/shared_cyclical.h",
        stable=False,
        consumes=False,
        guard="none",
        retains_unresolved_source=False,
        allows_cycles=True,
        exposures={
            "plain": LVALUE | POINTER,
            "alternative": LVALUE | POINTER,
            "pointer": LVALUE | POINTER,
            "array": 0,
            "bounded_array": LVALUE | POINTER,
            "shared_handle": VALUE | LVALUE | POINTER,
            "shared_array_handle": VALUE | LVALUE | POINTER,
            "unique_handle": 0,
            "unique_array_handle": 0,
            "optional": 0,
        },
    ),
)

FORMS = (
    Form(
        name="plain",
        cpp_type="int",
        headers=(),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="alternative",
        cpp_type="std::variant<int, long>",
        headers=("variant",),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="pointer",
        cpp_type="int *",
        headers=(),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="array",
        cpp_type="int[]",
        headers=(),
        has_value=False,
        has_lvalue=False,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="bounded_array",
        cpp_type="int[2]",
        headers=(),
        has_value=False,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="unique_handle",
        cpp_type="std::unique_ptr<int>",
        headers=("memory",),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="unique_array_handle",
        cpp_type="std::unique_ptr<int[]>",
        headers=("memory",),
        has_value=False,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="shared_handle",
        cpp_type="std::shared_ptr<int>",
        headers=("memory",),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="shared_array_handle",
        cpp_type="std::shared_ptr<int[]>",
        headers=("memory",),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
    Form(
        name="optional",
        cpp_type="std::optional<int>",
        headers=("optional",),
        has_value=True,
        has_lvalue=True,
        has_rvalue_reference=True,
        has_pointer=True,
    ),
)

FORM_ENUM = ("unknown", *(form.name for form in FORMS))


def exposure(scope: Scope, form: Form) -> int:
    """Return the exposure of one scope/form cell.

    A form the library does not implement is never exposed, whatever the scope
    table says. A scope with no explicit table exposes every other form it can
    store for the borrowed categories. Scopes with an explicit table must name
    every form, including the forms they reject with zero.
    """
    if form.name in UNAVAILABLE_FORMS:
        return 0
    if not scope.exposures:
        return VALUE | LVALUE | POINTER
    try:
        return scope.exposures[form.name]
    except KeyError as error:
        raise ValueError(
            f"scope {scope.name!r} does not declare form {form.name!r}"
        ) from error


def scope_exposures(scope: Scope) -> list[int]:
    rows = []
    for form_name in FORM_ENUM:
        if form_name == "unknown":
            rows.append(0)
            continue
        form = next(form for form in FORMS if form.name == form_name)
        rows.append(exposure(scope, form))
    return rows


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope in SCOPES:
        for form in FORMS:
            bits = exposure(scope, form)
            enabled = bits != 0
            if scope.name == "shared_cyclical":
                # shared_cyclical publishes its own cells: the leaf and a
                # sharing handle, independent of the generic form lists.
                has_value = form.name in {
                    "shared_handle",
                    "shared_array_handle",
                }
                has_lvalue = enabled
                has_rvalue = False
                has_pointer = enabled
            else:
                has_value = bool(bits & VALUE) and form.has_value
                has_lvalue = bool(bits & LVALUE) and form.has_lvalue
                has_rvalue = bool(bits & RVALUE) and form.has_rvalue_reference
                has_pointer = bool(bits & POINTER) and form.has_pointer
            rows.append(
                {
                    "scope_name": scope.name,
                    "scope_type": scope.cpp_type,
                    "scope_header": scope.header,
                    "scope_stable": scope.stable,
                    "scope_consumes": scope.consumes,
                    "scope_guard": scope.guard,
                    "scope_retains": scope.retains_unresolved_source,
                    "scope_allows_cycles": scope.allows_cycles,
                    "form_name": form.name,
                    "form_type": form.cpp_type,
                    "form_headers": form.headers,
                    "exposure": bits,
                    "enabled": enabled,
                    "value": has_value,
                    "lvalue": has_lvalue,
                    "rvalue": has_rvalue,
                    "pointer": has_pointer,
                }
            )
    return rows


def validate() -> None:
    scope_names = [scope.name for scope in SCOPES]
    form_names = [form.name for form in FORMS]
    if len(scope_names) != len(set(scope_names)):
        raise ValueError("duplicate scope name")
    if len(form_names) != len(set(form_names)):
        raise ValueError("duplicate form name")
    if not scope_names or not form_names:
        raise ValueError("scope and form catalogs must not be empty")
    for scope in SCOPES:
        if scope.exposures and set(scope.exposures) != set(form_names):
            raise ValueError(
                f"scope {scope.name!r} must name every form: "
                f"missing {sorted(set(form_names) - set(scope.exposures))}, "
                f"extra {sorted(set(scope.exposures) - set(form_names))}"
            )
    build_rows()


def _strip_comments(text: str) -> str:
    return re.sub(r"//[^\n]*", "", text)


def catalog_names(include_directory: Path) -> tuple[set[str], set[str]]:
    """Read the library's built-in scope and form names.

    The model is the source of coverage, but the library owns the declaration.
    Comparing them here makes a new built-in scope or form fail the check when
    the model has not been extended, instead of silently leaving the new cell
    untested.
    """
    storage_root = include_directory / "dingo" / "storage"
    policy_header = storage_root / "storage_scope_policy.h"
    policy_text = _strip_comments(policy_header.read_text(encoding="utf-8"))
    form_match = re.search(
        r"enum\s+class\s+storage_form\s*\{(.*?)\};",
        policy_text,
        re.DOTALL,
    )
    if form_match is None:
        raise ValueError(f"storage_form enum not found in {policy_header}")
    library_forms = {
        name.strip()
        for name in form_match.group(1).split(",")
        if name.strip() and name.strip() != "unknown"
    }

    library_scopes = set()
    for header in sorted(storage_root.glob("*.h")):
        text = _strip_comments(header.read_text(encoding="utf-8"))
        for match in re.finditer(
            r"storage_scope_policy\s*<\s*([A-Za-z_]\w*)\s*>", text
        ):
            name = match.group(1)
            if name != "StorageTag":
                library_scopes.add(name)

    return library_scopes, library_forms


def validate_catalog(include_directory: Path) -> None:
    validate()
    library_scopes, library_forms = catalog_names(include_directory)
    model_scopes = {scope.name for scope in SCOPES}
    model_forms = {form.name for form in FORMS}
    if library_scopes != model_scopes:
        raise ValueError(
            "scope catalog is out of date: "
            f"missing {sorted(library_scopes - model_scopes)}, "
            f"stale {sorted(model_scopes - library_scopes)}"
        )
    if library_forms != model_forms:
        raise ValueError(
            "form catalog is out of date: "
            f"missing {sorted(library_forms - model_forms)}, "
            f"stale {sorted(model_forms - library_forms)}"
        )


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("storage_cells.cpp.j2")
    headers = sorted(
        {scope.header for scope in SCOPES}
        | {header for form in FORMS for header in form.headers}
    )
    return template.render(
        rows=build_rows(),
        scopes=SCOPES,
        forms=FORMS,
        headers=headers,
        value=VALUE,
        lvalue=LVALUE,
        rvalue=RVALUE,
        pointer=POINTER,
    )


def render_header(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("scopes.gen.h.j2")
    return template.render(
        scopes=[
            {
                "name": scope.name,
                "stable": scope.stable,
                "consumes": scope.consumes,
                "guard": scope.guard,
                "retains": scope.retains_unresolved_source,
                "allows_cycles": scope.allows_cycles,
                "exposures": scope_exposures(scope),
            }
            for scope in SCOPES
        ],
        shared_cyclical_lists=scope_exposures(
            next(scope for scope in SCOPES if scope.name == "shared_cyclical")
        ),
    )


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def format_cpp(content: str, executable: str | None) -> str:
    if executable is None:
        return content
    result = subprocess.run(
        [executable, "--style=file"],
        input=content,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(f"clang-format failed: {result.stderr}")
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--header",
        type=Path,
        help="committed scope-policy header to generate or check",
    )
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument(
        "--clang-format",
        default=shutil.which("clang-format"),
        help="clang-format executable used for the committed header",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    content = render(arguments.out)
    header_content = (
        format_cpp(render_header(arguments.header), arguments.clang_format)
        if arguments.header is not None
        else None
    )
    if arguments.check:
        if not arguments.out.exists() or arguments.out.read_text(
            encoding="utf-8"
        ) != content:
            raise SystemExit(f"{arguments.out} is out of date")
        if arguments.header is not None and (
            not arguments.header.exists()
            or arguments.header.read_text(encoding="utf-8") != header_content
        ):
            raise SystemExit(f"{arguments.header} is out of date")
        return
    write_text_if_changed(arguments.out, content)
    if arguments.header is not None and header_content is not None:
        write_text_if_changed(arguments.header, header_content)


if __name__ == "__main__":
    main()
