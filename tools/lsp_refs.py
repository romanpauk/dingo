#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Exact reference counts for header declarations, through clangd.

The lexical scanner in ``deadcode.py`` guesses references by matching names.
This reports the same question from clangd's symbol index: for every type
declaration in the headers, ask the language server for its references and count
them. A declaration with no reference beyond itself is unreachable by any other
code.

It needs clangd and a ``compile_commands.json``. Every header is opened, which
makes each one a translation unit, so a reference from one header to another is
visible without waiting for a project index.

Reference counts are per symbol, not per reachability: a template used only
through its ``_t`` alias has no direct references and is still alive. Read the
result together with the reachability report in ``deadcode.py``.

This is a report, not a gate.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote

DECLARATION_SPAN = re.compile(
    r"^\s*(?:template\s*<[^;{}]*>\s*)?"
    r"(?:struct|class|enum(?:\s*class)?|union|using)\s+([A-Za-z_]\w*)",
    re.MULTILINE,
)
FUNCTION_SPAN = re.compile(
    r"^(?:([A-Za-z_][\w:<>,\s\*&]*?)\b(\w+)\s*\([^;{}]*\)\s*(?:const\s*)?)\{",
    re.MULTILINE,
)
VARIABLE_SPAN = re.compile(
    r"^(?:template\s*<[^;{}]*>\s*)?(?:inline\s+|static\s+)*constexpr\s+"
    r"[\w:<>,&\*\s]+?\b([A-Za-z_]\w*)\s*(?:=|\{)",
    re.MULTILINE,
)
ENTRY_HEADERS = ("container.h", "runtime_container.h", "static_container.h")

DECLARATION = re.compile(
    r"^\s*(?:template\s*<[^;{}]*>\s*)?"
    r"(?:struct|class|enum(?:\s+class)?|union|using)\s+([A-Za-z_]\w*)",
    re.MULTILINE,
)


@dataclass(frozen=True)
class Declaration:
    path: Path
    name: str
    line: int
    character: int


class Client:
    """A minimal JSON-RPC client over clangd's stdio transport."""

    def __init__(self, executable: str, root: Path, compile_commands: Path):
        self.process = subprocess.Popen(
            [
                executable,
                "--background-index=1",
                f"--compile-commands-dir={compile_commands}",
                "--log=error",
            ],
            cwd=root,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        self.next_id = 0
        self.lock = threading.Lock()
        self.opened: set[Path] = set()

    def close(self) -> None:
        try:
            self.request("shutdown", {}, timeout=10)
            self.notify("exit", {})
        finally:
            self.process.terminate()

    def send(self, payload: dict[str, object]) -> None:
        body = json.dumps(payload).encode()
        assert self.process.stdin is not None
        self.process.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.process.stdin.flush()

    def notify(self, method: str, params: dict[str, object]) -> None:
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def request(
        self, method: str, params: dict[str, object], timeout: float = 120
    ) -> dict[str, object] | None:
        with self.lock:
            self.next_id += 1
            request_id = self.next_id
        self.send(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": method,
                "params": params,
            }
        )
        deadline = time.time() + timeout
        while time.time() < deadline:
            message = self.read()
            if message is None:
                return None
            if message.get("id") == request_id and "method" not in message:
                return message
        return None

    def read(self) -> dict[str, object] | None:
        assert self.process.stdout is not None
        headers: dict[str, str] = {}
        while True:
            line = self.process.stdout.readline()
            if not line:
                return None
            line = line.strip()
            if not line:
                break
            key, _, value = line.decode().partition(":")
            headers[key.strip().lower()] = value.strip()
        length = int(headers.get("content-length", 0))
        if not length:
            return None
        return json.loads(self.process.stdout.read(length))

    def open(self, path: Path) -> None:
        path = path.resolve()
        if path in self.opened:
            return
        self.opened.add(path)
        self.notify(
            "textDocument/didOpen",
            {
                "textDocument": {
                    "uri": uri(path),
                    "languageId": "cpp",
                    "version": 1,
                    "text": path.read_text(encoding="utf-8"),
                }
            },
        )


def uri(path: Path) -> str:
    return "file://" + quote(str(path))


@dataclass(frozen=True)
class Span:
    name: str
    start: int
    end: int


def spans(text: str) -> list[Span]:
    """Declaration and namespace-scope function bodies with their offsets."""
    rows: list[Span] = []
    for match in DECLARATION_SPAN.finditer(text):
        brace = text.find("{", match.end())
        semicolon = text.find(";", match.end())
        if brace == -1 or (semicolon != -1 and semicolon < brace):
            rows.append(
                Span(match.group(1), match.start(), semicolon + 1 if semicolon != -1 else len(text))
            )
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
        rows.append(Span(match.group(1), match.start(), end))
    for match in VARIABLE_SPAN.finditer(text):
        semicolon = text.find(";", match.end())
        rows.append(
            Span(match.group(1), match.start(), semicolon + 1 if semicolon != -1 else len(text))
        )
    for match in FUNCTION_SPAN.finditer(text):
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
        rows.append(Span(match.group(2), match.start(), end))
    return sorted(rows, key=lambda span: span.start)


def enclosing(spans_of_file: list[Span], offset: int) -> Span | None:
    """The innermost span containing the offset."""
    best: Span | None = None
    for span in spans_of_file:
        if span.start <= offset < span.end:
            if best is None or (span.start >= best.start and span.end <= best.end):
                best = span
    return best


def offset_of(text: str, line: int, character: int) -> int:
    start = 0
    for _ in range(line):
        start = text.index("\n", start) + 1
    return start + character


def declarations(root: Path) -> list[Declaration]:
    rows: list[Declaration] = []
    seen: set[tuple[Path, str]] = set()
    for header in sorted(root.rglob("*.h")):
        header = header.resolve()
        text = header.read_text(encoding="utf-8")
        for match in DECLARATION.finditer(text):
            name = match.group(1)
            if len(name) < 5 or name.isupper():
                continue
            start = match.start(1)
            line = text.count("\n", 0, start)
            character = start - (text.rfind("\n", 0, start) + 1)
            if (header, name) in seen:
                continue
            seen.add((header, name))
            rows.append(Declaration(header, name, line, character))
        for match in VARIABLE_SPAN.finditer(text):
            name = match.group(1)
            start = match.start(1)
            if (header, name) in seen:
                continue
            seen.add((header, name))
            rows.append(
                Declaration(
                    header,
                    name,
                    text.count("\n", 0, start),
                    start - (text.rfind("\n", 0, start) + 1),
                )
            )
    return rows


def references(
    client: Client, declaration: Declaration
) -> list[dict[str, object]] | None:
    response = client.request(
        "textDocument/references",
        {
            "textDocument": {"uri": uri(declaration.path)},
            "position": {
                "line": declaration.line,
                "character": declaration.character,
            },
            "context": {"includeDeclaration": True},
        },
        timeout=60,
    )
    if response is None or "result" not in response:
        return None
    result = response["result"]
    return result if isinstance(result, list) else None


def path_of(uri_text: str) -> Path:
    from urllib.parse import unquote

    return Path(unquote(uri_text.removeprefix("file://")))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("include/dingo"))
    parser.add_argument(
        "--compile-commands",
        type=Path,
        default=Path("build"),
        help="directory containing compile_commands.json",
    )
    parser.add_argument("--wait", type=float, default=30.0)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--show-live", action="store_true")
    arguments = parser.parse_args()

    executable = shutil.which("clangd")
    if executable is None:
        raise SystemExit("clangd is not installed")

    repository = arguments.root.resolve().parents[1]
    client = Client(executable, repository, arguments.compile_commands.resolve())
    response = client.request(
        "initialize",
        {"processId": None, "rootUri": uri(repository), "capabilities": {}},
    )
    if response is None:
        raise SystemExit("clangd did not answer initialize")
    client.notify("initialized", {})

    # Opening every header makes each one a translation unit, so a reference in
    # one header to another is visible to the server. The project index is not
    # required.
    for header in sorted(arguments.root.rglob("*.h")):
        client.open(header)
    time.sleep(arguments.wait)

    rows = declarations(arguments.root)
    if arguments.limit:
        rows = rows[: arguments.limit]

    unreferenced: list[Declaration] = []
    unknown = 0

    def is_root_file(path: Path) -> bool:
        try:
            relative = path.resolve().relative_to(repository)
        except ValueError:
            return False
        if relative.parts[0] in {"test", "examples"}:
            return True
        return relative.parts[0] == "include" and path.name in ENTRY_HEADERS

    for index, declaration in enumerate(rows, start=1):
        locations = references(client, declaration)
        if locations is None:
            unknown += 1
            continue
        if len(locations) <= 1:
            unreferenced.append(declaration)
        if index % 100 == 0:
            print(f"# {index}/{len(rows)}", file=sys.stderr)
    client.close()

    print(f"## Unreferenced by clangd ({len(unreferenced)} of {len(rows)})\n")
    if unreferenced:
        print("| header | declaration |")
        print("| --- | --- |")
        for declaration in unreferenced:
            try:
                path = str(declaration.path.relative_to(repository))
            except ValueError:
                path = declaration.path.as_posix()
            print(f"| `{path}` | `{declaration.name}` |")

    if unknown:
        print(f"\n{unknown} declarations could not be queried", file=sys.stderr)


if __name__ == "__main__":
    main()
