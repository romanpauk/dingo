//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The indexes of the key values of the candidates of a request. An index is a
// class template index<Value, Candidate> that a lookup backend names, which
// is default constructible and assignable and has
//
//   void build(const index_item<Value, Candidate> *items, std::size_t count);
//   template <typename Visit> void visit(const Value &key, Visit &&visit)
//   const;
//
// build() takes the candidates in the order of registration, which live until
// the next build(); visit() calls visit(candidate) for each candidate of `key`
// in that order until it returns false. Every index visits the candidates a
// linear scan does, in the same order, so it never changes which bindings a
// request names. The indexes of the library are such indexes, which its
// lookup backends name as a backend of the user names its own.
// spec: index_structures

#include <cstddef>

namespace dingo {

// A candidate of a request with the key value it is found by.
template <typename Value, typename Candidate> struct index_item {
  Value key;
  Candidate candidate;
};

} // namespace dingo
