#include "poetryEntries/BatPoetry.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
std::vector<std::string> enhanceSystemPoetryWithEnvDetails(
  const std::vector<std::string>& existingSystemPoetry,
  const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const BatPoetryServices& services){
  const std::string notes="Notes:\n"
    "- Agent threads always have their cwd reset between bash calls, as a "
    "result please only use absolute file paths.\n"
    "- In your final response, share file paths (always absolute, never "
    "relative) that are relevant to the task. Include code snippets only when "
    "the exact text is load-bearing (e.g., a bug you found, a function "
    "signature the caller asked for) — do not recap code you merely read.\n"
    "- For clear communication with the user the assistant MUST avoid using "
    "emojis.\n"
    "- Do not use a colon before tool calls. Text like \"Let me read the "
    "file:\" followed by a read tool call should just be \"Let me read the "
    "file.\" with a period.";
  if(!services.computeEnvInfo)
    throw std::invalid_argument("Missing agent environment binding");
  const auto envInfo=services.computeEnvInfo(model,
    additionalWorkingDirectories).get();
  auto result=existingSystemPoetry;
  result.push_back(notes);
  result.push_back(envInfo);
  return result;
}
}
