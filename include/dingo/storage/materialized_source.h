//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename Type>
void destroy_rvalue_source_value(Type& value) {
    if constexpr (std::is_array_v<Type>) {
        for (size_t i = std::extent_v<Type>; i > 0; --i) {
            destroy_rvalue_source_value(value[i - 1]);
        }
    } else if constexpr (!std::is_trivially_destructible_v<Type>) {
        value.~Type();
    }
}

template <typename Source> class rvalue_source {
  public:
    using value_type = Source;

    template <typename Value,
              typename = std::enable_if_t<
                  std::is_constructible_v<Source, Value&&> &&
                  !std::is_same_v<std::remove_cv_t<std::remove_reference_t<Value>>,
                                  rvalue_source>>>
    explicit rvalue_source(Value&& value) {
        new (&storage_) Source(std::forward<Value>(value));
        initialized_ = true;
    }

    template <typename Builder>
    explicit rvalue_source(std::in_place_t, Builder&& builder) {
        builder(&storage_);
        initialized_ = true;
    }

    rvalue_source(const rvalue_source&) = delete;
    rvalue_source& operator=(const rvalue_source&) = delete;
    rvalue_source(rvalue_source&&) = delete;
    rvalue_source& operator=(rvalue_source&&) = delete;

    ~rvalue_source() {
        if (initialized_) {
            destroy_rvalue_source_value(get());
        }
    }

    Source* get_ptr() {
        return std::launder(reinterpret_cast<Source*>(&storage_));
    }
    const Source* get_ptr() const {
        return std::launder(reinterpret_cast<const Source*>(&storage_));
    }

    Source& get() & { return *get_ptr(); }
    const Source& get() const & { return *get_ptr(); }
    Source get() && { return std::move(*get_ptr()); }

    Source& operator*() { return get(); }
    const Source& operator*() const { return get(); }

    Source* operator->() { return get_ptr(); }
    const Source* operator->() const { return get_ptr(); }

  private:
    alignas(Source) std::byte storage_[sizeof(Source)];
    bool initialized_ = false;
};

template <typename Source> struct lvalue_source {
    using value_type = Source;

    explicit lvalue_source(Source& source) : value_(std::addressof(source)) {}

    Source& get() { return *value_; }
    const Source& get() const { return *value_; }

    Source* get_ptr() { return value_; }
    const Source* get_ptr() const { return value_; }

    Source& operator*() { return get(); }
    const Source& operator*() const { return get(); }

    Source* operator->() { return get_ptr(); }
    const Source* operator->() const { return get_ptr(); }

  private:
    Source* value_;
};

template <typename Source> struct pointer_source {
    using value_type = Source;

    explicit pointer_source(Source* source) : value_(source) {}

    Source* get() { return value_; }
    const Source* get() const { return value_; }

    Source& operator*() { return *value_; }
    const Source& operator*() const { return *value_; }

    Source* operator->() { return value_; }
    const Source* operator->() const { return value_; }

  private:
    Source* value_;
};

template <typename SourceCapability> struct materialized_source_traits;

template <typename Source>
struct materialized_source_traits<rvalue_source<Source>> {
    using value_type = Source;
    static constexpr bool reference_like = false;

    template <typename Capability>
    static decltype(auto) value(Capability&& source) {
        return std::forward<Capability>(source).get();
    }

    static Source& reference(rvalue_source<Source>& source) {
        return source.get();
    }

    static const Source& reference(const rvalue_source<Source>& source) {
        return source.get();
    }

    static Source* pointer(rvalue_source<Source>& source) {
        return source.get_ptr();
    }

    static const Source* pointer(const rvalue_source<Source>& source) {
        return source.get_ptr();
    }
};

template <typename Source>
struct materialized_source_traits<lvalue_source<Source>> {
    using value_type = Source;
    static constexpr bool reference_like = true;

    template <typename Capability>
    static decltype(auto) value(Capability&& source) {
        return std::forward<Capability>(source).get();
    }

    static Source& reference(lvalue_source<Source>& source) {
        return source.get();
    }

    static const Source& reference(const lvalue_source<Source>& source) {
        return source.get();
    }

    static Source* pointer(lvalue_source<Source>& source) {
        return source.get_ptr();
    }

    static const Source* pointer(const lvalue_source<Source>& source) {
        return source.get_ptr();
    }
};

template <typename Source>
struct materialized_source_traits<pointer_source<Source>> {
    using value_type = Source;
    static constexpr bool reference_like = true;

    template <typename Capability>
    static decltype(auto) value(Capability&& source) {
        return std::forward<Capability>(source).get();
    }

    static Source& reference(pointer_source<Source>& source) {
        return *source.get();
    }

    static const Source& reference(const pointer_source<Source>& source) {
        return *source.get();
    }

    static Source* pointer(pointer_source<Source>& source) {
        return source.get();
    }

    static const Source* pointer(const pointer_source<Source>& source) {
        return source.get();
    }
};

template <typename Source, typename Builder>
rvalue_source<Source> make_rvalue_source(std::in_place_t, Builder&& builder) {
    return rvalue_source<Source>(std::in_place, std::forward<Builder>(builder));
}

template <typename Source>
auto make_rvalue_source(Source&& source)
    -> rvalue_source<std::remove_cv_t<std::remove_reference_t<Source>>> {
    using value_type = std::remove_cv_t<std::remove_reference_t<Source>>;
    return rvalue_source<value_type>(std::forward<Source>(source));
}

template <typename Source>
lvalue_source<Source> make_lvalue_source(Source& source) {
    return lvalue_source<Source>(source);
}

template <typename Source>
pointer_source<Source> make_pointer_source(Source* source) {
    return pointer_source<Source>(source);
}

template <typename Source>
lvalue_source<Source> make_resolved_source(Source& source) {
    return make_lvalue_source(source);
}

template <typename Source>
pointer_source<Source> make_resolved_source(Source* source) {
    return make_pointer_source(source);
}

} // namespace detail
} // namespace dingo
