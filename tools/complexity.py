#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Rank library headers by the signals of accreted complexity.

Size alone says little: a large header that encodes one contract is fine, while
a modest header that grew one specialization or one branch per case is the shape
that a fresh implementation would not reproduce.

The score combines four signals, all measured mechanically:

- churn: how many commits touched the file, from git;
- branches: how many ``if constexpr`` arms it holds;
- specializations: how many template specializations it declares;
- forwarders: how many single-statement bodies only forward to another call.

Dividing by fan-in keeps public contracts out of the top: a file included by
most of the library is a surface, not an accumulation, so the answer there is to
extract from it rather than to rewrite it.

``--clusters`` answers a different question: which files implement the same
thing. It compares the names each header declares (types, aliases, functions)
with TF-IDF weights, so ubiquitous names such as ``resolve`` or ``bind`` count
for little, and groups files that share specific vocabulary. Two headers can be
variants rather than equals, so the edge also accepts a high containment when
one file is largely a subset of the other.

This is a report, not a gate. It exists to order a review, not to fail a build.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path

BRANCH = re.compile(r"\bif constexpr\b")
SPECIALIZATION = re.compile(
    r"^\s*(?:template\s*<[^;{}]*>\s*)?(?:struct|class)\s+\w+\s*<",
    re.MULTILINE,
)
FORWARDER = re.compile(
    r"\{\s*return\s+[\w:<>,\s\.\[\]]+\([^;{}]*\);\s*\}", re.MULTILINE
)
INCLUDE = re.compile(r'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)
DECLARATION = re.compile(
    r"^\s*(?:template\s*<[^;{}]*>\s*)?"
    r"(?:struct|class|enum(?:\s+class)?|using|union)\s+([A-Za-z_]\w*)",
    re.MULTILINE,
)
DEFINITION = re.compile(
    r"^\s*(?:[A-Za-z_][\w:<>,&\*\s]*?)\b([a-z_]\w*)\s*\(", re.MULTILINE
)


@dataclass(frozen=True)
class Metrics:
    path: str
    lines: int
    churn: int
    fan_in: int
    branches: int
    specializations: int
    forwarders: int
    score: float


def churn(root: Path) -> dict[str, int]:
    """How many commits touched each tracked file under ``root``."""
    relative = root.as_posix()
    result = subprocess.run(
        ["git", "log", "--numstat", "--pretty=format:", "--", relative],
        capture_output=True,
        text=True,
        check=False,
    )
    counts: dict[str, int] = {}
    for line in result.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) != 3:
            continue
        counts[parts[2]] = counts.get(parts[2], 0) + 1
    return counts


SOURCE_ROOTS = ("include", "examples", "test", "tools")


def fan_in_index(root: Path) -> dict[str, int]:
    """How many translation units include each header, counted once per file."""
    repository = root.parents[1]
    counts: dict[str, int] = {}
    for source_root in SOURCE_ROOTS:
        base = repository / source_root
        if not base.is_dir():
            continue
        for candidate in base.rglob("*"):
            if candidate.suffix not in {".h", ".hpp", ".cpp"}:
                continue
            try:
                text = candidate.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            included = {Path(item).name for item in INCLUDE.findall(text)}
            for name in included:
                counts[name] = counts.get(name, 0) + 1
    return counts


def measure(
    path: Path,
    root: Path,
    churn_by_file: dict[str, int],
    fan_in_by_name: dict[str, int],
) -> Metrics:
    text = path.read_text(encoding="utf-8")
    name = path.name
    fan_in = fan_in_by_name.get(name, 0)
    branches = len(BRANCH.findall(text))
    specializations = len(SPECIALIZATION.findall(text))
    forwarders = len(FORWARDER.findall(text))
    lines = text.count("\n")
    try:
        key = path.relative_to(root.parents[1]).as_posix()
    except ValueError:
        key = path.as_posix()
    touched = churn_by_file.get(key, 0)
    score = touched * (branches + specializations / 4 + forwarders) / max(fan_in, 1)
    return Metrics(
        path=key,
        lines=lines,
        churn=touched,
        fan_in=fan_in,
        branches=branches,
        specializations=specializations,
        forwarders=forwarders,
        score=round(score, 1),
    )


def collect(root: Path) -> list[Metrics]:
    churn_by_file = churn(root)
    fan_in_by_name = fan_in_index(root)
    rows = [
        measure(path, root, churn_by_file, fan_in_by_name)
        for path in sorted(root.rglob("*.h"))
    ]
    return sorted(rows, key=lambda row: row.score, reverse=True)


def declared_names(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    return set(DECLARATION.findall(text)) | set(DEFINITION.findall(text))


def cluster_headers(root: Path, threshold: float = 0.28) -> list[dict[str, object]]:
    """Group headers that declare the same vocabulary."""
    names = {}
    for path in sorted(root.rglob("*.h")):
        declared = declared_names(path)
        if len(declared) >= 6:
            names[path.as_posix()] = declared
    if not names:
        return []

    document_frequency: dict[str, int] = {}
    for declared in names.values():
        for word in declared:
            document_frequency[word] = document_frequency.get(word, 0) + 1
    total = len(names)
    weight = {
        word: math.log((total + 1) / (count + 1)) + 1
        for word, count in document_frequency.items()
    }

    def vector(declared: set[str]) -> dict[str, float]:
        raw = {word: weight[word] for word in declared}
        norm = math.sqrt(sum(value * value for value in raw.values())) or 1.0
        return {word: value / norm for word, value in raw.items()}

    vectors = {name: vector(declared) for name, declared in names.items()}
    edges: list[tuple[float, str, str]] = []
    ordered = sorted(vectors)
    for index, left in enumerate(ordered):
        for right in ordered[index + 1 :]:
            a, b = vectors[left], vectors[right]
            if len(a) > len(b):
                a, b = b, a
            cosine = sum(value * b.get(word, 0.0) for word, value in a.items())
            shared = names[left] & names[right]
            containment = len(shared) / max(min(len(names[left]), len(names[right])), 1)
            if cosine >= threshold or (containment >= 0.6 and len(shared) >= 6):
                edges.append((round(max(cosine, containment), 2), left, right))
    edges.sort(reverse=True)

    parent = {name: name for name in names}

    def find(name: str) -> str:
        while parent[name] != name:
            parent[name] = parent[parent[name]]
            name = parent[name]
        return name

    for _, left, right in edges:
        root_left, root_right = find(left), find(right)
        if root_left != root_right:
            parent[root_right] = root_left

    groups: dict[str, list[str]] = {}
    for name in names:
        groups.setdefault(find(name), []).append(name)

    clusters = []
    for members in groups.values():
        if len(members) < 2:
            continue
        shared = set.intersection(*(names[member] for member in members))
        specific = sorted(shared, key=lambda word: -weight[word])[:8]
        clusters.append(
            {
                "members": sorted(members),
                "shared": specific,
                "edges": [edge for edge in edges if edge[1] in members],
            }
        )
    clusters.sort(key=lambda cluster: -len(cluster["members"]))
    return clusters


def nearest_neighbours(root: Path, paths: list[str], limit: int = 3) -> list[str]:
    """The closest headers by declared vocabulary, for files the clusters miss."""
    names = {}
    for path in sorted(root.rglob("*.h")):
        declared = declared_names(path)
        if len(declared) >= 6:
            names[path.as_posix()] = declared
    if not names:
        return []
    document_frequency: dict[str, int] = {}
    for declared in names.values():
        for word in declared:
            document_frequency[word] = document_frequency.get(word, 0) + 1
    total = len(names)
    weight = {
        word: math.log((total + 1) / (count + 1)) + 1
        for word, count in document_frequency.items()
    }

    def vector(declared: set[str]) -> dict[str, float]:
        raw = {word: weight[word] for word in declared}
        norm = math.sqrt(sum(value * value for value in raw.values())) or 1.0
        return {word: value / norm for word, value in raw.items()}

    vectors = {name: vector(declared) for name, declared in names.items()}
    lines = []
    for path in paths:
        if path not in vectors:
            continue
        scored = []
        for other, other_vector in vectors.items():
            if other == path:
                continue
            a, b = vectors[path], other_vector
            if len(a) > len(b):
                a, b = b, a
            scored.append((sum(value * b.get(word, 0.0) for word, value in a.items()), other))
        scored.sort(reverse=True)
        closest = ", ".join(
            f"{score:.2f} `{other.replace(root.as_posix() + '/', '')}`"
            for score, other in scored[:limit]
        )
        lines.append(f"- `{path.replace(root.as_posix() + '/', '')}`: {closest}")
    return lines


def render_clusters(clusters: list[dict[str, object]]) -> str:
    lines = []
    for cluster in clusters:
        members = cluster["members"]
        lines.append(f"### {len(members)} files")
        for member in members:
            lines.append(f"- `{member}`")
        if cluster["shared"]:
            lines.append(f"  shared: {', '.join(cluster['shared'])}")
        strongest = max(cluster["edges"], key=lambda edge: edge[0])
        lines.append(f"  strongest: {strongest[0]} {strongest[1]} <-> {strongest[2]}")
        lines.append("")
    return "\n".join(lines)


def render(rows: list[Metrics], top: int) -> str:
    lines = [
        "| score | churn | branches | specs | forwarders | lines | fan-in | file |",
        "| ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for row in rows[:top]:
        lines.append(
            f"| {row.score} | {row.churn} | {row.branches} | "
            f"{row.specializations} | {row.forwarders} | {row.lines} | "
            f"{row.fan_in} | `{row.path}` |"
        )
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path("include/dingo"),
        help="header tree to measure (default: include/dingo)",
    )
    parser.add_argument("--top", type=int, default=20)
    parser.add_argument(
        "--clusters",
        action="store_true",
        help="group headers that declare the same vocabulary",
    )
    parser.add_argument(
        "--neighbours",
        action="store_true",
        help="print the closest headers for the highest-scoring files",
    )
    parser.add_argument("--json", type=Path)
    arguments = parser.parse_args()

    rows = collect(arguments.root)
    if arguments.clusters:
        print(render_clusters(cluster_headers(arguments.root)), end="")
    else:
        print(render(rows, arguments.top), end="")

    if arguments.neighbours:
        print("## closest headers", end="\n\n")
        top_paths = [row.path for row in rows[: arguments.top]]
        for line in nearest_neighbours(arguments.root, top_paths):
            print(line)
    if arguments.json is not None:
        arguments.json.write_text(
            json.dumps([asdict(row) for row in rows], indent=2, sort_keys=True)
            + "\n",
            encoding="utf-8",
        )


if __name__ == "__main__":
    main()
