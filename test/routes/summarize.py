#!/usr/bin/env python3
"""Summarizes observed route tables written by extract.py.

Usage:

    summarize.py [FILE ...] [--strict] [--examples N] [--no-differences]
                 [--compare-user-wrappers [--top N]]

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

With --compare-user-wrappers the summary instead pairs every case of a user
wrapper shape (user_unique, user_shared, user_optional anywhere in the stored
type or the request) with the case of the same domain, leaf, and scope in which
each user wrapper is replaced by its std counterpart (std::unique_ptr,
std::shared_ptr, std::optional) in both the stored type and the request, and
prints how the outcome or observations differ. Pairs are grouped into patterns
by the wrapper-agnostic shape of the stored type and the request (the leaf is
written T, a wrapper by its kind: unique, shared, optional), the two results,
and the scope; the patterns are ranked by whether the availability changes
(an "ok" against anything else), then by the number of cases. --top N limits
the patterns shown (default 10).
"""

import argparse
import collections
import json
import re
import sys
from pathlib import Path

OBSERVED_DIR = Path(__file__).resolve().parents[2] / "build" / "routes" / "observed"
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


def leaf_label(record):
    """The leaf as written in a case id: impl[iface,other_iface]."""
    interfaces = record.get("interfaces", "")
    return f"{record['leaf']}[{interfaces}]" if interfaces else record["leaf"]


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
        key = (leaf_label(record), record["scope"], record["stored"], record["request"])
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


USER_WRAPPER_RE = re.compile(r"\buser_(unique|shared|optional)\b")
STD_WRAPPER = {
    "unique": "std::unique_ptr",
    "shared": "std::shared_ptr",
    "optional": "std::optional",
}
KIND_NAME = {"std::unique_ptr": "unique", "std::shared_ptr": "shared",
             "std::optional": "optional"}


def to_std(text):
    return USER_WRAPPER_RE.sub(lambda m: STD_WRAPPER[m.group(1)], text)


def generic_shape(text, leaf):
    """The type with the leaf written T and every std wrapper by its kind, so
    that the shape reads the same for every leaf; a user wrapper keeps its
    name."""
    text = re.sub(rf"\b{re.escape(leaf)}\b", "T", text)
    for std, kind in KIND_NAME.items():
        text = text.replace(std, kind)
    return text


def result(record):
    """The outcome of a case, as one string."""
    return outcome(record)


def differing_fields(std, user):
    return tuple(f for f in OBSERVATION_FIELDS if std[f] != user[f])


def observation_text(record, fields):
    return " ".join(f"{f}={json.dumps(record[f], sort_keys=True)}" for f in fields)


CARRIER_RE = re.compile(r"^(?:const )?(.*?)(?: const)?(&&|&|\*)?$")


def request_base(request):
    """The request without its carrier: const T& and T* both read T."""
    match = CARRIER_RE.match(request)
    return match.group(1)


def compare_user_wrappers(records, top, examples):
    index = {
        (r["domain"], r["leaf"], r["interfaces"], r["scope"], r["stored"], r["request"]): r
        for r in records
    }
    patterns = collections.defaultdict(list)
    pairs = unmatched = 0
    for record in records:
        if not USER_WRAPPER_RE.search(record["stored"]):
            continue
        key = (
            record["domain"],
            record["leaf"],
            record["interfaces"],
            record["scope"],
            to_std(record["stored"]),
            to_std(record["request"]),
        )
        std = index.get(key)
        if std is None:
            unmatched += 1
            continue
        pairs += 1
        std_outcome, user_outcome = outcome(std), outcome(record)
        fields = ()
        if std_outcome == user_outcome:
            if std_outcome != "ok":
                continue
            fields = differing_fields(std, record)
            if not fields:
                continue
        leaf = record["leaf"]
        # A registration outcome does not depend on the request.
        registration = any(
            o.startswith("registration:") for o in (std_outcome, user_outcome)
        )
        pattern = (
            generic_shape(record["stored"], leaf),
            "(any)"
            if registration
            else generic_shape(request_base(record["request"]), leaf),
            std_outcome,
            user_outcome,
            fields,
        )
        patterns[pattern].append((record, std))
    differing = sum(len(v) for v in patterns.values())
    print(
        f"\nuser wrappers against std wrappers: {pairs} paired cases, "
        f"{differing} differ in {len(patterns)} patterns (stored shape and request "
        f"type modulo carrier, both outcomes), {unmatched} without a std "
        f"counterpart"
    )
    kinds = collections.Counter()
    for (_, _, std_outcome, user_outcome, _), cases in patterns.items():
        if std_outcome == "ok" and user_outcome != "ok":
            kind = "std ok, user not ok"
        elif std_outcome != "ok" and user_outcome == "ok":
            kind = "std not ok, user ok"
        elif std_outcome == "ok":
            kind = "both ok, observations differ"
        else:
            kind = "both not ok, different outcomes"
        kinds[kind] += len(cases)
    print("; ".join(f"{kind}: {count}" for kind, count in sorted(kinds.items())))

    def rank(item):
        (stored, request, std_outcome, user_outcome, fields), cases = item
        availability = (std_outcome == "ok") != (user_outcome == "ok")
        return (not availability, -len(cases), stored, request)

    for number, (pattern, cases) in enumerate(sorted(patterns.items(), key=rank)[:top], 1):
        stored, request, std_outcome, user_outcome, fields = pattern
        print(f"\n[{number}] {len(cases)} cases  stored {stored}  request {request}")
        print(f"    std : {std_outcome}")
        print(f"    user: {user_outcome}")
        user, std = cases[0]
        if fields:
            print(f"    first difference: {user['id']}")
            print(f"      std : {observation_text(std, fields)}")
            print(f"      user: {observation_text(user, fields)}")
        for name, getter in (
            ("scopes", lambda c: c["scope"]),
            ("domains", lambda c: c["domain"]),
            ("leaves", lambda c: c["leaf"]),
            ("carriers", lambda c: carrier(c["request"])),
        ):
            counter = collections.Counter(getter(user) for user, _ in cases)
            print(f"    {name}: " + ", ".join(f"{k}={v}" for k, v in sorted(counter.items())))
        shown = sorted({user["id"] for user, _ in cases})
        for case_id in shown[: examples or None][:2]:
            print(f"    e.g. {case_id}")


def carrier(request):
    """The carrier a request spelling applies: value, &, const &, &&, *, const *."""
    if request.endswith("&&"):
        return "&&"
    if request.endswith("&"):
        return "const &" if request.startswith("const ") else "&"
    if request.endswith("*"):
        return "const *" if request.startswith("const ") else "*"
    return "value"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--examples", type=int, default=6)
    parser.add_argument("--no-differences", action="store_true")
    parser.add_argument(
        "--compare-user-wrappers",
        action="store_true",
        help="compare the user wrapper shapes with their std counterparts",
    )
    parser.add_argument(
        "--top", type=int, default=10, help="patterns shown by --compare-user-wrappers"
    )
    args = parser.parse_args()

    files = args.files or sorted(OBSERVED_DIR.glob("*.jsonl"))
    for path in files:
        records = load(path)
        print(f"== {path.stem}: {len(records)} cases")
        if args.compare_user_wrappers:
            compare_user_wrappers(records, args.top, args.examples)
            continue
        print_counts(records)
        if not args.no_differences:
            print_differences(records, args.strict, args.examples)
    return 0


if __name__ == "__main__":
    sys.exit(main())
