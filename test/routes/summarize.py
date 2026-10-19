#!/usr/bin/env python3
"""Summarizes observed route tables written by extract.py.

Usage:

    summarize.py [FILE ...] [--strict] [--examples N] [--no-differences]

Without files every test/routes/observed/*.jsonl is summarized. For each file
the summary prints

- the number of cases per domain and the counts of each outcome per domain and
  per (domain, scope); an outcome is "ok", "compile_error", "throws:<name>",
  "crash:<signal>", or "registration:<outcome>" when the registration itself
  does not compile or throws
- every (leaf, scope, stored, request) whose availability or observations
  differ between the domains, grouped by pattern and ordered by group size.
  Availability is "ok" against anything else; observations are the counts,
  identity, stability, and the second (warm) resolve of cases that are "ok" in
  at least two domains. With --strict the other outcomes must agree as well, so
  a compile error against an exception is a difference.

Each pattern lists its cases as one line per (leaf, scope, stored) with the
requests; --examples N limits the lines per pattern (0 lists them all).
"""

import argparse
import collections
import json
import sys
from pathlib import Path

OBSERVED_DIR = Path(__file__).resolve().parent / "observed"
DOMAIN_ORDER = ("runtime", "static", "mixed")
OBSERVATION_FIELDS = (
    "counts",
    "identity",
    "stable",
    "warm_counts",
    "warm_identity",
)


def load(path):
    with path.open() as stream:
        return [json.loads(line) for line in stream]


def outcome(record):
    if record["registration"] != "ok":
        return f"registration:{record['registration']}"
    return record["resolve"]


def counts_by_outcome(records):
    by_domain = collections.defaultdict(collections.Counter)
    by_scope = collections.defaultdict(collections.Counter)
    for record in records:
        label = outcome(record)
        by_domain[record["domain"]][label] += 1
        by_scope[(record["domain"], record["scope"])][label] += 1
    return by_domain, by_scope


def print_counts(records):
    by_domain, by_scope = counts_by_outcome(records)
    for domain in sorted(by_domain, key=DOMAIN_ORDER.index):
        total = sum(by_domain[domain].values())
        print(f"{domain}: {total} cases")
        for label, count in sorted(by_domain[domain].items()):
            print(f"  {count:6}  {label}")
        for (d, scope), counter in sorted(by_scope.items()):
            if d != domain:
                continue
            parts = ", ".join(f"{l}={c}" for l, c in sorted(counter.items()))
            print(f"    {scope}: {parts}")


def observation(record):
    return {field: record[field] for field in OBSERVATION_FIELDS}


def differences(records, strict):
    """Yields (pattern, key) for every (leaf, scope, stored, request) whose
    availability or observations differ between domains."""
    cases = collections.defaultdict(dict)
    for record in records:
        key = (record["leaf"], record["scope"], record["stored"], record["request"])
        cases[key][record["domain"]] = record
    for key, by_domain in cases.items():
        if len(by_domain) < 2:
            continue
        domains = [d for d in DOMAIN_ORDER if d in by_domain]
        labels = {d: outcome(by_domain[d]) for d in domains}
        available = {d: labels[d] == "ok" for d in domains}
        if len(set(available.values())) > 1 or (
            strict and len(set(labels.values())) > 1
        ):
            kind = "availability" if len(set(available.values())) > 1 else "outcome"
            pattern = f"{kind}: " + " ".join(f"{d}={labels[d]}" for d in domains)
            yield pattern, key
            continue
        ok = [d for d in domains if available[d]]
        if len(ok) < 2:
            continue
        differing = [
            field
            for field in OBSERVATION_FIELDS
            if len({json.dumps(by_domain[d][field], sort_keys=True) for d in ok}) > 1
        ]
        if differing:
            groups = collections.defaultdict(list)
            for d in ok:
                groups[json.dumps(observation(by_domain[d]), sort_keys=True)].append(d)
            split = " | ".join("+".join(g) for g in groups.values())
            yield f"observation ({','.join(differing)}): {split}", key


def print_differences(records, strict, examples):
    patterns = collections.defaultdict(list)
    for pattern, key in differences(records, strict):
        patterns[pattern].append(key)
    total = sum(len(keys) for keys in patterns.values())
    print(f"\ncross-domain differences: {total} cases in {len(patterns)} patterns")
    ranked = sorted(patterns.items(), key=lambda item: (-len(item[1]), item[0]))
    for rank, (pattern, keys) in enumerate(ranked, 1):
        print(f"\n[{rank}] {len(keys)} cases  {pattern}")
        lines = collections.defaultdict(list)
        for leaf, scope, stored, request in sorted(keys):
            lines[(leaf, scope, stored)].append(request)
        shown = sorted(lines.items())
        if examples:
            shown = shown[:examples]
        for (leaf, scope, stored), requests in shown:
            print(f"    {leaf} {scope} {stored}: {', '.join(requests)}")
        if examples and len(lines) > examples:
            print(f"    ... {len(lines) - examples} more (leaf, scope, stored)")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--examples", type=int, default=6)
    parser.add_argument("--no-differences", action="store_true")
    args = parser.parse_args()

    files = args.files or sorted(OBSERVED_DIR.glob("*.jsonl"))
    for path in files:
        records = load(path)
        print(f"== {path.stem}: {len(records)} cases")
        print_counts(records)
        if not args.no_differences:
            print_differences(records, args.strict, args.examples)
    return 0


if __name__ == "__main__":
    sys.exit(main())
