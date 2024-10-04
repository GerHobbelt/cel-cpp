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

#include "checker/optional.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_join.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/standard_library.h"
#include "checker/type_check_issue.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "extensions/protobuf/type_reflector.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::cel::checker_internal::MakeTestParsedAst;
using ::google::api::expr::test::v1::proto3::TestAllTypes;
using ::testing::_;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Property;

using AstType = ast_internal::Type;

MATCHER_P(IsOptionalType, inner_type, "") {
  const ast_internal::Type& type = arg;
  if (!type.has_abstract_type()) {
    return false;
  }
  const auto& abs_type = type.abstract_type();
  if (abs_type.name() != "optional_type") {
    *result_listener << "expected optional_type, got: " << abs_type.name();
    return false;
  }
  if (abs_type.parameter_types().size() != 1) {
    *result_listener << "unexpected number of parameters: "
                     << abs_type.parameter_types().size();
    return false;
  }

  if (inner_type == abs_type.parameter_types()[0]) {
    return true;
  }

  *result_listener << "unexpected inner type: "
                   << abs_type.parameter_types()[0].type_kind().index();
  return false;
}

struct TestCase {
  std::string expr;
  testing::Matcher<ast_internal::Type> result_type_matcher;
  std::string error_substring;
};

class OptionalTest : public testing::TestWithParam<TestCase> {};

TEST_P(OptionalTest, Runner) {
  TypeCheckerBuilder builder;
  const TestCase& test_case = GetParam();
  ASSERT_THAT(builder.AddLibrary(StandardLibrary()), IsOk());
  ASSERT_THAT(builder.AddLibrary(OptionalCheckerLibrary()), IsOk());
  google::protobuf::LinkMessageReflection<TestAllTypes>();
  builder.AddTypeProvider(
      std::make_unique<cel::extensions::ProtoTypeReflector>());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> checker,
                       std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));

  ASSERT_OK_AND_ASSIGN(auto result, checker->Check(std::move(ast)));

  if (!test_case.error_substring.empty()) {
    EXPECT_THAT(result.GetIssues(),
                Contains(Property(&TypeCheckIssue::message,
                                  HasSubstr(test_case.error_substring))))
        << absl::StrJoin(result.GetIssues(), "\n",
                         [](std::string* out, const auto& i) {
                           absl::StrAppend(out, i.message());
                         });
    return;
  }

  EXPECT_THAT(result.GetIssues(), IsEmpty())
      << "for expression: " << test_case.expr;
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  const auto& ast_impl = ast_internal::AstImpl::CastFromPublicAst(*checked_ast);

  int64_t root_id = ast_impl.root_expr().id();

  EXPECT_THAT(ast_impl.GetType(root_id), test_case.result_type_matcher)
      << "for expression: " << test_case.expr;
}

INSTANTIATE_TEST_SUITE_P(
    OptionalTests, OptionalTest,
    ::testing::Values(
        TestCase{
            "optional.of('abc')",
            IsOptionalType(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{
            "optional.ofNonZeroValue('')",
            IsOptionalType(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{
            "optional.none()",
            IsOptionalType(AstType(ast_internal::DynamicType())),
        },
        TestCase{
            "optional.of('abc').hasValue()",
            Eq(AstType(ast_internal::PrimitiveType::kBool)),
        },
        TestCase{
            "optional.of('abc').value()",
            Eq(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{
            "type(optional.of('abc')) == optional_type",
            Eq(AstType(ast_internal::PrimitiveType::kBool)),
        },
        TestCase{
            "type(optional.of('abc')) == optional_type",
            Eq(AstType(ast_internal::PrimitiveType::kBool)),
        },
        TestCase{
            "optional.of('abc').or(optional.of('def'))",
            IsOptionalType(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{"optional.of('abc').or(optional.of(1))", _,
                 "no matching overload for 'or'"},
        TestCase{
            "optional.of('abc').orValue('def')",
            Eq(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{"optional.of('abc').orValue(1)", _,
                 "no matching overload for 'orValue'"},
        TestCase{
            "{'k': 'v'}.?k",
            IsOptionalType(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{"1.?k", _,
                 "expression of type 'int' cannot be the operand of a select "
                 "operation"},
        TestCase{
            "{'k': {'k': 'v'}}.?k.?k2",
            IsOptionalType(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{
            "{'k': {'k': 'v'}}.?k.k2",
            IsOptionalType(AstType(ast_internal::PrimitiveType::kString)),
        },
        TestCase{"{?'k': optional.of('v')}",
                 Eq(AstType(ast_internal::MapType(
                     std::unique_ptr<AstType>(
                         new AstType(ast_internal::PrimitiveType::kString)),
                     std::unique_ptr<AstType>(
                         new AstType(ast_internal::PrimitiveType::kString)))))},
        TestCase{"{'k': 'v', ?'k2': optional.none()}",
                 Eq(AstType(ast_internal::MapType(
                     std::unique_ptr<AstType>(
                         new AstType(ast_internal::PrimitiveType::kString)),
                     std::unique_ptr<AstType>(
                         new AstType(ast_internal::PrimitiveType::kString)))))},
        TestCase{"{'k': 'v', ?'k2': 'v'}", _,
                 "expected type 'optional_type<string>' but found 'string'"},
        TestCase{"[?optional.of('v')]",
                 Eq(AstType(ast_internal::ListType(std::unique_ptr<AstType>(
                     new AstType(ast_internal::PrimitiveType::kString)))))},
        TestCase{"['v', ?optional.none()]",
                 Eq(AstType(ast_internal::ListType(std::unique_ptr<AstType>(
                     new AstType(ast_internal::PrimitiveType::kString)))))},
        TestCase{"['v1', ?'v2']", _,
                 "expected type 'optional_type<string>' but found 'string'"},
        TestCase{"google.api.expr.test.v1.proto3.TestAllTypes{?single_int64: "
                 "optional.of(1)}",
                 Eq(AstType(ast_internal::MessageType(
                     "google.api.expr.test.v1.proto3.TestAllTypes")))},
        TestCase{"[0][?1]",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kInt64))},
        TestCase{"[[0]][?1][?1]",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kInt64))},
        TestCase{"[[0]][?1][1]",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kInt64))},
        TestCase{"{0: 1}[?1]",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kInt64))},
        TestCase{"{0: {0: 1}}[?1][?1]",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kInt64))},
        TestCase{"{0: {0: 1}}[?1][1]",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kInt64))},
        TestCase{"{0: {0: 1}}[?1]['']", _, "no matching overload for '_[_]'"},
        TestCase{"{0: {0: 1}}[?1][?'']", _, "no matching overload for '_[?_]'"},
        TestCase{"optional.of('abc').optMap(x, x + 'def')",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kString))},
        TestCase{"optional.of('abc').optFlatMap(x, optional.of(x + 'def'))",
                 IsOptionalType(AstType(ast_internal::PrimitiveType::kString))}

        ));

}  // namespace
}  // namespace cel
