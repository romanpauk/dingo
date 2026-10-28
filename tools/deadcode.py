#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Find type declarations in the headers that nothing references.

A header-only template library cannot use the linker or the compiler's unused
warnings to find dead declarations: templates are only emitted when instantiated,
and a member aliases or an enum has no storage at all. The cheapest reliable
signal is a reference count over the source tree.

Every ``struct``, ``class``, ``enum``, ``union`` and ``using`` declaration is
counted, matching the name with the ``_t`` and ``_v`` suffixes that alias
templates conventionally use. A declaration with no reference outside its own
declaration is reported as dead. A declaration referenced only from ``test/`` is
reported separately: it is either a test-only helper promoted into the public
headers or an API with no user.

This is a report, not a gate. A hit needs a reading: an exception type that
users may catch, for example, is not dead merely because the library does not
throw it.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path

DECLARATION = re.compile(
    r"^\s*(?:template\s*<[^;{}]*>\s*)?"
    r"(?:struct|class|enum(?:\s+class)?|union|using)\s+([A-Za-z_]\w*)",
    re.MULTILINE,
)
SOURCE_ROOTS = ("include", "examples", "test")
SUFFIXES = ("", "_t", "_v")


@dataclass(frozen=True)
class Finding:
    path: str
    name: str
    references: int


def source_texts(repository: Path) -> dict[Path, str]:
    texts: dict[Path, str] = {}
    for root in SOURCE_ROOTS:
        base = repository / root
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.suffix not in {".h", ".hpp", ".cpp"}:
                continue
            try:
                texts[path] = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
    return texts


def reference_count(name: str, texts: dict[Path, str], *, exclude: Path) -> int:
    pattern = re.compile(
        r"\b" + re.escape(name) + r"(?:" + "|".join(SUFFIXES[1:]) + r")?\b"
    )
    return sum(
        len(pattern.findall(text)) for path, text in texts.items() if path != exclude
    )


def repository_of(root: Path) -> Path:
    """The directory that owns the ``include``, ``examples`` and ``test`` trees."""
    for candidate in root.parents:
        if (candidate / "include").is_dir():
            return candidate
    return root.parent


def scan(root: Path) -> tuple[list[Finding], list[Finding]]:
    repository = repository_of(root)
    texts = source_texts(repository)
    dead: list[Finding] = []
    test_only: list[Finding] = []
    for header in sorted(root.rglob("*.h")):
        text = texts.get(header)
        if text is None:
            continue
        for name in sorted(set(DECLARATION.findall(text))):
            if len(name) < 5:
                continue
            pattern = re.compile(
                r"\b" + re.escape(name) + r"(?:" + "|".join(SUFFIXES[1:]) + r")?\b"
            )
            own = len(pattern.findall(text))
            outside = reference_count(name, texts, exclude=header)
            if outside == 0 and own <= 1:
                dead.append(
                    Finding(header.as_posix(), name, 0)
                )
                continue
            if outside and own <= 1 and all(
                path.relative_to(repository).parts[0] == "test"
                for path in texts
                if path != header and pattern.search(texts[path])
            ):
                test_only.append(
                    Finding(header.as_posix(), name, outside)
                )
    return dead, test_only


def render(dead: list[Finding], test_only: list[Finding]) -> str:
    lines = [f"## Unreferenced declarations ({len(dead)})", ""]
    if dead:
        lines += ["| header | declaration |", "| --- | --- |"]
        for finding in dead:
            path = finding.path.replace("include/dingo/", "")
            lines.append(f"| `{path}` | `{finding.name}` |")
    lines += ["", f"## Referenced only by tests ({len(test_only)})", ""]
    if test_only:
        lines += ["| header | declaration | references |", "| --- | --- | ---: |"]
        for finding in test_only:
            path = finding.path.replace("include/dingo/", "")
            lines.append(f"| `{path}` | `{finding.name}` | {finding.references} |")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("include/dingo"))
    parser.add_argument("--all", action="store_true", help="include test-only rows")
    arguments = parser.parse_args()

    dead, test_only = scan(arguments.root)
    print(render(dead, test_only if arguments.all else []), end="")


if __name__ == "__main__":
    main()
