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
import json
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
ENTRY_HEADERS = ("container.h", "runtime_container.h", "static_container.h")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
FUNCTION = re.compile(
    r"^(?:([A-Za-z_][\w:<>,\s\*&]*?)\b(\w+)\s*\([^;{}]*\)\s*(?:const\s*)?)\{",
    re.MULTILINE,
)


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


def declaration_bodies(text: str) -> dict[str, str]:
    """Each declared name mapped to its signature and body.

    The body is taken to the matching closing brace, or to the next semicolon
    for an alias. This is a lexical approximation: braces inside strings or
    comments are not special-cased, which can only widen a body, never hide a
    reference.
    """
    bodies: dict[str, str] = {}
    for match in DECLARATION.finditer(text):
        name = match.group(1)
        start = match.start()
        brace = text.find("{", match.end())
        semicolon = text.find(";", match.end())
        if brace == -1 or (semicolon != -1 and semicolon < brace):
            body = text[start : semicolon + 1 if semicolon != -1 else len(text)]
            bodies[name] = bodies.get(name, "") + body
            continue
        depth = 0
        end = brace
        while end < len(text):
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
                if depth == 0:
                    end += 1
                    break
            end += 1
        bodies[name] = bodies.get(name, "") + text[start:end]
    return bodies


def _resolve(identifier: str, names: set[str]) -> str | None:
    if identifier in names:
        return identifier
    for suffix in SUFFIXES[1:]:
        if identifier.endswith(suffix) and identifier[: -len(suffix)] in names:
            return identifier[: -len(suffix)]
    return None


def function_bodies(text: str) -> dict[str, str]:
    """Namespace-scope function definitions, which the declaration scan misses.

    Members live inside a class body and are covered by that body. A free
    function starts at column zero in this codebase, which keeps the scan away
    from statement blocks and lambdas.
    """
    bodies: dict[str, str] = {}
    for match in FUNCTION.finditer(text):
        name = match.group(2)
        brace = text.find("{", match.end() - 1)
        depth = 0
        end = brace
        while end < len(text):
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
                if depth == 0:
                    end += 1
                    break
            end += 1
        bodies.setdefault(name, text[match.start() : end])
    return bodies


def _live_names(
    bodies: dict[str, str], names: set[str], texts: dict[Path, str],
    repository: Path,
) -> set[str]:
    live: set[str] = set()
    for path, text in texts.items():
        relative = path.relative_to(repository)
        if relative.parts[0] in {"test", "examples"} or (
            relative.parts[0] == "include" and path.name in ENTRY_HEADERS
        ):
            for identifier in IDENTIFIER.findall(text):
                resolved = _resolve(identifier, names)
                if resolved is not None:
                    live.add(resolved)

    pending = list(live)
    while pending:
        name = pending.pop()
        for identifier in IDENTIFIER.findall(bodies.get(name, "")):
            resolved = _resolve(identifier, names)
            if resolved is not None and resolved not in live:
                live.add(resolved)
                pending.append(resolved)
    return live


def unreachable(root: Path) -> list[Finding]:
    """Declarations not reachable from the tests, examples, or entry headers."""
    repository = repository_of(root)
    texts = source_texts(repository)

    declared: dict[str, Path] = {}
    bodies: dict[str, str] = {}
    for header in sorted(root.rglob("*.h")):
        text = texts.get(header)
        if text is None:
            continue
        found = declaration_bodies(text)
        found.update(function_bodies(text))
        for name, body in found.items():
            if len(name) < 5 or name.isupper():
                continue
            declared.setdefault(name, header)
            # A name can declare several specializations; every body is part of
            # what the name reaches.
            bodies[name] = bodies.get(name, "") + body
    names = set(declared)
    live = _live_names(bodies, names, texts, repository)
    return [
        Finding(declared[name].as_posix(), name, 0)
        for name in sorted(names - live)
    ]


def cross_check(dead: list[Finding], lsp_report: Path) -> list[str]:
    """Declarations the textual scan and clangd both report as unreferenced."""
    reported = {
        str(finding.path) + ":" + finding.name for finding in dead
    }
    agreed = []
    for item in json.loads(lsp_report.read_text(encoding="utf-8")):
        if item in reported:
            agreed.append(item)
    return sorted(agreed)


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
    parser.add_argument(
        "--reachable",
        action="store_true",
        help="report declarations unreachable from tests, examples, or entry headers",
    )
    parser.add_argument(
        "--lsp",
        type=Path,
        help="clangd report JSON; print only declarations both methods agree on",
    )
    parser.add_argument(
        "--check",
        type=Path,
        help="baseline to compare the agreed set against",
    )
    arguments = parser.parse_args()

    dead, test_only = scan(arguments.root)

    if arguments.lsp is not None:
        agreed = cross_check(dead, arguments.lsp)
        print(f"## Confirmed by both methods ({len(agreed)})\n")
        for item in agreed:
            print(f"- `{item}`")
        if arguments.check is not None:
            expected = json.loads(arguments.check.read_text(encoding="utf-8"))
            if sorted(agreed) != sorted(expected):
                added = sorted(set(agreed) - set(expected))
                removed = sorted(set(expected) - set(agreed))
                for item in added:
                    print(f"+ {item}", file=sys.stderr)
                for item in removed:
                    print(f"- {item}", file=sys.stderr)
                raise SystemExit("the agreed set changed; update the baseline")
        return

    print(render(dead, test_only if arguments.all else []), end="")
    if arguments.reachable:
        rows = unreachable(arguments.root)
        print(f"\n## Unreachable declarations ({len(rows)})\n")
        if rows:
            print("| header | declaration |")
            print("| --- | --- |")
            for finding in rows:
                path = finding.path.replace("include/dingo/", "")
                print(f"| `{path}` | `{finding.name}` |")


if __name__ == "__main__":
    main()
