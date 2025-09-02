//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/class_instance_conversions.h>
#include <dingo/decay.h>
#include <dingo/exceptions.h>
#include <dingo/resolving_context.h>

namespace dingo {
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
}

#include <dingo/type_conversion.h>

namespace dingo {

struct unique;
struct external;
struct shared;
struct shared_cyclical;

template <typename T, bool DefaultConstructible = std::is_default_constructible_v<T>>
struct class_recursion_guard {
    class_recursion_guard() {
        if (this->visited_)
            throw type_recursion_exception();
        this->visited_ = true;
    }

    ~class_recursion_guard() { this->visited_ = false; }
    static bool visited_;
};

template <typename T, bool DefaultConstructible> bool class_recursion_guard<T, DefaultConstructible>::visited_ = false;

template <typename T> struct class_recursion_guard<T, true> {};

template <typename RTTI, typename Type, typename Storage,
          typename StorageTag = typename Storage::tag_type>
struct class_instance_resolver;

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, unique> {
    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage)
    {
        return storage.resolve(context, container);
    }

    template <typename Target, typename Source, typename Context, typename Container, typename Factory>
    void* resolve_address(Context& context, Container& container,
        Storage& storage, Factory& factory) {
        // TODO: GCC warns about unused variables in factory where we have no clue if they will be used...
        (void)container;
        (void)storage;

        [[maybe_unused]] class_recursion_guard<decay_t<typename Storage::type>>
            recursion_guard;
        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                factory, context);
        return ::dingo::get_address(context, std::forward<decltype(instance)>(instance));
    }

private:
    template <typename T, typename Context, typename... Args> T& construct_conversion(Context& context, Args&&... args) {
        return context.template construct<T>(std::forward<Args>(args)...);
    }
};

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, shared>
    : class_instance_conversions< rebind_type_t< typename Storage::conversions::conversion_types, Type > >
{
    using class_instance_conversions_type = class_instance_conversions<
        rebind_type_t<typename Storage::conversions::conversion_types, Type>>;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage)
    {
        return storage.resolve(context, container);
    }

    // TODO: main entry to resolution
    template <typename Target, typename Source, typename Context, typename Container, typename Factory>
    void* resolve_address(Context& context, Container& container,
        Storage& storage, Factory& factory) {
        if (!initialized_) {
            [[maybe_unused]] class_recursion_guard<
                decay_t<typename Storage::type>> recursion_guard;

            // Note to self:
            // Closure is used to construct temporary. If we get to push, it means there was no exception,
            // closure is popped and that will preserve it, as only closures left on stack will get destroyed
            // by resolving_context. That is why there is no scope guard.

            context.push(&closure_);
            storage.resolve(context, container);

            auto&& instance =
                type_conversion<typename Storage::tag_type, Target, Source>::apply(factory, context);
            initialized_ = true;

            void* p = ::dingo::get_address(context, std::forward<decltype(instance)>(instance));
            context.pop();
            return p;
        }

        // TODO: this very crudely expects no resolution to happen
        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(factory, context);
        return ::dingo::get_address(context, std::forward<decltype(instance)>(instance));
    }

    template <typename T, typename Context, typename... Args> T& construct_conversion(Context& context, Args&&... args) {
        (void)context;
        return conversions().template construct<T>(std::forward<Args>(args)...);
    }

  private:
    auto& conversions() {
        return static_cast<class_instance_conversions_type&>(*this);
    }

    bool initialized_ = false;

    // TODO: closure is not needed for default constructible types
    resolving_context::closure closure_;
};

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, shared_cyclical>
    : class_instance_resolver<RTTI, Type, Storage, external> {};

template <typename RTTI, typename Type, typename Storage>
struct class_instance_resolver<RTTI, Type, Storage, external>
    : class_instance_conversions< rebind_type_t<typename Storage::conversions::conversion_types,
                              Type>>
{
    using class_instance_conversions_type = class_instance_conversions<
        rebind_type_t<typename Storage::conversions::conversion_types,
                            Type>>;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage)
    {
        return storage.resolve(context, container);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename Target, typename Source, typename Context, typename Container, typename Factory>
    void* resolve_address(Context& context, Container&,
        Storage&, Factory& factory) {
        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(factory, context);
        return ::dingo::get_address(context, std::forward<decltype(instance)>(instance));
    }

    template <typename T, typename Context, typename... Args> T& construct_conversion(Context& context, Args&&... args) {
        (void)context;
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

} // namespace dingo
