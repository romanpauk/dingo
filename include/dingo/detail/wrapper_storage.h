//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/aligned_storage.h>
#include <dingo/wrapper_traits.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename T>
inline constexpr bool is_wrapper_object_v =
    !std::is_pointer_v<wrapper_base_t<T>> &&
    !std::is_same_v<wrapper_base_t<T>, wrapper_element_t<T>>;

template <typename Source, typename Target>
inline constexpr bool is_stored_wrapper_family_v =
    is_wrapper_object_v<Source> &&
    std::is_same_v<wrapper_rebind_t<Source, wrapper_element_t<Target>>,
                   wrapper_base_t<Target>>;

template <typename Type, typename StoredType, typename = void>
struct stored_wrapper_type;

template <typename Type, typename StoredType>
struct stored_wrapper_type<
    Type, StoredType,
    std::enable_if_t<is_stored_wrapper_family_v<Type, StoredType>>> {
    using type = StoredType;
};

template <typename Type, typename StoredType>
struct stored_wrapper_type<
    Type, StoredType,
    std::enable_if_t<!is_stored_wrapper_family_v<Type, StoredType> &&
                     has_wrapper_rebind_v<Type, StoredType>>> {
    using type = wrapper_rebind_t<Type, StoredType>;
};

template <typename Type, typename StoredType>
using stored_wrapper_type_t = typename stored_wrapper_type<Type, StoredType>::type;

template <typename...>
inline constexpr bool always_false_v = false;

template <typename StoredWrapper, typename Source>
StoredWrapper convert_stored_wrapper(Source&& source) {
    using source_type = std::decay_t<Source>;
    using source_base = wrapper_base_t<source_type>;
    using target_base = wrapper_base_t<StoredWrapper>;
    using target_element = wrapper_element_t<StoredWrapper>;

    if constexpr (std::is_constructible_v<StoredWrapper, Source>) {
        return StoredWrapper(std::forward<Source>(source));
    } else if constexpr (has_wrapper_release_v<source_type> &&
                         has_wrapper_adopt_v<StoredWrapper, target_element>) {
        auto source_wrapper = std::forward<Source>(source);
        return wrapper_traits<target_base>::template adopt<target_element>(
            wrapper_traits<source_base>::release(source_wrapper));
    } else {
        static_assert(always_false_v<StoredWrapper, Source>,
                      "wrapper storage requires constructible conversion or "
                      "release/adopt support");
    }
}

template <typename Type, typename StoredType, typename Factory>
class shared_wrapper_storage_instance : Factory {
    using stored_wrapper_type = stored_wrapper_type_t<Type, StoredType>;
    using storage_type = dingo::aligned_storage_t<sizeof(stored_wrapper_type),
                                                  alignof(stored_wrapper_type)>;

  public:
    template <typename... Args>
    shared_wrapper_storage_instance(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    ~shared_wrapper_storage_instance() { reset(); }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(empty());
        new (&instance_) stored_wrapper_type(convert_stored_wrapper<stored_wrapper_type>(
            Factory::template construct<Type>(context, container)));
        initialized_ = true;
    }

    stored_wrapper_type& get() const {
        return *reinterpret_cast<stored_wrapper_type*>(&instance_);
    }

    void reset() {
        if (initialized_) {
            initialized_ = false;
            get().~stored_wrapper_type();
        }
    }

    bool empty() const { return !initialized_; }

  private:
    mutable storage_type instance_;
    bool initialized_ = false;
};

template <typename Type, typename StoredType>
class external_wrapper_storage_instance {
    using stored_wrapper_type = stored_wrapper_type_t<Type, StoredType>;

  public:
    template <typename T>
    external_wrapper_storage_instance(T&& instance)
        : instance_(convert_stored_wrapper<stored_wrapper_type>(
              std::forward<T>(instance))) {}

    stored_wrapper_type& get() { return instance_; }

  private:
    stored_wrapper_type instance_;
};

template <typename Type>
struct shared_cyclical_wrapper_state {
    bool constructed = false;
};

template <typename Type>
class shared_cyclical_wrapper_deleter {
    using storage_type = dingo::aligned_storage_t<sizeof(Type), alignof(Type)>;

  public:
    explicit shared_cyclical_wrapper_deleter(
        std::shared_ptr<shared_cyclical_wrapper_state<Type>> state)
        : state_(std::move(state)) {}

    void operator()(Type* ptr) {
        if constexpr (!std::is_trivially_destructible_v<Type>) {
            if (state_ && state_->constructed) {
                ptr->~Type();
            }
        }

        delete reinterpret_cast<storage_type*>(ptr);
    }

  private:
    std::shared_ptr<shared_cyclical_wrapper_state<Type>> state_;
};

template <typename Wrapper, typename ReboundWrapper>
inline constexpr bool supports_shared_cyclical_wrapper_types_v =
    wrapper_traits<wrapper_base_t<Wrapper>>::is_indirect &&
    has_wrapper_adopt_with_deleter_v<
        ReboundWrapper, wrapper_element_t<ReboundWrapper>,
        wrapper_element_t<Wrapper>,
        shared_cyclical_wrapper_deleter<wrapper_element_t<Wrapper>>>;

template <typename Type, typename StoredType, typename = void>
struct supports_shared_cyclical_wrapper_storage : std::false_type {};

template <typename Type, typename StoredType>
struct supports_shared_cyclical_wrapper_storage<
    Type, StoredType, std::enable_if_t<is_wrapper_object_v<Type> &&
                                       has_wrapper_rebind_v<Type, StoredType>>>
    : std::bool_constant<supports_shared_cyclical_wrapper_types_v<
          Type, stored_wrapper_type_t<Type, StoredType>>> {};

template <typename Type, typename StoredType>
inline constexpr bool supports_shared_cyclical_wrapper_storage_v =
    supports_shared_cyclical_wrapper_storage<Type, StoredType>::value;

template <typename Type, typename StoredType, typename Factory>
class shared_cyclical_wrapper_storage_instance : Factory {
    using concrete_type = wrapper_element_t<Type>;
    using stored_wrapper_type = stored_wrapper_type_t<Type, StoredType>;
    using stored_base = wrapper_base_t<stored_wrapper_type>;
    using stored_element = wrapper_element_t<stored_wrapper_type>;
    using deleter_type = shared_cyclical_wrapper_deleter<concrete_type>;
    using state_type = shared_cyclical_wrapper_state<concrete_type>;
    using wrapper_storage_type =
        dingo::aligned_storage_t<sizeof(stored_wrapper_type),
                                 alignof(stored_wrapper_type)>;
    using object_storage_type =
        dingo::aligned_storage_t<sizeof(concrete_type), alignof(concrete_type)>;

  public:
    template <typename... Args>
    shared_cyclical_wrapper_storage_instance(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    ~shared_cyclical_wrapper_storage_instance() { reset(); }

    template <typename Context>
    stored_wrapper_type& resolve(Context&) {
        assert(empty());
        auto state = std::make_shared<state_type>();
        std::unique_ptr<object_storage_type> storage(new object_storage_type);
        concrete_type* instance_ptr =
            reinterpret_cast<concrete_type*>(storage.get());

        stored_wrapper_type wrapper(
            wrapper_traits<stored_base>::template adopt<stored_element>(
                instance_ptr, deleter_type(state)));

        new (&instance_) stored_wrapper_type(std::move(wrapper));
        storage.release();
        state_ = std::move(state);
        instance_ptr_ = instance_ptr;
        initialized_ = true;
        return get();
    }

    stored_wrapper_type& get() const {
        return *reinterpret_cast<stored_wrapper_type*>(&instance_);
    }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(initialized_);
        Factory::template construct<concrete_type*>(instance_ptr_, context,
                                                    container);
        state_->constructed = true;
    }

    void reset() {
        if (initialized_) {
            initialized_ = false;
            instance_ptr_ = nullptr;
            state_.reset();
            get().~stored_wrapper_type();
        }
    }

    bool empty() const { return !initialized_; }

  private:
    mutable wrapper_storage_type instance_;
    concrete_type* instance_ptr_ = nullptr;
    std::shared_ptr<state_type> state_;
    bool initialized_ = false;
};

} // namespace detail
} // namespace dingo
