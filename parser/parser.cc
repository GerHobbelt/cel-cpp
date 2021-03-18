#include "parser/parser.h"

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "parser/cel_grammar.inc/cel_grammar/CelLexer.h"
#include "parser/cel_grammar.inc/cel_grammar/CelParser.h"
#include "parser/source_factory.h"
#include "parser/visitor.h"
#include "antlr4-runtime.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using ::antlr4::ANTLRInputStream;
using ::antlr4::CommonTokenStream;
using ::antlr4::ParseCancellationException;
using ::antlr4::ParserRuleContext;

using ::antlr4::tree::ErrorNode;
using ::antlr4::tree::ParseTreeListener;
using ::antlr4::tree::TerminalNode;

using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;

using ::cel_grammar::CelLexer;
using ::cel_grammar::CelParser;

namespace {

// ExprRecursionListener extends the standard ANTLR CelParser to ensure that
// recursive entries into the 'expr' rule are limited to a configurable depth so
// as to prevent stack overflows.
class ExprRecursionListener : public ParseTreeListener {
 public:
  explicit ExprRecursionListener(
      const int max_recursion_depth = kDefaultMaxRecursionDepth)
      : max_recursion_depth_(max_recursion_depth), recursion_depth_(0) {}
  ~ExprRecursionListener() override {}

  void visitTerminal(TerminalNode* node) override{};
  void visitErrorNode(ErrorNode* error) override{};
  void enterEveryRule(ParserRuleContext* ctx) override;
  void exitEveryRule(ParserRuleContext* ctx) override;

 private:
  const int max_recursion_depth_;
  int recursion_depth_;
};

void ExprRecursionListener::enterEveryRule(ParserRuleContext* ctx) {
  // Throw a ParseCancellationException since the parsing would otherwise
  // continue if this were treated as a syntax error and the problem would
  // continue to manifest.
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    if (recursion_depth_ >= max_recursion_depth_) {
      throw ParseCancellationException(
          absl::StrFormat("Expression recursion limit exceeded. limit: %d",
                          max_recursion_depth_));
    }
    recursion_depth_++;
  }
}

void ExprRecursionListener::exitEveryRule(ParserRuleContext* ctx) {
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    recursion_depth_--;
  }
}

class RecoveryLimitErrorStrategy : public antlr4::DefaultErrorStrategy {
 public:
  explicit RecoveryLimitErrorStrategy(
      int recovery_limit = kDefaultErrorRecoveryLimit)
      : recovery_limit_(recovery_limit), recovery_attempts_(0) {}

  void recover(antlr4::Parser* recognizer, std::exception_ptr e) override {
    checkRecoveryLimit(recognizer);
    antlr4::DefaultErrorStrategy::recover(recognizer, e);
  }

  antlr4::Token* recoverInline(antlr4::Parser* recognizer) override {
    checkRecoveryLimit(recognizer);
    return antlr4::DefaultErrorStrategy::recoverInline(recognizer);
  }

 private:
  void checkRecoveryLimit(antlr4::Parser* recognizer) {
    if (recovery_attempts_++ >= recovery_limit_) {
      std::string too_many_errors =
          absl::StrFormat("More than %d parse errors.", recovery_limit_);
      recognizer->notifyErrorListeners(too_many_errors);
      throw ParseCancellationException(too_many_errors);
    }
  }

  int recovery_limit_;
  int recovery_attempts_;
};

}  // namespace

absl::StatusOr<ParsedExpr> Parse(const std::string& expression,
                                 const std::string& description,
                                 int max_recursion_depth,
                                 int error_recovery_limit) {
  return ParseWithMacros(expression, Macro::AllMacros(), description,
                         max_recursion_depth, error_recovery_limit);
}

absl::StatusOr<ParsedExpr> ParseWithMacros(const std::string& expression,
                                           const std::vector<Macro>& macros,
                                           const std::string& description,
                                           int max_recursion_depth,
                                           int error_recovery_limit) {
  auto result = EnrichedParse(expression, macros, description,
                              max_recursion_depth, error_recovery_limit);
  if (result.ok()) {
    return result->parsed_expr();
  }
  return result.status();
}

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description, int max_recursion_depth,
    int error_recovery_limit) {
  ANTLRInputStream input(expression);
  CelLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  CelParser parser(&tokens);
  ExprRecursionListener listener(max_recursion_depth);
  ParserVisitor visitor(description, expression, max_recursion_depth, macros);

  lexer.removeErrorListeners();
  parser.removeErrorListeners();
  lexer.addErrorListener(&visitor);
  parser.addErrorListener(&visitor);
  parser.addParseListener(&listener);

  // Limit the number of error recovery attempts to prevent bad expressions
  // from consuming lots of cpu / memory.
  std::shared_ptr<RecoveryLimitErrorStrategy> error_strategy(
      new RecoveryLimitErrorStrategy(error_recovery_limit));
  parser.setErrorHandler(error_strategy);

  CelParser::StartContext* root;
  try {
    root = parser.start();
  } catch (const ParseCancellationException& e) {
    if (visitor.hasErrored()) {
      return absl::InvalidArgumentError(visitor.errorMessage());
    }
    return absl::CancelledError(e.what());
  } catch (const std::exception& e) {
    return absl::AbortedError(e.what());
  }

  Expr expr = visitor.visit(root).as<Expr>();
  if (visitor.hasErrored()) {
    return absl::InvalidArgumentError(visitor.errorMessage());
  }

  // root is deleted as part of the parser context
  ParsedExpr parsed_expr;
  *(parsed_expr.mutable_expr()) = std::move(expr);
  auto enriched_source_info = visitor.enrichedSourceInfo();
  *(parsed_expr.mutable_source_info()) = visitor.sourceInfo();
  return VerboseParsedExpr(std::move(parsed_expr),
                           std::move(enriched_source_info));
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
