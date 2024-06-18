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

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/values/values.h"

namespace cel {

absl::Status ParsedStructValueInterface::Equal(ValueManager& value_manager,
                                               ValueView other,
                                               Value& result) const {
  if (auto parsed_struct_value = As<ParsedStructValueView>(other);
      parsed_struct_value.has_value() &&
      NativeTypeId::Of(*this) == NativeTypeId::Of(*parsed_struct_value)) {
    return EqualImpl(value_manager, *parsed_struct_value, result);
  }
  if (auto struct_value = As<StructValueView>(other);
      struct_value.has_value()) {
    return common_internal::StructValueEqual(value_manager, *this,
                                             *struct_value, result);
  }
  result = BoolValueView{false};
  return absl::OkStatus();
}

absl::Status ParsedStructValueInterface::EqualImpl(ValueManager& value_manager,
                                                   ParsedStructValueView other,
                                                   Value& result) const {
  return common_internal::StructValueEqual(value_manager, *this, other, result);
}

absl::StatusOr<int> ParsedStructValueInterface::Qualify(
    ValueManager&, absl::Span<const SelectQualifier>, bool, Value&) const {
  return absl::UnimplementedError("Qualify not supported.");
}

}  // namespace cel
