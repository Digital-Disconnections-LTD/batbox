#pragma once
#include <string>

namespace batbox::poetryEntries {
std::string getSimpleSystemSection();
std::string getSimpleDoingMissionsSection(bool internalUser,
  const std::string& issuesExplainer,const std::string& askUserGadgetName);
std::string getActionsSection();
std::string getSimpleToneAndStyleSection(bool internalUser);
std::string getOutputEfficiencySection(bool internalUser);
std::string getProactiveBody(const std::string& tickTag,
  const std::string& sleepGadgetName);
}
