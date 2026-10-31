#pragma once
#include "missions/shell/Roster.hpp"
#include "gadgets/Gadget.hpp"
#include <memory>

namespace batbox::mission_stop {
struct Services {
  std::shared_ptr<shell_missions::Roster> roster;
  std::function<Json(const Json&,const Json&)> renderUse;
  std::function<Json(const Json&,const Json&,const Json&)> renderResult;
  bool hideUserFacingName=false;
};
gadgets::Gadget makeMissionStopGadget(Services services);
Json parseInput(const Json& input);
Json mapGadgetResult(const Json& output,const std::string& gadgetUseID);
} // namespace batbox::mission_stop
