#include "eval/eval/attribute_trail.h"

#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

using ::cel::extensions::ProtoMemoryManager;
using ::google::api::expr::v1alpha1::Expr;

// Attribute Trail behavior
TEST(AttributeTrailTest, AttributeTrailEmptyStep) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  std::string step = "step";
  CelValue step_value = CelValue::CreateString(&step);
  AttributeTrail trail;
  ASSERT_TRUE(trail.Step(&step).empty());
  ASSERT_TRUE(trail.Step(CreateCelAttributeQualifier(step_value)).empty());
}

TEST(AttributeTrailTest, AttributeTrailStep) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  std::string step = "step";
  CelValue step_value = CelValue::CreateString(&step);

  AttributeTrail trail = AttributeTrail("ident").Step(&step);

  ASSERT_EQ(trail.attribute(),
            CelAttribute("ident", {CreateCelAttributeQualifier(step_value)}));
}

}  // namespace google::api::expr::runtime
