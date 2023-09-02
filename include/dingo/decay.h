#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>

#include <memory>
#include <type_traits>

namespace dingo {
// TODO: this has misleadnig name
template <class T> struct decay : std::decay<T> {};
template <class T> struct decay<T*> : decay<T> {};
template <class T> struct decay<T&> : decay<T> {};
template <class T> struct decay<T&&> : decay<T> {};

template <class T, class Deleter> struct decay<std::unique_ptr<T, Deleter>> : std::decay<T> {};

template <class T> struct decay<std::shared_ptr<T>> : std::decay<T> {};

template <class T, class Tag> struct decay<annotated<T, Tag>> : std::decay<annotated<typename decay<T>::type, Tag>> {};

template <class T> using decay_t = typename decay<T>::type;
} // namespace dingo