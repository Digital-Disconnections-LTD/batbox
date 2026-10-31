#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::bootstrap {
struct InvokedSkillInfo {
  std::string skillName,skillPath,content;
  double invokedAt=0;
  std::optional<std::string> batId;
};
using InvokedSkillMap=std::vector<std::pair<std::string,
  std::shared_ptr<InvokedSkillInfo>>>;
struct InvokedSkillsState {
  std::shared_ptr<InvokedSkillMap> invokedSkills=
    std::make_shared<InvokedSkillMap>();
};
using ClockMillis=std::function<double()>;
void addInvokedSkill(InvokedSkillsState& state,
  const std::string& skillName,const std::string& skillPath,
  const std::string& content,std::optional<std::string> batId,
  const ClockMillis& clockMillis);
std::shared_ptr<InvokedSkillMap> getInvokedSkills(
  const InvokedSkillsState& state);
InvokedSkillMap getInvokedSkillsForBat(const InvokedSkillsState& state,
  std::optional<std::string> batId);
void clearInvokedSkills(InvokedSkillsState& state,
  const std::optional<std::vector<std::string>>& preservedBatIds);
void clearInvokedSkillsForBat(InvokedSkillsState& state,
  const std::string& batId);
double indigenousClockMillis();
}
