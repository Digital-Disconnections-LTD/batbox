#include "bootstrap/InvokedSkills.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace batbox::bootstrap {
void addInvokedSkill(InvokedSkillsState& state,
  const std::string& skillName,const std::string& skillPath,
  const std::string& content,std::optional<std::string> batId,
  const ClockMillis& clockMillis){
  if(!clockMillis)throw std::invalid_argument("clockMillis is required");
  const std::string key=batId.value_or("")+":"+skillName;
  auto info=std::make_shared<InvokedSkillInfo>(
    InvokedSkillInfo{skillName,skillPath,content,clockMillis(),
      std::move(batId)});
  for(auto& [existingKey,existingInfo]:*state.invokedSkills)
    if(existingKey==key){existingInfo=std::move(info);return;}
  state.invokedSkills->emplace_back(key,std::move(info));
}
std::shared_ptr<InvokedSkillMap> getInvokedSkills(
  const InvokedSkillsState& state){return state.invokedSkills;}
InvokedSkillMap getInvokedSkillsForBat(const InvokedSkillsState& state,
  std::optional<std::string> batId){
  InvokedSkillMap filtered;
  for(const auto& [key,skill]:*state.invokedSkills)
    if(skill->batId==batId)filtered.emplace_back(key,skill);
  return filtered;
}
void clearInvokedSkills(InvokedSkillsState& state,
  const std::optional<std::vector<std::string>>& preservedBatIds){
  if(!preservedBatIds||preservedBatIds->empty()){
    state.invokedSkills->clear();return;
  }
  const std::unordered_set<std::string> preserved(
    preservedBatIds->begin(),preservedBatIds->end());
  auto& entries=*state.invokedSkills;
  entries.erase(std::remove_if(entries.begin(),entries.end(),
    [&](const auto& entry){
      const auto& bat=entry.second->batId;
      return !bat||!preserved.contains(*bat);
    }),entries.end());
}
void clearInvokedSkillsForBat(InvokedSkillsState& state,
  const std::string& batId){
  auto& entries=*state.invokedSkills;
  entries.erase(std::remove_if(entries.begin(),entries.end(),
    [&](const auto& entry){return entry.second->batId==batId;}),
    entries.end());
}
}
