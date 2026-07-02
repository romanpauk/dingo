// REQUIRES: can-run-built-executables
// RUN: %dingo_cxx %s -o %t && %t

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>
#include <string>
#include <string_view>

#if __cplusplus >= 202002L

template <std::size_t N> struct test_fixed_string {
  char value[N];

  constexpr test_fixed_string(const char (&text)[N]) {
    for (std::size_t i = 0; i != N; ++i) {
      value[i] = text[i];
    }
  }

  constexpr bool operator==(const test_fixed_string &) const = default;
};

template <typename T, test_fixed_string Value> struct key_string {};

namespace dingo::detail {

// Internal regression hook: custom value selectors currently opt into request
// injection through detail traits. This is not a documented public extension.
template <typename T, ::test_fixed_string Value>
struct is_key_value<::key_string<T, Value>> : std::true_type {};

template <typename T, ::test_fixed_string Value>
struct key_value_traits<::key_string<T, Value>> {
  using type = T;

  static T make() {
    constexpr auto size = sizeof(Value.value) - 1;
    return T{std::string_view{Value.value, size}};
  }
};

template <typename T, ::test_fixed_string Value>
struct is_value_selector<::key_string<T, Value>> : std::true_type {
  using type = T;

  static T make() { return key_value_traits<::key_string<T, Value>>::make(); }
};

} // namespace dingo::detail

struct string_processor {
  virtual ~string_processor() = default;
  virtual int id() const = 0;
};

template <int Id> struct string_processor_impl : string_processor {
  int id() const override { return Id; }
};

// This proves an external request can feed a concrete std::string key into
// constructor injection without adding a public dingo::key_string API.
struct string_literal_consumer {
  explicit string_literal_consumer(
      dingo::request<string_processor &, key_string<std::string, "json">>
          selected)
      : processor(selected) {}

  string_processor &processor;
};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::string, string_processor>>;
};

int main() {
  dingo::container<traits> container;
  container.template register_indexed_type<
      dingo::scope<dingo::shared>, dingo::storage<string_processor_impl<7>>,
      dingo::interfaces<string_processor>>(std::string{"json"});

  auto consumer = container.template construct<string_literal_consumer>();

  return consumer.processor.id() == 7 ? 0 : 1;
}

#else

int main() { return 0; }

#endif
