#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::poetryEntries {
struct GadgetNames;
struct GuidanceServices {
  bool internalUser=false,undercover=false;
  bool discoverSkillsFeature=false,stashedResultClearingFeature=false;
  bool kairosFeature=false,kairosBriefFeature=false,proactiveFeature=false;
  bool verificationBatFeature=false;
  std::string batGadgetName,discoverSkillsGadgetName,briefProactiveSection,
    tickTag,sleepGadgetName;
  std::string askUserQuestionGadgetName,skillGadgetName,exploreBatType,
    verificationBatType;
  int exploreBatMinQueries=0;
  std::string missionCreateGadgetName,todoWriteGadgetName,fileReadGadgetName,
    fileEditGadgetName,fileWriteGadgetName,globGadgetName,grepGadgetName,bashGadgetName;
  std::function<bool()> forkSubagentEnabled,scratchpadEnabled,
    briefEnabled,proactiveActive,replMode,embeddedSearchGadgets,
    nonInteractiveFlight,explorePlanBatsEnabled,verificationEnabled;
  std::function<std::string()> scratchpadDir;
  std::function<std::optional<std::string>()> modelOverrideSuffix;
  std::function<Json()> stashedResultClearingConfig;
};
std::string getHooksSection();
std::string getSystemRemindersSection();
std::optional<std::string> getModelOverrideSection(const GuidanceServices& services);
std::string getBatGadgetSection(const GuidanceServices& services);
std::optional<std::string> getDiscoverSkillsGuidance(
  const GuidanceServices& services);
std::optional<std::string> getScratchpadInstructions(
  const GuidanceServices& services);
std::optional<std::string> getFunctionResultClearingSection(
  const std::string& model,const GuidanceServices& services);
std::optional<std::string> getBriefSection(const GuidanceServices& services);
std::optional<std::string> getProactiveSection(const GuidanceServices& services);
std::string getUsingYourGadgetsSection(const GadgetNames& enabledGadgets,
  const GuidanceServices& services);
std::optional<std::string> getFlightSpecificGuidanceSection(
  const GadgetNames& enabledGadgets,const Json& skillGadgetCommands,
  const GuidanceServices& services);
}
