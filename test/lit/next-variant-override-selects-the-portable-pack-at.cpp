// Overriding the language-tier variants selects the portable implementation,
// so it is compiled and checked on compilers that have the builtin.
// REQUIRES: linux
// RUN: %dingo_next_cxx -fsyntax-only -DDINGO_VARIANT_PACK_AT_BUILTIN=0 \
// RUN:   -DDINGO_VARIANT_PACK_AT_INDEXING=0 %s

#include <dingo/core/base/mechanics.h>

#include <type_traits>

static_assert(std::is_same_v<dingo::core::pack_at_t<1, int, char>, char>);
static_assert(DINGO_VARIANT_PACK_AT_BUILTIN == 0);
static_assert(DINGO_VARIANT_PACK_AT_INDEXING == 0);

int main() {}
