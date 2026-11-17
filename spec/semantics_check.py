"""Compares the semantics of the library with the specification, for every
stored object and every request.

The library determines what a request resolves to by dingo/semantics/model.h,
of no C++ type: a layer is its properties, in the universe of the layers a
search may make. This check gives it two domains and compares its answer for
every category and lifetime with the answer of spec/semantics.py: the fewest
copies and moves of the leaf of what satisfies the request, or that nothing
does. Both domains are finite, so the comparison is complete.

- The classification of the types the model spells: every stored form and
  every request shape of every bound interface, of every leaf, classified as
  the layers the specification defines them to be made of.
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

Each domain but the pairs is compared in parts, in processes at once. The
answers of the specification of each part are kept in the work directory and
made again when a source of the specification or of this check changes: the
specification computes its answers far slower than the program, and a change of
the library alone does not change its answers.
"""

import argparse
import dataclasses
import hashlib
from compression import zstd
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
                             InterfaceSet, Leaf, Position, Scope, Selection,
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
LIMITS = {frozenset(): 0, semantics.LIMITATIONS: 7}
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


def classified_layer(layers, index):
    """A layer of a stack as the classification program prints it: its
    properties, those of the layer an owned value of it converts to, and
    those of the layer it converts to of an array of unknown bound when it
    holds an array of a known bound, each or "-"."""
    layer = semantics.LAYERS[layers[index]]

    def printed(names):
        return (" ".join(map(str, properties(semantics.LAYERS[names[0]],
                                             ())[:-2]))
                if names else "-")
    holds_bounded = (index + 1 < len(layers) and
                     semantics.LAYERS[layers[index + 1]].bounded)
    return (f"{printed((layers[index],))} > {printed(layer.transfers)} > "
            f"{printed(layer.converts_unknown_bound if holds_bounded else ())}")


def classified_stack(layers, element):
    """A stack as the classification program prints it."""
    return " | ".join([classified_layer(layers, index)
                       for index in range(len(layers))] +
                      [str(STACK_ELEMENTS[element])])


def classification(compiler, flags, work):
    """The stacks the library classifies the types the model spells as, each
    compared with the layers the specification defines the type to be made of;
    and the traits of the leaves. Returns the entries and those that differ."""
    from spec.generator.model import grammar, spell_shape, spell_stored
    from spec.generator.registrations import LEAF_TYPES, bound_interface
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
                            traits.factory_made))))
        for form in StoredForm:
            stored = spell_stored(form).replace("L", spelled)
            layers = semantics.FORMS[form]
            entries.append((
                f"stored/{form.value}/{leaf.value}",
                f"print_stack<std::remove_reference_t<{stored}>, {spelled}>()",
                classified_stack(layers, "leaf")))
        for bound in bounds():
            interface = bound_interface(bound, leaf)
            # A shape C++ cannot spell of the interface in any category, as
            # the grammar defines, is no type to classify.
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
                    classified_stack(layers, bound.element)))
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
#include "model/leaves.h"

#include <dingo/request/classification.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <cstdio>
#include <type_traits>

using namespace dingo::model;

namespace {
void print_kind(const dingo::semantics::layer_kind &kind) {
  std::printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
              static_cast<int>(kind.owns), kind.shares_element, kind.copyable,
              kind.wraps_value, kind.holds_element_only, kind.relocatable,
              kind.gives_element, kind.bounded, kind.may_be_empty,
              kind.has_empty_value, kind.adopts, kind.adopts_bounded,
              kind.decays, kind.adopted,
              kind.adopted_array, kind.rebinds, kind.observes,
              kind.qualifies);
}

template <typename Target> void print_target() {
  if constexpr (std::is_void_v<Target>) {
    std::printf("-");
  } else {
    print_kind(dingo::detail::wrapper_kind_v<Target>);
  }
}

template <typename Tag> void print_layer() {
  print_kind(dingo::detail::tag_kind<Tag>());
  if constexpr (std::is_same_v<Tag, dingo::detail::pointer_layer_tag> ||
                std::is_same_v<Tag, dingo::detail::array_layer_tag> ||
                std::is_same_v<Tag, dingo::detail::bounded_array_layer_tag>) {
    std::printf(" > - > -");
  } else {
    using identity = typename Tag::identity;
    std::printf(" > ");
    print_target<typename dingo::detail::shared_layer<identity>::type>();
    std::printf(" > ");
    print_target<std::conditional_t<
        dingo::detail::converts_unknown_bound<identity>(),
        typename dingo::detail::unknown_bound_layer<identity>::type, void>>();
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
  printer<dingo::detail::stack_t<T, Leaf>>::print();
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
      dingo::detail::leaf_traits_v<Leaf>();
  std::printf("%d %d %d %d\\n", traits.copyable, traits.movable,
              traits.default_constructible, traits.factory_made);
}
} // namespace

int main() {
ENTRIES
}
"""


def properties(layer, universe, object_layer=False):
    """A layer as the program reads it: its properties, the layers of the
    universe an owned value of it converts to, and those it converts to of an
    array of unknown bound, each as a mask. A layer of an object converts to
    none of the latter: the library classifies the layer of an array of a
    known bound as a layer of its own."""
    transfers = sum(1 << universe.index(name) for name in layer.transfers
                   if name in universe)
    unknown_bound = 0 if object_layer else sum(
        1 << universe.index(name) for name in layer.converts_unknown_bound
        if name in universe)
    return [OWNERS[layer.owner], int(layer.shares_element),
            int(layer.copyable), int(layer.wraps_value),
            int(layer.holds_element_only), int(layer.relocatable),
            int(layer.gives_element), int(layer.bounded),
            int(layer.may_be_empty), int(layer.has_empty_value),
            int(layer.adopts), int(layer.adopts_bounded),
            int(bool(layer.decay_layer)),
            int(layer.adopted_layers is not None),
            int(bool(layer.adopted_layers)), int(layer.rebinds),
            int(layer.observes), int(layer.qualifies), transfers, unknown_bound]


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
              f"{shape.value}/{bound.element}{list(bound.layers)}",
              semantics.shape_quals(shape, bound))
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
    is possible, as semantics.possible defines. One that converts to the layer
    of an array of unknown bound converts to itself."""
    emptiness = ((False, False), (True, False), (True, True))
    # A layer takes over a pointer to an array of a known bound only if it
    # takes over pointers.
    adoption = ((False, False), (True, False), (True, True))
    for index, (owner, shares, copyable, made, holds, relocatable, empty,
                adopts, rebinds, observes, qualifies, unknown_bound) in enumerate(
                    itertools.product(
                        semantics.Owner, (False, True), (False, True),
                        (False, True), (False, True), (False, True),
                        emptiness, adoption, (False, True),
                        (False, True), (False, True), (False, True))):
        name = f"made{index}"
        layer = semantics.Layer(
            name, owner, shares_element=shares, copyable=copyable,
            wraps_value=made, holds_element_only=holds,
            relocatable=relocatable, may_be_empty=empty[0],
            has_empty_value=empty[1], adopts=adopts[0],
            adopts_bounded=adopts[1], rebinds=rebinds,
            observes=observes, qualifies=qualifies,
            converts_unknown_bound=(name,) if unknown_bound else ())
        if semantics.possible(layer):
            yield layer


def lifetime(scope):
    return (semantics.Lifetime.REQUEST if scope.makes
            else semantics.Lifetime.STABLE)


def makes(layers, context):
    """Whether a scope makes a stack, as makes_storage defines for a stored
    form."""
    if context.scope.given or not layers:
        return True
    outer = semantics.LAYERS[layers[0]]
    if outer.bounded and not context.leaf.default_constructible:
        return False
    return not (context.scope.publishes_early and
                context.leaf.factory_made and outer.shares_element)


def swept():
    """Each layer a type could define, stored and requested in every stack of
    it, of a pointer and of an array, of every scope and leaf; one that
    converts to the layer of an array of unknown bound stored only around an
    array of a known bound."""
    for layer in made_layers():
        semantics.LAYERS[layer.name] = layer
        name = layer.name
        universe = LANGUAGE + (name,)
        stored = ((), (name,), (name, "array"), (name, "array2"),
                  (name, name))
        if layer.converts_unknown_bound:
            # A layer that converts to one of an array of unknown bound is its
            # twin that does not but where it holds an array of a known bound,
            # which only a stored object holds.
            stored = ((name, "array2"),)
        requested = [stack for depth in range(4)
                     for stack in itertools.product((name, "pointer"),
                                                    repeat=depth)]
        requested += [(name, "array"), (name, "array2")]
        goals = [(stack, element, f"{list(stack)}/{element}")
                 for stack in requested for element in ELEMENTS]
        # The element const, in a stack of the layer, of it and a pointer,
        # and of it twice.
        goals += [(stack, element, f"{list(stack)}/const {element}",
                   (0,) * len(stack) + (semantics.CONST,))
                  for stack in ((name,), (name, "pointer"), ("pointer", name),
                                (name, name))
                  for element in ELEMENTS]
        for scope, scope_traits in semantics.SCOPES.items():
            for leaf, traits in semantics.LEAVES.items():
                for abstract in (False, True):
                    context = semantics.Context(
                        traits, scope_traits, abstract, semantics.LIMITATIONS,
                        universe)
                    for layers in stored:
                        if not makes(layers, context):
                            continue
                        start = semantics.Object(
                            layers, "leaf",
                            (lifetime(scope_traits),) * (len(layers) + 1),
                            (semantics.Origin.MADE if scope_traits.makes
                             else semantics.Origin.BORROWED),
                            instance=True,
                            owns_pointee=scope_traits.makes and
                            bool(layers) and
                            semantics.LAYERS[layers[0]].adopted_layers
                            is not None)
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
                    layer, transfers=tuple(
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
                        start = semantics.Object(
                            layers, "leaf",
                            (lifetime(scope_traits),) * (len(layers) + 1),
                            (semantics.Origin.MADE if scope_traits.makes
                             else semantics.Origin.BORROWED),
                            instance=True,
                            owns_pointee=scope_traits.makes and
                            bool(layers) and
                            semantics.LAYERS[layers[0]].adopted_layers
                            is not None)
                        yield (f"{list(layers)}/{scope.value}/{leaf.value}/"
                               f"{'interface' if abstract else 'leaf'}/"
                               f"{[(layer.name, layer.transfers) for layer in (first, second)]}",
                               context, start, goals)


POSITIONS = list(Position)
ENTRIES = list(Entry)


def rules_cases(starts, kept=None):
    """The requests of the rules domain of some stored objects, each with the
    answers of the specification's fewest for every category, position,
    entry point, selection and container: those kept, an iterator of them,
    when given."""
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
                    answers = [next(kept)] if kept is not None else []
                    for category in (Category if kept is None else ()):
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
                                  len(shaped),
                                  semantics.shape_quals(shape, bound)))


def compare_pairs_part(arguments):
    """compare of a part of the pairs, in a process of its own."""
    program, universes = arguments
    return compare_answers(program, None, list(domain_cases(paired(universes))))


def compare_pairs(program, jobs):
    """compare of every pair, in parts compared in processes at once."""
    universes = list(pairs())
    size = 64
    parts = [(program, universes[index:index + size])
             for index in range(0, len(universes), size)]
    count, resolved, differ = 0, 0, []
    with multiprocessing.Pool(jobs) as pool:
        for found in pool.imap_unordered(compare_pairs_part, parts):
            count += found[0]
            resolved += found[1]
            differ += found[2][:20]
    return count, resolved, differ


# The qualifiers a check sweeps: of the object a request is of, every one;
# of a level below it, and of what a scope stores, those a type declares.
QUALS = (0, semantics.CONST, semantics.VOLATILE,
         semantics.CONST | semantics.VOLATILE)
INNER_QUALS = (0, semantics.CONST)


def qualified():
    """The stored objects of the standard layers of at most one layer, of
    every qualifier of each level a scope stores, with a leaf that copies a
    volatile one or not, and the requests of them of every qualifier of the
    object they are of and of each level below."""
    goals = []
    for shape in semantics.SHAPES:
        for bound in bounds():
            layers = semantics.SHAPES[shape] + bound.layers
            if len(layers) > 1 or shape.const_element:
                continue
            for quals in itertools.product(QUALS, *[INNER_QUALS] *
                                           len(layers)):
                goals.append((layers, bound.element,
                              f"{shape.value}/{bound.element}"
                              f"{list(bound.layers)}/{list(quals)}", quals))
    leaves = ((Leaf.COPYABLE, False), (Leaf.COPYABLE, True),
              (Leaf.MOVE_ONLY, False))
    for scope in Scope:
        for form in StoredForm:
            if len(semantics.FORMS[form]) > 1:
                continue
            for leaf, copies_volatile in leaves:
                traits = dataclasses.replace(semantics.LEAVES[leaf],
                                             copies_volatile=copies_volatile)
                for abstract in (False, True):
                    context = semantics.Context(
                        traits, semantics.SCOPES[scope], abstract)
                    if not semantics.makes_storage(form, context):
                        continue
                    depth = len(semantics.FORMS[form])
                    for quals in itertools.product(
                            QUALS[:3], *[INNER_QUALS] * depth):
                        yield (f"{scope.value}/{form.value}/{leaf.value}/"
                               f"{int(copies_volatile)}/"
                               f"{'interface' if abstract else 'leaf'}/"
                               f"{list(quals)}",
                               context,
                               semantics.source(scope, form,
                                                quals=tuple(quals)),
                               goals)


def consistent(domain):
    """The requests of a domain a container makes: of the abstract interface
    exactly where the context requests it. What a binding publishes reaches
    what its stored object does only for them."""
    for name, context, start, goals in domain:
        yield name, context, start, [
            goal for goal in goals
            if (goal[1] == "interface") == context.abstract]


def expected(reached, context, layers, element, quals=None,
             candidates=None):
    """The answers of the specification for a request, by its search of
    every object the stored object reaches."""
    if candidates is None:
        candidates = semantics.shape_candidates(reached)
    objects = candidates.get((element, layers), ())
    answers = []
    for category in Category:
        for planned_lifetime in LIFETIMES:
            # The cheapest, as the derivation is chosen: the copies and the moves
            # of the leaf, whether it may be absent, the objects kept and
            # made.
            costs = [semantics.affected(cost, effect)[:2] +
                     (int(object.absent),) +
                     semantics.affected(cost, effect)[2:]
                     for object, effect in semantics.satisfying(
                         objects, category, planned_lifetime, layers, context,
                         quals)
                     for cost in reached[object]]
            answers.append("%d,%d,%d,%d,%d" % min(costs) if costs else "-")
    return " ".join(answers)


def encode(context, start, layers, element, shape_depth=0, quals=None):
    """A request as the program reads it."""
    universe = order(context.universe)
    made = universe[len(LANGUAGE):]
    quals = quals or (0,) * (len(layers) + 1)
    numbers = [LIMITS[context.limits], int(context.leaf.copyable),
               int(context.leaf.movable),
               int(context.leaf.default_constructible),
               int(context.leaf.copies_volatile),
               int(context.scope.makes),
               int(context.scope.publishes_early), int(context.abstract),
               len(made)]
    for name in made:
        numbers += properties(semantics.LAYERS[name], universe)
    numbers += [len(start.layers),
                *(universe.index(name) for name in start.layers),
                ELEMENTS[start.element], start.lifetime.value,
                ORIGINS[start.origin], *start.quals,
                int(start.owns_pointee), len(layers),
                *(universe.index(name) for name in layers),
                ELEMENTS[element], *quals, shape_depth]
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
                                               universe, object_layer=True))
    return [" ".join(language), " ".join(standard_layers)]


def domain_cases(domain, mode=None, kept=None):
    """The requests of a domain, each its name, the answers of the
    specification and the line the program reads: the answers kept, an
    iterator of them, when given, which spares the search."""
    for name, context, start, goals in domain:
        # What the stored object reaches, or, through what a binding
        # publishes, what a request reaches from that.
        reached = (None if kept is not None else
                   semantics.requested(start, context) if mode == "split"
                   else semantics.reachable(start, context))
        candidates = (semantics.shape_candidates(reached) if reached is not None
                      else None)
        for layers, element, goal, *qualified in goals:
            quals = qualified[0] if qualified else None
            yield (f"{name}/{goal}",
                   next(kept) if kept is not None else
                   expected(reached, context, layers, element, quals,
                            candidates),
                   encode(context, start, layers, element, 0, quals))


def compare_answers(program, mode, cases):
    """The answers of the program to some requests: how many there are, how
    many answers of the specification resolve, and the requests whose
    answers differ."""
    run = subprocess.run([str(program)] + ([mode] if mode else []),
                         input="\n".join(header() + [line for _, _, line in
                                                     cases]) + "\n",
                         capture_output=True, text=True)
    answers = run.stdout.splitlines()
    if run.returncode != 0 or len(answers) != len(cases):
        print(run.stderr or f"{len(answers)} answers to {len(cases)} requests")
        return len(cases), 0, [(name, want, "") for name, want, _ in cases]
    resolved = sum(answer != "-" for _, want, _ in cases
                   for answer in want.split())
    return len(cases), resolved, [
        (name, want, got.strip())
        for (name, want, _), got in zip(cases, answers)
        if want != got.strip()][:20]


# The domains compared in parts, by name: each made of no arguments, with
# the mode the program resolves it in. The rules domain is made of its
# stored objects instead.
DOMAINS = {
    "standard": (standard, None),
    "swept": (swept, None),
    "standard_split": (lambda: consistent(standard()), "split"),
    "swept_split": (lambda: consistent(swept()), "split"),
    "qualified": (qualified, None),
    "qualified_split": (lambda: consistent(qualified()), "split"),
    "rules": (None, "rules"),
}

# The parts a domain is compared in, in processes at once: every PARTS-th
# stored object of it. As many whatever the processors, so that the answers
# of the specification kept of a part are of the same part.
PARTS = 64


def part_cases(domain, part, kept=None):
    """The requests of a part of a domain, with the answers kept, an iterator
    of them, when given."""
    if domain == "rules":
        starts = [(scope, form, leaf, interfaces, limits)
                  for scope in Scope for form in StoredForm
                  for leaf in semantics.LEAVES for interfaces in InterfaceSet
                  for limits in LIMITS]
        return rules_cases(starts[part::PARTS], kept)
    make, mode = DOMAINS[domain]
    return domain_cases((item for index, item in enumerate(make())
                     if index % PARTS == part), mode, kept)


def specification_hash():
    """What the answers of the specification are of: its sources and those
    of this check."""
    digest = hashlib.sha256()
    root = Path(__file__).parent
    for path in sorted(root.rglob("*.py")):
        digest.update(path.relative_to(root).as_posix().encode() + b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


def spec_answers(cache, domain, part):
    """The requests of a part of a domain, with the answers of the
    specification: those kept in the cache, a directory, while the sources of
    the specification are the same, and made again otherwise. The
    specification computes its answers far slower than the program, and a
    change of the library alone does not change its answers. Only the answers
    are kept: the names and the lines of the requests are made again, which
    needs no search."""
    directory, digest = cache
    path = directory / f"{domain}-{part}.zst"
    if path.exists():
        with zstd.open(path, "rt") as kept:
            if kept.readline().rstrip("\n") == digest:
                text = kept.read()
                answers = text.split("\n") if text else []
                try:
                    cases = list(part_cases(domain, part, iter(answers)))
                except RuntimeError:
                    # Fewer answers than requests: made again.
                    cases = None
                if cases is not None and len(cases) == len(answers):
                    return cases
    cases = list(part_cases(domain, part))
    written = path.with_suffix(".part")
    with zstd.open(written, "wt", level=9) as kept:
        kept.write(digest + "\n")
        kept.write("\n".join(want for _, want, _ in cases))
    written.replace(path)
    return cases


def compare_domain_part(arguments):
    """compare of a part of a domain, in a process of its own."""
    program, domain, part, cache = arguments
    return compare_answers(program, DOMAINS[domain][1],
                    spec_answers(cache, domain, part))


def compare(program, domain, jobs, cache):
    """The requests of a domain, how many of their answers resolve, and
    those whose answers differ: in parts compared in processes at once."""
    count, resolved, differ = 0, 0, []
    with multiprocessing.Pool(jobs) as pool:
        for found in pool.imap_unordered(
                compare_domain_part, [(program, domain, part, cache)
                               for part in range(PARTS)]):
            count += found[0]
            resolved += found[1]
            differ += found[2]
    return count, resolved, sorted(differ)[:20]


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
    cache = (options.work / "answers", specification_hash())
    cache[0].mkdir(exist_ok=True)
    domains = [
        ("standard layers", 14, "standard"),
        ("every layer a type could define", 14, "swept"),
        ("standard layers, through what a binding publishes", 14,
         "standard_split"),
        ("every layer a type could define, through what a binding publishes",
         14, "swept_split"),
        ("standard layers of every qualifier", 14, "qualified"),
        ("standard layers of every qualifier, through what a binding "
         "publishes", 14, "qualified_split"),
        ("the rules of requests, every stored object", 336, "rules")]
    domains = [(title, answers,
                lambda domain=domain: compare(program, domain, options.jobs,
                                              cache))
               for title, answers, domain in domains]
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
