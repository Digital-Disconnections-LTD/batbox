#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/PoetryBlocks.hpp"
#include "poetryEntries/SystemPoetry.hpp"

namespace batbox::poetryEntries {
std::string getUsingYourGadgetsSection(const GadgetNames& enabledGadgets,
  const GuidanceServices& services){
  const auto missionGadgetName=enabledGadgets.contains(services.missionCreateGadgetName)
    ?services.missionCreateGadgetName:
    enabledGadgets.contains(services.todoWriteGadgetName)
      ?services.todoWriteGadgetName:std::string{};
  if(missionGadgetName.empty())return {};
  Json items=Json::array({"Break down and manage your work with the "+
    missionGadgetName+" tool. These tools are helpful for planning your work "
    "and helping the user track your progress. Mark each task as completed as "
    "soon as you are done with the task. Do not batch up multiple tasks before "
    "marking them as completed."});
  auto bullets=prependBullets(items);
  bullets.insert(bullets.begin(),"# Using your tools");
  std::string result;
  for(std::size_t i=0;i<bullets.size();++i){if(i)result+='\n';result+=bullets[i];}
  return result;
}
}
