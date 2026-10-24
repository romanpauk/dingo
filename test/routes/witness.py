#!/usr/bin/env python3
"""Runs the witnesses of the conversion rules against the next implementation.

A witness is the smallest case of the route probes that shows one rule of the
conversion rules, chosen by the reference implementation from the cases of
extract.py, the kind probes among them:

- a route: the smallest served case whose route takes it, for every route of
  a kind of the library or of the kind probes and every rule of a leaf
- a refusal: the smallest binding a scope refuses to register, for every
  reason of refusal
- a condition: the smallest request a route refuses because of one of its
  conditions, a predicate it requires of its element, the request category
  it needs a scope to expose, or the sharing handle the copy rule needs in a
  scope that copies handles: one the reference refuses and serves by the
  route when the model relaxes that condition
- a mutation: the smallest request the kind a mutation differs from serves
  and the mutation does not, with the request of that kind beside it

The witnesses of each domain are compiled into one program, whose cases the
reference expects to fail to compile are found by compiling them alone, and
the observations are compared with the reference as conform.py compares the
route table. It fails on a difference, on a route without a witness, and on a
route declared unreachable that has one. It takes a minute where the route
table takes twenty; the route table remains the check of every case.

    witness.py --work-dir DIR [--cxx-command CMD] [--jobs N]

The default compiler command is $CXX -std=c++20 with the include paths of the
next implementation.
"""

import argparse
import concurrent.futures
import json
import os
import signal
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))
sys.path.insert(0, str(HERE))

import conform  # noqa: E402
import extract  # noqa: E402
import spec  # noqa: E402
from spec.reference.routes import Model, resolve  # noqa: E402
from user_kinds import kind_probes, reference_kinds  # noqa: E402

# The routes no case of the probes can take, and why.
UNREACHABLE = {
    "upcast_copied_shared_of_kind_shared_uncopyable": (
        "a sharing handle that is not copyable is never copied, so never "
        "upcast as a copy"
    ),
    "convert_borrowed": "no leaf of the probes declares a user conversion",
    "convert_consumed": "no leaf of the probes declares a user conversion",
}

# The conditions no case of the probes is refused by alone, and why: each is
# implied by another for the scopes of the library, which a scope of the user
# need not imply.
UNREACHABLE_CONDITIONS = {
    "exposure": (
        "the scopes of the library imply it: only a consuming scope makes an "
        "rvalue and it exposes rvalues, every scope that makes an lvalue "
        "exposes its address, and the scope that does not expose values "
        "copies only handles"
    ),
}


@dataclass
class Case:
    group: object
    index: int
    outcome: object
    labels: list = field(default_factory=list)

    @property
    def request(self):
        return self.group.requests[self.index]

    @property
    def key(self):
        group = self.group
        return (group.scope, group.leaf, group.interfaces, group.stored, self.request)

    def size(self):
        """The order of the smallest case: its type trees, then its spelling."""
        stored = conform.parse_type_tree(self.group.stored).size()
        request = conform.parse_request(self.request).type_tree.size()
        return (
            stored + request,
            len(self.group.interfaces),
            len(self.group.stored) + len(self.request),
            extract.SCOPES.index(self.group.scope),
            self.group.key,
            self.request,
        )


def universe(model):
    """Every case of the probes in the runtime domain, with the outcome the
    reference implementation predicts."""
    cases = []
    for group in extract.build_groups(("runtime",), kinds=True):
        stored = conform.parse_type_tree(group.stored)
        interfaces = tuple(conform.LEAVES[name] for name in group.interfaces)
        for index, request in enumerate(group.requests):
            outcome = resolve(
                model, group.scope, stored, conform.parse_request(request), interfaces
            )
            cases.append(Case(group, index, outcome))
    return cases


def witnessed_routes(model):
    """The routes that must have a witness: the rules of a leaf and the forms
    of the kinds of the library and of the kind probes."""
    kinds = {kind.id for kind in model.spec.wrapper_semantics}
    kinds |= {user.kind.id for user in kind_probes}
    owners = {
        form.id: kind.id for kind in model.semantics_by_id.values() for form in kind.forms
    }
    return [
        route.id
        for route in model.rules
        if route.id not in owners or owners[route.id] in kinds
    ]


def conditions(model):
    """The conditions of the witnessed routes, as (route id, condition)."""
    witnessed = set(witnessed_routes(model))
    found = []
    for route in model.rules:
        if route.id not in witnessed:
            continue
        # An upcast or a conversion requires a leaf, which is what it converts,
        # rather than a condition that refuses a request.
        found += [
            (route.id, requirement.predicate.value)
            for requirement in route.requires
            if requirement.predicate.value != "leaf"
        ]
        if route.exposure is not None:
            found.append((route.id, "exposure"))
        if route.id == "copy":
            found.append((route.id, "handle_copies"))
    return found


# The model of a worker process, which decides the cases of the relaxations.
WORKER = {}


def start_worker():
    WORKER["spec"] = spec.load_spec()


def decided_by(condition, refused):
    """The cases of `refused`, by key, that the reference serves by the route of
    `condition` when it relaxes it."""
    model = Model(WORKER["spec"], kinds=reference_kinds, relaxed=frozenset({condition}))
    route = condition[0]
    decided = []
    for scope, leaf, interfaces, stored, request in refused:
        outcome = resolve(
            model,
            scope,
            conform.parse_type_tree(stored),
            conform.parse_request(request),
            tuple(conform.LEAVES[name] for name in interfaces),
        )
        if outcome.served and route in outcome.rules:
            decided.append((scope, leaf, interfaces, stored, request))
    return decided


def choose_conditions(model, cases, jobs):
    """The smallest case each condition of a witnessed route decides, by
    condition, or None."""
    refused = [
        case.key
        for case in cases
        if case.outcome.registered and case.outcome.found and not case.outcome.served
    ]
    by_key = {case.key: case for case in cases}
    found = conditions(model)
    with concurrent.futures.ProcessPoolExecutor(
        max_workers=jobs, initializer=start_worker
    ) as pool:
        decided = list(pool.map(decided_by, found, [refused] * len(found)))
    return {
        condition: min((by_key[key] for key in keys), key=Case.size) if keys else None
        for condition, keys in zip(found, decided)
    }


def choose(model, cases, jobs):
    """The witnesses, as cases with the labels of what they witness, and the
    routes without one."""
    chosen = {}

    def add(case, label):
        chosen.setdefault(case.key, case).labels.append(label)

    missing = []
    for route in witnessed_routes(model):
        taking = [c for c in cases if c.outcome.served and route in c.outcome.rules]
        if not taking:
            missing.append(route)
            continue
        add(min(taking, key=lambda c: (len(c.outcome.rules), c.size())), f"route {route}")

    refusals = {}
    for case in cases:
        if not case.outcome.registered and case.index == 0:
            refusals.setdefault(case.outcome.reason, []).append(case)
    for reason, refused in sorted(refusals.items()):
        add(min(refused, key=Case.size), f"refusal: {reason}")

    by_key = {case.key: case for case in cases}
    for user in kind_probes:
        if user.mutates is None:
            continue
        mutated, like = f"{user.wrapper}<", f"{user.mutates}<"
        pairs = []
        for case in cases:
            if mutated not in case.group.stored + case.request:
                continue
            if not case.outcome.registered or case.outcome.served:
                continue
            scope, leaf, interfaces, stored, request = case.key
            twin = by_key.get(
                (scope, leaf, interfaces, stored.replace(mutated, like), request.replace(mutated, like))
            )
            if twin is not None and twin.outcome.served:
                pairs.append((case, twin))
        if not pairs:
            missing.append(f"mutation {user.wrapper}")
            continue
        case, twin = min(pairs, key=lambda pair: pair[0].size())
        add(case, f"mutation {user.wrapper}: refuses what {user.mutates} serves")
        add(twin, f"mutation {user.wrapper}: the request {user.mutates} serves")

    for (route, condition), case in choose_conditions(model, cases, jobs).items():
        if case is None:
            missing.append(f"condition {condition} of {route}")
        else:
            add(case, f"condition {condition} of {route}")
            if condition in UNREACHABLE_CONDITIONS:
                missing.append(f"stale: condition {condition} of {route} has a witness")
    return list(chosen.values()), missing


def case_fields(case, domain, index):
    group = extract.Group(domain, case.group.leaf, case.group.scope, case.group.type_tree, case.group.interfaces)
    group.requests = case.group.requests
    external = (
        extract.spell(group.type_tree.external, group.leaf) if group.scope == "external" else ""
    )
    return {
        "index": index,
        "id": group.case(case.index),
        "labels": case.labels,
        "domain": domain,
        "scope": group.scope,
        "stored": group.stored,
        "leaf": group.leaf,
        "interfaces": list(group.interfaces),
        "declarations": [extract.spell(d, group.leaf) for d in group.type_tree.declarations],
        "external_argument": external,
        "request": case.request,
    }


class Run:
    def __init__(self, toolchain, work_dir, jobs):
        self.toolchain = toolchain
        self.work_dir = work_dir
        self.jobs = jobs
        prelude = extract.ENVIRONMENT.get_template("prelude.h.j2").render(
            implementation="next",
            headers=extract.provided_headers("next"),
            user_wrapper_traits="user_wrapper_traits_next.h",
        )
        work_dir.mkdir(parents=True, exist_ok=True)
        (work_dir / "prelude.h").write_text(prelude)
        for header in extract.PRELUDE_HEADERS:
            (work_dir / header).write_text((extract.TEMPLATES / header).read_text())
        (work_dir / "user_kinds_next.h").write_text(extract.cpp_declarations())

    def map(self, function, items):
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
            return list(pool.map(function, list(items)))

    def render(self, cases, guarded):
        return extract.ENVIRONMENT.get_template("witness.cpp.j2").render(
            cases=cases, guarded=guarded
        )

    def syntax(self, source, *defines):
        return self.toolchain.compile(
            ["-fsyntax-only", f"-I{self.work_dir}", *defines, str(source)]
        )[0]

    def domain(self, domain, cases):
        """The rows of the witnesses in `domain`, as the route table has them."""
        fields = [case_fields(case, domain, index) for index, case in enumerate(cases)]
        source = self.work_dir / f"{domain}.cpp"
        source.write_text(self.render(fields, True))
        failing = set()
        registration_errors = set()
        if not self.syntax(source):
            alone = self.map(lambda f: self.syntax(source, f"-DONLY={f['index']}"), fields)
            failing = {f["index"] for f, ok in zip(fields, alone) if not ok}
            registers = self.map(
                lambda index: self.syntax(source, f"-DONLY={index}", "-DREGISTRATION_ONLY"),
                sorted(failing),
            )
            registration_errors = {
                index for index, ok in zip(sorted(failing), registers) if not ok
            }
        observed = self.observe(domain, [f for f in fields if f["index"] not in failing])
        rows = []
        for f in fields:
            row = {
                "counts": None,
                "domain": domain,
                "id": f["id"],
                "identity": None,
                "interfaces": ",".join(f["interfaces"]),
                "leaf": f["leaf"],
                "message": None,
                "registration": "ok",
                "request": f["request"],
                "resolve": "compile_error",
                "scope": f["scope"],
                "stable": None,
                "stored": f["stored"],
                "warm_counts": None,
                "warm_identity": None,
            }
            if f["index"] in registration_errors:
                row["registration"] = "compile_error"
                row["resolve"] = "not_run"
            elif f["index"] in observed:
                row.update(observed[f["index"]])
                row.pop("warm_resolve", None)
                if row["registration"] not in ("ok", "compile_error"):
                    row["registration"] = "throws:" + row["registration"]
            rows.append(row)
        return rows

    def observe(self, domain, fields):
        """The JSON lines the cases print, by index; a crash is attributed to
        the case being run and the rest continue."""
        if not fields:
            return {}
        source = self.work_dir / f"{domain}.run.cpp"
        executable = self.work_dir / f"{domain}.exe"
        source.write_text(self.render(fields, False))
        ok, error = self.toolchain.compile(
            ["-O0", f"-I{self.work_dir}", str(source), "-o", str(executable)]
        )
        if not ok:
            raise RuntimeError(f"{source} fails to build: {error}")
        indices = [f["index"] for f in fields]
        result, position = {}, 0
        while position < len(indices):
            process = subprocess.run(
                [str(executable), str(indices[position])], capture_output=True, text=True
            )
            for line in process.stdout.splitlines():
                parsed = json.loads(line)
                result[parsed.pop("i")] = parsed
            if process.returncode == 0:
                break
            crashed = next(i for i in indices[position:] if i not in result)
            name = (
                signal.Signals(-process.returncode).name
                if process.returncode < 0
                else f"exit{process.returncode}"
            )
            result[crashed] = {"resolve": f"crash:{name}", "registration": "ok"}
            position = indices.index(crashed) + 1
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--cxx-command")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument(
        "--list", action="store_true", help="print the witnesses and run none"
    )
    args = parser.parse_args()

    started = time.time()
    model = Model(spec.load_spec(), kinds=reference_kinds)
    cases = universe(model)
    witnesses, missing = choose(model, cases, args.jobs)
    def unreachable_reason(name):
        if name in UNREACHABLE:
            return UNREACHABLE[name]
        condition = name.split(" ")[1] if name.startswith("condition ") else None
        return UNREACHABLE_CONDITIONS.get(condition)

    unreachable = [name for name in missing if unreachable_reason(name)]
    missing = [name for name in missing if not unreachable_reason(name)]
    stale = [
        route
        for route in UNREACHABLE
        if any(f"route {route}" in label for case in witnesses for label in case.labels)
    ]

    if args.list:
        for case in sorted(witnesses, key=lambda c: c.labels):
            group = case.group
            label = extract.leaf_label(group.leaf, group.interfaces)
            print(f"{'; '.join(case.labels)}\n    {group.scope} {label} {group.stored} -> {case.request}")
        return 0

    include = f"-I{(PROJECT_ROOT / 'include').resolve()}"
    command = args.cxx_command or (os.environ.get("CXX", "c++") + f" -std=c++20 {include}")
    run = Run(extract.Toolchain(command), args.work_dir.resolve(), args.jobs)
    rows = [row for domain in extract.DOMAINS for row in run.domain(domain, witnesses)]
    differences = conform.compare(rows, model)

    print(
        f"{len(witnesses)} witnesses of {len(cases)} cases, {len(rows)} rows "
        f"in {time.time() - started:.0f}s: {len(differences)} differences"
    )
    witness_of = {}
    for case in witnesses:
        for domain in extract.DOMAINS:
            group = case.group
            case_id = extract.case_id(
                domain, group.leaf, group.scope, group.stored, case.request, group.interfaces
            )
            witness_of[case_id] = case.labels
    for case_id, name, have, want in differences:
        print(f"  {case_id}: {name} observed {have!r}, reference {want!r}")
        for label in witness_of.get(case_id, ()):
            print(f"      witness of {label}")
    for route in missing:
        print(f"  no witness of {route}")
    for route in stale:
        print(f"  {route} is declared unreachable but has a witness")
    reasons = {}
    for name in unreachable:
        reasons.setdefault(unreachable_reason(name), []).append(name)
    for reason, names in reasons.items():
        print(f"  {len(names)} unreachable: {reason}")
    return 1 if differences or missing or stale else 0


if __name__ == "__main__":
    sys.exit(main())
