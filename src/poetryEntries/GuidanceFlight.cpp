#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/PoetryBlocks.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
template<class Fn> void need(const Fn& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Missing guidance binding: ")+name);
}
std::string lines(const std::vector<std::string>& items){
  std::string result;
  for(std::size_t i=0;i<items.size();++i){if(i)result+='\n';result+=items[i];}
  return result;
}
}
std::optional<std::string> getFlightSpecificGuidanceSection(
  const GadgetNames& enabledGadgets,const Json& skillGadgetCommands,
  const GuidanceServices& services){
  const bool hasAsk=enabledGadgets.contains(services.askUserQuestionGadgetName);
  const bool hasSkills=skillGadgetCommands.is_array()&&
    !skillGadgetCommands.empty()&&enabledGadgets.contains(services.skillGadgetName);
  const bool hasBat=enabledGadgets.contains(services.batGadgetName);
  need(services.embeddedSearchGadgets,"embeddedSearchTools");
  const auto searchGadgets=services.embeddedSearchGadgets()
    ?"`find` or `grep` via the "+services.bashGadgetName+" tool"
    :"the "+services.globGadgetName+" or "+services.grepGadgetName;
  Json items=Json::array();
  if(hasAsk)items.push_back("If you do not understand why the user has "
    "denied a tool call, use the "+services.askUserQuestionGadgetName+
    " to ask them.");
  need(services.nonInteractiveFlight,"nonInteractiveSession");
  if(!services.nonInteractiveFlight())items.push_back(
    "If you need the user to run a shell command themselves (e.g., an "
    "interactive login like `gcloud auth login`), suggest they type `! "
    "<command>` in the prompt — the `!` prefix runs the command in this "
    "session so its output lands directly in the conversation.");
  if(hasBat)items.push_back(getBatGadgetSection(services));
  if(hasBat){
    need(services.explorePlanBatsEnabled,"explorePlanAgentsEnabled");
    if(services.explorePlanBatsEnabled()){
      need(services.forkSubagentEnabled,"forkSubagentEnabled");
      if(!services.forkSubagentEnabled()){
        items.push_back("For simple, directed codebase searches (e.g. for a "
          "specific file/class/function) use "+searchGadgets+" directly.");
        items.push_back("For broader codebase exploration and deep research, "
          "use the "+services.batGadgetName+" tool with subagent_type="+
          services.exploreBatType+". This is slower than using "+
          searchGadgets+" directly, so use this only when a simple, directed "
          "search proves to be insufficient or when your task will clearly "
          "require more than "+
          std::to_string(services.exploreBatMinQueries)+" queries.");
      }
    }
  }
  if(hasSkills)items.push_back(
    "/<skill-name> (e.g., /commit) is shorthand for users to invoke a "
    "user-invocable skill. When executed, the skill gets expanded to a full "
    "prompt. Use the "+services.skillGadgetName+
    " tool to execute them. IMPORTANT: Only use "+services.skillGadgetName+
    " for skills listed in its user-invocable skills section - do not guess "
    "or use built-in CLI commands.");
  if(!services.discoverSkillsGadgetName.empty()&&hasSkills&&
     enabledGadgets.contains(services.discoverSkillsGadgetName)){
    const auto guidance=getDiscoverSkillsGuidance(services);
    if(guidance)items.push_back(*guidance);
  }
  if(hasBat&&services.verificationBatFeature){
    need(services.verificationEnabled,"verificationEnabled");
    if(services.verificationEnabled())items.push_back(
      "The contract: when non-trivial implementation happens on your turn, "
      "independent adversarial verification must happen before you report "
      "completion — regardless of who did the implementing (you directly, "
      "a fork you spawned, or a subagent). You are the one reporting to the "
      "user; you own the gate. Non-trivial means: 3+ file edits, backend/API "
      "changes, or infrastructure changes. Spawn the "+services.batGadgetName+
      " tool with subagent_type=\""+services.verificationBatType+
      "\". Your own checks, caveats, and a fork's self-checks do NOT "
      "substitute — only the verifier assigns a verdict; you cannot "
      "self-assign PARTIAL. Pass the original user request, all files "
      "changed (by anyone), the approach, and the plan file path if "
      "applicable. Flag concerns if you have them but do NOT share test "
      "results or claim things work. On FAIL: fix, resume the verifier with "
      "its findings plus your fix, repeat until PASS. On PASS: spot-check "
      "it — re-run 2-3 commands from its report, confirm every PASS has a "
      "Command run block with output that matches your re-run. If any PASS "
      "lacks a command block or diverges, resume the verifier with the "
      "specifics. On PARTIAL (from the verifier): report what passed and "
      "what could not be verified.");
  }
  if(items.empty())return std::nullopt;
  auto bullets=prependBullets(items);
  bullets.insert(bullets.begin(),"# Session-specific guidance");
  return lines(bullets);
}
}
