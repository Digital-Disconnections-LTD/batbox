#pragma once
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::bootstrap {
using PoetrySectionStash=std::vector<
  std::pair<std::string,std::optional<std::string>>>;
struct PoetryState {
  std::shared_ptr<PoetrySectionStash> systemPoetrySectionStash=
    std::make_shared<PoetrySectionStash>();
  std::optional<std::string> lastEmittedDate;
  std::shared_ptr<std::vector<std::string>> additionalDirectoriesForBatboxMd=
    std::make_shared<std::vector<std::string>>();
  std::shared_ptr<std::vector<std::string>> poetryStash1hAllowlist;
  std::optional<bool> poetryStash1hEligible;
  std::optional<bool> afkModeHeaderLatched,fastModeHeaderLatched,
    stashEditingHeaderLatched,thinkingClearLatched;
};
std::shared_ptr<PoetrySectionStash> getSystemPoetrySectionStash(
  const PoetryState& state);
void setSystemPoetrySectionStashEntry(PoetryState& state,
  std::string name,std::optional<std::string> value);
void clearSystemPoetrySectionState(PoetryState& state);
const std::optional<std::string>& getLastEmittedDate(const PoetryState& state);
void setLastEmittedDate(PoetryState& state,std::optional<std::string> date);
std::shared_ptr<std::vector<std::string>>
  getAdditionalDirectoriesForBatboxMd(const PoetryState& state);
void setAdditionalDirectoriesForBatboxMd(PoetryState& state,
  std::shared_ptr<std::vector<std::string>> directories);
std::shared_ptr<std::vector<std::string>> getPoetryStash1hAllowlist(
  const PoetryState& state);
void setPoetryStash1hAllowlist(PoetryState& state,
  std::shared_ptr<std::vector<std::string>> allowlist);
const std::optional<bool>& getPoetryStash1hEligible(const PoetryState& state);
void setPoetryStash1hEligible(PoetryState& state,std::optional<bool> eligible);
const std::optional<bool>& getAfkModeHeaderLatched(const PoetryState& state);
void setAfkModeHeaderLatched(PoetryState& state,bool value);
const std::optional<bool>& getFastModeHeaderLatched(const PoetryState& state);
void setFastModeHeaderLatched(PoetryState& state,bool value);
const std::optional<bool>& getStashEditingHeaderLatched(
  const PoetryState& state);
void setStashEditingHeaderLatched(PoetryState& state,bool value);
const std::optional<bool>& getThinkingClearLatched(const PoetryState& state);
void setThinkingClearLatched(PoetryState& state,bool value);
void clearBetaHeaderLatches(PoetryState& state);
}
