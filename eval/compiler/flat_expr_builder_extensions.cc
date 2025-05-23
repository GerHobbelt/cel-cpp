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
#include "eval/compiler/flat_expr_builder_extensions.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/expr.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

namespace {

using Subexpression = google::api::expr::runtime::ProgramBuilder::Subexpression;

// Remap a recursive program to its parent if the parent is a transparent
// wrapper.
void MaybeReassignChildRecursiveProgram(Subexpression* parent) {
  if (parent->IsFlattened() || parent->IsRecursive()) {
    return;
  }
  if (parent->elements().size() != 1) {
    return;
  }
  auto* child_alternative =
      absl::get_if<Subexpression*>(&parent->elements()[0]);
  if (child_alternative == nullptr) {
    return;
  }

  auto& child_subexpression = *child_alternative;
  if (!child_subexpression->IsRecursive()) {
    return;
  }

  auto child_program = child_subexpression->ExtractRecursiveProgram();
  parent->set_recursive_program(std::move(child_program.step),
                                child_program.depth);
}

}  // namespace

Subexpression::Subexpression(const cel::Expr* self, ProgramBuilder* owner)
    : self_(self), parent_(nullptr), owner_(owner) {}

size_t Subexpression::ComputeSize() const {
  if (IsFlattened()) {
    return flattened_elements().size();
  } else if (IsRecursive()) {
    return 1;
  }
  std::vector<const Subexpression*> to_expand{this};
  size_t size = 0;
  while (!to_expand.empty()) {
    const auto* expr = to_expand.back();
    to_expand.pop_back();
    if (expr->IsFlattened()) {
      size += expr->flattened_elements().size();
      continue;
    } else if (expr->IsRecursive()) {
      size += 1;
      continue;
    }
    for (const auto& elem : expr->elements()) {
      if (auto* child = absl::get_if<Subexpression*>(&elem); child != nullptr) {
        to_expand.push_back(*child);
      } else {
        size += 1;
      }
    }
  }
  return size;
}

absl::optional<int> Subexpression::RecursiveDependencyDepth() const {
  auto* tree = absl::get_if<TreePlan>(&program_);
  int depth = 0;
  if (tree == nullptr) {
    return absl::nullopt;
  }
  for (const auto& element : *tree) {
    auto* subexpression = absl::get_if<Subexpression*>(&element);
    if (subexpression == nullptr) {
      return absl::nullopt;
    }
    if (!(*subexpression)->IsRecursive()) {
      return absl::nullopt;
    }
    depth = std::max(depth, (*subexpression)->recursive_program().depth);
  }
  return depth;
}

std::vector<std::unique_ptr<DirectExpressionStep>>
Subexpression::ExtractRecursiveDependencies() const {
  auto* tree = absl::get_if<TreePlan>(&program_);
  std::vector<std::unique_ptr<DirectExpressionStep>> dependencies;
  if (tree == nullptr) {
    return {};
  }
  for (const auto& element : *tree) {
    auto* subexpression = absl::get_if<Subexpression*>(&element);
    if (subexpression == nullptr) {
      return {};
    }
    if (!(*subexpression)->IsRecursive()) {
      return {};
    }
    dependencies.push_back((*subexpression)->ExtractRecursiveProgram().step);
  }
  return dependencies;
}

Subexpression* ABSL_NULLABLE Subexpression::ExtractChild(Subexpression* child) {
  ABSL_DCHECK(child != nullptr);
  if (IsFlattened()) {
    return nullptr;
  }
  for (auto iter = elements().begin(); iter != elements().end(); ++iter) {
    Subexpression::Element& element = *iter;
    if (!absl::holds_alternative<Subexpression*>(element)) {
      continue;
    }
    Subexpression* candidate = absl::get<Subexpression*>(element);
    if (candidate != child) {
      continue;
    }
    elements().erase(iter);
    return candidate;
  }
  return nullptr;
}

int Subexpression::CalculateOffset(int base, int target) const {
  ABSL_DCHECK(!IsFlattened());
  ABSL_DCHECK(!IsRecursive());
  ABSL_DCHECK_GE(base, 0);
  ABSL_DCHECK_GE(target, 0);
  ABSL_DCHECK_LE(base, elements().size());
  ABSL_DCHECK_LE(target, elements().size());

  int sign = 1;

  if (target <= base) {
    // target is before base so have to consider the size of the base step and
    // target (offset is end of base to beginning of target).
    int tmp = base;
    base = target - 1;
    target = tmp + 1;
    sign = -1;
  }

  int sum = 0;
  for (int i = base + 1; i < target; ++i) {
    const auto& element = elements()[i];
    if (auto* subexpr = absl::get_if<Subexpression*>(&element);
        subexpr != nullptr) {
      sum += (*subexpr)->ComputeSize();
    } else {
      sum += 1;
    }
  }

  return sign * sum;
}

void Subexpression::Flatten() {
  struct Record {
    Subexpression* subexpr;
    size_t offset;
  };

  if (IsFlattened()) {
    return;
  }

  std::vector<std::unique_ptr<const ExpressionStep>> flat;

  std::vector<Record> flatten_stack;

  flatten_stack.push_back({this, 0});
  while (!flatten_stack.empty()) {
    Record top = flatten_stack.back();
    flatten_stack.pop_back();
    size_t offset = top.offset;
    auto* subexpr = top.subexpr;
    if (subexpr->IsFlattened()) {
      auto& elements = subexpr->flattened_elements();
      absl::c_move(elements, std::back_inserter(flat));
      elements.clear();
      continue;
    } else if (subexpr->IsRecursive()) {
      flat.push_back(std::make_unique<WrappedDirectStep>(
          std::move(subexpr->ExtractRecursiveProgram().step),
          subexpr->self_->id()));
      continue;
    }
    auto& elements = subexpr->elements();
    size_t size = elements.size();
    size_t i = offset;
    for (; i < size; ++i) {
      auto& element = elements[i];
      if (auto* child = absl::get_if<Subexpression*>(&element);
          child != nullptr) {
        // push resume then child so child elements are processed first.
        flatten_stack.push_back({subexpr, i + 1});
        flatten_stack.push_back({*child, 0});
        break;
      } else if (auto* step =
                     absl::get_if<std::unique_ptr<ExpressionStep>>(&element);
                 step != nullptr) {
        flat.push_back(std::move(*step));
      } else {
        ABSL_UNREACHABLE();
      }
    }
    if (i == size) {
      elements.clear();
    }
  }
  program_ = std::move(flat);
}

Subexpression::RecursiveProgram Subexpression::ExtractRecursiveProgram() {
  ABSL_DCHECK(IsRecursive());
  auto result = std::move(absl::get<RecursiveProgram>(program_));
  program_.emplace<std::vector<Subexpression::Element>>();
  return result;
}

bool Subexpression::ExtractTo(
    std::vector<std::unique_ptr<const ExpressionStep>>& out) {
  if (!IsFlattened()) {
    return false;
  }

  out.reserve(out.size() + flattened_elements().size());
  absl::c_move(flattened_elements(), std::back_inserter(out));
  program_.emplace<std::vector<Element>>();

  return true;
}

std::vector<std::unique_ptr<const ExpressionStep>>
ProgramBuilder::FlattenSubexpression(Subexpression* expr) {
  std::vector<std::unique_ptr<const ExpressionStep>> out;

  if (!expr) {
    return out;
  }

  expr->Flatten();
  expr->ExtractTo(out);
  return out;
}

ProgramBuilder::ProgramBuilder()
    : root_(nullptr), current_(nullptr), subprogram_map_() {}

ExecutionPath ProgramBuilder::FlattenMain() {
  auto out = FlattenSubexpression(root_);
  root_ = nullptr;
  return out;
}

std::vector<ExecutionPath> ProgramBuilder::FlattenSubexpressions() {
  std::vector<ExecutionPath> out;
  out.reserve(extracted_subexpressions_.size());
  for (auto& subexpression : extracted_subexpressions_) {
    out.push_back(FlattenSubexpression(subexpression));
  }
  extracted_subexpressions_.clear();
  return out;
}

Subexpression* ABSL_NULLABLE ProgramBuilder::EnterSubexpression(
    const cel::Expr* expr, size_t size_hint) {
  Subexpression* subexpr = MakeSubexpression(expr);
  if (subexpr == nullptr) {
    return subexpr;
  }

  subexpr->elements().reserve(size_hint);
  if (current_ == nullptr) {
    root_ = subexpr;
    current_ = subexpr;
    return subexpr;
  }

  current_->AddSubexpression(subexpr);
  subexpr->parent_ = current_->self_;
  current_ = subexpr;
  return subexpr;
}

Subexpression* ABSL_NULLABLE ProgramBuilder::ExitSubexpression(
    const cel::Expr* expr) {
  ABSL_DCHECK(expr == current_->self_);
  ABSL_DCHECK(GetSubexpression(expr) == current_);

  MaybeReassignChildRecursiveProgram(current_);

  Subexpression* result = GetSubexpression(current_->parent_);
  ABSL_DCHECK(result != nullptr || current_ == root_);
  current_ = result;
  return result;
}

Subexpression* ABSL_NULLABLE ProgramBuilder::GetSubexpression(
    const cel::Expr* expr) {
  auto it = subprogram_map_.find(expr);
  if (it == subprogram_map_.end()) {
    return nullptr;
  }

  return it->second.get();
}

void ProgramBuilder::AddStep(std::unique_ptr<ExpressionStep> step) {
  if (current_ == nullptr) {
    return;
  }
  current_->AddStep(std::move(step));
}

int ProgramBuilder::ExtractSubexpression(const cel::Expr* expr) {
  auto it = subprogram_map_.find(expr);
  if (it == subprogram_map_.end()) {
    return -1;
  }
  auto* subexpression = it->second.get();
  auto parent_it = subprogram_map_.find(subexpression->parent_);
  if (parent_it == subprogram_map_.end()) {
    return -1;
  }

  auto* parent = parent_it->second.get();

  auto* child = parent->ExtractChild(subexpression);

  if (child == nullptr) {
    return -1;
  }

  extracted_subexpressions_.push_back(child);
  return extracted_subexpressions_.size() - 1;
}

Subexpression* ABSL_NULLABLE ProgramBuilder::MakeSubexpression(
    const cel::Expr* expr) {
  auto [it, inserted] = subprogram_map_.try_emplace(
      expr, absl::WrapUnique(new Subexpression(expr, this)));
  if (!inserted) {
    return nullptr;
  }

  return it->second.get();
}

bool PlannerContext::IsSubplanInspectable(const cel::Expr& node) const {
  return program_builder_.GetSubexpression(&node) != nullptr;
}

ExecutionPathView PlannerContext::GetSubplan(const cel::Expr& node) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return ExecutionPathView();
  }
  subexpression->Flatten();
  return subexpression->flattened_elements();
}

absl::StatusOr<ExecutionPath> PlannerContext::ExtractSubplan(
    const cel::Expr& node) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  subexpression->Flatten();

  ExecutionPath out;
  subexpression->ExtractTo(out);

  return out;
}

absl::Status PlannerContext::ReplaceSubplan(const cel::Expr& node,
                                            ExecutionPath path) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  // Make sure structure for descendents is erased.
  if (!subexpression->IsFlattened()) {
    subexpression->Flatten();
  }

  subexpression->flattened_elements() = std::move(path);

  return absl::OkStatus();
}

void ProgramBuilder::Reset() {
  root_ = nullptr;
  current_ = nullptr;
  extracted_subexpressions_.clear();
  subprogram_map_.clear();
}

absl::Status PlannerContext::ReplaceSubplan(
    const cel::Expr& node, std::unique_ptr<DirectExpressionStep> step,
    int depth) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  subexpression->set_recursive_program(std::move(step), depth);
  return absl::OkStatus();
}

absl::Status PlannerContext::AddSubplanStep(
    const cel::Expr& node, std::unique_ptr<ExpressionStep> step) {
  auto* subexpression = program_builder_.GetSubexpression(&node);

  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  subexpression->AddStep(std::move(step));

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
