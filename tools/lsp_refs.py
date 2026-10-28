#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Report declarations that no code references, using clangd.

The claim is deliberately narrow, so it can be trusted:

- the inventory comes from clangd's hierarchical document symbols, not from a
  lexical scan;
- references come from clangd's symbol index;
- members are attributed to the class that contains them, and specialisations
  to their primary template, so a class is never reported because one of its
  members happens to have no direct reference;
- only namespace-scope declarations are reported.

A declaration is reported when it, and every declaration grouped with it, has
no reference other than itself. That is a lower bound on dead code: it says
"nothing references this", not "nothing can reach this". Template instantiation
is not a reference, so a template used only through an alias stays out of the
report only if the alias is referenced.

``--check`` compares the result with the committed baseline, so a new or a
disappeared entry fails the target instead of passing silently.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote, unquote

NAMESPACE = 3
ENTRY_HEADERS = ("container.h", "runtime_container.h", "static_container.h")
BASELINE = Path("tools/lsp_refs_baseline.json")


@dataclass(frozen=True)
class Symbol:
    file: Path
    name: str
    start: tuple[int, int]
    end: tuple[int, int]
    select: tuple[int, int]

    @property
    def key(self) -> tuple[str, int, int]:
        return (self.file.as_posix(), self.start[0], self.start[1])

    @property
    def base_name(self) -> str:
        return self.name.split("<", 1)[0].strip()


class Client:
    """A minimal JSON-RPC client over clangd's stdio transport."""

    def __init__(self, executable: str, root: Path, compile_commands: Path):
        self.process = subprocess.Popen(
            [
                executable,
                "--background-index",
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
            {"jsonrpc": "2.0", "id": request_id, "method": method, "params": params}
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
    return "file://" + quote(str(path.resolve()))


def path_of(text: str) -> Path:
    return Path(unquote(text.removeprefix("file://")))


def flatten(
    file: Path, items: list[dict[str, object]], out: list[Symbol]
) -> None:
    for item in items:
        if int(item.get("kind", 0)) == NAMESPACE:
            flatten(file, item.get("children") or [], out)
            continue
        selection = item.get("selectionRange", item["range"])
        out.append(
            Symbol(
                file,
                str(item.get("name", "")),
                (int(item["range"]["start"]["line"]), int(item["range"]["start"]["character"])),
                (int(item["range"]["end"]["line"]), int(item["range"]["end"]["character"])),
                (
                    int(selection["start"]["line"]),
                    int(selection["start"]["character"]),
                ),
            )
        )
        flatten(file, item.get("children") or [], out)


def containing(symbols: list[Symbol], symbol: Symbol) -> Symbol | None:
    best: Symbol | None = None
    for candidate in symbols:
        if candidate.key == symbol.key:
            continue
        if candidate.start <= symbol.select <= candidate.end:
            if best is None or (
                candidate.start >= best.start and candidate.end <= best.end
            ):
                best = candidate
    return best


def reference_count(
    client: Client, symbol: Symbol
) -> int | None:
    response = client.request(
        "textDocument/references",
        {
            "textDocument": {"uri": uri(symbol.file)},
            "position": {
                "line": symbol.select[0],
                "character": symbol.select[1],
            },
            "context": {"includeDeclaration": True},
        },
        timeout=60,
    )
    if response is None or "result" not in response:
        return None
    result = response["result"]
    if not isinstance(result, list):
        return None
    own = uri(symbol.file)
    return sum(
        1
        for item in result
        if not (
            item.get("uri") == own
            and (item.get("range") or {}).get("start") == {
                "line": symbol.select[0],
                "character": symbol.select[1],
            }
        )
    )


def scan(root: Path, wait: float) -> tuple[list[str], list[str]]:
    executable = shutil.which("clangd")
    if executable is None:
        raise RuntimeError("clangd is not installed")
    repository = root.resolve().parents[1]
    client = Client(executable, repository, (repository / "build").resolve())
    try:
        response = client.request(
            "initialize",
            {
                "processId": None,
                "rootUri": uri(repository),
                "capabilities": {
                    "textDocument": {
                        "documentSymbol": {"hierarchicalDocumentSymbolSupport": True}
                    }
                },
            },
        )
        if response is None:
            raise RuntimeError("clangd did not answer initialize")
        client.notify("initialized", {})

        headers = sorted(root.rglob("*.h"))
        for header in headers:
            client.open(header)

        # The library's users are the tests and the examples. Without them the
        # server never sees a construct used only from a translation unit.
        for directory in ("test", "examples"):
            base = repository / directory
            if not base.is_dir():
                continue
            for source in sorted(base.rglob("*.cpp")):
                if "generated" in source.parts:
                    continue
                client.open(source)
        time.sleep(wait)

        symbols_by_file: dict[Path, list[Symbol]] = {}
        for header in headers:
            response = client.request(
                "textDocument/documentSymbol",
                {"textDocument": {"uri": uri(header)}},
                timeout=60,
            )
            if response is None or "result" not in response:
                continue
            rows: list[Symbol] = []
            flatten(header.resolve(), response["result"], rows)
            symbols_by_file[header.resolve()] = rows

        # Group siblings that differ only by template arguments: a
        # specialisation and its primary are one declaration.
        # Group by name across the whole project: a use of a specialisation
        # counts as a use of the primary, and the primary is what a reader
        # would delete.
        groups: dict[str, list[Symbol]] = {}
        containers: dict[tuple[str, int, int], tuple[str, int, int]] = {}
        for file, rows in symbols_by_file.items():
            for symbol in rows:
                groups.setdefault(symbol.base_name, []).append(symbol)
                parent = containing(rows, symbol)
                if parent is not None:
                    containers[symbol.key] = parent.key

        # A member is not reported on its own, and neither is a declaration
        # whose group is referenced.
        unreferenced: list[str] = []
        unknown: list[str] = []
        for name, members in sorted(groups.items()):
            if any(symbol.key in containers for symbol in members):
                continue
            counts = [reference_count(client, symbol) for symbol in members]
            if any(count is None for count in counts):
                unknown.append(name)
                continue
            if max(counts) == 0:
                primary = next(
                    (symbol for symbol in members if symbol.name == name), members[0]
                )
                unreferenced.append(
                    f"{primary.file.relative_to(repository)}:{name}"
                )
        return unreferenced, unknown
    finally:
        client.close()


def render(unreferenced: list[str], unknown: list[str]) -> str:
    lines = [f"## Unreferenced declarations ({len(unreferenced)})", ""]
    for item in unreferenced:
        path, _, name = item.partition(":")
        lines.append(f"- `{path}` `{name}`")
    if unknown:
        lines += ["", f"{len(unknown)} declarations could not be queried:"]
        for item in unknown:
            lines.append(f"- {item}")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("include/dingo"))
    parser.add_argument("--wait", type=float, default=15.0)
    parser.add_argument("--json", type=Path)
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail when the result differs from the committed baseline",
    )
    arguments = parser.parse_args()

    unreferenced, unknown = scan(arguments.root, arguments.wait)
    print(render(unreferenced, unknown), end="")
    if arguments.json is not None:
        arguments.json.write_text(
            json.dumps(unreferenced, indent=2) + "\n", encoding="utf-8"
        )

    if arguments.check:
        if unknown:
            raise SystemExit(
                f"{len(unknown)} declarations could not be queried; "
                "the report is incomplete"
            )
        if not BASELINE.exists():
            raise SystemExit(f"missing baseline: {BASELINE}")
        expected = json.loads(BASELINE.read_text(encoding="utf-8"))
        added = sorted(set(unreferenced) - set(expected))
        removed = sorted(set(expected) - set(unreferenced))
        if added or removed:
            for item in added:
                print(f"+ {item}", file=sys.stderr)
            for item in removed:
                print(f"- {item}", file=sys.stderr)
            raise SystemExit("the unreferenced set changed; update the baseline")


if __name__ == "__main__":
    main()
