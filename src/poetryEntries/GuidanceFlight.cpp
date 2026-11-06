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
  const auto searchGadgets="the "+services.globGadgetName+" or "+services.grepGadgetName;
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
  if(hasSkills)items.push_back(
    "/<skill-name> (e.g., /commit) is shorthand for users to invoke a "
    "user-invocable skill. When executed, the skill gets expanded to a full "
    "prompt. Use the "+services.skillGadgetName+
    " tool to execute them. IMPORTANT: Only use "+services.skillGadgetName+
    " for skills listed in its user-invocable skills section - do not guess "
    "or use built-in CLI commands.");
  if(items.empty())return std::nullopt;
  auto bullets=prependBullets(items);
  bullets.insert(bullets.begin(),"# Session-specific guidance");
  return lines(bullets);
}
}
