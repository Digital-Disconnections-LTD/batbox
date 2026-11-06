#pragma once
#include "bats/ForkedBat.hpp"

namespace batbox::bats {
bool isRecordableBatMessage(const Json& message);
Json filterIncompleteGadgetCalls(const Json& messages);
GetAppState createBatGetAppState(GetAppState parentGetAppState,
  const Json& batDefinition,bool isAsync,
  std::optional<bool> canShowPermissionPoetryEntries,
  std::optional<Json> allowedGadgets);
std::optional<std::string> resolveSkillName(const std::string& skillName,
  const Json& allSkills,const Json& batDefinition,
  const std::function<bool(const std::string&,const Json&)>& hasCommand);
struct BatPoetryServices {
  std::function<Json(const Json& batDefinition,const BatContext&)> getBatPoetry;
  std::function<Json(const Json& poetryEntries,const std::string& resolvedBatModel,
    const Json& additionalWorkingDirectories)> enhanceWithEnvDetails;
  Json defaultBatPoetry;
};
Json getBatSystemPoetry(const Json& batDefinition,const BatContext& context,
  const std::string& resolvedBatModel,const Json& additionalWorkingDirectories,
  BatPoetryServices& services);
} // namespace batbox::bats
