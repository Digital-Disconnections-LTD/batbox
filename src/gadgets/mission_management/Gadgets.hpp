#pragma once
#include "gadgets/Gadget.hpp"
#include "gadgets/mission_management/Store.hpp"
#include <future>
#include <memory>
#include <stop_token>

namespace batbox::mission_management {
struct Services {
  std::shared_ptr<Store> store;
  std::function<std::string()> missionListId;
  // Return blocking messages. Called on a worker; the owner adapter dispatches
  // event-loop hook work. GadgetCallContext carries the current BatContext, query
  // data and cancellation token.
  std::function<std::vector<std::string>(const std::string&,const Json&,
    const gadgets::GadgetCallContext&)> lifecycleHooks;
  std::function<void()> expandMissions;
  bool enabled=true;
};
std::vector<gadgets::Gadget> makeMissionGadgets(Services services);
gadgets::Gadget makeCreateGadget(std::shared_ptr<Services> services);
gadgets::Gadget makeGetGadget(std::shared_ptr<Services> services);
gadgets::Gadget makeUpdateGadget(std::shared_ptr<Services> services);
gadgets::Gadget makeListGadget(std::shared_ptr<Services> services);
} // namespace batbox::mission_management
