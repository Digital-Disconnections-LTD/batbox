#include "poetryEntries/Guidance.hpp"

namespace batbox::poetryEntries {
std::string getBatGadgetSection(const GuidanceServices& services){
  return "Use the "+services.batGadgetName+" tool with specialized agents when "
    "the task at hand matches the agent's description. Subagents are valuable "
    "for parallelizing independent queries or for protecting the main context "
    "window from excessive results, but they should not be used excessively "
    "when not needed. Importantly, avoid duplicating work that subagents are "
    "already doing - if you delegate research to a subagent, do not also "
    "perform the same searches yourself. When your next step depends on a "
    "background subagent's result, wait for it (TaskOutput with block: true) "
    "instead of starting a duplicate; if you have independent work, do that "
    "first, then wait.";
}
}
