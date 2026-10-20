//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The factories of a stored shape. A constructor factory constructs the leaf,
// by its detected or its declared constructor, whose arguments the resolver
// resolves, and every layer is made by its layer traits from an argument that
// converts to its element; the element is made at that conversion, so a layer
// constructs it in place. A function or callable factory returns the stored
// object.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/constructor.h>
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo::core {

// How a factory makes the leaf of a stored shape: by its detected constructor,
// or by list-initializing it from the declared argument types, each resolved
// as the type it names.
struct detected_leaf {
  template <typename Leaf, typename Resolver>
  static Leaf make(Resolver &resolver) {
    return invoke_constructor<Leaf>(resolver, [](auto &&...args) {
      return Leaf{std::forward<decltype(args)>(args)...};
    });
  }
};

template <typename... Args> struct declared_leaf {
  template <typename Leaf, typename Resolver>
  static Leaf make(Resolver &resolver) {
    return Leaf{resolver.template resolve<Args>()...};
  }
};

template <typename Stored, typename LeafMaker = detected_leaf,
          typename Resolver>
Stored make_stored(Resolver &resolver);

namespace detail {
// Converts to the object of `Type`, made by the factory at the conversion.
template <typename Type, typename LeafMaker, typename Resolver>
class made_element {
public:
  explicit made_element(Resolver &resolver) : resolver_(resolver) {}

  operator Type() const { return make_stored<Type, LeafMaker>(resolver_); }

private:
  Resolver &resolver_;
};
} // namespace detail

// The stored shape: the leaf made by `LeafMaker` inside the layers, each made
// from its element.
template <typename Stored, typename LeafMaker, typename Resolver>
Stored make_stored(Resolver &resolver) {
  using traits = layer_traits<Stored>;
  if constexpr (traits::kind == layer_kind::leaf) {
    return LeafMaker::template make<Stored>(resolver);
  } else {
    using element = type_list_at_t<0, typename traits::arguments>;
    return traits::make(
        detail::made_element<element, LeafMaker, Resolver>(resolver));
  }
}

// A factory that constructs a leaf of type `Leaf`, or of the stored leaf when
// it is void, by `LeafMaker` inside the layers of the stored shape. A shape of
// another leaf converts to the stored one.
template <typename LeafMaker, typename Leaf = void> struct constructor_factory {
  template <typename Stored, typename Resolver>
  Stored make(Resolver &resolver) const {
    if constexpr (std::is_void_v<Leaf> ||
                  std::is_same_v<Leaf, leaf_of_t<Stored>>) {
      return make_stored<Stored, LeafMaker>(resolver);
    } else {
      return Stored(
          make_stored<replace_leaf_t<Stored, Leaf>, LeafMaker>(resolver));
    }
  }
};

// The factory of a binding that declares none.
using detected_factory = constructor_factory<detected_leaf>;

// Whether the detected factory makes a request of `Type`: a value, neither a
// reference nor a pointer, of a leaf that is an object type and not abstract.
template <typename Type>
struct has_detected_factory
    : std::bool_constant<!std::is_reference_v<Type> &&
                         !std::is_pointer_v<Type> && !std::is_array_v<Type> &&
                         std::is_object_v<leaf_of_t<Type>> &&
                         !std::is_abstract_v<leaf_of_t<Type>>> {};

// Calls `function` with its arguments, `Args`, each resolved as the type it
// names; the result constructs the stored object.
template <typename Stored, typename... Args, typename Function,
          typename Resolver>
Stored invoke_factory(type_list<Args...> /*arguments*/, Function &function,
                      Resolver &resolver) {
  return Stored(std::invoke(function, resolver.template resolve<Args>()...));
}

namespace detail {
template <typename Signature> struct function_arguments;

template <typename Result, typename... Args>
struct function_arguments<std::function<Result(Args...)>> {
  using type = type_list<Args...>;
};
} // namespace detail

// The argument types of a callable: of the signature std::function deduces for
// it, or of a member function, whose object comes first.
template <typename Callable, typename = void> struct callable_arguments {};

template <typename Callable>
struct callable_arguments<
    Callable, std::void_t<decltype(std::function{std::declval<Callable>()})>>
    : detail::function_arguments<decltype(std::function{
          std::declval<Callable>()})> {};

template <typename Class, typename Result, typename... Args>
struct callable_arguments<Result (Class::*)(Args...)> {
  using type = type_list<Class &, Args...>;
};

template <typename Class, typename Result, typename... Args>
struct callable_arguments<Result (Class::*)(Args...) const> {
  using type = type_list<const Class &, Args...>;
};

template <typename Class, typename Result, typename... Args>
struct callable_arguments<Result (Class::*)(Args...) noexcept> {
  using type = type_list<Class &, Args...>;
};

template <typename Class, typename Result, typename... Args>
struct callable_arguments<Result (Class::*)(Args...) const noexcept> {
  using type = type_list<const Class &, Args...>;
};

template <typename Callable>
using callable_arguments_t = typename callable_arguments<Callable>::type;

// A factory that calls a function, whose result is the stored object.
template <auto Function> struct function_factory {
  template <typename Stored, typename Resolver>
  Stored make(Resolver &resolver) const {
    auto function = Function;
    return invoke_factory<Stored>(callable_arguments_t<decltype(Function)>(),
                                  function, resolver);
  }
};

// A factory that calls a callable object supplied at registration, whose
// result is the stored object; `Arguments` lists its argument types.
template <typename Arguments, typename Callable> class callable_factory {
public:
  explicit callable_factory(Callable callable)
      : callable_(std::move(callable)) {}

  template <typename Stored, typename Resolver>
  Stored make(Resolver &resolver) {
    return invoke_factory<Stored>(Arguments(), callable_, resolver);
  }

private:
  Callable callable_;
};

template <typename Type> struct is_callable_factory : std::false_type {};

template <typename Arguments, typename Callable>
struct is_callable_factory<callable_factory<Arguments, Callable>>
    : std::true_type {};

// Constructs the stored shape at `where` by `factory`; arrays are
// value-initialized in place.
template <typename Stored, typename Factory, typename Resolver>
void construct_stored(void *where, Factory &factory, Resolver &resolver) {
  if constexpr (std::is_array_v<Stored>) {
    ::new (where) Stored();
  } else {
    ::new (where) Stored(factory.template make<Stored>(resolver));
  }
}

} // namespace dingo::core
