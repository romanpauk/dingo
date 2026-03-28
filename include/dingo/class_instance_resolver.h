//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/class_instance_conversions.h>
#include <dingo/exceptions.h>
#include <dingo/resolving_context.h>
#include <dingo/type_conversion.h>
#include <dingo/type_list.h>

namespace dingo {

struct unique;
struct external;
struct shared;
struct shared_cyclical;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif

template <typename T, bool DefaultConstructible = std::is_default_constructible_v<T>>
struct class_recursion_guard {
    explicit class_recursion_guard(resolving_context& context)
        : context_(&context)
        , parent_(context.active_type_frame_)
        , frame_{parent_, describe_type<T>()} {
        context_->active_type_frame_ = &frame_;
        if (visited_) {
            auto exception = detail::make_type_recursion_exception<T>(context);
            context_->active_type_frame_ = parent_;
            throw exception;
        }
        visited_ = true;
    }

    ~class_recursion_guard() {
        visited_ = false;
        context_->active_type_frame_ = parent_;
    }

  private:
    resolving_context* context_;
    const typename resolving_context::type_frame* parent_;
    typename resolving_context::type_frame frame_;
    static thread_local bool visited_;
};

template <typename T, bool DefaultConstructible>
thread_local bool class_recursion_guard<T, DefaultConstructible>::visited_ =
    false;

template <typename T> struct class_recursion_guard<T, true> {
    explicit class_recursion_guard(resolving_context&) {}
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

template <typename Context, typename T>
void* get_address(Context& context, T&& instance) {
    if constexpr (std::is_reference_v<T>) {
        return &instance;
    } else if constexpr (std::is_pointer_v<T>) {
        return instance;
    } else {
        return &context.template construct<T>(std::forward<T>(instance));
    }
}

template <typename RTTI, typename Type, typename Storage,
          typename StorageTag = typename Storage::tag_type>
struct class_instance_resolver;

namespace detail {
template <typename Type, typename Storage>
using resolver_conversion_types_t =
    rebind_leaf_t<typename Storage::conversions::conversion_types, Type>;

template <typename Types>
static constexpr bool resolver_has_conversion_cache_v =
    type_list_size_v<Types> != 0;

template <typename RTTI, typename Type, typename Storage,
          bool HasConversionCache>
struct shared_class_instance_resolver;

template <typename RTTI, typename Type, typename Storage>
struct shared_class_instance_resolver<RTTI, Type, Storage, false> {
    ~shared_class_instance_resolver() { closure_.reset(); }

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage) {
        return storage.resolve(context, container);
    }

    template <typename Target, typename Source, typename Context,
              typename Container, typename Factory>
    void* resolve_address(Context& context, Container& container,
                          Storage& storage, Factory& factory,
                          type_descriptor requested_type,
                          type_descriptor registered_type) {
        if (!storage.is_resolved()) {
            [[maybe_unused]] class_recursion_guard<
                leaf_type_t<typename Storage::type>>
                recursion_guard(context);

            // Shared instances can capture references to temporaries created
            // while constructing the object graph, even when there is no
            // conversion cache to preserve.
            context.push(&closure_);
            storage.resolve(context, container);

            auto&& instance =
                type_conversion<typename Storage::tag_type, Target, Source>::
                    apply(factory, context, requested_type, registered_type);
            void* p = ::dingo::get_address(
                context, std::forward<decltype(instance)>(instance));
            context.pop();
            return p;
        }

        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                factory, context, requested_type, registered_type);
        return ::dingo::get_address(
            context, std::forward<decltype(instance)>(instance));
    }

  private:
    resolving_context::closure closure_;
};

template <typename RTTI, typename Type, typename Storage>
struct shared_class_instance_resolver<RTTI, Type, Storage, true>
    : class_instance_conversions<resolver_conversion_types_t<Type, Storage>> {
    using class_instance_conversions_type =
        class_instance_conversions<resolver_conversion_types_t<Type, Storage>>;

    ~shared_class_instance_resolver() { closure_.reset(); }

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage) {
        return storage.resolve(context, container);
    }

    template <typename Target, typename Source, typename Context,
              typename Container, typename Factory>
    void* resolve_address(Context& context, Container& container,
                          Storage& storage, Factory& factory,
                          type_descriptor requested_type,
                          type_descriptor registered_type) {
        if (!initialized_) {
            [[maybe_unused]] class_recursion_guard<
                leaf_type_t<typename Storage::type>>
                recursion_guard(context);

            context.push(&closure_);
            storage.resolve(context, container);

            auto&& instance =
                type_conversion<typename Storage::tag_type, Target, Source>::
                    apply(factory, context, requested_type, registered_type);
            initialized_ = true;

            void* p = ::dingo::get_address(
                context, std::forward<decltype(instance)>(instance));
            context.pop();
            return p;
        }

        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                factory, context, requested_type, registered_type);
        return ::dingo::get_address(
            context, std::forward<decltype(instance)>(instance));
    }

    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context&, Args&&... args) {
        return conversions().template construct<T>(std::forward<Args>(args)...);
    }

  private:
    auto& conversions() {
        return static_cast<class_instance_conversions_type&>(*this);
    }

    bool initialized_ = false;
    resolving_context::closure closure_;
};

template <typename RTTI, typename Type, typename Storage,
          bool HasConversionCache>
struct external_class_instance_resolver;

template <typename RTTI, typename Type, typename Storage>
struct external_class_instance_resolver<RTTI, Type, Storage, false> {
    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage) {
        return storage.resolve(context, container);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename Target, typename Source, typename Context,
              typename Container, typename Factory>
    void* resolve_address(Context& context, Container&, Storage&,
                          Factory& factory, type_descriptor requested_type,
                          type_descriptor registered_type) {
        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                factory, context, requested_type, registered_type);
        return ::dingo::get_address(
            context, std::forward<decltype(instance)>(instance));
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

template <typename RTTI, typename Type, typename Storage>
struct external_class_instance_resolver<RTTI, Type, Storage, true>
    : class_instance_conversions<resolver_conversion_types_t<Type, Storage>> {
    using class_instance_conversions_type =
        class_instance_conversions<resolver_conversion_types_t<Type, Storage>>;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage) {
        return storage.resolve(context, container);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename Target, typename Source, typename Context,
              typename Container, typename Factory>
    void* resolve_address(Context& context, Container&, Storage&,
                          Factory& factory, type_descriptor requested_type,
                          type_descriptor registered_type) {
        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                factory, context, requested_type, registered_type);
        return ::dingo::get_address(
            context, std::forward<decltype(instance)>(instance));
    }

    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context&, Args&&... args) {
        return conversions().template construct<T>(std::forward<Args>(args)...);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  private:
    auto& conversions() {
        return static_cast<class_instance_conversions_type&>(*this);
    }
};
} // namespace detail

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, unique> {
    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage) {
        return storage.resolve(context, container);
    }

    template <typename Target, typename Source, typename Context,
              typename Container, typename Factory>
    void* resolve_address(Context& context, Container& container,
                          Storage& storage, Factory& factory,
                          type_descriptor requested_type,
                          type_descriptor registered_type) {
        // TODO: GCC warns about unused variables in factory where we have no
        // clue if they will be used...
        (void)container;
        (void)storage;

        [[maybe_unused]] class_recursion_guard<leaf_type_t<typename Storage::type>>
            recursion_guard(context);
        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                factory, context, requested_type, registered_type);
        return ::dingo::get_address(
            context, std::forward<decltype(instance)>(instance));
    }

  private:
    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context& context, Args&&... args) {
        return context.template construct<T>(std::forward<Args>(args)...);
    }
};

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, shared>
    : detail::shared_class_instance_resolver<
          RTTI, Type, Storage,
          detail::resolver_has_conversion_cache_v<
              detail::resolver_conversion_types_t<Type, Storage>>> {};

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, shared_cyclical>
    : class_instance_resolver<RTTI, Type, Storage, external> {};

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, external>
    : detail::external_class_instance_resolver<
          RTTI, Type, Storage,
          detail::resolver_has_conversion_cache_v<
              detail::resolver_conversion_types_t<Type, Storage>>> {};

} // namespace dingo
