// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PAGE_SIZE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PAGE_SIZE_H_

#include <cstddef>

#include "absl/base/attributes.h"

namespace cel::internal {

ABSL_ATTRIBUTE_CONST_FUNCTION size_t PageSize();

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PAGE_SIZE_H_
