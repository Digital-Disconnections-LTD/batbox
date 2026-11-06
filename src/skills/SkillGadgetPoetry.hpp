#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::skills {
struct SkillPoetryServices {
  std::function<double(const std::string&)> stringWidth;
  std::function<std::string(const std::string&,double)> truncate;
  std::optional<std::string> charBudgetEnvironment;
};
double getSkillCharBudget(std::optional<double> contextWindowTokens,
  const std::optional<std::string>& environment);
std::string getCommandDescription(const Json& command);
std::string formatCommandDescription(const Json& command);
std::string formatCommandsWithinBudget(const std::vector<Json>& commands,
  std::optional<double> contextWindowTokens,const SkillPoetryServices& services);
std::string getSkillGadgetPoetry(const std::string& cwd);
}
