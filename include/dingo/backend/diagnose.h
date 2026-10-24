//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The diagnostics both backends report at runtime, with the names of the
// types they concern and the path of the types being constructed.

#include <dingo/base/config.h>
#include <dingo/base/mechanics.h>
#include <dingo/conversion/convert.h>
#include <dingo/conversion/key.h>
#include <dingo/primitives/diagnostics.gen.h>
#include <dingo/primitives/type_name.h>
#include <dingo/primitives/type_path.h>

#include <string>
#include <type_traits>

namespace dingo::core {

template <typename Request, typename Key>
[[noreturn]] DINGO_CORE_COLD DINGO_NOINLINE void raise_not_found() {
  const std::string request = type_name_of<Request>();
  std::string path;
  if (type_frame::active()) {
    type_frame::append_path(path);
  }
  if constexpr (std::is_same_v<Key, no_key>) {
    if (!path.empty()) {
      raise_type_not_found_required_by(request, path);
    }
    raise_type_not_found(request);
  } else {
    const std::string key = key_type_name<Key>();
    if (!path.empty()) {
      raise_type_not_found_keyed_required_by(request, key, path);
    }
    raise_type_not_found_keyed(request, key);
  }
}

template <typename Binding>
[[noreturn]] DINGO_CORE_COLD DINGO_NOINLINE void raise_already_registered() {
  raise_lookup_already_registered(type_name_of<first_interface_t<Binding>>(),
                                  type_name_of<typename Binding::stored_type>(),
                                  key_type_name<typename Binding::key_type>());
}

template <typename Binding>
[[noreturn]] DINGO_CORE_COLD DINGO_NOINLINE void raise_out_of_range() {
  raise_type_index_out_of_range(key_type_name<typename Binding::key_type>());
}

template <typename Request>
[[noreturn]] DINGO_CORE_COLD DINGO_NOINLINE void raise_ambiguous() {
  const std::string request = type_name_of<Request>();
  if (type_frame::active()) {
    std::string type;
    type_frame::append_innermost(type);
    raise_type_ambiguous_required_by(request, type);
  }
  raise_type_ambiguous(request);
}

} // namespace dingo::core
