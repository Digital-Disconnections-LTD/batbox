#pragma once
#include "missions/shell/Roster.hpp"
#include "gadgets/Gadget.hpp"
#include <memory>

namespace batbox::mission_output {
struct Services {
  std::shared_ptr<shell_missions::Roster> roster;
  std::function<long long()> nowMs;
  std::function<std::string(const std::string&)> getMissionOutputPath;
  std::function<Json(const Json&,const Json&,const Json&)> renderResult;
  std::function<Json(const Json&,const Json&)> renderProgress;
  std::function<Json(const Json&,const Json&)> renderError;
  std::function<Json(const Json&,const Json&)> renderRejected;
  std::function<Json(const Json&)> renderTag;
  std::size_t maxOutputLength=32000;
  bool enabled=true;
};
Json parseInput(const Json& input);
Json formatMissionOutput(const std::string& output,const std::string& missionId,
  std::size_t maxLength,const std::function<std::string(const std::string&)>& getMissionOutputPath);
Json getMissionOutputData(const shell_missions::MissionRecord& mission,const shell_missions::Roster& roster);
Json mapGadgetResult(const Json& data,const std::string& gadgetUseID,
  std::size_t maxLength,const std::function<std::string(const std::string&)>& getMissionOutputPath);
gadgets::Gadget makeMissionOutputGadget(Services services);
} // namespace batbox::mission_output
