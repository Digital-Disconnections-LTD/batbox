#pragma once
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include <exception>

namespace batbox::instructions::hooks::runtime {
struct ValidationIssue {
  std::vector<std::string> path;
  std::string message;
};
struct SchemaResult {
  std::optional<Json> json;
  std::vector<ValidationIssue> issues;
};
struct HookParseResult {
  std::optional<Json> json;
  std::optional<std::string> plainText,validationError;
};
struct HookParserServices {
  // The Zod hookJSONOutputSchema safeParse dependency must be supplied by a
  // real indigenous schema implementation. It returns Zod-stripped data/issues.
  std::function<SchemaResult(const Json&)> schemaSafeParse;
  std::function<void(const std::string&)> logDebug;
  std::function<std::string(std::exception_ptr)> stringifyThrown;
};
HookParseResult validateHookJson(const std::string& jsonString,
  const HookParserServices& services);
HookParseResult parseHookOutput(const std::string& stdoutText,
  const HookParserServices& services);
HookParseResult parseHttpHookOutput(const std::string& body,
  const HookParserServices& services);
void bindHookOutputParsers(ExecutorServices& executor,HookParserServices services);
} // namespace batbox::instructions::hooks::runtime
