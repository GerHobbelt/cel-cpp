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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_COMPREHENSIONS_V2_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_COMPREHENSIONS_V2_FUNCTIONS_H_

#include "absl/status/status.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

// Register comprehension v2 functions.
absl::Status RegisterComprehensionsV2Functions(FunctionRegistry& registry,
                                               const RuntimeOptions& options);
absl::Status RegisterComprehensionsV2Functions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_COMPREHENSIONS_V2_FUNCTIONS_H_
