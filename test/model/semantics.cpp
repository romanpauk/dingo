// Answers the requests spec/semantics_check.py gives on its standard input by
// the semantics of the library (dingo/semantics/model.h), one line for each:
// for every category and lifetime, the fewest copies and moves of the leaf
// of what satisfies the request, or "-" when nothing does.
//
// The first line is the layers of C++ itself as the specification describes
// them, which have to be those of the library; the second the standard layers
// it describes, which have to be those the library classifies the standard
// wrappers as. A request is a line of
// numbers: the limitations, the leaf (copyable, movable), the scope (makes,
// publishes early), whether the abstract interface is requested, the layers
// of the universe after those of C++ itself, the stored object (its depth,
// its layers, its element, its lifetime, its origin) and the request (its
// depth, its layers, its element). A layer is its properties, as
// layer_kind declares them.

#include <dingo/request/classification.h>
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

bool read_kind(layer_kind &kind) {
  unsigned values[15] = {};
  for (unsigned &value : values) {
    if (!read(value)) {
      return false;
    }
  }
  kind.owns = static_cast<owner>(values[0]);
  bool *flags[] = {&kind.shares_element,  &kind.copyable,
                   &kind.made_from_value, &kind.holds_element_only,
                   &kind.relocatable,     &kind.gives_element,
                   &kind.bounded,         &kind.may_be_empty,
                   &kind.has_empty_value, &kind.adopts,
                   &kind.decays,          &kind.adopted,
                   &kind.adopted_array};
  for (unsigned index = 0; index < 13; ++index) {
    *flags[index] = values[index + 1] != 0;
  }
  kind.converts = values[14];
  return true;
}

bool same(const layer_kind &a, const layer_kind &b) {
  return a.owns == b.owns && a.shares_element == b.shares_element &&
         a.copyable == b.copyable && a.made_from_value == b.made_from_value &&
         a.holds_element_only == b.holds_element_only &&
         a.relocatable == b.relocatable && a.gives_element == b.gives_element &&
         a.bounded == b.bounded && a.may_be_empty == b.may_be_empty &&
         a.has_empty_value == b.has_empty_value && a.adopts == b.adopts &&
         a.decays == b.decays && a.adopted == b.adopted &&
         a.adopted_array == b.adopted_array && a.converts == b.converts;
}

// Whether a plan replays: each of its steps is a step the semantics take
// from the thing before it, to the thing after it, and the copies and the
// moves of the leaf of the steps and of how the last thing satisfies the
// request are those of the answer.
bool replays(const plan &made, const context &with, const answer &result) {
  if (!made.found) {
    return false;
  }
  cost so_far;
  for (std::size_t index = 0; index < made.count; ++index) {
    const thing &from = made.things[index];
    const transitions_list taken = steps(from, with);
    bool found = false;
    for (std::size_t each = 0; each < taken.count && !found; ++each) {
      found = taken.items[each].kind == made.steps[index] &&
              taken.items[each].reached == made.things[index + 1];
    }
    if (!found) {
      return false;
    }
    so_far = affected(so_far, effect_of(made.steps[index], from, with));
  }
  const thing &last = made.things[made.count];
  const bool anew = makes_leaf_anew(last, with);
  if (made.how == satisfaction::move) {
    so_far = affected(so_far, anew ? effect::move : effect::none);
  } else if (made.how == satisfaction::copy) {
    so_far = affected(so_far, anew ? effect::copy : effect::none);
  }
  return so_far == result.fewest;
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
  // With "plans", a line of plans for each request instead of its answers:
  // for each category, of the lifetime of the request, how it is satisfied
  // and each step with the layer it makes, or "-".
  const bool plans =
      count > 1 && arguments[1][0] == 'p' && arguments[1][1] == 'l';
  // With "rules", for each category, position, entry point, whether the
  // request is selected and whether the runtime container resolves it, the
  // fewest copies and moves of resolve, or "-".
  const bool rules =
      count > 1 && arguments[1][0] == 'r' && arguments[1][1] == 'u';
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
             makes = 0, early = 0, abstract = 0, made = 0, start_element = 0,
             start_lifetime = 0, start_origin = 0, goal_element = 0,
             shape_depth = 0;
    if (!read(limits)) {
      return 0;
    }
    universe layers;
    thing start;
    request goal;
    if (!read(copyable) || !read(movable) || !read(constructible) ||
        !read(makes) || !read(early) || !read(abstract) || !read(made) ||
        made > max_kinds - layers.count) {
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
        !read_layers(goal.layers, goal.depth) || !read(goal_element) ||
        !read(shape_depth)) {
      std::fprintf(stderr, "malformed request\n");
      return 1;
    }
    start.of = static_cast<element>(start_element);
    for (std::size_t index = 0; index <= start.depth; ++index) {
      start.lifetimes[index] = static_cast<lifetime>(start_lifetime);
    }
    start.from = static_cast<origin>(start_origin);
    start.instance = true;
    goal.of = static_cast<element>(goal_element);
    goal.shape_depth = static_cast<std::uint8_t>(shape_depth);
    const context with{{copyable != 0, movable != 0, constructible != 0},
                       {makes != 0, false, early != 0},
                       abstract != 0,
                       static_cast<std::uint8_t>(limits),
                       &layers};
    if (rules) {
      const search searched = reach(start, with, goal);
      for (unsigned taken = 0; taken < 7; ++taken) {
        for (unsigned at = 0; at < 4; ++at) {
          for (unsigned through = 0; through < 3; ++through) {
            for (bool selected : {false, true}) {
              for (bool runtime : {false, true}) {
                const answer result =
                    resolve(start, with, goal, static_cast<category>(taken),
                            {static_cast<position>(at),
                             static_cast<entry>(through), selected, runtime},
                            &searched);
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
    const search found = reach(start, with, goal);
    if (found.overflow) {
      std::printf("overflow\n");
      continue;
    }
    if (plans) {
      for (unsigned taken = 0; taken < 7; ++taken) {
        const answer result = satisfy(found, goal, static_cast<category>(taken),
                                      lifetime::request, with);
        const plan made = plan_of(found, result);
        if (!made.found) {
          std::printf("- ");
          continue;
        }
        std::printf("%u:%u,%u", static_cast<unsigned>(made.how),
                    made.fewest.copies, made.fewest.moves);
        for (std::size_t index = 0; index < made.count; ++index) {
          const thing &next = made.things[index + 1];
          std::printf(":%u/%u/%u", static_cast<unsigned>(made.steps[index]),
                      next.depth ? static_cast<unsigned>(next.layers[0]) : 255u,
                      next.depth > 1 ? static_cast<unsigned>(next.layers[1])
                                     : 255u);
        }
        std::printf(" ");
      }
      std::printf("\n");
      continue;
    }
    for (unsigned taken = 0; taken < 7; ++taken) {
      for (lifetime needed : {lifetime::request, lifetime::stable}) {
        const answer result =
            satisfy(found, goal, static_cast<category>(taken), needed, with);
        if (result.found && !replays(plan_of(found, result), with, result)) {
          std::printf("replay ");
        } else if (result.found) {
          std::printf("%u,%u ", result.fewest.copies, result.fewest.moves);
        } else {
          std::printf("- ");
        }
      }
    }
    std::printf("\n");
  }
}
