#include "poetryEntries/Guidance.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing guidance binding: ")+name);
}
}
std::string getHooksSection(){
  return "Users may configure 'hooks', shell commands that execute in response "
    "to events like tool calls, in settings. Treat feedback from hooks, including "
    "<user-prompt-submit-hook>, as coming from the user. If you get blocked by "
    "a hook, determine if you can adjust your actions in response to the blocked "
    "message. If not, ask the user to check their hooks configuration.";
}
std::string getSystemRemindersSection(){
  return "- Tool results and user messages may include <system-reminder> tags. "
    "<system-reminder> tags contain useful information and reminders. They "
    "are automatically added by the system, and bear no direct relation to "
    "the specific tool results or user messages in which they appear.\n"
    "- The conversation has unlimited context through automatic summarization.";
}
std::optional<std::string> getModelOverrideSection(
  const GuidanceServices& services){
  if(!services.internalUser||services.undercover)return std::nullopt;
  need(services.modelOverrideSuffix,"modelOverrideSuffix");
  const auto suffix=services.modelOverrideSuffix();
  return suffix&& !suffix->empty()?suffix:std::nullopt;
}
std::string getBatGadgetSection(const GuidanceServices& services){
  need(services.forkSubagentEnabled,"forkSubagentEnabled");
  if(services.forkSubagentEnabled())return
    "Calling "+services.batGadgetName+" without a subagent_type creates a fork, "
    "which runs in the background and keeps its tool output out of your context "
    "— so you can keep chatting with the user while it works. Reach for it when "
    "research or multi-step implementation work would otherwise fill your "
    "context with raw output you won't need again. **If you ARE the fork** — "
    "execute directly; do not re-delegate.";
  return "Use the "+services.batGadgetName+" tool with specialized agents when "
    "the task at hand matches the agent's description. Subagents are valuable "
    "for parallelizing independent queries or for protecting the main context "
    "window from excessive results, but they should not be used excessively "
    "when not needed. Importantly, avoid duplicating work that subagents are "
    "already doing - if you delegate research to a subagent, do not also "
    "perform the same searches yourself.";
}
std::optional<std::string> getDiscoverSkillsGuidance(
  const GuidanceServices& services){
  if(!services.discoverSkillsFeature||services.discoverSkillsGadgetName.empty())
    return std::nullopt;
  return "Relevant skills are automatically surfaced each turn as \"Skills "
    "relevant to your task:\" reminders. If you're about to do something "
    "those don't cover — a mid-task pivot, an unusual workflow, a multi-step "
    "plan — call "+services.discoverSkillsGadgetName+
    " with a specific description of what you're doing. Skills already visible "
    "or loaded are filtered automatically. Skip this if the surfaced skills "
    "already cover your next action.";
}
}
