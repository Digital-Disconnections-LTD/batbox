#pragma once
#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
struct BatPoetryServices {
  GuidanceServices guidance;
  std::function<bool()> skillSearchEnabled;
  std::function<std::future<std::string>(const std::string&,
    const std::optional<std::vector<std::string>>&)> computeEnvInfo;
};
std::string defaultBatPoetry();
std::vector<std::string> enhanceSystemPoetryWithEnvDetails(
  const std::vector<std::string>& existingSystemPoetry,
  const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const std::optional<GadgetNames>& enabledGadgetNames,
  const BatPoetryServices& services);
}
