// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/type/type_conversion_traits.h>

struct wrapper {
  int value;
};

struct target {
  explicit target(int &);
};

namespace dingo {
template <> struct type_traits<wrapper> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = false;

  using value_type = int;

  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;

  static int &borrow(wrapper &source) { return source.value; }
};

template <> struct type_conversion_traits<target, wrapper> {
  static target convert(wrapper &);
};
} // namespace dingo

static_assert(dingo::detail::is_type_conversion_available_v<target, wrapper &,
                                                            dingo::borrow>);

// CHECK: type_conversion_traits specialization must declare
// CHECK-SAME: required_access<Source> as borrow or consume
