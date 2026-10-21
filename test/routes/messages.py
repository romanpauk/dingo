#!/usr/bin/env python3
"""Compares the exception messages of the library with those of the baseline.

test/routes/extract.py records the message of every exception a request
throws. For every case that throws the same exception in both tables, this
script compares the messages of the library with those of the baseline, the
table extracted with --implementation baseline from the commit in
test/differential/baseline-commit. Every difference must be declared by a behavior change of the
spec whose `cases` match `messages:<case id>`, and every behavior change with a
`messages:` pattern must match a difference. The exit status is 0 when both
hold, 1 otherwise.

A compiler may spell one type with or without a default template argument,
depending on how the type was first spelled in the translation unit, so the
default deleter of std::unique_ptr is not compared.

Usage:
    messages.py [--baseline FILE] [--observed FILE] [--summary]
"""

import argparse
import collections
import fnmatch
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Importing the spec writes no bytecode into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))

import spec  # noqa: E402

ROUTES_DIR = PROJECT_ROOT / "build" / "routes"


def single_table(directory):
    tables = sorted(directory.glob("*.jsonl"))
    return tables[0] if len(tables) == 1 else None


def load(path):
    return {row["id"]: row for row in map(json.loads, path.read_text().splitlines())}


DEFAULT_ARGUMENTS = (", std::default_delete<",)


def without_default_arguments(message):
    """The message without the default template arguments of DEFAULT_ARGUMENTS,
    each through its matching closing bracket, and without the space the
    compiler puts between two closing brackets."""
    for argument in DEFAULT_ARGUMENTS:
        while (start := message.find(argument)) != -1:
            depth, index = 1, start + len(argument)
            while depth:
                depth += {"<": 1, ">": -1}.get(message[index], 0)
                index += 1
            message = message[:start] + message[index:]
    return message.replace("> >", ">>").replace(" >", ">")


def compare(baseline, observed):
    """[(case, observed message, baseline message)] of the cases that throw
    the same exception in both tables with different messages."""
    differences = []
    for case, row in sorted(observed.items()):
        expected = baseline.get(case)
        if expected is None or not row["resolve"].startswith("throws:"):
            continue
        if expected["resolve"] != row["resolve"]:
            continue
        if expected.get("message") is None or row.get("message") is None:
            continue
        if without_default_arguments(expected["message"]) != without_default_arguments(
            row["message"]
        ):
            differences.append((case, row["message"], expected["message"]))
    return differences


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--baseline", type=Path, default=None)
    parser.add_argument("--observed", type=Path, default=None)
    parser.add_argument("--summary", action="store_true", help="list each difference")
    args = parser.parse_args(argv)
    baseline = args.baseline or single_table(ROUTES_DIR / "observed")
    observed = args.observed or single_table(ROUTES_DIR / "observed-next")
    if baseline is None or observed is None:
        print("pass --baseline and --observed, or extract one table of each", file=sys.stderr)
        return 2

    differences = compare(load(baseline), load(observed))
    changes = [
        (change.id, [p[len("messages:") :] for p in change.cases if p.startswith("messages:")])
        for change in spec.load_spec().behavior_changes
    ]
    matched = collections.Counter()
    undeclared = []
    for case, have, want in differences:
        owners = [c for c, patterns in changes if any(fnmatch.fnmatchcase(case, p) for p in patterns)]
        for owner in owners:
            matched[owner] += 1
        if not owners:
            undeclared.append((case, have, want))
    stale = [c for c, patterns in changes if patterns and not matched[c]]

    print(f"compared messages: {len(differences)} differences, "
          f"{len(differences) - len(undeclared)} declared")
    for change_id, count in sorted(matched.items()):
        print(f"  {change_id}: {count}")
    for case, have, want in undeclared if args.summary else undeclared[:20]:
        print(f"UNDECLARED {case}\n    library:  {have}\n    baseline: {want}")
    for change_id in stale:
        print(f"STALE {change_id}: its messages: patterns match no difference")
    return 1 if undeclared or stale else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
