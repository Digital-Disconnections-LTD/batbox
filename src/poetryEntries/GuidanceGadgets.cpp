#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/PoetryBlocks.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
std::string joinLines(const std::vector<std::string>& lines){
  std::string result;
  for(std::size_t i=0;i<lines.size();++i){if(i)result+='\n';result+=lines[i];}
  return result;
}
}
std::string getUsingYourGadgetsSection(const GadgetNames& enabledGadgets,
  const GuidanceServices& services){
  if(!services.replMode)
    throw std::invalid_argument("Missing REPL mode binding");
  const auto missionGadgetName=enabledGadgets.contains(services.missionCreateGadgetName)
    ?services.missionCreateGadgetName:
    enabledGadgets.contains(services.todoWriteGadgetName)
      ?services.todoWriteGadgetName:std::string{};
  const auto missionGuidance=missionGadgetName.empty()?std::string{}:
    "Break down and manage your work with the "+missionGadgetName+
    " tool. These tools are helpful for planning your work and helping the "
    "user track your progress. Mark each task as completed as soon as you "
    "are done with the task. Do not batch up multiple tasks before marking "
    "them as completed.";
  if(services.replMode()){
    if(missionGuidance.empty())return {};
    Json items=Json::array({missionGuidance});
    auto bullets=prependBullets(items);
    bullets.insert(bullets.begin(),"# Using your tools");
    return joinLines(bullets);
  }
  if(!services.embeddedSearchGadgets)
    throw std::invalid_argument("Missing embedded-search binding");
  const bool embedded=services.embeddedSearchGadgets();
  Json subitems=Json::array({
    "To read files use "+services.fileReadGadgetName+
      " instead of cat, head, tail, or sed",
    "To edit files use "+services.fileEditGadgetName+" instead of sed or awk",
    "To create files use "+services.fileWriteGadgetName+
      " instead of cat with heredoc or echo redirection",
  });
  if(!embedded){
    subitems.push_back("To search for files use "+services.globGadgetName+
      " instead of find or ls");
    subitems.push_back("To search the content of files, use "+
      services.grepGadgetName+" instead of grep or rg");
  }
  subitems.push_back("Reserve using the "+services.bashGadgetName+
    " exclusively for system commands and terminal operations that require "
    "shell execution. If you are unsure and there is a relevant dedicated "
    "tool, default to using the dedicated tool and only fallback on using "
    "the "+services.bashGadgetName+
    " tool for these if it is absolutely necessary.");
  Json items=Json::array();
  items.push_back("Do NOT use the "+services.bashGadgetName+
    " to run commands when a relevant dedicated tool is provided. Using "
    "dedicated tools allows the user to better understand and review your "
    "work. This is CRITICAL to assisting the user:");
  items.push_back(subitems);
  if(!missionGuidance.empty())items.push_back(missionGuidance);
  items.push_back("You can call multiple tools in a single response. If you "
    "intend to call multiple tools and there are no dependencies between them, "
    "make all independent tool calls in parallel. Maximize use of parallel "
    "tool calls where possible to increase efficiency. However, if some tool "
    "calls depend on previous calls to inform dependent values, do NOT call "
    "these tools in parallel and instead call them sequentially. For instance, "
    "if one operation must complete before another starts, run these "
    "operations sequentially instead.");
  auto bullets=prependBullets(items);
  bullets.insert(bullets.begin(),"# Using your tools");
  return joinLines(bullets);
}
}
