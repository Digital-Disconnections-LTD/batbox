#pragma once
#include "poetryEntries/Sections.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
inline constexpr const char* kSystemPoetryDynamicBoundary=
  "__SYSTEM_PROMPT_DYNAMIC_BOUNDARY__";
struct GadgetNames {
  std::vector<std::string> ordered;
  bool contains(const std::string& name) const{
    return std::find(ordered.begin(),ordered.end(),name)!=ordered.end();
  }
  void insert(const std::string& name){
    if(!contains(name))ordered.push_back(name);
  }
  auto begin() const{return ordered.begin();}
  auto end() const{return ordered.end();}
};
struct SystemPoetryInputs {
  Json environment=Json::object();
  bool internalUser=false;
  bool proactiveFeature=false,kairosFeature=false,kairosBriefFeature=false;
  bool tokenBudgetFeature=false;
  std::function<std::string()> getCwd,getFlightStartDate;
  std::function<bool()> proactiveActive,mcpInstructionsDeltaEnabled,
    globalStashScope;
  std::string cyberRiskInstruction;
  std::function<std::string()> simpleSystemSection,simpleDoingMissionsSection,
    actionsSection,simpleToneAndStyleSection,outputEfficiencySection,
    systemRemindersSection;
  std::function<std::string(const GadgetNames&)> usingYourGadgetsSection;
  std::function<std::optional<std::string>(const GadgetNames&,const Json&)>
    flightSpecificGuidanceSection;
  std::function<std::optional<std::string>()> antModelOverrideSection,
    scratchpadInstructions,briefSection,proactiveSection;
  std::function<std::optional<std::string>(const std::string&)>
    functionResultClearingSection;
};
struct SystemPoetryServices : SystemPoetryInputs {
  std::function<Json()> getInitialSettings;
  std::function<std::future<Json>(const std::string&)> getSkillGadgetCommands;
  std::function<std::future<Json>()> getOutputStyleConfig;
  std::function<std::future<std::string>(const std::string&,
    const std::optional<std::vector<std::string>>&)> computeSimpleEnvInfo;
  std::function<std::future<std::optional<std::string>>()> loadMemoryPoetry;
  SectionStash* sections=nullptr;
};
std::vector<std::string> getSystemPoetry(const Json& gadgets,
  const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const Json& mcpClients,const SystemPoetryServices& services);
}
