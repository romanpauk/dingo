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

namespace dingo {

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
    template <typename Conversions, typename Context, typename Container>
    void* resolve(Context& context, Container& container, Storage& storage,
                  const typename RTTI::type_index& type) {
        if (!find_type<RTTI>(Conversions{}, type))
            throw type_not_convertible_exception();

        class_recursion_guard<decay_t<typename Storage::type>> recursion_guard;

        // TODO: this should avoid the move. Provide memory to construct into
        // to the factory.
        auto&& instance =
            context.template construct<typename Storage::stored_type>(
                storage.resolve(context, container));
        auto* address = type_traits<
            std::remove_reference_t<decltype(instance)>>::get_address(instance);
        return convert_type<RTTI, decay_t<TypeInterface>*,
                            typename Storage::tag_type>(context, Conversions{},
                                                        type, address);
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

    template <typename Conversions, typename Context, typename Container>
    void* resolve(Context& context, Container& container, Storage& storage,
                  const typename RTTI::type_index& type) {
        if (!find_type<RTTI>(Conversions{}, type))
            throw type_not_convertible_exception();

        if (!initialized_) {
            class_recursion_guard<decay_t<typename Storage::type>>
                recursion_guard;
            class_storage_reset<decay_t<typename Storage::stored_type>>
                storage_reset(context, &storage);

            // TODO: not all types will need a rollback
            context.register_resettable(this);
            context.increment();
            storage.resolve(context, container);
            initialized_ = true;
            context.decrement();
        }

        auto&& instance = storage.resolve(context, container);
        auto* address = type_traits<
            std::remove_reference_t<decltype(instance)>>::get_address(instance);
        return convert_type<RTTI, decay_t<TypeInterface>*,
                            typename Storage::tag_type>(
            temporary(), Conversions{}, type, address);
    }

  private:
    auto& temporary() {
        return static_cast<class_instance_temporary_type&>(*this);
    }

    bool initialized_ = false;
};

template <typename RTTI, typename TypeInterface, typename Storage>
struct class_instance_resolver<RTTI, TypeInterface, Storage, shared_cyclical>
    : class_instance_resolver<RTTI, TypeInterface, Storage, shared> {};

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

    template <typename Conversions, typename Context, typename Container>
    void* resolve(Context& context, Container& container, Storage& storage,
                  const typename RTTI::type_index& type) {
        if (!find_type<RTTI>(Conversions{}, type))
            throw type_not_convertible_exception();

        auto&& instance = storage.resolve(context, container);
        auto* address = type_traits<
            std::remove_reference_t<decltype(instance)>>::get_address(instance);
        return convert_type<RTTI, decay_t<TypeInterface>*,
                            typename Storage::tag_type>(
            temporary(), Conversions{}, type, address);
    }

  private:
    auto& temporary() {
        return static_cast<class_instance_temporary_type&>(*this);
    }
};

} // namespace dingo
