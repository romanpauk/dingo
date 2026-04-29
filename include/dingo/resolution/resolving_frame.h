//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/resolution/resolving_frame_fwd.h>
#include <dingo/type/type_descriptor.h>

namespace dingo {
namespace detail {

class context_path_state;

class resolving_frame {
  public:
    resolving_frame(context_path_state& context, type_descriptor type);

    resolving_frame(const resolving_frame&) = delete;
    resolving_frame& operator=(const resolving_frame&) = delete;
    resolving_frame(resolving_frame&&) = delete;
    resolving_frame& operator=(resolving_frame&&) = delete;

    ~resolving_frame();

  private:
    friend class context_path_state;

    context_path_state* context_;
    resolving_frame* parent_ = nullptr;
    type_descriptor type_;
};

} // namespace detail

} // namespace dingo
