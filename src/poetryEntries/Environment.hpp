#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
struct KnowledgePattern {std::string contains,cutoff;};
struct EnvironmentServices {
  Json environment=Json::object();
  std::string platform;
  bool internalUser=false,undercover=false,worktree=false;
  std::function<std::future<bool>()> getIsGit;
  std::function<std::string()> getCwd;
  std::function<std::optional<std::string>(const std::string&)> marketingName;
  std::function<std::string(const std::string&)> canonicalModelName;
  std::vector<KnowledgePattern> knowledgePatterns;
  std::function<std::string()> osVersion,osRelease;
  std::string productModelFamilyDescription,productAvailabilityDescription,
    fastModeDescription;
};
std::optional<std::string> getKnowledgeCutoff(const std::string& model,
  const EnvironmentServices& services);
std::string getShellInfoLine(const EnvironmentServices& services);
std::string getUnameSR(const EnvironmentServices& services);
std::string computeEnvInfo(const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const EnvironmentServices& services);
std::string computeSimpleEnvInfo(const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const EnvironmentServices& services);
}
