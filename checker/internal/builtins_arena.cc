// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/builtins_arena.h"

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {

absl::Nonnull<google::protobuf::Arena*> BuiltinsArena() {
  static absl::NoDestructor<google::protobuf::Arena> kArena;
  return &(*kArena);
}

}  // namespace cel::checker_internal