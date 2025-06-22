//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/class_instance_temporary.h>
#include <dingo/decay.h>
#include <dingo/exceptions.h>
#include <dingo/resettable_i.h>
#include <dingo/type_conversion.h>
#include <dingo/type_list.h>
#include <dingo/type_traits.h>

#include <cstdlib>
#include <map>
#include <optional>
#include <stdio.h>
namespace dingo {

struct unique;
struct external;
struct shared;
struct shared_cyclical;

template <typename T> struct class_recursion_guard_base {
    static bool visited_;
};

template <typename T> bool class_recursion_guard_base<T>::visited_ = false;

template <typename T, bool Enabled = !std::is_default_constructible_v<T>>
struct class_recursion_guard : class_recursion_guard_base<T> {
    class_recursion_guard() {
        if (this->visited_)
            throw type_recursion_exception();
        this->visited_ = true;
    }

    ~class_recursion_guard() { this->visited_ = false; }
};

template <typename T> struct class_recursion_guard<T, false> {};

template <typename T, bool Enabled = !std::is_trivially_destructible_v<T>>
struct class_storage_reset {
    template <typename Context, typename Storage>
    class_storage_reset(Context& context, Storage* storage) {
        if (!storage->is_resolved())
            context.register_resettable(storage);
    }
};

template <typename T> struct class_storage_reset<T, false> {
    template <typename Context> class_storage_reset(Context&, void*) {}
};

// TODO: All the code here should use rollback/recursion guard only if they are
// required, eg. the instance will be constructed, that is non-default (so can
// cause recursion), has throwing constructor etc. Rollback support has a price.

template <typename RTTI, typename TypeInterface, typename Storage,
          typename StorageTag = typename Storage::tag_type>
struct class_instance_resolver;

template <typename RTTI, typename TypeInterface, typename Storage>
struct class_instance_resolver<RTTI, TypeInterface, Storage, unique> {
    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage)
    //-> decltype(context.template construct<typename Storage::stored_type>(
    //    storage.resolve(context, container)))
    {
        [[maybe_unused]] class_recursion_guard<decay_t<typename Storage::type>>
            recursion_guard;
        return storage.resolve(context, container);
    }

    template <typename Context> auto& get_temporary_context(Context& context) {
        return context;
    }
};

template <typename RTTI, typename TypeInterface, typename Storage>
struct class_instance_resolver<RTTI, TypeInterface, Storage, shared>
    : class_instance_temporary<
          RTTI, rebind_type_t<typename Storage::conversions::conversion_types,
                              decay_t<TypeInterface>>>,
      resettable_i {

    using class_instance_temporary_type = class_instance_temporary<
        RTTI, rebind_type_t<typename Storage::conversions::conversion_types,
                            decay_t<TypeInterface>>>;

    void reset() override {
        initialized_ = false;
        temporary().reset();
    }

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage)
    // -> decltype(storage.resolve(context, container))
    {
        if (!initialized_) {
            [[maybe_unused]] class_recursion_guard<
                decay_t<typename Storage::type>> recursion_guard;

            storage.resolve(context, container);
            initialized_ = true;
        }

        return storage.resolve(context, container);
    }

    template <typename Context> auto& get_temporary_context(Context&) {
        return temporary();
    }

  private:
    auto& temporary() {
        return static_cast<class_instance_temporary_type&>(*this);
    }

    bool initialized_ = false;
};

template <typename RTTI, typename TypeInterface, typename Storage>
struct class_instance_resolver<RTTI, TypeInterface, Storage, shared_cyclical>
    : class_instance_resolver<RTTI, TypeInterface, Storage, external> {};

template <typename RTTI, typename TypeInterface, typename Storage>
struct class_instance_resolver<RTTI, TypeInterface, Storage, external>
    : class_instance_temporary<
          RTTI, rebind_type_t<typename Storage::conversions::conversion_types,
                              decay_t<TypeInterface>>>,
      resettable_i {

    using class_instance_temporary_type = class_instance_temporary<
        RTTI, rebind_type_t<typename Storage::conversions::conversion_types,
                            decay_t<TypeInterface>>>;

    void reset() override { temporary().reset(); }

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container,
                           Storage& storage)
    // -> decltype(storage.resolve(context, container))
    {
        return storage.resolve(context, container);
    }

    template <typename Context> auto& get_temporary_context(Context&) {
        return temporary();
    }

  private:
    auto& temporary() {
        return static_cast<class_instance_temporary_type&>(*this);
    }
};

} // namespace dingo
