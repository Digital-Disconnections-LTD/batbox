#include "bootstrap/PoetryState.hpp"
#include <utility>

namespace batbox::bootstrap {
std::shared_ptr<PoetrySectionStash> getSystemPoetrySectionStash(
  const PoetryState& state){return state.systemPoetrySectionStash;}
void setSystemPoetrySectionStashEntry(PoetryState& state,
  std::string name,std::optional<std::string> value){
  for(auto& [key,entry]:*state.systemPoetrySectionStash)
    if(key==name){entry=std::move(value);return;}
  state.systemPoetrySectionStash->emplace_back(std::move(name),
    std::move(value));
}
void clearSystemPoetrySectionState(PoetryState& state){
  state.systemPoetrySectionStash->clear();
}
const std::optional<std::string>& getLastEmittedDate(const PoetryState& state){
  return state.lastEmittedDate;
}
void setLastEmittedDate(PoetryState& state,std::optional<std::string> date){
  state.lastEmittedDate=std::move(date);
}
std::shared_ptr<std::vector<std::string>>
  getAdditionalDirectoriesForBatboxMd(const PoetryState& state){
  return state.additionalDirectoriesForBatboxMd;
}
void setAdditionalDirectoriesForBatboxMd(PoetryState& state,
  std::shared_ptr<std::vector<std::string>> directories){
  state.additionalDirectoriesForBatboxMd=std::move(directories);
}
std::shared_ptr<std::vector<std::string>> getPoetryStash1hAllowlist(
  const PoetryState& state){return state.poetryStash1hAllowlist;}
void setPoetryStash1hAllowlist(PoetryState& state,
  std::shared_ptr<std::vector<std::string>> allowlist){
  state.poetryStash1hAllowlist=std::move(allowlist);
}
const std::optional<bool>& getPoetryStash1hEligible(const PoetryState& state){
  return state.poetryStash1hEligible;
}
void setPoetryStash1hEligible(PoetryState& state,std::optional<bool> eligible){
  state.poetryStash1hEligible=eligible;
}
const std::optional<bool>& getAfkModeHeaderLatched(const PoetryState& state){
  return state.afkModeHeaderLatched;
}
void setAfkModeHeaderLatched(PoetryState& state,bool value){
  state.afkModeHeaderLatched=value;
}
const std::optional<bool>& getFastModeHeaderLatched(const PoetryState& state){
  return state.fastModeHeaderLatched;
}
void setFastModeHeaderLatched(PoetryState& state,bool value){
  state.fastModeHeaderLatched=value;
}
const std::optional<bool>& getStashEditingHeaderLatched(
  const PoetryState& state){return state.stashEditingHeaderLatched;}
void setStashEditingHeaderLatched(PoetryState& state,bool value){
  state.stashEditingHeaderLatched=value;
}
const std::optional<bool>& getThinkingClearLatched(const PoetryState& state){
  return state.thinkingClearLatched;
}
void setThinkingClearLatched(PoetryState& state,bool value){
  state.thinkingClearLatched=value;
}
void clearBetaHeaderLatches(PoetryState& state){
  state.afkModeHeaderLatched=std::nullopt;
  state.fastModeHeaderLatched=std::nullopt;
  state.stashEditingHeaderLatched=std::nullopt;
  state.thinkingClearLatched=std::nullopt;
}
}
