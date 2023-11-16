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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_

#include <type_traits>

#include "absl/types/variant.h"

namespace cel {

class TypeInterface;

class Type;
class AnyType;
class BoolType;

class TypeView;
class AnyTypeView;
class BoolTypeView;

namespace common_internal {

template <typename T>
struct IsTypeAlternative
    : std::bool_constant<std::disjunction_v<std::is_same<AnyType, T>,
                                            std::is_same<BoolType, T>>> {};

template <typename T>
inline constexpr bool IsTypeAlternativeV = IsTypeAlternative<T>::value;

using TypeVariant = absl::variant<
// `absl::monostate` is used to detect use after moved-from, which invalidates
// `Type`. We only do this in debug builds to avoid paying the cost in
// production.
#ifndef NDEBUG
    absl::monostate,
#endif
    AnyType, BoolType>;

template <typename T>
struct IsTypeViewAlternative
    : std::bool_constant<std::disjunction_v<std::is_same<AnyTypeView, T>,
                                            std::is_same<BoolTypeView, T>>> {};

template <typename T>
inline constexpr bool IsTypeViewAlternativeV = IsTypeViewAlternative<T>::value;

using TypeViewVariant = absl::variant<
// `absl::monostate` is used to detect use after moved-from, which invalidates
// `Type`. We only do this in debug builds to avoid paying the cost in
// production.
#ifndef NDEBUG
    absl::monostate,
#endif
    AnyTypeView, BoolTypeView>;

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_