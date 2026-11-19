// Answers the requests spec/semantics_check.py gives on its standard input by
// the semantics of the library (dingo/semantics/model.h), one line for each:
// for every category and lifetime, the cost of what satisfies the request at
// the least cost: the copies and the moves of the leaf, whether it may be
// absent, the objects kept and made; or "-" when nothing does.
//
// The first line is the layers of C++ itself as the specification describes
// them, which have to be those of the library; the second the standard layers
// it describes, which have to be those the library classifies the standard
// wrappers as. A request is a line of
// numbers: the limitations, the leaf (copyable, movable, default
// constructible, copies a volatile one), the scope (makes, publishes early),
// whether the abstract interface is requested, the layers of the universe
// after those of C++ itself, the stored object (its depth, its layers, its
// element, its lifetime, its origin, the qualifiers of each level, whether
// the request owns what its outer pointer points to) and the
// request (its depth, its layers, its element, the qualifiers of each level,
// the depth of its shape). A layer is its properties, as layer_kind declares
// them.

#include <dingo/request/classification.h>
#include <dingo/semantics/extension.h>
#include <dingo/semantics/model.h>

#include <cstdio>
#include <initializer_list>
#include <memory>
#include <optional>
#include <variant>

using namespace dingo::semantics;

namespace {
struct leaf {};
struct other {};

// The standard wrappers, as the specification names its standard layers
// after those of C++ itself.
using standard = dingo::detail::layer_universe<dingo::type_list<
    dingo::detail::wrapper_layer_tag<std::shared_ptr<leaf>>,
    dingo::detail::wrapper_layer_tag<std::unique_ptr<leaf>>,
    dingo::detail::wrapper_layer_tag<std::optional<leaf>>,
    dingo::detail::alternative_layer_tag<std::variant<leaf, other>, leaf>>>;

bool read(unsigned &value) { return std::scanf("%u", &value) == 1; }

// The qualifiers of each level of a stack of a depth.
bool read_quals(std::uint8_t *quals, std::size_t depth) {
  for (std::size_t index = 0; index <= depth; ++index) {
    unsigned value = 0;
    if (!read(value) || value > 3) {
      return false;
    }
    quals[index] = static_cast<std::uint8_t>(value);
  }
  return true;
}

bool read_kind(layer_kind &kind) {
  unsigned values[20] = {};
  for (unsigned &value : values) {
    if (!read(value)) {
      return false;
    }
  }
  kind.owns = static_cast<owner>(values[0]);
  bool *flags[] = {&kind.shares_element,  &kind.copyable,
                   &kind.wraps_value,     &kind.holds_element_only,
                   &kind.relocatable,     &kind.gives_element,
                   &kind.bounded,         &kind.may_be_empty,
                   &kind.has_empty_value, &kind.adopts,
                   &kind.adopts_bounded,  &kind.decays,
                   &kind.adopted,         &kind.adopted_array,
                   &kind.rebinds,         &kind.observes,
                   &kind.qualifies};
  for (unsigned index = 0; index < 17; ++index) {
    *flags[index] = values[index + 1] != 0;
  }
  kind.transfers = values[18];
  kind.converts_unknown_bound = values[19];
  return true;
}

bool same(const layer_kind &a, const layer_kind &b) {
  return a.owns == b.owns && a.shares_element == b.shares_element &&
         a.copyable == b.copyable && a.wraps_value == b.wraps_value &&
         a.holds_element_only == b.holds_element_only &&
         a.relocatable == b.relocatable && a.gives_element == b.gives_element &&
         a.bounded == b.bounded && a.may_be_empty == b.may_be_empty &&
         a.has_empty_value == b.has_empty_value && a.adopts == b.adopts &&
         a.adopts_bounded == b.adopts_bounded && a.decays == b.decays &&
         a.adopted == b.adopted && a.adopted_array == b.adopted_array &&
         a.rebinds == b.rebinds && a.observes == b.observes &&
         a.qualifies == b.qualifies && a.transfers == b.transfers &&
         a.converts_unknown_bound == b.converts_unknown_bound;
}

// The cost of the steps of a derivation, before how the last object satisfies
// the request, if each of its steps is a step the semantics take from the
// object before it, to the object after it. A derivation in two halves
// continues the states of a closure (closure) by an extension: its first step
// past them is from the layer source of the type and the form of the state
// before it, which the extension starts from.
struct stepped_cost {
  bool steps = false;
  cost fewest;
};

stepped_cost derivation_cost(const derivation &made, const context &with,
                             std::size_t closure) {
  cost so_far;
  for (std::size_t index = 0; index < made.count; ++index) {
    const bool extended =
        made.states[index] < closure && made.states[index + 1] >= closure;
    const object from = extended
                            ? source_object(made.objects[index],
                                            object_form(made.objects[index]))
                            : made.objects[index];
    // A step the semantics allow, or one a binding takes, as the release of
    // what a pointer it made points to.
    transitions_list taken = steps(from, with);
    each_binding_step(from, made.objects[0], with, taken);
    bool found = false;
    for (std::size_t each = 0; each < taken.count && !found; ++each) {
      found = taken.items[each].kind == made.steps[index] &&
              taken.items[each].reached == made.objects[index + 1];
    }
    if (!found) {
      return {};
    }
    so_far =
        stepped(so_far, made.steps[index], from, made.objects[index + 1], with);
  }
  return {true, so_far};
}

// What how the last object of a derivation satisfies the request does to the
// leaf.
effect satisfied_effect(const derivation &made, const context &with) {
  const bool anew = makes_leaf_anew(made.objects[made.count], with);
  if (made.access == access::move) {
    return anew ? effect::move : effect::none;
  }
  if (made.access == access::copy) {
    return anew ? effect::copy : effect::none;
  }
  return effect::none;
}

// Whether a derivation replays: each of its steps is a step the semantics take
// from the object before it, to the object after it, and the cost of the steps
// and of how the last object satisfies the request is that of the derivation.
bool replays(const derivation &made, const context &with,
             std::size_t closure = no_state) {
  if (!made.found) {
    return false;
  }
  const stepped_cost steps = derivation_cost(made, with, closure);
  return steps.steps &&
         affected(steps.fewest, satisfied_effect(made, with)) == made.fewest;
}

// The layers of a universe, each the same layer of it: the request and the
// binding are of one universe.
layer_map same_layers() {
  layer_map made;
  for (std::size_t index = 0; index < max_kinds; ++index) {
    made.indices[index] = static_cast<layer>(index);
  }
  return made;
}

// The context of a request type alone, as both containers derive its
// extensions in: of no scope, and of no leaf traits of an interface.
context request_context(const context &with) {
  context made = with;
  made.scope = {false, false, false};
  if (with.abstract) {
    made.leaf = {false, false};
  }
  return made;
}

// The derivations of a request in two halves, as both containers derive it,
// for each category and lifetime.
struct category_derivations {
  lifetime_pair<derivation> categories[7];
};

category_derivations derive_halves(const search &closed, const context &with,
                                   const request &goal) {
  category_derivations made;
  const context own = request_context(with);
  const layer_map layers_map = same_layers();
  for (unsigned taken = 0; taken < 7; ++taken) {
    for (lifetime needed : {lifetime::request, lifetime::stable}) {
      made.categories[taken][needed] =
          split_derivation(closed, with, goal, static_cast<category>(taken),
                           needed, own, goal, layers_map);
    }
  }
  return made;
}

// The fewest copies and moves of the leaf a request resolves with, as
// spec/semantics.py's fewest gives it: of the derivation both containers
// choose, as a container hands it over; none when it does not resolve.
// resolve<T &&>() and construct<T &&>() return the value moved out of what
// the request refers to. construct<T>() of a value it cannot resolve makes T
// of the leaf the container resolves by value: a new handle, optional or
// array of it, the rest of a bounded array value-initialized. The value is
// moved into it once it is resolved. The derivations of the request and of
// the leaf by value are given, made once for every rule.
answer resolve(const category_derivations &derived,
               const derivation &leaf_value, std::size_t closure,
               const context &with, const request &goal, category taken,
               const request_rules &rules) {
  const entry through =
      request_entry(rules.through, rules.position, rules.selected, taken);
  if (selector_rejects(goal, taken, rules.position, through, rules.selected,
                       with)) {
    return {};
  }
  const bool returned =
      rules.position == position::direct &&
      (through == entry::resolve || through == entry::construct) &&
      taken == category::rvalue_reference;
  const derivation &made =
      derived.categories[static_cast<unsigned>(taken)]
                        [required_lifetime(rules.position, through)];
  answer result;
  result.overflow = made.overflow;
  if (made.found && !made.overflow) {
    const stepped_cost steps = derivation_cost(made, with, closure);
    const bool anew = makes_leaf_anew(made.objects[made.count], with);
    result.found = steps.steps;
    result.overflow = !steps.steps;
    result.fewest = handed_over(steps.fewest,
                                returned && anew ? effect::move
                                                 : satisfied_effect(made, with),
                                rules.runtime);
  }
  if (result.found || result.overflow) {
    return result;
  }
  bool bounded = false;
  for (std::size_t index = 0; index < goal.shape_depth; ++index) {
    bounded = bounded || with.kind(goal.layers[index]).bounded;
  }
  if (rules.position == position::direct && through == entry::construct &&
      goal.element == element::leaf && goal.shape_depth != 0 &&
      (with.leaf.default_constructible || !bounded)) {
    category_derivations leaf;
    leaf.categories[static_cast<unsigned>(category::value)] = {leaf_value,
                                                               leaf_value};
    answer value = resolve(
        leaf, leaf_value, closure, with, request{}, category::value,
        {position::direct, entry::resolve, rules.selected, rules.runtime});
    if (value.found) {
      value.fewest.moves = static_cast<std::uint8_t>(value.fewest.moves + 1);
    }
    return value;
  }
  return result;
}

bool read_layers(layer *layers, std::uint8_t &depth) {
  unsigned count = 0;
  if (!read(count) || count > max_layers) {
    return false;
  }
  depth = static_cast<std::uint8_t>(count);
  for (unsigned index = 0; index < count; ++index) {
    unsigned value = 0;
    if (!read(value)) {
      return false;
    }
    layers[index] = static_cast<layer>(value);
  }
  return true;
}
} // namespace

int main(int count, char **arguments) {
  // With "derivations", a line of derives for each request instead of
  // its answers: for each category, of the lifetime of the request, how it
  // is satisfied and each step with the layer it makes, or "-". A derivation
  // is of what the binding publishes, as the containers derive it.
  const bool derivations =
      count > 1 && arguments[1][0] == 'd' && arguments[1][1] == 'e';
  // With "rules", for each category, position, entry point, whether the
  // request is selected and whether the runtime container resolves it, the
  // fewest copies and moves of resolve, of the derivation in two halves, or
  // "-".
  const bool rules =
      count > 1 && arguments[1][0] == 'r' && arguments[1][1] == 'u';
  // With "halves", the answers of the derivation in two halves, as both
  // containers derive it: of the closure of what the binding publishes, else
  // of the extension of a state of it of the request type alone.
  const bool halves = (count > 1 && arguments[1][0] == 'h') || derivations;
  const universe language;
  unsigned language_count = 0;
  if (!read(language_count) || language_count != language.count) {
    std::fprintf(stderr, "the layers of C++ itself differ in number\n");
    return 1;
  }
  for (unsigned index = 0; index < language_count; ++index) {
    layer_kind kind;
    if (!read_kind(kind) || !same(kind, language.kinds[index])) {
      std::fprintf(stderr, "the layer %u of C++ itself differs\n", index);
      return 1;
    }
  }
  const universe &classified = standard::value;
  unsigned standard_count = 0;
  if (!read(standard_count) ||
      standard_count != classified.count - language.count) {
    std::fprintf(stderr, "the standard layers differ in number\n");
    return 1;
  }
  for (unsigned index = 0; index < standard_count; ++index) {
    layer_kind kind;
    if (!read_kind(kind) ||
        !same(kind, classified.kinds[language.count + index])) {
      std::fprintf(stderr,
                   "the standard layer %u differs from its classification\n",
                   index);
      return 1;
    }
  }
  for (;;) {
    unsigned limits = 0, copyable = 0, movable = 0, constructible = 0,
             copies_volatile = 0, makes = 0, early = 0, abstract = 0, made = 0,
             start_element = 0, start_owns = 0, start_lifetime = 0,
             start_origin = 0, goal_element = 0, shape_depth = 0;
    if (!read(limits)) {
      return 0;
    }
    universe layers;
    object start;
    request goal;
    if (!read(copyable) || !read(movable) || !read(constructible) ||
        !read(copies_volatile) || !read(makes) || !read(early) ||
        !read(abstract) || !read(made) || made > max_kinds - layers.count) {
      std::fprintf(stderr, "malformed request\n");
      return 1;
    }
    for (unsigned index = 0; index < made; ++index) {
      layer_kind kind;
      if (!read_kind(kind)) {
        std::fprintf(stderr, "malformed layer\n");
        return 1;
      }
      layers.add(kind);
    }
    if (!read_layers(start.layers, start.depth) || !read(start_element) ||
        !read(start_lifetime) || !read(start_origin) ||
        !read_quals(start.quals, start.depth) || !read(start_owns) ||
        !read_layers(goal.layers, goal.depth) || !read(goal_element) ||
        !read_quals(goal.quals, goal.depth) || !read(shape_depth)) {
      std::fprintf(stderr, "malformed request\n");
      return 1;
    }
    start.element = static_cast<element>(start_element);
    for (std::size_t index = 0; index <= start.depth; ++index) {
      start.lifetimes[index] = static_cast<lifetime>(start_lifetime);
    }
    start.origin = static_cast<origin>(start_origin);
    start.owns_pointee = start_owns != 0;
    start.instance = true;
    goal.element = static_cast<element>(goal_element);
    goal.shape_depth = static_cast<std::uint8_t>(shape_depth);
    const context with{{copyable != 0, movable != 0, constructible != 0, false,
                        copies_volatile != 0},
                       {makes != 0, false, early != 0},
                       abstract != 0,
                       static_cast<std::uint8_t>(limits),
                       &layers};
    if (rules) {
      const search closed =
          closed_layer_sources(layer_sources(start, with), with);
      if (closed.overflow) {
        std::printf("overflow\n");
        continue;
      }
      const category_derivations derived = derive_halves(closed, with, goal);
      const derivation leaf_value = split_derivation(
          closed, with, request{}, category::value, lifetime::stable,
          request_context(with), request{}, same_layers());
      for (unsigned taken = 0; taken < 7; ++taken) {
        for (unsigned at = 0; at < 4; ++at) {
          for (unsigned through = 0; through < 3; ++through) {
            for (bool selected : {false, true}) {
              for (bool runtime : {false, true}) {
                const answer result =
                    resolve(derived, leaf_value, closed.count, with, goal,
                            static_cast<category>(taken),
                            {static_cast<position>(at),
                             static_cast<entry>(through), selected, runtime});
                if (result.overflow) {
                  std::printf("overflow ");
                } else if (result.found) {
                  std::printf("%u,%u ", result.fewest.copies,
                              result.fewest.moves);
                } else {
                  std::printf("- ");
                }
              }
            }
          }
        }
      }
      std::printf("\n");
      continue;
    }
    if (halves) {
      const search closed =
          closed_layer_sources(layer_sources(start, with), with);
      if (closed.overflow) {
        std::printf("overflow\n");
        continue;
      }
      const context own = request_context(with);
      const layer_map layers_map = same_layers();
      for (unsigned taken = 0; taken < 7; ++taken) {
        for (lifetime needed : {lifetime::request, lifetime::stable}) {
          // A derivation prints the lifetime of the request alone.
          if (derivations && needed != lifetime::request) {
            continue;
          }
          const derivation derived =
              split_derivation(closed, with, goal, static_cast<category>(taken),
                               needed, own, goal, layers_map);
          if (derived.overflow) {
            std::printf("overflow ");
          } else if (!derived.found) {
            std::printf("- ");
          } else if (derivations) {
            std::printf("%u:%u,%u", static_cast<unsigned>(derived.access),
                        derived.fewest.copies, derived.fewest.moves);
            for (std::size_t index = 0; index < derived.count; ++index) {
              const object &next = derived.objects[index + 1];
              std::printf(
                  ":%u/%u/%u", static_cast<unsigned>(derived.steps[index]),
                  next.depth ? static_cast<unsigned>(next.layers[0]) : 255u,
                  next.depth > 1 ? static_cast<unsigned>(next.layers[1])
                                 : 255u);
            }
            std::printf(" ");
          } else if (!replays(derived, with, closed.count)) {
            std::printf("replay ");
          } else {
            std::printf("%u,%u,%u,%u,%u ", derived.fewest.copies,
                        derived.fewest.moves, derived.absent ? 1u : 0u,
                        derived.fewest.kept, derived.fewest.made);
          }
        }
      }
      std::printf("\n");
      continue;
    }
    const search found = reach(start, with, goal);
    if (found.overflow) {
      std::printf("overflow\n");
      continue;
    }
    for (unsigned taken = 0; taken < 7; ++taken) {
      for (lifetime needed : {lifetime::request, lifetime::stable}) {
        const answer result =
            satisfy(found, goal, static_cast<category>(taken), needed, with);
        if (result.found && !replays(answer_derivation(found, result), with)) {
          std::printf("replay ");
        } else if (result.found) {
          std::printf("%u,%u,%u,%u,%u ", result.fewest.copies,
                      result.fewest.moves, result.absent ? 1u : 0u,
                      result.fewest.kept, result.fewest.made);
        } else {
          std::printf("- ");
        }
      }
    }
    std::printf("\n");
  }
}
