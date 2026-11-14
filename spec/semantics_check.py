"""Compares the semantics of the library with the specification, for every
stored object and every request.

The library decides what a request resolves to by dingo/semantics/model.h,
of no C++ type: a layer is its properties, in the universe of the layers a
search may make. This check gives it two domains and compares its answer for
every category and lifetime with the answer of spec/semantics.py: the fewest
copies and moves of the leaf of what satisfies the request, or that nothing
does. Both domains are finite, so the comparison is complete.

- The classification of the types the model spells: every stored form and
  every request shape of every bound interface, of every leaf, classified as
  the layers the specification says they are made of.
- The standard layers: every stored object the specification makes, with and
  without the known limitations, and every request of every shape of every
  interface a registration binds.
- Every layer a type could define: each combination of the properties of a
  layer, beside the layers of C++ itself, stored and requested in every
  stack of it and of a pointer, of every scope and leaf.
- Every pair of layers a type could define, with each conversion between
  them that is possible: stored and requested in every stack of both of them
  and of a pointer, of every scope and leaf. With --pairs only: it takes
  longer.
"""

import argparse
import dataclasses
import itertools
import multiprocessing
import os
from pathlib import Path
import shlex
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1]))

from spec import semantics  # noqa: E402
from spec.generator import driver  # noqa: E402
from spec.vocabulary import (Category, Container, Entry,  # noqa: E402
                             InterfaceSet, Position, Scope, Selection,
                             StoredForm)

# The layers of C++ itself, at their indices in every universe of the
# library.
LANGUAGE = ("pointer", "array_pointer", "array", "array2")
ELEMENTS = {"leaf": 0, "interface": 1}
LIFETIMES = (semantics.Lifetime.REQUEST, semantics.Lifetime.STABLE)
ORIGINS = {semantics.Origin.BORROWED: 0, semantics.Origin.MADE: 1,
           semantics.Origin.COPIED: 2}
OWNERS = {semantics.Owner.NONE: 0, semantics.Owner.SOLE: 1,
          semantics.Owner.SHARED: 2}
LIMITS = {frozenset(): 0, semantics.LIMITATIONS: 3}
SOURCE = Path(__file__).parents[1] / "test" / "model" / "semantics.cpp"
# The standard layers, which the program classifies the standard wrappers as.
STANDARD = tuple(semantics.LAYERS)
STACK_ELEMENTS = {"leaf": 0, "interface": 1, "other": 2}
# How the classification program makes a request of a category of a shape,
# which spells an array as C++ does.
CATEGORY_TYPES = {
    Category.VALUE: "value_of",
    Category.CONST: "std::add_const_t",
    Category.LVALUE_REFERENCE: "std::add_lvalue_reference_t",
    Category.CONST_LVALUE_REFERENCE: "const_lvalue_of",
    Category.RVALUE_REFERENCE: "std::add_rvalue_reference_t",
    Category.POINTER: "std::add_pointer_t",
    Category.CONST_POINTER: "const_pointer_of",
}


def classified_layer(name):
    """A layer as the classification program prints it: its properties, and
    those of the layer an owned value of it converts to, or "-"."""
    layer = semantics.LAYERS[name]
    own = " ".join(map(str, properties(layer, ())[:-1]))
    target = (" ".join(map(str, properties(
        semantics.LAYERS[layer.converts_to[0]], ())[:-1]))
              if layer.converts_to else "-")
    return f"{own} > {target}"


def classification(compiler, flags, work):
    """The stacks the library classifies the types the model spells as, each
    compared with the layers the specification says the type is made of; and
    the traits of the leaves. Returns the entries and those that differ."""
    from spec.generator.model import grammar, spell_shape, spell_stored
    from spec.generator.registrations import LEAF_TYPES, interface_of
    leaves = [(leaf, f"dingo::model::{LEAF_TYPES[leaf]}")
              for leaf in semantics.LEAVES]
    entries = []
    for scope, traits in semantics.SCOPES.items():
        entries.append((f"scope/{scope.value}",
                        f"print_scope<dingo::{scope.value}>()",
                        " ".join(str(int(value)) for value in (
                            traits.makes, traits.given,
                            traits.publishes_early))))
    for leaf, spelled in leaves:
        traits = semantics.LEAVES[leaf]
        entries.append((f"leaf/{leaf.value}",
                        f"print_leaf<{spelled}>()",
                        " ".join(str(int(value)) for value in (
                            traits.copyable, traits.movable,
                            traits.default_constructible,
                            traits.made_by_factory))))
        for form in StoredForm:
            stored = spell_stored(form).replace("L", spelled)
            layers = semantics.FORMS[form]
            entries.append((
                f"stored/{form.value}/{leaf.value}",
                f"print_stack<std::remove_reference_t<{stored}>, {spelled}>()",
                " | ".join([classified_layer(name) for name in layers] +
                           [str(STACK_ELEMENTS["leaf"])])))
        for bound in bounds():
            interface = interface_of(bound, leaf)
            # A shape C++ cannot spell of the interface in any category, as
            # the grammar says, is no type to classify.
            requests = [request for request in grammar(
                (interface,), tuple(semantics.SHAPES)) if not request.invalid]
            valid = {request.shape for request in requests}
            for request in requests:
                shape = spell_shape(request.shape, interface.spelled)
                entries.append((
                    f"category/{request.name}/{bound.element}"
                    f"{list(bound.layers)}/{leaf.value}",
                    f"print_category<{CATEGORY_TYPES[request.category]}<"
                    f"{shape}>, {shape}>()",
                    f"{list(Category).index(request.category)} 1"))
            for shape in semantics.SHAPES:
                if shape not in valid:
                    continue
                inner = interface.spelled
                layers = semantics.SHAPES[shape] + bound.layers
                entries.append((
                    f"request/{shape.value}/{bound.element}"
                    f"{list(bound.layers)}/{leaf.value}",
                    f"print_stack<{spell_shape(shape, inner)}, {spelled}>()",
                    " | ".join([classified_layer(name) for name in layers] +
                               [str(STACK_ELEMENTS[bound.element])])))
    source = work / "classification.cpp"
    source.write_text(CLASSIFICATION.replace(
        "ENTRIES", "\n".join(f"  {call};" for _, call, _ in entries)))
    program = work / "classification"
    subprocess.run(driver.compiler_command(
        compiler, *flags, "-I" + str(SOURCE.parents[1]), str(source), "-o",
        str(program)), check=True)
    run = subprocess.run([str(program)], capture_output=True, text=True,
                         check=True)
    printed = run.stdout.splitlines()
    if len(printed) != len(entries):
        return len(entries), [("program", f"{len(entries)} lines",
                               f"{len(printed)} lines")]
    return len(entries), [(name, want, got) for (name, _, want), got in
                          zip(entries, printed) if want != got]


CLASSIFICATION = """// Generated by spec/semantics_check.py. Do not edit.
#include "model/probe.h"

#include <dingo/request/classification.h>

#include <cstdio>
#include <type_traits>

using namespace dingo::model;

namespace {
void print_kind(const dingo::semantics::layer_kind &kind) {
  std::printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
              static_cast<int>(kind.owns), kind.shares_element, kind.copyable,
              kind.made_from_value, kind.holds_element_only, kind.relocatable,
              kind.gives_element, kind.bounded, kind.may_be_empty,
              kind.has_empty_value, kind.adopts, kind.decays, kind.adopted,
              kind.adopted_array, kind.rebinds);
}

template <typename Tag> void print_layer() {
  print_kind(dingo::detail::tag_kind<Tag>());
  std::printf(" > ");
  if constexpr (std::is_same_v<Tag, dingo::detail::pointer_layer_tag> ||
                std::is_same_v<Tag, dingo::detail::array_layer_tag> ||
                std::is_same_v<Tag, dingo::detail::bounded_array_layer_tag>) {
    std::printf("-");
  } else {
    using target =
        typename dingo::detail::shared_layer<typename Tag::identity>::type;
    if constexpr (std::is_void_v<target>) {
      std::printf("-");
    } else {
      print_kind(dingo::detail::wrapper_kind_v<target>);
    }
  }
  std::printf(" | ");
}

template <typename Stack> struct printer;

template <typename... Tags, dingo::detail::stack_element Element>
struct printer<dingo::detail::type_stack<dingo::type_list<Tags...>, Element>> {
  static void print() {
    (print_layer<Tags>(), ...);
    std::printf("%d\\n", static_cast<int>(Element));
  }
};

template <typename T, typename Leaf> void print_stack() {
  printer<dingo::detail::stack_of_t<T, Leaf>>::print();
}

template <typename X> using value_of = X;
template <typename X> using const_lvalue_of = const X &;
template <typename X> using const_pointer_of = const X *;

template <typename Request, typename Shape> void print_category() {
  using category = dingo::detail::requested_category<Request>;
  std::printf("%d %d\\n",
              category::modeled ? static_cast<int>(category::value) : -1,
              std::is_same_v<typename category::shape, Shape>);
}

template <typename Tag> void print_scope() {
  constexpr dingo::semantics::scope_traits scope =
      dingo::detail::scope_semantics_v<Tag>;
  std::printf("%d %d %d\\n", scope.makes, scope.given, scope.publishes_early);
}

template <typename Leaf> void print_leaf() {
  constexpr dingo::semantics::leaf_traits traits =
      dingo::detail::leaf_traits_of<Leaf>();
  std::printf("%d %d %d %d\\n", traits.copyable, traits.movable,
              traits.default_constructible, traits.made_by_factory);
}
} // namespace

int main() {
ENTRIES
}
"""


def properties(layer, universe):
    """A layer as the program reads it: its properties, and the layers of the
    universe an owned value of it converts to, as a mask."""
    converts = sum(1 << universe.index(name) for name in layer.converts_to
                   if name in universe)
    return [OWNERS[layer.owner], int(layer.shares_element),
            int(layer.copyable), int(layer.made_from_value),
            int(layer.holds_element_only), int(layer.relocatable),
            int(layer.gives_element), int(layer.bounded),
            int(layer.may_be_empty), int(layer.has_empty_value),
            int(layer.adopts), int(bool(layer.decays_to)),
            int(layer.adopted_with is not None),
            int(bool(layer.adopted_with)), int(layer.rebinds), converts]


def order(names):
    """A universe as the library orders it: the layers of C++ itself first."""
    return LANGUAGE + tuple(name for name in names if name not in LANGUAGE)


def bounds():
    """Every interface a registration binds, of the leaf or of its abstract
    interface: the other alternative of an alternative type is no element
    the semantics reach."""
    found = {semantics.LEAF, semantics.INTERFACE}
    for form in StoredForm:
        for interfaces in InterfaceSet:
            for selection in Selection:
                found.update(semantics.bound_interfaces(form, interfaces,
                                                        selection))
    return sorted(bound for bound in found if bound.element in ELEMENTS)


def standard():
    """The stored objects the specification makes, with the context of their
    registration, and the requests of them."""
    goals = [(semantics.SHAPES[shape] + bound.layers, bound.element,
              f"{shape.value}/{bound.element}{list(bound.layers)}")
             for shape in semantics.SHAPES for bound in bounds()]
    for scope in Scope:
        for form in StoredForm:
            for leaf, traits in semantics.LEAVES.items():
                for abstract in (False, True):
                    for interfaces in InterfaceSet:
                        for limits in LIMITS:
                            context = semantics.Context(
                                traits, semantics.SCOPES[scope], abstract,
                                limits)
                            if not semantics.makes_storage(form, context):
                                continue
                            yield (f"{scope.value}/{form.value}/{leaf.value}/"
                                   f"{'interface' if abstract else 'leaf'}/"
                                   f"{interfaces.value}/{LIMITS[limits]}",
                                   context,
                                   semantics.source(scope, form, interfaces),
                                   goals)


def made_layers():
    """Every layer a type could define, by its properties, beside the layers
    of C++ itself: it gives its element, is no raw pointer and no array, and
    is possible, as semantics.possible says."""
    emptiness = ((False, False), (True, False), (True, True))
    for index, (owner, shares, copyable, made, holds, relocatable, empty,
                adopts, rebinds) in enumerate(itertools.product(
                    semantics.Owner, (False, True), (False, True),
                    (False, True), (False, True), (False, True), emptiness,
                    (False, True), (False, True))):
        layer = semantics.Layer(
            f"made{index}", owner, shares_element=shares, copyable=copyable,
            made_from_value=made, holds_element_only=holds,
            relocatable=relocatable, may_be_empty=empty[0],
            has_empty_value=empty[1], adopts=adopts, rebinds=rebinds)
        if semantics.possible(layer):
            yield layer


def lifetime(scope):
    return (semantics.Lifetime.REQUEST if scope.makes
            else semantics.Lifetime.STABLE)


def makes(layers, context):
    """Whether a scope makes a stack, as makes_storage says of a stored
    form."""
    if context.scope.given or not layers:
        return True
    outer = semantics.LAYERS[layers[0]]
    if outer.bounded and not context.leaf.default_constructible:
        return False
    return not (context.scope.publishes_early and
                context.leaf.made_by_factory and outer.shares_element)


def swept():
    """Each layer a type could define, stored and requested in every stack of
    it, of a pointer and of an array, of every scope and leaf."""
    for layer in made_layers():
        semantics.LAYERS[layer.name] = layer
        name = layer.name
        universe = LANGUAGE + (name,)
        stored = ((), (name,), (name, "array"), (name, "array2"),
                  (name, name))
        requested = [stack for depth in range(4)
                     for stack in itertools.product((name, "pointer"),
                                                    repeat=depth)]
        requested += [(name, "array"), (name, "array2")]
        goals = [(stack, element, f"{list(stack)}/{element}")
                 for stack in requested for element in ELEMENTS]
        for scope, scope_traits in semantics.SCOPES.items():
            for leaf, traits in semantics.LEAVES.items():
                for abstract in (False, True):
                    context = semantics.Context(
                        traits, scope_traits, abstract, semantics.LIMITATIONS,
                        universe)
                    for layers in stored:
                        if not makes(layers, context):
                            continue
                        start = semantics.Thing(
                            layers, "leaf",
                            (lifetime(scope_traits),) * (len(layers) + 1),
                            (semantics.Origin.MADE if scope_traits.makes
                             else semantics.Origin.BORROWED),
                            instance=True)
                        yield (f"{list(layers)}/{scope.value}/{leaf.value}/"
                               f"{'interface' if abstract else 'leaf'}/{name}",
                               context, start, goals)


def pairs():
    """Every pair of layers a type could define, each with the conversions
    between them that are possible: none, either or both."""
    made = list(made_layers())
    for first, second in itertools.combinations(made, 2):
        ways = [(first, second), (second, first)]
        ways = [way for way in ways if semantics.possible_conversion(*way)]
        for count in range(len(ways) + 1):
            for converting in itertools.combinations(ways, count):
                yield tuple(dataclasses.replace(
                    layer, converts_to=tuple(
                        target.name for source, target in converting
                        if source is layer))
                    for layer in (first, second))


def paired(universes):
    """The stored objects and the requests of pairs of layers: those that
    hold both layers, requested in every stack of them, of a pointer and of
    an array; and those that hold either or none, requested in the stacks of
    both, which the domain of every single layer has not."""
    for first, second in universes:
        semantics.LAYERS[first.name] = first
        semantics.LAYERS[second.name] = second
        semantics.reachable.cache_clear()
        names = (first.name, second.name)
        universe = LANGUAGE + names
        stacks = [stack for depth in range(4)
                  for stack in itertools.product(names + ("pointer",),
                                                 repeat=depth)]
        stacks += [(name, array) for name in names
                   for array in ("array", "array2")]
        every = [(stack, element, f"{list(stack)}/{element}")
                 for stack in stacks for element in ELEMENTS]
        both = [goal for goal in every if set(names) <= set(goal[0])]
        stored = [((first.name, second.name), every),
                  ((second.name, first.name), every),
                  ((), both), ((first.name,), both), ((second.name,), both)]
        for scope, scope_traits in semantics.SCOPES.items():
            for leaf, traits in semantics.LEAVES.items():
                for abstract in (False, True):
                    context = semantics.Context(
                        traits, scope_traits, abstract, semantics.LIMITATIONS,
                        universe)
                    for layers, goals in stored:
                        if not makes(layers, context):
                            continue
                        start = semantics.Thing(
                            layers, "leaf",
                            (lifetime(scope_traits),) * (len(layers) + 1),
                            (semantics.Origin.MADE if scope_traits.makes
                             else semantics.Origin.BORROWED),
                            instance=True)
                        yield (f"{list(layers)}/{scope.value}/{leaf.value}/"
                               f"{'interface' if abstract else 'leaf'}/"
                               f"{[(layer.name, layer.converts_to) for layer in (first, second)]}",
                               context, start, goals)


POSITIONS = list(Position)
ENTRIES = list(Entry)


def rules_cases(starts):
    """The requests of the rules domain of some stored objects, each with the
    answers of the specification's fewest for every category, position,
    entry point, selection and container."""
    for scope, form, leaf, interfaces, limits in starts:
        traits = semantics.LEAVES[leaf]
        for abstract in (False, True):
            context = semantics.Context(traits, semantics.SCOPES[scope],
                                        abstract, limits)
            if not semantics.makes_storage(form, context):
                continue
            start = semantics.source(scope, form, interfaces)
            for bound in bounds():
                if (bound.element == "interface") != abstract:
                    continue
                for shape, shaped in semantics.SHAPES.items():
                    layers = shaped + bound.layers
                    answers = []
                    for category in Category:
                        for position in POSITIONS:
                            for entry in ENTRIES:
                                for selection in (Selection.PLAIN,
                                                  Selection.ANNOTATED):
                                    for container in (Container.STATIC,
                                                      Container.RUNTIME):
                                        cost = semantics.fewest(
                                            scope, form, leaf, bound, shape,
                                            category, position, entry, limits,
                                            selection, interfaces, container)
                                        answers.append(
                                            "%d,%d" % cost if cost else "-")
                    yield (f"{scope.value}/{form.value}/{leaf.value}/"
                           f"{interfaces.value}/{LIMITS[limits]}/"
                           f"{shape.value}/{bound.element}"
                           f"{list(bound.layers)}",
                           " ".join(answers),
                           encode(context, start, layers, bound.element,
                                  len(shaped)))


def compare_rules_part(arguments):
    """The rules domain of some stored objects, in a process of its own."""
    program, starts = arguments
    cases = list(rules_cases(starts))
    run = subprocess.run([str(program), "rules"],
                         input="\n".join(header() + [line for _, _, line in
                                                     cases]) + "\n",
                         capture_output=True, text=True)
    answers = run.stdout.splitlines()
    if run.returncode != 0 or len(answers) != len(cases):
        return len(cases), 0, [(name, want, "") for name, want, _ in cases]
    resolved = sum(answer != "-" for _, want, _ in cases
                   for answer in want.split())
    return len(cases), resolved, [
        (name, want, got.strip())
        for (name, want, _), got in zip(cases, answers)
        if want != got.strip()][:20]


def compare_rules(program, jobs):
    """The rules domain: every stored object of every scope, form, leaf,
    interfaces and limitations, in parts compared in processes at once."""
    starts = [(scope, form, leaf, interfaces, limits)
              for scope in Scope for form in StoredForm
              for leaf in semantics.LEAVES for interfaces in InterfaceSet
              for limits in LIMITS]
    parts = [(program, starts[index:index + 8])
             for index in range(0, len(starts), 8)]
    count, resolved, differ = 0, 0, []
    with multiprocessing.Pool(jobs) as pool:
        for found in pool.imap_unordered(compare_rules_part, parts):
            count += found[0]
            resolved += found[1]
            differ += found[2]
    return count, resolved, differ


def compare_part(arguments):
    """compare of a part of the pairs, in a process of its own."""
    program, universes = arguments
    return compare(program, paired(universes))


def compare_pairs(program, jobs):
    """compare of every pair, in parts compared in processes at once."""
    universes = list(pairs())
    size = 64
    parts = [(program, universes[index:index + size])
             for index in range(0, len(universes), size)]
    count, resolved, differ = 0, 0, []
    with multiprocessing.Pool(jobs) as pool:
        for found in pool.imap_unordered(compare_part, parts):
            count += found[0]
            resolved += found[1]
            differ += found[2][:20]
    return count, resolved, differ


def consistent(domain):
    """The requests of a domain a container makes: of the abstract interface
    exactly where the context requests it. What a binding publishes reaches
    what its stored object does only for them."""
    for name, context, start, goals in domain:
        yield name, context, start, [
            goal for goal in goals
            if (goal[1] == "interface") == context.abstract]


def expected(reached, context, layers, element):
    """The answers of the specification for a request, by its search of
    every thing the stored object reaches."""
    things = [thing for thing in reached if thing.element == element]
    answers = []
    for category in Category:
        for lifetime_of in LIFETIMES:
            costs = [semantics.affected(cost, effect)
                     for thing, effect in semantics.satisfactions(
                         things, category, lifetime_of, layers, context)
                     for cost in reached[thing]]
            answers.append("%d,%d" % min(costs) if costs else "-")
    return " ".join(answers)


def encode(context, start, layers, element, shape_depth=0):
    """A request as the program reads it."""
    universe = order(context.universe)
    made = universe[len(LANGUAGE):]
    numbers = [LIMITS[context.limits], int(context.leaf.copyable),
               int(context.leaf.movable),
               int(context.leaf.default_constructible),
               int(context.scope.makes),
               int(context.scope.publishes_early), int(context.abstract),
               len(made)]
    for name in made:
        numbers += properties(semantics.LAYERS[name], universe)
    numbers += [len(start.layers),
                *(universe.index(name) for name in start.layers),
                ELEMENTS[start.element], start.lifetime.value,
                ORIGINS[start.origin], len(layers),
                *(universe.index(name) for name in layers),
                ELEMENTS[element], shape_depth]
    return " ".join(map(str, numbers))


def header():
    """The first lines the program reads: the layers of C++ itself, and the
    standard layers, as the specification describes them."""
    language = [str(len(LANGUAGE))]
    for name in LANGUAGE:
        language += map(str, properties(semantics.LAYERS[name], LANGUAGE))
    universe = order(STANDARD)
    standard_layers = [str(len(universe) - len(LANGUAGE))]
    for name in universe[len(LANGUAGE):]:
        standard_layers += map(str, properties(semantics.LAYERS[name],
                                               universe))
    return [" ".join(language), " ".join(standard_layers)]


def compare(program, domain, mode=None):
    """The requests of a domain, how many of their answers resolve, and those
    whose answers differ."""
    cases = []
    lines = header()
    for name, context, start, goals in domain:
        # What the stored object reaches, or, through what a binding
        # publishes, what a request reaches from that.
        reached = (semantics.requested(start, context) if mode == "split"
                   else semantics.reachable(start, context))
        for layers, element, goal in goals:
            cases.append((f"{name}/{goal}",
                          expected(reached, context, layers, element)))
            lines.append(encode(context, start, layers, element))
    run = subprocess.run([str(program)] + ([mode] if mode else []),
                         input="\n".join(lines) + "\n",
                         capture_output=True, text=True)
    answers = run.stdout.splitlines()
    if run.returncode != 0 or len(answers) != len(cases):
        print(run.stderr or f"{len(answers)} answers to {len(cases)} requests")
        return len(cases), 0, [(name, want, "") for name, want in cases]
    resolved = sum(answer != "-" for _, want in cases
                   for answer in want.split())
    return len(cases), resolved, [
        (name, want, got.strip())
        for (name, want), got in zip(cases, answers)
        if want != got.strip()][:20]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--flags", default="-std=c++17 -O2 -Iinclude",
                        help="compiler flags, with include paths")
    parser.add_argument("--pairs", action="store_true",
                        help="compare every pair of layers a type could "
                        "define too")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    options = parser.parse_args()
    options.work.mkdir(parents=True, exist_ok=True)
    program = options.work / "semantics"
    subprocess.run(driver.compiler_command(
        options.compiler, *shlex.split(options.flags), str(SOURCE), "-o",
        str(program)), check=True)
    failed = False
    count, differ = classification(options.compiler,
                                   shlex.split(options.flags), options.work)
    print(f"classification: {count} types, {len(differ)} differ")
    for name, want, got in differ[:20]:
        print(f"  {name}\n    specification {want}\n    library       {got}")
    failed = bool(differ)
    # Each domain with the answers of a request: one for each category and
    # lifetime, or for each category and rules of a request.
    domains = [("standard layers", 14, lambda: compare(program, standard())),
               ("every layer a type could define", 14,
                lambda: compare(program, swept())),
               ("standard layers, through what a binding publishes", 14,
                lambda: compare(program, consistent(standard()), "split")),
               ("every layer a type could define, through what a binding "
                "publishes", 14,
                lambda: compare(program, consistent(swept()), "split"))]
    domains.append(("the rules of requests, every stored object", 336,
                    lambda: compare_rules(program, options.jobs)))
    if options.pairs:
        domains.append(("every pair of layers a type could define", 14,
                        lambda: compare_pairs(program, options.jobs)))
    for title, answers, run in domains:
        count, resolved, differ = run()
        print(f"{title}: {count} requests, {count * answers} answers, {resolved} "
              f"of them resolved, {len(differ)} requests differ")
        for name, want, got in differ[:20]:
            print(f"  {name}\n    specification {want}\n    library       {got}")
        failed = failed or bool(differ)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
