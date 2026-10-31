#pragma once
#include "bats/ForkedBat.hpp"
#include "gadgets/Gadget.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::commands_indigenous {
enum class PageKind {Status,Config,Bats,Missions,Permissions};
struct PageRequest {
  PageKind kind;
  std::shared_ptr<bats::BatContext> context;
  gadgets::Gadgets availableGadgets;
  std::function<void(const Json&)> retryDeniedCommands;
};
struct PageServices {
  std::function<std::shared_ptr<bats::BatContext>()> getContext;
  std::function<gadgets::Gadgets(const Json&)> getGadgets;
  std::function<void(const Json&)> appendPermissionRetryMessage;
  std::function<void(PageRequest,interactive::CommandUi::Done,
    interactive::CommandUi::Ready,interactive::CommandUi::Error)> openPage;
};
user_input::CommandEntry makePageCommand(PageKind kind,
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<PageServices> services);
} // namespace batbox::commands_indigenous
