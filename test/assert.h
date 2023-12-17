//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

namespace dingo {
template <typename Type, typename NonConvertibleTypes, typename Container>
void AssertTypeNotConvertible(Container& container) {
    for_each(NonConvertibleTypes{}, [&](auto element) {
        ASSERT_THROW(
            container.template resolve<typename decltype(element)::type>(),
            type_not_convertible_exception);
    });
}

template <class T> void AssertClass(T&& cls) {
    ASSERT_EQ(cls.GetName(), "Class");
}

template <class T> void AssertClass(T* cls) { AssertClass(*cls); }

template <class T> void AssertClass(std::unique_ptr<T>&& cls) {
    AssertClass(*cls);
}

template <class T> void AssertClass(std::shared_ptr<T>&& cls) {
    AssertClass(*cls);
}

} // namespace dingo