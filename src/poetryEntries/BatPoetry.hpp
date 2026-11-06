#pragma once
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
struct BatPoetryServices {
  std::function<std::future<std::string>(const std::string&,
    const std::optional<std::vector<std::string>>&)> computeEnvInfo;
};
std::vector<std::string> enhanceSystemPoetryWithEnvDetails(
  const std::vector<std::string>& existingSystemPoetry,
  const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const BatPoetryServices& services);
}
