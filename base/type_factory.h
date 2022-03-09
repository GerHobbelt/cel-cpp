// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_

#include "absl/base/attributes.h"
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/type.h"

namespace cel {

// TypeFactory provides member functions to get and create type implementations
// of builtin types.
class TypeFactory {
 public:
  virtual ~TypeFactory() = default;

  Persistent<const NullType> GetNullType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const ErrorType> GetErrorType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DynType> GetDynType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const AnyType> GetAnyType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const BoolType> GetBoolType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const IntType> GetIntType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const UintType> GetUintType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DoubleType> GetDoubleType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const StringType> GetStringType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const BytesType> GetBytesType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DurationType> GetDurationType()
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const TimestampType> GetTimestampType()
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

 protected:
  // Prevent direct intantiation until more pure virtual methods are added.
  explicit TypeFactory(MemoryManager& memory_manager)
      : memory_manager_(memory_manager) {}

  // Ignore unused for now, as it will be used in the future.
  ABSL_ATTRIBUTE_UNUSED MemoryManager& memory_manager() const {
    return memory_manager_;
  }

 private:
  template <typename T>
  static Persistent<const T> WrapSingletonType() {
    // This is not normal, but we treat the underlying object as having been
    // arena allocated. The only way to do this is through
    // TransientHandleFactory.
    return Persistent<const T>(
        base_internal::TransientHandleFactory<const T>::template MakeUnmanaged<
            const T>(T::Get()));
  }

  MemoryManager& memory_manager_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_
