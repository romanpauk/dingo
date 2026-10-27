#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Shared rendering and output handling for the model families."""

from __future__ import annotations

from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined


def environment() -> Environment:
    return Environment(
        loader=FileSystemLoader(Path(__file__).parent.parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )


def render_template(name: str, **context: object) -> str:
    return environment().get_template(name).render(**context)


def trait_context(traits, base: str) -> dict[str, object]:
    """The template context that renames a family for one leaf trait."""
    suffix = "" if traits.copyable else f"_{traits.name}"
    return {
        "leaf": traits.cpp,
        "move_only": not traits.copyable,
        "namespace": f"{base}{suffix}",
        "suite": f"{base}{suffix}",
    }


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def emit(path: Path, content: str, *, check: bool) -> None:
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            raise SystemExit(f"{path} is out of date")
        return
    write_text_if_changed(path, content)
