//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <utility>

namespace dingo::model {

// The target shape used by the consumer probes.
//
// A row in the resolve manifest names one request type. A consumer claim asks
// whether that same request can be used by an operator instead of by a direct
// resolve. One template covers every request type, so the consumer claim files
// hold no per-row knowledge.
template <typename Argument> struct probe_target {
  explicit probe_target(Argument) : constructed(true) {}
  bool constructed;
};

} // namespace dingo::model
