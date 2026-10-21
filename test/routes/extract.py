#!/usr/bin/env python3
"""Extracts the conversion ("route") behavior of the library.

For every combination of domain, leaf kind, scope, stored shape, and request the
extractor records what the library does: whether the registration compiles,
whether the resolution compiles, throws, or succeeds, and for a successful
resolution the constructions it performed and the identity of the leaf it
handed out. The table is ground truth for a reference implementation of the
route rules; it describes the code as it is, not as it is documented.

Usage:

    extract.py --work-dir build/routes [--jobs N] [--cxx-command CMD]
               [--include-dir DIR] [--domains runtime,static,mixed]
               [--output FILE]

    CXX=g++ extract.py --work-dir build/routes
    extract.py --work-dir build/routes --implementation baseline \\
        --include-dir build/differential/baseline-source/include

The table describes the library, the next implementation, by default. With
--implementation baseline it describes the baseline, the library of the commit
in test/differential/baseline-commit, whose include directory --include-dir
names. The default output is <work-dir>/observed-next/<toolchain>.jsonl for the
library and <work-dir>/observed/<toolchain>.jsonl for the baseline, where the
toolchain key is the one test/contracts/probe.py uses, such as
gcc16-x86_64-c++20. The table is not committed: it is regenerated from the
headers. The file is JSON Lines sorted by case id; every line is one case.

Universe (the tables below):

- domains: "runtime" is dingo::container<> with register_type, "static" is
  dingo::static_container over a bindings list, "mixed" is dingo::container
  over a bindings list (static binding, resolved through the container).
- leaf kinds: "leaf" (copyable, movable) and "move_only_leaf"; both count
  default, copy, and move constructions and destructions and register every
  live instance address with a creation sequence number. "impl" is a third,
  instrumented the same way, that derives from the polymorphic interfaces
  "iface" and "other_iface"; it is registered in two variants, with
  dingo::interfaces<iface> and with dingo::interfaces<iface, other_iface>.
  The addresses of its interface subobjects are registered as aliases of the
  instance, so an interface reference reaches the same identity.
- scopes: shared, unique, external, shared_cyclical. External registrations
  pass an instance; the static and mixed containers cannot take one, which
  shows as a registration compile error.
- stored shapes: T, T*, T&, unique_ptr/shared_ptr of T and T[], optional<T>,
  T[2], variant<T, other_leaf>, and the nine {unique_ptr, shared_ptr, optional}
  of {unique_ptr<T>, shared_ptr<T>, optional<T>} combinations. The leaf and
  move_only_leaf kinds have 21 more: the user wrappers user_unique<T>,
  user_shared<T>, user_optional<T> (class templates defined in the prelude and
  made known to dingo through type_traits, storage_traits, and
  type_conversion_traits specializations, as test/support/custom_wrappers.h
  does; their storage_traits are written like the built-in cells of the
  matching std handle, so a wrapped wrapper is published as well, and no
  storage_traits exist for shared_cyclical; the next implementation declares
  each of a layer kind of the user instead, see user_kinds.py), and the
  combinations user_X<std_Y<T>> and std_Y<user_X<T>> for X, Y in {unique,
  shared, optional}.
- requests: the carriers value, &, const &, &&, *, const * applied to T,
  unique_ptr<T>, shared_ptr<T>, optional<T>, other_leaf for the variant, the
  stored type, and every wrapper layer of the stored type, deduplicated. The
  spelling in the case id is the one written in the generated C++ with single
  spaces: "leaf&", "const leaf&", "leaf const*" only after a pointer
  ("leaf* const&"), "leaf(&)[2]" for arrays.

A case id is "<domain>/<leaf>/<scope>/<stored>/<request>"; for impl the leaf is
followed by its interface list in brackets, "impl[iface,other_iface]". A record
has the keys

  counts         {"copy", "default", "destroy", "move"} deltas during the first
                 resolve of an "ok" case, else null
  domain, leaf, scope, stored, request, id
  interfaces     the comma-separated interface list of the registration
                 ("iface", "iface,other_iface"), "" for leaf and
                 move_only_leaf
  identity       "existing" (the leaf was live before the call), "new" (created
                 during the call), "none" (empty handle), "unknown" (the
                 address is not live when the call returns: a dangling
                 result), or null for a plain leaf value and for a case that
                 is not "ok"
  registration   "ok", "compile_error", or "throws:<exception>"
  resolve        "ok", "compile_error", "throws:<exception>", "crash:<signal>",
                 or "not_run" when the registration is not "ok"
  stable         whether a second resolve reaches the same leaf address (false
                 for a plain leaf value, null when not "ok")
  warm_counts, warm_identity
                 the same observations for that second resolve, made while the
                 result of the first is still alive

Every case uses a fresh container: the first resolve is a cold resolve.
<exception> is the most specific dingo exception type caught (they are caught
in the order of include/dingo/core/primitives/exceptions.h; "exception" is the
common base), else "std" or "unknown".

Method: per (domain, leaf, scope, stored) one generated program contains one
function per request. A registration-only compile decides the registration
outcome; the requests that compile (all at once, else one -fsyntax-only compile
per request, which is the only way to observe a static compile error) are then
built into one executable per group that runs each request on a fresh container
and prints one JSON line. A crash is attributed to the request being run and
the remaining requests continue.
"""

import argparse
import concurrent.futures
import itertools
import json
import os
import platform
import re
import shlex
import signal
import subprocess

import jinja2
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from user_kinds import cpp_declarations, kind_probes  # noqa: E402
from spec.schema import LayerTrait  # noqa: E402

# ---------------------------------------------------------------------------
# The universe.
# ---------------------------------------------------------------------------

DOMAINS = ("runtime", "static", "mixed")

# The registered kinds as (leaf, interfaces). The interfaces are the
# dingo::interfaces<...> of the registration; none for the plain leaves.
LEAF_KINDS = (
    ("leaf", ()),
    ("move_only_leaf", ()),
    ("impl", ("iface",)),
    ("impl", ("iface", "other_iface")),
)

# Leaf kinds that also have the user wrapper shapes.
USER_WRAPPER_LEAVES = ("leaf", "move_only_leaf")

SCOPES = ("shared", "unique", "external", "shared_cyclical")

# Carriers applied to a base type. The spelling is built by carrier_spellings.
CARRIERS = ("value", "&", "const &", "&&", "*", "const *")

# The dingo exception types, most derived first. All are siblings derived from
# dingo::exception; arena_allocation_exception exists in debug builds only.
EXCEPTIONS = (
    "type_not_found_exception",
    "type_not_convertible_exception",
    "type_ambiguous_exception",
    "type_recursion_exception",
    "lookup_already_registered_exception",
    "type_index_out_of_range_exception",
    "virtual_pointer_exception",
    "arena_allocation_exception",
)


@dataclass(frozen=True)
class Shape:
    """A stored shape. {T} is the leaf; the external expression builds the
    instance an external registration is given; declarations are objects the
    probe owns for it."""

    name: str
    spelling: str
    external: str
    layers: tuple = ()
    declarations: tuple = ()
    # Registered without an instance in a scope that constructs.
    external_note: str = ""
    # A user wrapper shape: requested through the user wrappers of T.
    user: bool = False
    # The bases of the requests, as (template, abstract, interface) of the
    # leaf {T} and of the interface {I}, which only a binding of interfaces
    # requests; empty for the bases of request_bases.
    bases: tuple = ()


# The wrapper families: the short name, the spelling of the wrapper of {inner},
# and the expression building one around the expression {arg} of an {inner}
# (empty for a default constructed inner leaf).
WRAPPERS = {
    "std::unique_ptr": (
        "unique_ptr",
        "std::unique_ptr<{inner}>",
        lambda inner, arg: f"std::make_unique<{inner}>({arg})",
    ),
    "std::shared_ptr": (
        "shared_ptr",
        "std::shared_ptr<{inner}>",
        lambda inner, arg: f"std::make_shared<{inner}>({arg})",
    ),
    "std::optional": (
        "optional",
        "std::optional<{inner}>",
        lambda inner, arg: f"std::optional<{inner}>(std::in_place"
        + (f", {arg})" if arg else ")"),
    ),
    "user_unique": (
        "user_unique",
        "user_unique<{inner}>",
        lambda inner, arg: f"user_unique<{inner}>(std::make_unique<{inner}>({arg}))",
    ),
    "user_shared": (
        "user_shared",
        "user_shared<{inner}>",
        lambda inner, arg: f"user_shared<{inner}>(std::make_shared<{inner}>({arg}))",
    ),
    "user_optional": (
        "user_optional",
        "user_optional<{inner}>",
        lambda inner, arg: f"user_optional<{inner}>(std::in_place"
        + (f", {arg})" if arg else ")"),
    ),
}
STD_WRAPPERS = ("std::unique_ptr", "std::shared_ptr", "std::optional")
USER_WRAPPERS = ("user_unique", "user_shared", "user_optional")


def _wrapper_shapes(outers, inners, user):
    """The shapes outer<inner<T>> for every outer and inner wrapper."""
    for outer, inner in itertools.product(outers, inners):
        o_name, o_spelling, o_make = WRAPPERS[outer]
        i_name, i_spelling, i_make = WRAPPERS[inner]
        inner_type = i_spelling.format(inner="{T}")
        inner_expression = i_make("{T}", "")
        yield Shape(
            name=f"{o_name}_of_{i_name}",
            spelling=o_spelling.format(inner=inner_type),
            external=o_make(inner_type, inner_expression),
            layers=(inner_type,),
            user=user,
        )


def _single_wrapper_shapes(wrappers, user):
    for wrapper in wrappers:
        name, spelling, make = WRAPPERS[wrapper]
        yield Shape(
            name=name,
            spelling=spelling.format(inner="{T}"),
            external=make("{T}", ""),
            user=user,
        )


SHAPES = (
    Shape("value", "{T}", "{T}{{}}"),
    Shape("pointer", "{T}*", "&owned", declarations=("{T} owned;",)),
    Shape("reference", "{T}&", "owned", declarations=("{T} owned;",)),
    Shape("unique_ptr", "std::unique_ptr<{T}>", "std::make_unique<{T}>()"),
    Shape("shared_ptr", "std::shared_ptr<{T}>", "std::make_shared<{T}>()"),
    Shape("optional", "std::optional<{T}>", "std::optional<{T}>(std::in_place)"),
    Shape("array", "{T}[2]", "owned", declarations=("{T} owned[2];",)),
    Shape("unique_ptr_array", "std::unique_ptr<{T}[]>", "std::make_unique<{T}[]>(2)"),
    Shape(
        "shared_ptr_array",
        "std::shared_ptr<{T}[]>",
        "std::shared_ptr<{T}[]>(new {T}[2])",
    ),
    Shape(
        "variant",
        "std::variant<{T}, other_leaf>",
        "std::variant<{T}, other_leaf>(std::in_place_index<0>)",
    ),
    *_wrapper_shapes(STD_WRAPPERS, STD_WRAPPERS, user=False),
)

# The user wrappers of T and their combinations with the std wrappers, both
# ways round.
USER_SHAPES = (
    *_single_wrapper_shapes(USER_WRAPPERS, user=True),
    *_wrapper_shapes(USER_WRAPPERS, STD_WRAPPERS, user=True),
    *_wrapper_shapes(STD_WRAPPERS, USER_WRAPPERS, user=True),
)


# The kind probes, of the next implementation: a wrapper of each kind of the
# user of user_kinds.py, requested as the leaf, as itself, and as the wrappers
# of the kinds its forms lead to; and a leaf or a pointer to one, requested as
# a wrapper of each kind that a request creates.
KIND_LEAF_KINDS = (
    ("leaf", ()),
    ("move_only_leaf", ()),
    ("impl", ("iface",)),
)


def kind_spelling(user, element):
    if user.kind.variadic:
        return f"{user.wrapper}<{element}, {user.arguments}>"
    return f"{user.wrapper}<{element}>"


def holds_interface(user):
    """Whether a wrapper of the kind holds an interface, which only a handle
    does: an abstract element is not held in place."""
    return LayerTrait.IN_PLACE not in user.kind.traits


def mutated(user):
    """The kind probe a mutation differs from, or the kind itself."""
    return next((other for other in kind_probes if other.wrapper == user.mutates), user)


def kind_bases(user):
    """The requests of a wrapper of a kind probe: of the kinds its forms lead
    to, or those of the kind it mutates, which it may no longer reach."""
    targets = [
        other
        for form in mutated(user).kind.forms
        if form.target is not None
        for other in kind_probes
        if other.kind.id == form.target
    ]
    bases = [("{T}", False, False), ("{I}", True, True)]
    for each in (user, *targets):
        bases.append((kind_spelling(each, "{T}"), False, False))
        if holds_interface(each):
            bases.append((kind_spelling(each, "{I}"), False, True))
    return tuple(bases)


def created_bases():
    bases = []
    for user in kind_probes:
        if user.created or mutated(user).created:
            bases.append((kind_spelling(user, "{T}"), False, False))
            if holds_interface(user):
                bases.append((kind_spelling(user, "{I}"), False, True))
    return tuple(bases)


KIND_SHAPES = (
    *(
        Shape(
            name=user.wrapper,
            spelling=kind_spelling(user, "{T}"),
            external=user.instance.replace("{W}", user.wrapper),
            bases=kind_bases(user),
        )
        for user in kind_probes
    ),
    # A variant whose second alternative leads to the leaf too, which holds
    # its first: a request routed through the second is not served.
    Shape(
        name="kind_variant_of_unique",
        spelling="kind_variant<{T}, std::unique_ptr<{T}>>",
        external="kind_variant<{T}, std::unique_ptr<{T}>>(std::in_place_index<0>)",
        bases=(
            ("{T}", False, False),
            ("{I}", True, True),
            ("kind_variant<{T}, std::unique_ptr<{T}>>", False, False),
            ("std::unique_ptr<{T}>", False, False),
            ("std::unique_ptr<{I}>", False, True),
        ),
    ),
    # A variant whose shortest route to the leaf takes the second
    # alternative while it holds the first: the route through the first, a
    # sibling of it, serves the request.
    Shape(
        name="kind_variant_held_unique",
        spelling="kind_variant<std::unique_ptr<{T}>, {T}>",
        external=(
            "kind_variant<std::unique_ptr<{T}>, {T}>(std::in_place_index<0>, "
            "std::make_unique<{T}>())"
        ),
        bases=(
            ("{T}", False, False),
            ("{I}", True, True),
            ("kind_variant<std::unique_ptr<{T}>, {T}>", False, False),
            ("std::unique_ptr<{T}>", False, False),
        ),
    ),
    Shape("kinds_value", "{T}", "{T}{{}}", bases=created_bases()),
    Shape(
        "kinds_pointer",
        "{T}*",
        "&owned",
        declarations=("{T} owned;",),
        bases=created_bases(),
    ),
)


def spell(template, leaf):
    return template.replace("{T}", leaf).replace("{{}}", "{}")


def stored_spelling(shape, leaf):
    return spell(shape.spelling, leaf)


ARRAY_RE = re.compile(r"^(.*)\[(\d+)\]$")


def carrier_spellings(base, abstract=False):
    """Valid spellings of the carriers applied to a base type, as
    (carrier, spelling) pairs. Invalid compositions are skipped: an abstract
    class has no value."""
    array = ARRAY_RE.match(base)
    if array:
        element, size = array.groups()
        return [
            ("&", f"{element}(&)[{size}]"),
            ("const &", f"const {element}(&)[{size}]"),
            ("&&", f"{element}(&&)[{size}]"),
            ("*", f"{element}(*)[{size}]"),
            ("const *", f"const {element}(*)[{size}]"),
        ]
    if base.endswith("&"):
        return [("value", base)]
    const = (lambda b: f"{b} const") if base.endswith("*") else (lambda b: f"const {b}")
    spellings = [
        ("value", base),
        ("&", f"{base}&"),
        ("const &", f"{const(base)}&"),
        ("&&", f"{base}&&"),
        ("*", f"{base}*"),
        ("const *", f"{const(base)}*"),
    ]
    return spellings[1:] if abstract else spellings


def request_bases(shape, leaf, interfaces):
    """The types the carriers are applied to, as (base, abstract) pairs."""
    if shape.bases:
        interface = interfaces[0] if interfaces else ""
        return [
            (template.replace("{T}", leaf).replace("{I}", interface), abstract)
            for template, abstract, needs_interface in shape.bases
            if interfaces or not needs_interface
        ]
    stored = stored_spelling(shape, leaf)
    layers = [spell(layer, leaf) for layer in shape.layers]
    if interfaces:
        return [
            *((i, True) for i in interfaces),
            *((f"std::unique_ptr<{i}>", False) for i in interfaces),
            *((f"std::shared_ptr<{i}>", False) for i in interfaces),
            (leaf, False),
            (stored, False),
            *((layer, False) for layer in layers),
        ]
    if shape.user:
        return [
            (leaf, False),
            *((f"{wrapper}<{leaf}>", False) for wrapper in USER_WRAPPERS),
            (stored, False),
            *((layer, False) for layer in layers),
        ]
    # The variant holds its first alternative; a request of the other one is
    # found and refused, as a binding names every alternative.
    alternatives = [("other_leaf", False)] if shape.name == "variant" else []
    return [
        (leaf, False),
        (f"std::unique_ptr<{leaf}>", False),
        (f"std::shared_ptr<{leaf}>", False),
        (f"std::optional<{leaf}>", False),
        *alternatives,
        (stored, False),
        *((layer, False) for layer in layers),
    ]


def requests_for(shape, leaf, interfaces=()):
    """The deduplicated request spellings for a (leaf, stored shape)."""
    requests = []
    for base, abstract in request_bases(shape, leaf, interfaces):
        for _, request in carrier_spellings(base, abstract):
            if request not in requests:
                requests.append(request)
    return requests


def leaf_label(leaf, interfaces=()):
    """The leaf as written in a case id: impl[iface,other_iface]."""
    return f"{leaf}[{','.join(interfaces)}]" if interfaces else leaf


def case_id(domain, leaf, scope, stored, request, interfaces=()):
    return f"{domain}/{leaf_label(leaf, interfaces)}/{scope}/{stored}/{request}"


# ---------------------------------------------------------------------------
# Generated C++.
# ---------------------------------------------------------------------------




@dataclass
class Group:
    domain: str
    leaf: str
    scope: str
    shape: Shape
    interfaces: tuple = ()
    requests: list = field(default_factory=list)

    @property
    def stored(self):
        return stored_spelling(self.shape, self.leaf)

    @property
    def label(self):
        return leaf_label(self.leaf, self.interfaces)

    @property
    def key(self):
        """The group as written in a case id up to the request."""
        return f"{self.domain}/{self.label}/{self.scope}/{self.stored}"

    @property
    def name(self):
        leaf = "+".join((self.leaf, *self.interfaces))
        return f"{leaf}-{self.scope}-{self.shape.name}"

    @property
    def directory(self):
        return self.domain

    def case(self, index):
        return case_id(
            self.domain,
            self.leaf,
            self.scope,
            self.stored,
            self.requests[index],
            self.interfaces,
        )


TEMPLATES = Path(__file__).resolve().parent / "templates"
ENVIRONMENT = jinja2.Environment(
    loader=jinja2.FileSystemLoader(str(TEMPLATES)),
    undefined=jinja2.StrictUndefined,
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)
# The headers the prelude copies next to the programs.
PRELUDE_HEADERS = (
    "probe_types.h",
    "probe_run.h",
    "user_wrapper_traits.h",
    "user_wrapper_traits_next.h",
    "kinded_wrappers_next.h",
)
PROBED_HEADERS = (
    "dingo/container.h",
    "dingo/static_container.h",
    "dingo/storage/external.h",
    "dingo/storage/shared.h",
    "dingo/storage/shared_cyclical.h",
    "dingo/storage/unique.h",
)


def provided_headers(implementation):
    """The probed public headers the implementation provides."""
    if implementation == "baseline":
        return PROBED_HEADERS
    include = PROJECT_ROOT / "include"
    return tuple(header for header in PROBED_HEADERS if (include / header).is_file())


def render(group, indices, guarded, implementation):
    """The program of a group. With guarded the program contains every request
    but a request is compiled only when ONLY is undefined or its index; the
    registration is always compiled. Otherwise it contains exactly the given
    requests and runs them, starting at argv[1]. The instance of an external
    binding is the argument of its registration, and in the next
    implementation of the static container that holds it."""
    fields = {
        "domain": group.domain,
        "scope": group.scope,
        "stored": group.stored,
        "interfaces": list(group.interfaces),
        "declarations": [spell(d, group.leaf) for d in group.shape.declarations],
        "external_argument": (
            spell(group.shape.external, group.leaf)
            if group.scope == "external"
            and (
                group.domain == "runtime"
                or (group.domain in ("static", "mixed") and implementation == "next")
            )
            else ""
        ),
        "requests": group.requests,
    }
    selected = range(len(group.requests)) if guarded else indices
    return ENVIRONMENT.get_template("group.cpp.j2").render(
        group=fields, selected=list(selected), guarded=guarded
    )


# ---------------------------------------------------------------------------
# Toolchain.
# ---------------------------------------------------------------------------


class Toolchain:
    def __init__(self, command):
        self.command = shlex.split(command)
        version = self._run([*self.command, "--version"]).lower()
        if "clang" in version:
            self.family = "clang"
        elif "gcc" in version or "g++" in version:
            self.family = "gcc"
        else:
            self.family = "unknown"
        self.major = self._run([*self.command, "-dumpversion"]).split(".", 1)[0]
        machine = platform.machine().lower()
        self.arch = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
        std = next(
            (arg for arg in self.command if arg.startswith("-std=")), "-std=default"
        )
        self.std = std.split("=", 1)[1]

    @property
    def key(self):
        return f"{self.family}{self.major}-{self.arch}-{self.std}"

    @staticmethod
    def _run(args):
        return subprocess.run(
            args, check=True, capture_output=True, text=True
        ).stdout.strip()

    def compile(self, args):
        """Returns (succeeded, first error line)."""
        result = subprocess.run(
            [*self.command, *args], capture_output=True, text=True
        )
        if result.returncode == 0:
            return True, ""
        error = next(
            (l for l in result.stderr.splitlines() if "error" in l),
            result.stderr.strip()[:300],
        )
        return False, error.strip()[:400]


# ---------------------------------------------------------------------------
# Extraction.
# ---------------------------------------------------------------------------


def build_groups(domains, pattern=None, user_wrappers=True, kinds=False):
    """The groups of the probes; with `kinds`, the kind probes too."""
    groups = []
    for domain, (leaf, interfaces), scope in itertools.product(
        domains, LEAF_KINDS, SCOPES
    ):
        user = user_wrappers and leaf in USER_WRAPPER_LEAVES
        shapes = SHAPES + (USER_SHAPES if user else ())
        if kinds and (leaf, interfaces) in KIND_LEAF_KINDS:
            shapes += KIND_SHAPES
        for shape in shapes:
            group = Group(domain, leaf, scope, shape, interfaces)
            group.requests = requests_for(shape, leaf, interfaces)
            if pattern is None or re.search(pattern, group.key):
                groups.append(group)
    return groups


class Extraction:
    def __init__(self, toolchain, work_dir, jobs, implementation="next"):
        self.toolchain = toolchain
        self.implementation = implementation
        self.work_dir = work_dir / implementation / toolchain.key
        self.jobs = jobs
        self.work_dir.mkdir(parents=True, exist_ok=True)
        prelude = ENVIRONMENT.get_template("prelude.h.j2").render(
            implementation=implementation,
            headers=provided_headers(implementation),
            user_wrapper_traits=(
                "user_wrapper_traits.h"
                if implementation == "baseline"
                else "user_wrapper_traits_next.h"
            ),
        )
        (self.work_dir / "prelude.h").write_text(prelude)
        for header in PRELUDE_HEADERS:
            (self.work_dir / header).write_text((TEMPLATES / header).read_text())
        (self.work_dir / "user_kinds_next.h").write_text(cpp_declarations())
        self.errors = {}

    def path(self, group, suffix):
        directory = self.work_dir / group.domain
        directory.mkdir(exist_ok=True)
        return directory / f"{group.name}{suffix}"

    def syntax(self, group, source, only):
        args = ["-fsyntax-only", f"-I{self.work_dir}", str(source)]
        if only is not None:
            args.insert(0, f"-DONLY={only}")
        return self.toolchain.compile(args)

    def map(self, function, items):
        items = list(items)
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
            return list(pool.map(function, items))

    def run(self, domains, pattern=None):
        groups = build_groups(domains, pattern, kinds=self.implementation == "next")
        for group in groups:
            self.path(group, ".cpp").write_text(
                render(group, range(len(group.requests)), True, self.implementation)
            )

        # Registration outcome: the program without any resolve.
        log("registration compiles", len(groups))
        outcomes = self.map(lambda g: self.syntax(g, self.path(g, ".cpp"), -1), groups)
        compiled = {}
        registration_errors = {}
        for group, (ok, error) in zip(groups, outcomes):
            if ok:
                compiled[id(group)] = True
            else:
                registration_errors[id(group)] = error

        # Resolve compile outcome: all requests at once, else one by one.
        live = [g for g in groups if id(g) in compiled]
        log("whole-group compiles", len(live))
        whole = self.map(lambda g: self.syntax(g, self.path(g, ".cpp"), None), live)
        compile_ok = {}
        singles = []
        for group, (ok, _) in zip(live, whole):
            if ok:
                compile_ok[id(group)] = set(range(len(group.requests)))
            else:
                compile_ok[id(group)] = set()
                singles += [(group, i) for i in range(len(group.requests))]
        log("per-request compiles", len(singles))
        results = self.map(
            lambda task: self.syntax(task[0], self.path(task[0], ".cpp"), task[1]),
            singles,
        )
        for (group, index), (ok, _) in zip(singles, results):
            if ok:
                compile_ok[id(group)].add(index)

        # Executables, one per group with at least one compiling request.
        log("executables", len(live))
        runs = self.map(lambda g: self.build_and_run(g, compile_ok[id(g)]), live)
        observed = {id(g): r for g, r in zip(live, runs)}

        records = []
        for group in groups:
            records += self.records(
                group,
                registration_errors.get(id(group)),
                observed.get(id(group)),
            )
        for group in groups:
            if id(group) in registration_errors:
                self.errors[group.key] = registration_errors[id(group)]
        records.sort(key=lambda r: r["id"])
        return records

    def build_and_run(self, group, indices):
        """Returns {index: parsed line or crash marker}, plus indices that fail
        to build as 'compile_error'."""
        indices = sorted(indices)
        result = {}
        executable = self.path(group, ".exe")
        while indices:
            source = self.path(group, ".run.cpp")
            source.write_text(render(group, indices, False, self.implementation))
            ok, error = self.toolchain.compile(
                ["-O0", f"-I{self.work_dir}", str(source), "-o", str(executable)]
            )
            if ok:
                break
            # Syntax-only compiles missed an error that code generation finds:
            # find the failing requests by building each alone.
            failing = []
            for index in indices:
                source_one = self.path(group, ".one.cpp")
                source_one.write_text(render(group, [index], False, self.implementation))
                if not self.toolchain.compile(
                    ["-c", f"-I{self.work_dir}", str(source_one), "-o", os.devnull]
                )[0]:
                    failing.append(index)
            if not failing:
                raise RuntimeError(f"{source} fails to build: {error}")
            for index in failing:
                result[index] = {"resolve": "compile_error", "registration": "ok"}
            indices = [i for i in indices if i not in failing]
        position = 0
        while position < len(indices):
            start = indices[position]
            process = subprocess.run(
                [str(executable), str(start)], capture_output=True, text=True
            )
            printed = []
            for line in process.stdout.splitlines():
                parsed = json.loads(line)
                result[parsed.pop("i")] = parsed
                printed.append(parsed)
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

    def records(self, group, registration_error, observed):
        for index, request in enumerate(group.requests):
            record = {
                "counts": None,
                "domain": group.domain,
                "id": group.case(index),
                "identity": None,
                "interfaces": ",".join(group.interfaces),
                "leaf": group.leaf,
                "message": None,
                "registration": "ok",
                "request": request,
                "resolve": "compile_error",
                "scope": group.scope,
                "stable": None,
                "stored": group.stored,
                "warm_counts": None,
                "warm_identity": None,
            }
            if registration_error is not None:
                record["registration"] = "compile_error"
                record["resolve"] = "not_run"
            elif observed is not None and index in observed:
                record.update(observed[index])
                record.pop("warm_resolve", None)
                if record["registration"] not in ("ok", "compile_error"):
                    record["registration"] = "throws:" + record["registration"]
            yield record


def log(message, count):
    print(f"[{time.strftime('%H:%M:%S')}] {message}: {count}", file=sys.stderr, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--include-dir",
        type=Path,
        default=None,
        help="include directory of the baseline, with --implementation baseline",
    )
    parser.add_argument(
        "--cxx-command",
        help="compiler command including the standard and include flags; by "
        "default $CXX -std=c++20 with the include paths of the library",
    )
    parser.add_argument(
        "--implementation",
        choices=("next", "baseline"),
        default="next",
        help="baseline probes the baseline, whose include directory "
        "--include-dir names",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="directory for generated programs and executables",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--filter",
        help="regular expression selecting groups by domain/leaf/scope/stored "
        "(development aid; the output then covers the selection only)",
    )
    args = parser.parse_args()

    domains = args.domains.split(",")
    unknown = [d for d in domains if d not in DOMAINS]
    if unknown:
        parser.error(f"unknown domains: {unknown}")

    if args.implementation == "baseline":
        if args.include_dir is None:
            parser.error("--implementation baseline needs --include-dir")
        include = f"-I{args.include_dir.resolve()}"
    else:
        include = f"-I{(PROJECT_ROOT / 'include').resolve()}"
    command = args.cxx_command or (os.environ.get("CXX", "c++") + f" -std=c++20 {include}")
    toolchain = Toolchain(command)
    started = time.time()
    extraction = Extraction(
        toolchain, args.work_dir.resolve(), args.jobs, args.implementation
    )
    records = extraction.run(domains, args.filter)
    observed = "observed" if args.implementation == "baseline" else "observed-next"
    output = args.output or args.work_dir / observed / f"{toolchain.key}.jsonl"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w") as stream:
        for record in records:
            stream.write(json.dumps(record, sort_keys=True) + "\n")
    errors = extraction.work_dir / "registration-errors.txt"
    errors.write_text(
        "".join(f"{key}\t{error}\n" for key, error in sorted(extraction.errors.items()))
    )
    per_domain = {d: sum(1 for r in records if r["domain"] == d) for d in domains}
    print(
        f"{toolchain.key}: {len(records)} cases {per_domain} in "
        f"{time.time() - started:.0f}s -> {output}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
