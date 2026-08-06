#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/type_conversion_traits.h>

#include <memory>
#include <optional>
#include <variant>

struct payload {};

using nested_source =
    std::optional<std::variant<std::shared_ptr<payload>, payload *>>;
using optional_source = std::optional<std::shared_ptr<payload>>;

static_assert(dingo::detail::is_type_conversion_available_v<
              payload &, std::shared_ptr<payload> &>);
static_assert(dingo::detail::is_type_conversion_available_v<
              const payload *, const std::shared_ptr<payload> &>);
static_assert(dingo::detail::is_type_conversion_available_v<payload &,
                                                            optional_source &>);
static_assert(dingo::detail::is_type_conversion_available_v<const payload &,
                                                            nested_source &>);

int main() { return 0; }
