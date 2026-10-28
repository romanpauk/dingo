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


def declarations(root: Path) -> list[Declaration]:
    rows: list[Declaration] = []
    seen: set[tuple[Path, str]] = set()
    for header in sorted(root.rglob("*.h")):
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
    return rows


def references(client: Client, declaration: Declaration) -> int | None:
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
    return len(result) if isinstance(result, list) else None


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
    for index, declaration in enumerate(rows, start=1):
        count = references(client, declaration)
        if count is None:
            unknown += 1
        elif count <= 1:
            unreferenced.append(declaration)
            if arguments.show_live:
                print(f"0 {declaration.path}:{declaration.line} {declaration.name}")
        if index % 100 == 0:
            print(f"# {index}/{len(rows)}", file=sys.stderr)
    client.close()

    print(f"## Unreferenced by clangd ({len(unreferenced)} of {len(rows)})\n")
    if unreferenced:
        print("| header | declaration |")
        print("| --- | --- |")
        for declaration in unreferenced:
            path = declaration.path.as_posix()
            try:
                path = str(declaration.path.relative_to(repository))
            except ValueError:
                pass
            print(f"| `{path}` | `{declaration.name}` |")
    if unknown:
        print(f"\n{unknown} declarations could not be queried", file=sys.stderr)


if __name__ == "__main__":
    main()
