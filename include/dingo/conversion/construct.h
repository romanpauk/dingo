//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The factories of a stored type tree. A constructor factory constructs the
// leaf, by its detected or its declared constructor, whose arguments the
// resolver resolves, and every wrapper is made by its wrapper traits from an
// argument that converts to its element; the element is made at that
// conversion, so a wrapper constructs it in place. A function or callable
// factory returns the stored object.

#include <dingo/base/mechanics.h>
#include <dingo/constructor/constructor.h>
#include <dingo/diagnostics/compile_diagnostics.gen.h>
#include <dingo/type/wrapper_traits.h>

#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo::core {

// How a factory makes the leaf of a stored type tree: by its detected
// constructor, or by list-initializing it from the declared argument types,
// each resolved as the type it names.
struct detected_leaf {
  template <typename Leaf, typename Resolver>
  static Leaf make(Resolver &resolver) {
    if constexpr (declared_constructor<Leaf>::declared ||
                  detected_constructor<Leaf>::kind ==
                      constructor_kind::concrete) {
      return invoke_constructor<Leaf>(resolver, [](auto &&...args) {
        return Leaf{std::forward<decltype(args)>(args)...};
      });
    } else {
      constexpr constructor_kind kind = detected_constructor<Leaf>::kind;
      static_assert(require<kind != constructor_kind::invalid,
                            diag::constructor_not_detected, Leaf>());
      static_assert(require<kind != constructor_kind::generic,
                            diag::constructor_generic, Leaf>());
      return Leaf(resolver);
    }
  }
};

template <typename... Args> struct declared_leaf {
  template <typename Leaf, typename Resolver>
  static Leaf make(Resolver &resolver) {
    return Leaf{resolver.template argument<Args>()...};
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

// The stored type tree: the leaf made by `LeafMaker` inside the wrappers, each
// made from its element.
template <typename Stored, typename LeafMaker, typename Resolver>
Stored make_stored(Resolver &resolver) {
  using traits = wrapper_traits<Stored>;
  if constexpr (!is_wrapper_v<Stored>) {
    return LeafMaker::template make<Stored>(resolver);
  } else {
    using element = type_list_at_t<0, typename traits::arguments>;
    return traits::make(
        detail::made_element<element, LeafMaker, Resolver>(resolver));
  }
}

namespace detail {
// Converts to the object of `Type`, made by `factory` at the conversion.
template <typename Type, typename Factory, typename Resolver>
class factory_element {
public:
  factory_element(const Factory &factory, Resolver &resolver)
      : factory_(factory), resolver_(resolver) {}

  operator Type() const { return factory_.template make<Type>(resolver_); }

private:
  const Factory &factory_;
  Resolver &resolver_;
};
} // namespace detail

// A factory that constructs a leaf of type `Leaf`, or of the stored leaf when
// it is void, by `LeafMaker` inside the wrappers of the stored type tree. A
// type tree of another leaf converts to the stored one, such as a handle of a
// leaf to one of its interface; when it does not, every wrapper of the stored
// type tree down to a variadic one is made by its wrapper traits from its
// element, and the variadic wrapper, or the stored leaf, converts from the leaf
// made.
template <typename LeafMaker, typename Leaf = void> struct constructor_factory {
  template <typename Stored, typename Resolver>
  Stored make(Resolver &resolver) const {
    if constexpr (std::is_void_v<Leaf> ||
                  std::is_same_v<Leaf, leaf_of_t<Stored>>) {
      return make_stored<Stored, LeafMaker>(resolver);
    } else if constexpr (std::is_convertible_v<replace_leaf_t<Stored, Leaf>,
                                               Stored>) {
      return make_stored<replace_leaf_t<Stored, Leaf>, LeafMaker>(resolver);
    } else if constexpr (is_wrapper_v<Stored> &&
                         !has_properties_v<Stored, property::variadic>) {
      using element =
          type_list_at_t<0, typename wrapper_traits<Stored>::arguments>;
      return wrapper_traits<Stored>::make(
          detail::factory_element<element, constructor_factory, Resolver>(
              *this, resolver));
    } else {
      return Stored(make_stored<Leaf, LeafMaker>(resolver));
    }
  }
};

// The factory of a binding that declares none.
using detected_factory = constructor_factory<detected_leaf>;

// Whether the detected factory makes a request of `Type`: a value, neither a
// reference nor a pointer, of a leaf that is an object type and not abstract;
// the leaf of a reference or a pointer may be incomplete, so it is not asked.
template <typename Type>
struct has_detected_factory
    : std::conjunction<std::negation<std::is_reference<Type>>,
                       std::negation<std::is_pointer<Type>>,
                       std::negation<std::is_array<Type>>,
                       std::is_object<leaf_of_t<Type>>,
                       std::negation<std::is_abstract<leaf_of_t<Type>>>> {};

// Calls `function` with its arguments, `Args`, each resolved as the type it
// names; the result constructs the stored object.
template <typename Stored, typename... Args, typename Function,
          typename Resolver>
Stored invoke_factory(type_list<Args...> /*arguments*/, Function &function,
                      Resolver &resolver) {
  return Stored(std::invoke(function, resolver.template argument<Args>()...));
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

namespace detail {
// The parameter types of a call operator, without its object.
template <typename Member> struct operator_arguments;

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...)> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) const> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) &> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) const &> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) &&> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) const &&> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) noexcept> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) const noexcept> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) & noexcept> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) const & noexcept> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) && noexcept> {
  using type = type_list<Args...>;
};

template <typename Class, typename Result, typename... Args>
struct operator_arguments<Result (Class::*)(Args...) const && noexcept> {
  using type = type_list<Args...>;
};

template <typename Callable, typename = void>
struct deduces_function : std::false_type {};

template <typename Callable>
struct deduces_function<
    Callable, std::void_t<decltype(std::function{std::declval<Callable>()})>>
    : std::true_type {};

template <typename Callable, typename = void>
struct has_call_operator : std::false_type {};

template <typename Callable>
struct has_call_operator<Callable, std::void_t<decltype(&Callable::operator())>>
    : std::true_type {};
} // namespace detail

// A callable whose call operator std::function does not deduce, as it takes
// its object as an rvalue, has the parameter types of its one call operator.
template <typename Callable>
struct callable_arguments<
    Callable, std::enable_if_t<!detail::deduces_function<Callable>::value &&
                               detail::has_call_operator<Callable>::value>>
    : detail::operator_arguments<decltype(&Callable::operator())> {};

template <typename Callable>
using callable_arguments_t = typename callable_arguments<Callable>::type;

// The factory of the next implementation that a public factory declares; a
// type that declares none, such as a callable factory, which a registration
// takes as its argument, is rejected.
template <typename Factory> struct factory_of {
  static_assert(
      require<dependent_false<Factory>, diag::factory_not_declared, Factory>());
  using type = void;
};

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

// The argument types `Factory` declares for the leaf `Leaf` it makes, or void
// when they are the arguments of a detected constructor, which are known only
// when the object is constructed.
template <typename Factory, typename Leaf> struct declared_arguments {
  using type = void;
};

template <typename... Args, typename Made, typename Leaf>
struct declared_arguments<constructor_factory<declared_leaf<Args...>, Made>,
                          Leaf> {
  using type = type_list<Args...>;
};

namespace detail {
template <typename Type, bool = declared_constructor<Type>::declared>
struct constructor_arguments {
  using type = void;
};

template <typename Type> struct constructor_arguments<Type, true> {
  using type = typename declared_constructor<Type>::arguments;
};
} // namespace detail

template <typename Made, typename Leaf>
struct declared_arguments<constructor_factory<detected_leaf, Made>, Leaf>
    : detail::constructor_arguments<
          std::conditional_t<std::is_void_v<Made>, Leaf, Made>> {};

template <auto Function, typename Leaf>
struct declared_arguments<function_factory<Function>, Leaf> {
  using type = callable_arguments_t<decltype(Function)>;
};

template <typename Arguments, typename Callable, typename Leaf>
struct declared_arguments<callable_factory<Arguments, Callable>, Leaf> {
  using type = Arguments;
};

template <typename Type> struct is_callable_factory : std::false_type {};

template <typename Arguments, typename Callable>
struct is_callable_factory<callable_factory<Arguments, Callable>>
    : std::true_type {};

// Constructs the stored type tree at `where` by `factory`; arrays are
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
