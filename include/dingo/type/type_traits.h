//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>

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

namespace detail {
template <typename T, typename, auto... Values>
struct is_brace_constructible_impl : std::false_type {};

template <typename T, auto... Values>
struct is_brace_constructible_impl<T, std::void_t<decltype(T{Values...})>,
                                   Values...> : std::true_type {};

template <typename T, auto... Values>
struct is_brace_constructible
    : is_brace_constructible_impl<T, void, Values...> {};

template <typename T, auto... Values>
inline constexpr bool is_brace_constructible_v =
    is_brace_constructible<T, Values...>::value;
} // namespace detail

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

template <typename T, typename = void> struct copy_constructible_traits;

namespace detail {
template <typename T, bool IsCollection>
struct copy_constructible_with_collection {
  static constexpr bool value = std::is_copy_constructible_v<T>;
};

template <typename T> struct copy_constructible_with_collection<T, true> {
  static constexpr bool value = copy_constructible_traits<
      typename collection_traits<T>::resolve_type>::value;
};

template <typename T, typename = void> struct copy_constructible_base {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = std::is_copy_constructible_v<value_type>;
};

template <typename T>
struct copy_constructible_base<
    T, std::void_t<decltype(collection_traits<std::remove_cv_t<
                                std::remove_reference_t<T>>>::is_collection)>> {
  using collection_type =
      collection_traits<std::remove_cv_t<std::remove_reference_t<T>>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value =
      copy_constructible_with_collection<value_type,
                                         collection_type::is_collection>::value;
};
} // namespace detail

template <typename T, typename>
struct copy_constructible_traits : detail::copy_constructible_base<T> {};

template <typename T>
inline constexpr bool is_copy_constructible_v =
    copy_constructible_traits<T>::value;

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
template <typename List, typename Selected> struct type_list_count;

template <typename Selected, typename... Alternatives>
struct type_list_count<type_list<Alternatives...>, Selected>
    : std::integral_constant<
          size_t,
          (0u + ... + (std::is_same_v<Selected, Alternatives> ? 1u : 0u))> {};

template <typename List> struct type_list_has_duplicates;

template <typename... Alternatives>
struct type_list_has_duplicates<type_list<Alternatives...>>
    : std::bool_constant<((type_list_count<type_list<Alternatives...>,
                                           Alternatives>::value > 1) ||
                          ...)> {};

template <typename Type, typename = void>
struct alternative_type_alternatives {};

template <typename Type>
struct alternative_type_alternatives<
    Type, std::enable_if_t<is_alternative_type_v<Type>>> {
  using type = typename type_traits<std::remove_cv_t<Type>>::alternatives;

  static_assert(!type_list_has_duplicates<type>::value,
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

template <typename T, size_t N, typename... Args>
T *make_bounded_array(Args &&...args) {
  static_assert(sizeof...(Args) <= N,
                "too many initializers for bounded array construction");
  return new T[N]{std::forward<Args>(args)...};
}

template <typename T, size_t N, typename... Args>
void construct_bounded_array(void *ptr, Args &&...args) {
  static_assert(sizeof...(Args) <= N,
                "too many initializers for bounded array construction");
  new (ptr) T[N]{std::forward<Args>(args)...};
}

template <typename T, typename... Args> T *make_dynamic_array(Args &&...args) {
  static_assert(sizeof...(Args) > 0,
                "dynamic arrays require an explicit size via a custom "
                "factory or element initializers");
  return new T[sizeof...(Args)]{std::forward<Args>(args)...};
}

template <typename Type, typename = void>
struct is_array_like_type : std::is_array<Type> {};

template <typename Type>
inline constexpr bool is_array_like_type_v = is_array_like_type<Type>::value;

template <typename Type, typename = void>
struct array_like_exact_interface_type {
  using type = Type;
};

template <typename Type>
using array_like_exact_interface_type_t =
    typename array_like_exact_interface_type<Type>::type;

// To with the const and volatile qualifiers of From.
template <typename From, typename To> struct copy_cv {
private:
  using const_type =
      std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;

public:
  using type = std::conditional_t<std::is_volatile_v<From>,
                                  std::add_volatile_t<const_type>, const_type>;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

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

template <typename Handle, typename = void> struct wrapper_storage_types_impl {
  using lvalue_reference_types = type_list<Handle &>;
  using pointer_types = type_list<Handle *>;
  using copyable_value_types =
      std::conditional_t<std::is_copy_constructible_v<Handle>,
                         type_list<Handle>, type_list<>>;
};

template <typename Handle>
struct wrapper_storage_types_impl<
    Handle, std::enable_if_t<type_traits<Handle>::enabled &&
                             type_traits<Handle>::is_pointer_like>> {
private:
  // Walk the wrapper chain once and collect all derived interface types
  // together instead of recursing over the same chain separately for each
  // result list.
  using next =
      wrapper_storage_types_impl<typename type_traits<Handle>::value_type>;
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

template <typename Handle>
using wrapper_storage_types = wrapper_storage_types_impl<Handle>;

// An owning handle whose element cannot be borrowed as an object owns an
// array.
template <typename Handle>
struct is_array_like_type<
    Handle, std::enable_if_t<type_traits<Handle>::is_owning_handle &&
                             !is_value_borrowable_v<Handle>>> : std::true_type {
};

template <typename Handle>
struct array_like_exact_interface_type<
    Handle, std::enable_if_t<type_traits<Handle>::is_owning_handle &&
                             !is_value_borrowable_v<Handle>>> {
  using type = typename type_traits<Handle>::value_type;
};
} // namespace detail

template <typename T>
struct type_traits<T *, std::enable_if_t<!std::is_void_v<T>>>
    : semantics::observer {
  using value_type = T;
  template <typename U> using rebind_t = U *;

  static T *get(T *ptr) { return ptr; }
  static T &borrow(T *ptr) { return *ptr; }
  static bool empty(T *ptr) { return ptr == nullptr; }
};

template <typename T>
struct type_traits<T *, std::enable_if_t<std::is_void_v<T>>>
    : semantics::observer {
  using value_type = T;
  template <typename U> using rebind_t = U *;

  static T *get(T *ptr) { return ptr; }
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

  static T *get(const std::unique_ptr<T, Deleter> &wrapper) {
    return wrapper.get();
  }
  static T &borrow(const std::unique_ptr<T, Deleter> &wrapper) {
    return *wrapper;
  }
  static bool empty(const std::unique_ptr<T, Deleter> &wrapper) {
    return wrapper.get() == nullptr;
  }

  template <typename... Args>
  static std::unique_ptr<T, Deleter> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
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

  template <typename... Args>
  static std::shared_ptr<Array> make(Args &&...args) {
    return std::shared_ptr<Array>(
        detail::make_dynamic_array<value_type>(std::forward<Args>(args)...));
  }
};

template <typename T>
struct type_traits<std::shared_ptr<T>, std::enable_if_t<!std::is_array_v<T>>>
    : semantics::shared_owner {
  using value_type = T;
  template <typename U> using rebind_t = std::shared_ptr<U>;

  static T *get(const std::shared_ptr<T> &wrapper) { return wrapper.get(); }
  static T &borrow(const std::shared_ptr<T> &wrapper) { return *wrapper; }
  static bool empty(const std::shared_ptr<T> &wrapper) {
    return wrapper.get() == nullptr;
  }

  template <typename... Args> static std::shared_ptr<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
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
  static bool empty(const std::optional<T> &wrapper) {
    return !wrapper.has_value();
  }

  template <typename Value> static std::optional<T> wrap(Value &&value) {
    return std::optional<T>(std::in_place, std::forward<Value>(value));
  }
  static std::optional<T> make_empty() { return std::nullopt; }

  template <typename... Args> static std::optional<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::optional<T>{
          std::in_place, detail::make_nested<T>(std::forward<Args>(args)...)};
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::optional<T>{std::in_place, std::forward<Args>(args)...};
    else
      return std::optional<T>{std::in_place, T{std::forward<Args>(args)...}};
  }
};

namespace detail {
template <typename T, typename... Args> T make_nested(Args &&...args) {
  return type_traits<T>::make(std::forward<Args>(args)...);
}
} // namespace detail

} // namespace dingo
