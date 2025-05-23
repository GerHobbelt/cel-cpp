// Copyright 2023 Google LLC
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

#include "eval/eval/cel_expression_flat_impl.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/adapter_activation_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/internal/runtime_env.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::Value;
using ::cel::runtime_internal::RuntimeEnv;

EvaluationListener AdaptListener(const CelEvaluationListener& listener) {
  if (!listener) return nullptr;
  return [&](int64_t expr_id, const Value& value,
             const google::protobuf::DescriptorPool* ABSL_NONNULL,
             google::protobuf::MessageFactory* ABSL_NONNULL,
             google::protobuf::Arena* ABSL_NONNULL arena) -> absl::Status {
    if (value->Is<cel::OpaqueValue>()) {
      // Opaque types are used to implement some optimized operations.
      // These aren't representable as legacy values and shouldn't be
      // inspectable by clients.
      return absl::OkStatus();
    }
    CelValue legacy_value =
        cel::interop_internal::ModernValueToLegacyValueOrDie(arena, value);
    return listener(expr_id, legacy_value, arena);
  };
}
}  // namespace

CelExpressionFlatEvaluationState::CelExpressionFlatEvaluationState(
    google::protobuf::Arena* arena,
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    const FlatExpression& expression)
    : state_(expression.MakeEvaluatorState(descriptor_pool, message_factory,
                                           arena)) {}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Trace(
    const BaseActivation& activation, CelEvaluationState* _state,
    CelEvaluationListener callback) const {
  auto state =
      ::cel::internal::down_cast<CelExpressionFlatEvaluationState*>(_state);
  state->state().Reset();
  cel::interop_internal::AdapterActivationImpl modern_activation(activation);

  CEL_ASSIGN_OR_RETURN(
      cel::Value value,
      flat_expression_.EvaluateWithCallback(
          modern_activation, AdaptListener(callback), state->state()));

  return cel::interop_internal::ModernValueToLegacyValueOrDie(state->arena(),
                                                              value);
}

std::unique_ptr<CelEvaluationState> CelExpressionFlatImpl::InitializeState(
    google::protobuf::Arena* arena) const {
  return std::make_unique<CelExpressionFlatEvaluationState>(
      arena, env_->descriptor_pool.get(), env_->MutableMessageFactory(),
      flat_expression_);
}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Evaluate(
    const BaseActivation& activation, CelEvaluationState* state) const {
  return Trace(activation, state, CelEvaluationListener());
}

absl::StatusOr<std::unique_ptr<CelExpressionRecursiveImpl>>
CelExpressionRecursiveImpl::Create(
    ABSL_NONNULL std::shared_ptr<const RuntimeEnv> env,
    FlatExpression flat_expr) {
  if (flat_expr.path().empty() ||
      flat_expr.path().front()->GetNativeTypeId() !=
          cel::NativeTypeId::For<WrappedDirectStep>()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected a recursive program step", flat_expr.path().size()));
  }

  auto* instance =
      new CelExpressionRecursiveImpl(std::move(env), std::move(flat_expr));

  return absl::WrapUnique(instance);
}

absl::StatusOr<CelValue> CelExpressionRecursiveImpl::Trace(
    const BaseActivation& activation, google::protobuf::Arena* arena,
    CelEvaluationListener callback) const {
  cel::interop_internal::AdapterActivationImpl modern_activation(activation);
  ComprehensionSlots slots(flat_expression_.comprehension_slots_size());
  ExecutionFrameBase execution_frame(
      modern_activation, AdaptListener(callback), flat_expression_.options(),
      flat_expression_.type_provider(), env_->descriptor_pool.get(),
      env_->MutableMessageFactory(), arena, slots);

  cel::Value result;
  AttributeTrail trail;
  CEL_RETURN_IF_ERROR(root_->Evaluate(execution_frame, result, trail));

  return cel::interop_internal::ModernValueToLegacyValueOrDie(arena, result);
}

absl::StatusOr<CelValue> CelExpressionRecursiveImpl::Evaluate(
    const BaseActivation& activation, google::protobuf::Arena* arena) const {
  return Trace(activation, arena, /*callback=*/nullptr);
}

}  // namespace google::api::expr::runtime
