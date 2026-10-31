#pragma once
#include "filesystem/FileOps.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::gadgets::files {
struct GrepServices {
  filesystem::PathServices paths;
  std::function<std::optional<std::string>(const std::string&)> suggestPathUnderCwd;
  std::function<std::vector<std::string>(const Json&)> getFileReadIgnorePatterns;
  std::function<std::vector<std::string>(const std::vector<std::string>&,const std::string&)> normalizePatternsToPath;
  std::function<std::vector<std::string>(const std::string&)> getGlobExclusionsForPluginStash;
  std::function<std::vector<std::string>(const std::vector<std::string>&,const std::string&,std::stop_token)> ripGrep;
  std::function<int(const std::string&,const std::string&)> localeCompare;
  bool testEnvironment=false;
  std::string fileNotFoundCwdNote="Note: your current working directory is";
};
struct HeadLimitResult {std::vector<std::string> items;std::optional<double> appliedLimit;};
HeadLimitResult applyHeadLimit(const std::vector<std::string>& items,
  std::optional<double> limit,double offset=0);
std::string formatLimitInfo(std::optional<double> appliedLimit,std::optional<double> appliedOffset);
ValidationResult validateGrepInput(const Json& input,const GrepServices& services);
GadgetResult callGrep(const Json& input,const GadgetCallContext& context,const GrepServices& services);
Json mapGrepResult(const Json& output,const std::string& gadgetUseID);
} // namespace batbox::gadgets::files
