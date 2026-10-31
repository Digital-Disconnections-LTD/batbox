#include "poetryEntries/BatPoetry.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
std::string defaultBatPoetry(){
  return "You are an agent for Batbox, an interactive coding assistant. Given "
    "the user's message, you should use the tools available to complete the "
    "task. Complete the task fully—don't gold-plate, but don't leave it "
    "half-done. When you complete the task, respond with a concise report "
    "covering what was done and any key findings — the caller will relay this "
    "to the user, so it only needs the essentials.";
}
std::vector<std::string> enhanceSystemPoetryWithEnvDetails(
  const std::vector<std::string>& existingSystemPoetry,
  const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const std::optional<GadgetNames>& enabledGadgetNames,
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
  std::optional<std::string> discoverGuidance;
  if(services.guidance.discoverSkillsFeature){
    if(!services.skillSearchEnabled)
      throw std::invalid_argument("Missing skill-search feature binding");
    if(services.skillSearchEnabled()&&
       !services.guidance.discoverSkillsGadgetName.empty()&&
       (!enabledGadgetNames||enabledGadgetNames->contains(
         services.guidance.discoverSkillsGadgetName)))
      discoverGuidance=getDiscoverSkillsGuidance(services.guidance);
  }
  if(!services.computeEnvInfo)
    throw std::invalid_argument("Missing agent environment binding");
  const auto envInfo=services.computeEnvInfo(model,
    additionalWorkingDirectories).get();
  auto result=existingSystemPoetry;
  result.push_back(notes);
  if(discoverGuidance)result.push_back(*discoverGuidance);
  result.push_back(envInfo);
  return result;
}
}
