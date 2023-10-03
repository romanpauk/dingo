#pragma once

#include <dingo/config.h>

#include <dingo/decay.h>
#include <dingo/exceptions.h>
#include <dingo/resettable_i.h>
#include <dingo/type_conversion.h>
#include <dingo/type_list.h>
#include <dingo/type_traits.h>

#include <cstdlib>
#include <optional>

namespace dingo {
// Required to support references
template <typename T> class class_instance_wrapper {
  public:
    template <typename... Args>
    class_instance_wrapper(Args&&... args)
        : instance_(std::forward<Args>(args)...) {}

    T& get() { return instance_; }

    auto get_address() {
        if constexpr (std::is_pointer_v<T>)
            return instance_;
        else
            return std::addressof(instance_);
    }

  private:
    T instance_;
};

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
struct class_instance_reset {
    template <typename Context, typename Instance>
    class_instance_reset(Context& context, Instance* instance) {
        context.register_class_instance(instance);
    }
};

template <typename T> struct class_instance_reset<T, false> {
    template <typename Context> class_instance_reset(Context&, void*) {}
};

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
          bool IsCaching = Storage::is_caching>
struct class_instance_resolver : public resettable_i {
    void reset() override { instance_.reset(); }

    template <typename Conversions, typename Context, typename Container>
    void* resolve(Context& context, Container& container, Storage& storage,
                  const typename RTTI::type_index& type) {
        if (!find_type<RTTI>(Conversions{}, type))
            throw type_not_convertible_exception();

        class_recursion_guard<decay_t<typename Storage::type>> recursion_guard;

        class_instance_reset<decay_t<typename Storage::type>> storage_reset(
            context, this);
        instance_.emplace(storage.resolve(context, container));
        return convert_type<RTTI>(Conversions{}, type,
                                  instance_->get_address());
    }

  private:
    std::optional<class_instance_wrapper<TypeInterface>> instance_;
};

template <typename RTTI, typename TypeInterface, typename Storage>
struct class_instance_resolver<RTTI, TypeInterface, Storage, true>
    : public resettable_i {
    void reset() override { instance_.reset(); }

    template <typename Conversions, typename Context, typename Container>
    void* resolve(Context& context, Container& container, Storage& storage,
                  const typename RTTI::type_index& type) {
        if (!find_type<RTTI>(Conversions{}, type))
            throw type_not_convertible_exception();

        if (!instance_) {
            class_recursion_guard<decay_t<typename Storage::type>>
                recursion_guard;
            class_storage_reset<decay_t<typename Storage::type>> storage_reset(
                context, &storage);

            // TODO: not all types will need a rollback
            context.register_resettable(this);
            context.increment();
            instance_.emplace(storage.resolve(context, container));
            context.decrement();
        }

        return convert_type<RTTI>(Conversions{}, type,
                                  instance_->get_address());
    }

  private:
    std::optional<class_instance_wrapper<TypeInterface>> instance_;
};
} // namespace dingo
