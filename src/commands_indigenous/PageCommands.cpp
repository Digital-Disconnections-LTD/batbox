#include "commands_indigenous/PageCommands.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
Json descriptor(PageKind kind){
  switch(kind){
    case PageKind::Status:return Json{{"type","local-jsx"},
      {"name","status"},{"immediate",true},
      {"description","Show Batbox version, model, account, API and Gadgets"}};
    case PageKind::Config:return Json{{"type","local-jsx"},
      {"name","config"},{"aliases",Json::array({"settings"})},
      {"description","Open configuration panel"}};
    case PageKind::Bats:return Json{{"type","local-jsx"},
      {"name","bats"},{"aliases",Json::array({"agents"})},{"description","Meet your Bats and edit their poetry"}};
    case PageKind::Missions:return Json{{"type","local-jsx"},
      {"name","missions"},{"aliases",Json::array({"tasks","bashes"})},
      {"description","List and manage background Missions"}};
    case PageKind::Permissions:return Json{{"type","local-jsx"},
      {"name","permissions"},{"aliases",Json::array({"allowed-tools"})},
      {"description","Manage allow and deny Gadget permission rules"}};
  }
  throw std::invalid_argument("Unknown page command");
}
}
user_input::CommandEntry makePageCommand(PageKind kind,
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<PageServices> services){
  if(!router||!services)throw std::invalid_argument(
    "Page command router and services required");
  return user_input::bindUiCommand(descriptor(kind),router,
    [kind,services](const std::string&){
      return [kind,services](auto done,auto ready,auto error){
        try{
          if(!services->openPage||!services->getContext)
            throw std::invalid_argument("Indigenous page and context imports required");
          auto context=services->getContext();
          if(!context)throw std::invalid_argument("Live command context required");
          PageRequest request{kind,context,{},{}};
          if(kind==PageKind::Bats){
            if(!services->getGadgets)
              throw std::invalid_argument("Bat Gadget catalog import required");
            Json appState;
            if(context->getTypedAppState){
              auto typed=context->getTypedAppState();
              if(!typed)throw std::invalid_argument("Live AppState required");
              appState=typed->snapshot();
            }else{
              if(!context->getAppState)
                throw std::invalid_argument("AppState getter required");
              appState=context->getAppState();
            }
            request.availableGadgets=services->getGadgets(
              value::property(appState,"toolPermissionContext"));
          }
          if(kind==PageKind::Permissions){
            if(!services->appendPermissionRetryMessage)
              throw std::invalid_argument("Permission retry import required");
            request.retryDeniedCommands=services->appendPermissionRetryMessage;
          }
          services->openPage(std::move(request),done,ready,error);
        }catch(const std::exception& failure){error(failure.what());}
      };
    });
}
} // namespace batbox::commands_indigenous
