#include <dingo/factory/constructor.h>
#include <dingo/static/activation_set.h>
#include <dingo/storage/shared.h>

#include <array>
#include <memory>
#include <type_traits>

struct stable_payload {};
struct empty_payload {};
using fixed_array = std::array<int, 8>;

using stable_registration =
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<std::shared_ptr<stable_payload>>>;
using stable_model = dingo::detail::binding_model<stable_registration>;
using stable_binding = dingo::detail::binding<stable_payload, stable_model>;

namespace dingo {
namespace detail {

template <>
struct constructor_detection<int, constructor_shape, list_initialization,
                             DINGO_CONSTRUCTOR_DETECTION_ARGS>;

template <>
struct constructor_detection<::empty_payload, constructor_shape,
                             list_initialization,
                             DINGO_CONSTRUCTOR_DETECTION_ARGS>;

template <>
struct constructor_arity<::fixed_array, constructor_shape, list_initialization,
                         DINGO_CONSTRUCTOR_DETECTION_ARGS>;

template <>
struct request_binding_resolutions<::stable_payload &, ::stable_payload,
                                   typename ::stable_model::storage_type,
                                   ::stable_payload &>;

template <>
struct binding_cache_types_impl<::stable_model, false, ::stable_payload>;

template <> struct binding_temporary_types<::stable_binding, false>;

} // namespace detail
} // namespace dingo

struct available_conversion {
  static constexpr bool available = true;
};

struct invalid_conversion_tail;

using selected_conversion = typename dingo::detail::select_conversion_candidate<
    available_conversion, invalid_conversion_tail>::type;
using selected_resolution =
    typename dingo::detail::binding_supports_request<stable_payload &,
                                                     stable_binding>::type;
using stable_cache = dingo::detail::binding_cache_types_t<stable_model>;
using stable_temporaries =
    dingo::detail::binding_temporary_types_t<stable_binding>;

static_assert(std::is_same_v<selected_conversion, available_conversion>);
static_assert(!std::is_void_v<selected_resolution>);
static_assert(std::is_same_v<stable_cache, dingo::type_list<>>);
static_assert(std::is_same_v<stable_temporaries, dingo::type_list<>>);
static_assert(dingo::constructor<int>::arity == 0);
static_assert(dingo::constructor<empty_payload>::arity == 0);
static_assert(dingo::constructor_detection<fixed_array>::arity == 8);

int main() { return 0; }
