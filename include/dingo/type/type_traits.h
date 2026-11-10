//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/exceptions.h>
#include <dingo/base/meta.h>
#include <dingo/base/type_list.h>

#include <array>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {
struct unique;
struct shared;
struct external;

template <class T> struct exact_lookup;
template <class T> struct collection_traits;

// The wrapper semantics a type_traits specialization names. The library knows
// a wrapper only by its semantics and by the operations of its type_traits; the
// standard wrappers are described below as a user's wrapper is.
namespace semantics {
// A handle to an object it does not own, such as a raw pointer.
struct observer {
  using semantics = observer;
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = false;
};

// The sole owner of an object, such as std::unique_ptr. A sole owner hands its
// object to the sharing handle of its family, share_t.
struct sole_owner {
  using semantics = sole_owner;
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = true;
};

// A handle sharing the ownership of an object, such as std::shared_ptr.
struct shared_owner {
  using semantics = shared_owner;
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = true;
};

// A value that holds an object in place or none, such as std::optional.
struct optional_value {
  using semantics = optional_value;
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_owning_handle = false;
};

// A value that holds one of several alternatives, such as std::variant. It is
// not a wrapper of one element, so it is not enabled as one.
struct alternative_value {
  using semantics = alternative_value;
  static constexpr bool enabled = false;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_owning_handle = false;
};
} // namespace semantics

// Makes a type a wrapper. A specialization derives from the semantics of the
// wrapper and provides
//
//   value_type     the element type
//   rebind_t<U>    the same wrapper of the element U
//   get(w)         the address of the element, or null
//   borrow(w)      the element, for a wrapper whose element can be borrowed
//                  as an object (not an array)
//   empty(w)       whether the wrapper holds no element
//   make(args...)  a wrapper of an element made from args
//   makes_nested   optional, false by default: whether make(args...) makes
//                  an element that is a wrapper of what makes its element too,
//                  so that a wrapper of wrappers is made at once of what
//                  makes its innermost element
//   share_t<U>     for a sole owner: the sharing handle of the element U
//   wrap(v), make_empty()
//                  for an optional value: a wrapper of v, and an empty one
//
// An alternative value provides instead
//
//   alternatives        the type_list of its alternatives
//   selected_type<S>    the alternative a source S constructs
//   wrap<A>(v)          a value holding v as the alternative A
//   get<A>(value)       the address of the alternative A, or null
template <typename T, typename = void> struct type_traits {
  static constexpr bool enabled = false;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_owning_handle = false;
};

template <typename T> struct type_traits<const T> : type_traits<T> {};

namespace detail {
template <typename T, typename = void>
struct is_value_borrowable : std::false_type {};

template <typename T>
struct is_value_borrowable<
    T, std::void_t<decltype(type_traits<T>::borrow(std::declval<T &>()))>>
    : std::true_type {};
} // namespace detail

// Whether the element of a wrapper can be borrowed as an object.
template <typename T>
inline constexpr bool is_value_borrowable_v =
    detail::is_value_borrowable<std::remove_cv_t<T>>::value;

namespace detail {
// A collection is copied with its elements.
template <typename T> constexpr bool copy_constructible() {
  using value_type = remove_cvref_t<T>;
  if constexpr (collection_traits<value_type>::is_collection) {
    return copy_constructible<
        typename collection_traits<value_type>::resolve_type>();
  } else {
    return std::is_copy_constructible_v<value_type>;
  }
}
} // namespace detail

template <typename T>
inline constexpr bool is_copy_constructible_v = detail::copy_constructible<T>();

namespace detail {
// Whether the type_traits of T declare Semantics.
template <typename T, typename Semantics, typename = void>
struct has_semantics : std::false_type {};

template <typename T, typename Semantics>
struct has_semantics<T, Semantics,
                     std::void_t<typename type_traits<T>::semantics>>
    : std::is_same<typename type_traits<T>::semantics, Semantics> {};
} // namespace detail

template <typename T>
inline constexpr bool is_alternative_type_v =
    detail::has_semantics<std::remove_cv_t<T>,
                          semantics::alternative_value>::value;

// An optional value: a wrapper that holds one element or none.
template <typename T>
inline constexpr bool is_optional_type_v =
    detail::has_semantics<std::remove_cv_t<std::remove_reference_t<T>>,
                          semantics::optional_value>::value;

namespace detail {
// Whether a value of T can exist: an abstract class cannot, nor can a value
// that holds an abstract element in place. References and pointers can. It is
// decided without instantiating T.
template <typename... Types>
constexpr bool is_formable_list(type_list<Types...>);

template <typename T> constexpr bool is_formable() {
  using type = std::remove_cv_t<T>;
  if constexpr (has_semantics<type, semantics::optional_value>::value) {
    return is_formable<typename type_traits<type>::value_type>();
  } else if constexpr (is_alternative_type_v<type>) {
    return is_formable_list(typename type_traits<type>::alternatives{});
  } else if constexpr (std::is_class_v<type>) {
    return !std::is_abstract_v<type>;
  } else {
    return true;
  }
}

template <typename... Types>
constexpr bool is_formable_list(type_list<Types...>) {
  return (is_formable<Types>() && ...);
}

template <typename T> inline constexpr bool is_formable_v = is_formable<T>();

template <typename Type, typename = void>
struct alternative_type_alternatives {};

template <typename Type>
struct alternative_type_alternatives<
    Type, std::enable_if_t<is_alternative_type_v<Type>>> {
  using type = typename type_traits<std::remove_cv_t<Type>>::alternatives;

  static_assert(type_list_size_v<type_list_unique_t<type>> ==
                    type_list_size_v<type>,
                "type_traits<T>::alternatives must not contain "
                "duplicate types");
};

template <typename Type>
using alternative_type_alternatives_t =
    typename alternative_type_alternatives<Type>::type;

template <typename T> struct variant_conversion_array {
  T value[1];
};

template <typename Alternative, typename Source, typename = void>
struct variant_conversion_candidate {
  static void select();
};

template <typename Alternative, typename Source>
struct variant_conversion_candidate<
    Alternative, Source,
    std::void_t<decltype(variant_conversion_array<Alternative>{
        {std::declval<Source>()}})>> {
  static type_list_iterator<Alternative> select(Alternative);
};

template <typename Source, typename... Alternatives>
struct variant_conversion_candidates
    : variant_conversion_candidate<Alternatives, Source>... {
  using variant_conversion_candidate<Alternatives, Source>::select...;
};

template <typename Source, typename... Alternatives>
using selected_variant_alternative_t =
    typename decltype(variant_conversion_candidates<Source, Alternatives...>::
                          select(std::declval<Source>()))::type;

template <typename Type, typename Selected, typename = void>
struct alternative_type_count : std::integral_constant<size_t, 0> {};

template <typename Type, typename Selected>
struct alternative_type_count<Type, Selected,
                              std::enable_if_t<is_alternative_type_v<Type>>>
    : type_list_count<alternative_type_alternatives_t<Type>,
                      std::remove_cv_t<Selected>> {};

} // namespace detail

template <typename... Alternatives>
struct type_traits<std::variant<Alternatives...>>
    : semantics::alternative_value {
  using alternatives = type_list<Alternatives...>;

  template <typename Source>
  using selected_type =
      detail::selected_variant_alternative_t<Source, Alternatives...>;

  template <typename Selected, typename Value>
  static std::variant<Alternatives...> wrap(Value &&value) {
    return std::variant<Alternatives...>(std::in_place_type<Selected>,
                                         std::forward<Value>(value));
  }

  template <typename Selected>
  static Selected *get(std::variant<Alternatives...> &value) {
    return std::get_if<Selected>(&value);
  }

  template <typename Selected>
  static const Selected *get(const std::variant<Alternatives...> &value) {
    return std::get_if<Selected>(&value);
  }
};

template <typename Type, typename Interface>
inline constexpr bool is_alternative_type_interface_compatible_v =
    detail::alternative_type_count<std::remove_cv_t<Type>,
                                   std::remove_cv_t<Interface>>::value == 1;

namespace detail {
template <typename T, typename... Args> T make_nested(Args &&...args);

// Whether T[N]{args...} is well-formed: its first elements are copy-initialized
// from the arguments, and the rest value-initialized.
template <typename T, size_t N, typename, typename... Args>
struct is_bounded_array_initializable : std::false_type {};

template <typename T, size_t N, typename... Args>
struct is_bounded_array_initializable<
    T, N, std::void_t<decltype(std::array<T, N>{{std::declval<Args>()...}})>,
    Args...> : std::true_type {};

template <typename T, size_t N, typename... Args>
inline constexpr bool is_bounded_array_initializable_v =
    is_bounded_array_initializable<T, N, void, Args...>::value;

template <typename T, size_t N, typename... Args>
T *make_bounded_array(Args &&...args) {
  static_assert(sizeof...(Args) <= N,
                "too many initializers for bounded array construction");
  if constexpr (!is_bounded_array_initializable_v<T, N, Args &&...>) {
    static_assert(is_bounded_array_initializable_v<T, N, Args &&...>,
                  "bounded array elements cannot be initialized: the first "
                  "are copy-initialized from the arguments and the rest "
                  "value-initialized; an array of elements that cannot be "
                  "is made by a factory");
    return nullptr;
  } else {
    return new T[N]{std::forward<Args>(args)...};
  }
}

template <typename T, size_t N, typename... Args>
void construct_bounded_array(void *ptr, Args &&...args) {
  static_assert(sizeof...(Args) <= N,
                "too many initializers for bounded array construction");
  if constexpr (!is_bounded_array_initializable_v<T, N, Args &&...>) {
    static_assert(is_bounded_array_initializable_v<T, N, Args &&...>,
                  "bounded array elements cannot be initialized: the first "
                  "are copy-initialized from the arguments and the rest "
                  "value-initialized; an array of elements that cannot be "
                  "is made by a factory");
  } else {
    new (ptr) T[N]{std::forward<Args>(args)...};
  }
}

template <typename T, typename... Args> T *make_dynamic_array(Args &&...args) {
  static_assert(sizeof...(Args) > 0,
                "dynamic arrays require an explicit size via a custom "
                "factory or element initializers");
  if constexpr (!is_bounded_array_initializable_v<T, sizeof...(Args),
                                                  Args &&...>) {
    static_assert(
        is_bounded_array_initializable_v<T, sizeof...(Args), Args &&...>,
        "dynamic array elements cannot be initialized: each is "
        "copy-initialized from its argument");
    return nullptr;
  } else {
    return new T[sizeof...(Args)]{std::forward<Args>(args)...};
  }
}

// Whether a handle can be made of Args, as its type_traits declares with
// makeable<Args...>; one that declares nothing is made of what makes its
// element.
template <typename Handle, typename Args, typename = void>
struct declared_makeable : std::true_type {};

template <typename Handle, typename... Args>
struct declared_makeable<
    Handle, type_list<Args...>,
    std::void_t<decltype(type_traits<Handle>::template makeable<Args...>)>>
    : std::bool_constant<type_traits<Handle>::template makeable<Args...>> {};

template <typename Handle, typename... Args>
struct handle_makeable : declared_makeable<Handle, type_list<Args...>> {};

template <typename T, size_t> using repeated_argument_t = T;

template <typename Handle, typename Argument, typename Indexes>
struct handle_makeable_by;

template <typename Handle, typename Argument, size_t... Is>
struct handle_makeable_by<Handle, Argument, std::index_sequence<Is...>>
    : handle_makeable<Handle, repeated_argument_t<Argument, Is>...> {};

template <typename Type, typename U, typename = void>
struct wrapper_rebind_leaf {
  using type = copy_cv_t<Type, U>;
};

template <typename Type, size_t N, typename U>
struct wrapper_rebind_leaf<Type[N], U, void> {
  using type = typename wrapper_rebind_leaf<Type, U>::type[N];
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<Type[], U, void> {
  using type = typename wrapper_rebind_leaf<Type, U>::type[];
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<
    Type, U,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type>>> {
  using type = typename type_traits<Type>::template rebind_t<
      typename wrapper_rebind_leaf<typename type_traits<Type>::value_type,
                                   U>::type>;
};

template <typename Type, typename U>
using wrapper_rebind_leaf_t = typename wrapper_rebind_leaf<Type, U>::type;

template <typename Handle, typename T, typename U, typename = void>
struct smart_array_pointer_types {
  using type = type_list<U *, Handle *>;
};

template <typename Handle, typename T, typename U>
struct smart_array_pointer_types<Handle, T, U,
                                 std::enable_if_t<std::is_array_v<T>>> {
  using type =
      type_list<exact_lookup<typename wrapper_rebind_leaf<T, U>::type> *,
                Handle *>;
};

template <typename Handle, typename = void> struct wrapper_storage_types {
  using lvalue_reference_types = type_list<Handle &>;
  using pointer_types = type_list<Handle *>;
  using copyable_value_types =
      std::conditional_t<std::is_copy_constructible_v<Handle>,
                         type_list<Handle>, type_list<>>;
};

// A handle or an optional is walked through to its element too: the chain is
// walked once, collecting every list together instead of recursing over it
// separately for each.
template <typename Handle>
struct wrapper_storage_types<
    Handle, std::enable_if_t<
                type_traits<Handle>::enabled &&
                (type_traits<Handle>::is_pointer_like ||
                 has_semantics<Handle, semantics::optional_value>::value)>> {
private:
  using next = wrapper_storage_types<typename type_traits<Handle>::value_type>;
  using copyable_handle_types =
      std::conditional_t<std::is_copy_constructible_v<Handle>,
                         type_list<Handle>, type_list<>>;

public:
  using lvalue_reference_types =
      type_list_cat_t<type_list<Handle &>,
                      typename next::lvalue_reference_types>;
  using pointer_types =
      type_list_cat_t<type_list<Handle *>, typename next::pointer_types>;
  using copyable_value_types =
      type_list_cat_t<copyable_handle_types,
                      typename next::copyable_value_types>;
};

// Whether every layer of a wrapper can exist: a handle can hold an abstract
// element, a value that holds its element in place cannot. It is decided
// without instantiating the wrapper.
template <typename Type> constexpr bool is_wrapper_formable() {
  if constexpr (type_traits<Type>::enabled && !std::is_pointer_v<Type>) {
    using element = typename type_traits<Type>::value_type;
    constexpr bool element_formable = is_wrapper_formable<element>();
    if constexpr (type_traits<Type>::is_pointer_like) {
      return element_formable;
    } else {
      return is_formable_v<Type> && element_formable;
    }
  } else {
    return true;
  }
}

template <typename Type>
inline constexpr bool is_wrapper_formable_v = is_wrapper_formable<Type>();

// An array, or an owning handle whose element cannot be borrowed as an object,
// which owns an array; its exact interface is the array.
template <typename Type, typename = void> struct array_like {
  static constexpr bool value = std::is_array_v<Type>;
  using exact_interface = Type;
};

template <typename Handle>
struct array_like<Handle,
                  std::enable_if_t<type_traits<Handle>::is_owning_handle &&
                                   !is_value_borrowable_v<Handle>>> {
  static constexpr bool value = true;
  using exact_interface = typename type_traits<Handle>::value_type;
};

template <typename Type>
inline constexpr bool is_array_like_type_v = array_like<Type>::value;

template <typename Type>
using array_like_exact_interface_type_t =
    typename array_like<Type>::exact_interface;
} // namespace detail

// A pointer observes its pointee, which it borrows unless it is void.
template <typename T> struct type_traits<T *> : semantics::observer {
  using value_type = T;
  template <typename U> using rebind_t = U *;

  static T *get(T *ptr) { return ptr; }
  template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
  static U &borrow(T *ptr) {
    return *ptr;
  }
  static bool empty(T *ptr) { return ptr == nullptr; }
};

template <typename Array, typename Deleter>
struct type_traits<
    std::unique_ptr<Array, Deleter>,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>>
    : semantics::sole_owner {
  using value_type = std::remove_extent_t<Array>;
  template <typename U>
  using rebind_t = std::unique_ptr<U[], std::default_delete<U[]>>;
  template <typename U> using share_t = std::shared_ptr<U[]>;

  static auto *get(const std::unique_ptr<Array, Deleter> &wrapper) {
    return wrapper.get();
  }
  static bool empty(const std::unique_ptr<Array, Deleter> &wrapper) {
    return wrapper.get() == nullptr;
  }

  // The array is made of an element of each argument, copy-initialized.
  template <typename... Args>
  static constexpr bool makeable =
      detail::is_bounded_array_initializable_v<value_type, sizeof...(Args),
                                               Args...>;

  template <typename... Args>
  static std::unique_ptr<Array, Deleter> make(Args &&...args) {
    return std::unique_ptr<Array, Deleter>(
        detail::make_dynamic_array<value_type>(std::forward<Args>(args)...));
  }
};

template <typename T, typename Deleter>
struct type_traits<std::unique_ptr<T, Deleter>,
                   std::enable_if_t<!std::is_array_v<T>>>
    : semantics::sole_owner {
  using value_type = T;
  template <typename U>
  using rebind_t = std::unique_ptr<U, std::default_delete<U>>;
  template <typename U> using share_t = std::shared_ptr<U>;
  static constexpr bool makes_nested = true;

  static T *get(const std::unique_ptr<T, Deleter> &wrapper) {
    return wrapper.get();
  }
  static T &borrow(const std::unique_ptr<T, Deleter> &wrapper) {
    return *wrapper;
  }
  // A consumed handle gives its element away, as it owns it alone; one with
  // a deleter of its own keeps it, as its deleter may expect to find it.
  template <
      typename D = Deleter,
      typename = std::enable_if_t<std::is_same_v<D, std::default_delete<T>>>>
  static T &&borrow(std::unique_ptr<T, Deleter> &&wrapper) {
    return std::move(*wrapper);
  }
  static bool empty(const std::unique_ptr<T, Deleter> &wrapper) {
    return wrapper.get() == nullptr;
  }

  template <typename... Args>
  static std::unique_ptr<T, Deleter> make(Args &&...args) {
    // An optional is made in place, as a shared_ptr makes one.
    if constexpr (is_optional_type_v<T> &&
                  std::is_constructible_v<T, std::in_place_t, Args...>)
      return std::unique_ptr<T, Deleter>(
          new T(std::in_place, std::forward<Args>(args)...));
    else if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::unique_ptr<T, Deleter>(
          new T(detail::make_nested<T>(std::forward<Args>(args)...)));
    // Work around direct-initialization in smart-pointer-backed factories.
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::unique_ptr<T, Deleter>(new T(std::forward<Args>(args)...));
    else
      return std::unique_ptr<T, Deleter>(new T{std::forward<Args>(args)...});
  }
};

template <typename Array>
struct type_traits<
    std::shared_ptr<Array>,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>>
    : semantics::shared_owner {
  using value_type = std::remove_extent_t<Array>;
  template <typename U> using rebind_t = std::shared_ptr<U[]>;

  static auto *get(const std::shared_ptr<Array> &wrapper) {
    return wrapper.get();
  }
  static bool empty(const std::shared_ptr<Array> &wrapper) {
    return wrapper.get() == nullptr;
  }

  // The array is made of an element of each argument, copy-initialized.
  template <typename... Args>
  static constexpr bool makeable =
      detail::is_bounded_array_initializable_v<value_type, sizeof...(Args),
                                               Args...>;

  template <typename... Args>
  static std::shared_ptr<Array> make(Args &&...args) {
    return std::shared_ptr<Array>(
        detail::make_dynamic_array<value_type>(std::forward<Args>(args)...));
  }
};

// A shared_ptr of a bounded array owns the array, as one of a dynamic array
// does, and releases it with delete[]. Its size is part of its type, so it is
// made without a size from the factory: its elements are made of the
// arguments, and the rest value-initialized.
template <typename Array>
struct type_traits<
    std::shared_ptr<Array>,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> != 0)>>
    : semantics::shared_owner {
  using value_type = std::remove_extent_t<Array>;
  template <typename U>
  using rebind_t = std::shared_ptr<U[std::extent_v<Array, 0>]>;

  static auto *get(const std::shared_ptr<Array> &wrapper) {
    return wrapper.get();
  }
  static bool empty(const std::shared_ptr<Array> &wrapper) {
    return wrapper.get() == nullptr;
  }

  // The first elements are copy-initialized from the arguments, the rest
  // value-initialized.
  template <typename... Args>
  static constexpr bool makeable = detail::is_bounded_array_initializable_v<
      value_type, std::extent_v<Array, 0>, Args...>;

  template <typename... Args>
  static std::shared_ptr<Array> make(Args &&...args) {
    return std::shared_ptr<Array>(
        detail::make_bounded_array<value_type, std::extent_v<Array, 0>>(
            std::forward<Args>(args)...));
  }
};

template <typename T>
struct type_traits<std::shared_ptr<T>, std::enable_if_t<!std::is_array_v<T>>>
    : semantics::shared_owner {
  using value_type = T;
  template <typename U> using rebind_t = std::shared_ptr<U>;
  static constexpr bool makes_nested = true;

  static T *get(const std::shared_ptr<T> &wrapper) { return wrapper.get(); }
  static T &borrow(const std::shared_ptr<T> &wrapper) { return *wrapper; }
  static bool empty(const std::shared_ptr<T> &wrapper) {
    return wrapper.get() == nullptr;
  }

  template <typename... Args> static std::shared_ptr<T> make(Args &&...args) {
    // An optional is made in place of the arguments of its element, which
    // may be moved neither in it nor out of it.
    if constexpr (is_optional_type_v<T> &&
                  std::is_constructible_v<T, std::in_place_t, Args...>)
      return std::make_shared<T>(std::in_place, std::forward<Args>(args)...);
    else if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::make_shared<T>(
          detail::make_nested<T>(std::forward<Args>(args)...));
    // Work around direct-initialization in std::make_shared().
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::make_shared<T>(std::forward<Args>(args)...);
    else
      return std::shared_ptr<T>(new T{std::forward<Args>(args)...});
  }
};

template <typename T>
struct type_traits<std::optional<T>> : semantics::optional_value {
  using value_type = T;
  template <typename U> using rebind_t = std::optional<U>;
  static constexpr bool makes_nested = true;

  static T *get(std::optional<T> &wrapper) {
    return wrapper ? std::addressof(*wrapper) : nullptr;
  }
  static const T *get(const std::optional<T> &wrapper) {
    return wrapper ? std::addressof(*wrapper) : nullptr;
  }
  static T &borrow(std::optional<T> &wrapper) { return wrapper.value(); }
  static const T &borrow(const std::optional<T> &wrapper) {
    return wrapper.value();
  }
  // A consumed optional gives its element away.
  static T &&borrow(std::optional<T> &&wrapper) {
    return std::move(wrapper.value());
  }
  static bool empty(const std::optional<T> &wrapper) {
    return !wrapper.has_value();
  }

  template <typename Value> static std::optional<T> wrap(Value &&value) {
    return std::optional<T>(std::in_place, std::forward<Value>(value));
  }
  static std::optional<T> make_empty() { return std::nullopt; }

  template <typename... Args> static std::optional<T> make(Args &&...args) {
    // An optional of an optional is made in place too, as its element may
    // be moved neither in it nor out of it.
    if constexpr (is_optional_type_v<T> &&
                  std::is_constructible_v<T, std::in_place_t, Args...>)
      return std::optional<T>{std::in_place, std::in_place,
                              std::forward<Args>(args)...};
    else if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::optional<T>{
          std::in_place, detail::make_nested<T>(std::forward<Args>(args)...)};
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::optional<T>{std::in_place, std::forward<Args>(args)...};
    else
      return std::optional<T>{std::in_place, T{std::forward<Args>(args)...}};
  }
};

namespace detail {
// A nested wrapper made of the arguments of its element, or moved when it is
// given as itself.
template <typename T, typename... Args> T make_nested(Args &&...args) {
  if constexpr (sizeof...(Args) == 1 &&
                (std::is_same_v<remove_cvref_t<Args>, T> && ...)) {
    return T(std::forward<Args>(args)...);
  } else {
    return type_traits<T>::make(std::forward<Args>(args)...);
  }
}
} // namespace detail

// The element a handle names its family by in owning_handles.
struct handle_element {};

// The owning handles a scope makes of what it makes for a request: of a value
// it makes, of a pointer it makes, and of an array, each family named by a
// handle of handle_element. A scope rebinds them with rebind_t to what it
// makes, leaving out those whose rebound handle no type_traits describes. A
// user adds a family by specializing owning_handles<>, listing these with it.
template <typename = void> struct owning_handles {
  using type = type_list<std::unique_ptr<handle_element>,
                         std::shared_ptr<handle_element>>;
};

namespace detail {
// The handles of the families of owning_handles of Element, as rvalue
// references, which a scope publishes. The families are looked up where a
// scope rebinds them, after a user's specialization of owning_handles.
template <typename Element> struct owning_handles_key {
  using type = void;
};

template <typename Element,
          typename Families = typename owning_handles<
              typename owning_handles_key<Element>::type>::type>
struct owning_handles_of;

template <typename Element, typename... Families>
struct owning_handles_of<Element, type_list<Families...>> {
  using type = type_list_cat_t<std::conditional_t<
      type_traits<
          typename type_traits<Families>::template rebind_t<Element>>::enabled,
      type_list<typename type_traits<Families>::template rebind_t<Element> &&>,
      type_list<>>...>;
};

template <typename Element>
using owning_handles_of_t = typename owning_handles_of<Element>::type;
} // namespace detail

} // namespace dingo
