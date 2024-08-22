// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TIMESTAMP_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TIMESTAMP_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class TypeParameters;

// `TimestampType` represents the primitive `timestamp` type.
class TimestampType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kTimestamp;
  static constexpr absl::string_view kName = "google.protobuf.Timestamp";

  TimestampType() = default;
  TimestampType(const TimestampType&) = default;
  TimestampType(TimestampType&&) = default;
  TimestampType& operator=(const TimestampType&) = default;
  TimestampType& operator=(TimestampType&&) = default;

  static TypeKind kind() { return kKind; }

  static absl::string_view name() { return kName; }

  static TypeParameters GetParameters();

  static std::string DebugString() { return std::string(name()); }

  constexpr void swap(TimestampType&) noexcept {}
};

inline constexpr void swap(TimestampType& lhs, TimestampType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(TimestampType, TimestampType) { return true; }

inline constexpr bool operator!=(TimestampType lhs, TimestampType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, TimestampType) {
  // TimestampType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const TimestampType& type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TIMESTAMP_TYPE_H_
