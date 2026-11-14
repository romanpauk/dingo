// Answers the requests spec/semantics_check.py gives on its standard input by
// the semantics of the library (dingo/semantics/model.h), one line for each:
// for every category and lifetime, the fewest copies and moves of the leaf
// of what satisfies the request, or "-" when nothing does.
//
// A request is a line of numbers: the limitations, the leaf (copyable,
// movable), the scope (makes, publishes early), whether the abstract
// interface is requested, the stored object (its depth, its layers, its
// element, its lifetime, its origin) and the request (its depth, its layers,
// its element).

#include <dingo/semantics/model.h>

#include <cstdio>
#include <initializer_list>

using namespace dingo::semantics;

namespace {
bool read(unsigned &value) { return std::scanf("%u", &value) == 1; }

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

int main() {
  for (;;) {
    unsigned limits = 0, copyable = 0, movable = 0, makes = 0, early = 0,
             abstract = 0, start_element = 0, start_lifetime = 0,
             start_origin = 0, goal_element = 0;
    if (!read(limits)) {
      return 0;
    }
    thing start;
    request goal;
    if (!read(copyable) || !read(movable) || !read(makes) || !read(early) ||
        !read(abstract) || !read_layers(start.layers, start.depth) ||
        !read(start_element) || !read(start_lifetime) || !read(start_origin) ||
        !read_layers(goal.layers, goal.depth) || !read(goal_element)) {
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
    const context with{{copyable != 0, movable != 0},
                       {makes != 0, false, early != 0},
                       abstract != 0,
                       static_cast<std::uint8_t>(limits)};
    const search found = reach(start, with, goal);
    if (found.overflow) {
      std::printf("overflow\n");
      continue;
    }
    for (unsigned taken = 0; taken < 7; ++taken) {
      for (lifetime needed : {lifetime::request, lifetime::stable}) {
        const answer result =
            satisfy(found, goal, static_cast<category>(taken), needed, with);
        if (result.found) {
          std::printf("%u,%u ", result.fewest.copies, result.fewest.moves);
        } else {
          std::printf("- ");
        }
      }
    }
    std::printf("\n");
  }
}
