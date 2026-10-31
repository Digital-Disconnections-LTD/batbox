#include "application_commands/PermissionCommand.hpp"
#include "app_state/PermissionModeBridge.hpp"
#include "app_state/provider/PermissionHelpers.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
struct Mode {const char* name;const char* description;};
constexpr std::array<Mode,5> modes={{{"default","Ask when a Gadget needs permission."},
  {"acceptEdits","Allow file edits in working directories; ask for other actions."},
  {"bypassPermissions","Skip all Gadget approval dialogs for this Flight and its Bats."},
  {"dontAsk","Deny actions that need approval instead of asking."},
  {"plan","Block Bash commands and file changes; read permissions still apply."}}};

class PermissionPanel final:public std::enable_shared_from_this<PermissionPanel> {
public:
  PermissionPanel(std::shared_ptr<app_state::AppStateStore> store,
      std::shared_ptr<bootstrap::FlightTransitions> transitions,bool configuredDisabled,bool interactive,
      interactive::CommandUi::Done done,interactive::CommandUi::Ready ready):
      store_(std::move(store)),transitions_(std::move(transitions)),
      configuredDisabled_(configuredDisabled),interactive_(interactive),
      done_(std::move(done)),ready_(std::move(ready)){}
  void start(const std::string& argument){
    const auto current=currentMode();
    for(std::size_t index=0;index<modes.size();++index)
      if(current==modes[index].name)selected_=index;
    if(!argument.empty()){
      if(app_state::permissionModeFromString(argument,false)!=argument)
        notice_="Unknown permission mode: "+argument;
      else{
        for(std::size_t index=0;index<modes.size();++index)
          if(argument==modes[index].name)selected_=index;
        if(apply())return;
      }
    }
    if(!interactive_){
      finish(notice_.empty()?"Permissions: "+current+" for this Flight. "+
        modes[selected_].description:notice_);
      return;
    }
    show();
  }
private:
  std::string currentMode()const{
    const auto state=store_->getState()->snapshot();
    const auto& mode=value::property(value::property(state,"toolPermissionContext"),"mode");
    return mode.is_string()?mode.get<std::string>():"default";
  }
  bool bypassDisabled()const{
    app_state::provider::BypassPolicyServices policy;
    policy.stashedDisableGate=[this]{return configuredDisabled_;};
    policy.getSettings=[this]{return Json(value::property(store_->getState()->snapshot(),"settings"));};
    return app_state::provider::isBypassPermissionsModeDisabled(policy);
  }
  void finish(std::optional<std::string> message={}){
    interactive::CommandCompletion completion;completion.result=std::move(message);
    completion.options.display=completion.result?
      interactive::DoneOptions::Display::System:interactive::DoneOptions::Display::Skip;
    done_(std::move(completion));
  }
  bool apply(){
    const std::string mode=modes[selected_].name;
    if(mode=="bypassPermissions"&&bypassDisabled()){
      notice_="bypassPermissions is disabled by settings or configuration.";
      return false;
    }
    store_->setState([this,&mode](const app_state::AppStateRef& previous){
      const Json context=value::property(previous->snapshot(),"toolPermissionContext");
      Json permission=context.is_object()?context:Json::object();
      const auto from=permission.value("mode",std::string("default"));
      if(from!=mode){
        if(transitions_){
          bootstrap::handlePlanModeTransition(*transitions_,from,mode);
          bootstrap::handleAutoModeTransition(*transitions_,from,mode);
          if(from=="plan"&&mode!="plan")
            bootstrap::setHasExitedPlanMode(*transitions_,true);
        }
        if(from=="plan"&&mode!="plan")permission.erase("prePlanMode");
      }
      permission["mode"]=mode;
      // Selecting bypass is explicit consent for this Flight, subject to policy.
      if(mode=="bypassPermissions")permission["isBypassPermissionsModeAvailable"]=true;
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->setPermissionContext(std::make_shared<value::SharedRecord>(
        value::SharedRecord::fromJson(permission)));
      return next;
    });
    finish("Permissions: "+mode+" for this Flight. "+modes[selected_].description+
      " Saved settings are unchanged.");
    return true;
  }
  void show(){
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
    request.bodyPreviewLines=4;
    request.id="permission-mode";request.title="Permissions";
    const auto current=currentMode();const bool disabled=bypassDisabled();
    request.body="Current mode: "+current+"\n"+modes[selected_].description+
      "\n↑/↓ choose · Enter apply · Esc cancel\nApplies to this Flight only.";
    if(!notice_.empty())request.body+="\n"+notice_;
    for(const auto& mode:modes){
      auto label=std::string(mode.name);
      if(current==mode.name)label+=" (current)";
      if(disabled&&std::string(mode.name)=="bypassPermissions")label+=" (disabled by policy)";
      request.choices.push_back(std::move(label));
    }
    request.selectedIndex=selected_;
    auto self=shared_from_this();ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel||
          action.kind==interactive::UiAction::Kind::Back){self->finish();return;}
      if(action.kind==interactive::UiAction::Kind::MoveSelection){
        self->selected_=std::min(action.index,modes.size()-1);
        self->notice_.clear();self->show();return;
      }
      if(action.kind==interactive::UiAction::Kind::Activate){
        self->selected_=std::min(action.index,modes.size()-1);
        if(!self->apply())self->show();
      }
    });
  }
  std::shared_ptr<app_state::AppStateStore> store_;
  std::shared_ptr<bootstrap::FlightTransitions> transitions_;
  bool configuredDisabled_=false,interactive_=true;
  interactive::CommandUi::Done done_;
  interactive::CommandUi::Ready ready_;
  std::size_t selected_=0;
  std::string notice_;
};
}
user_input::CommandEntry makePermissionsCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,
    const application::Config& config,std::shared_ptr<app_state::AppStateStore> store,
    std::shared_ptr<bootstrap::FlightTransitions> transitions){
  if(!owner.isOwnerThread()||!store)
    throw std::invalid_argument("Permission command requires the application owner and state");
  const bool configuredDisabled=application::bypassPermissionsDisabled(config,Json::object());
  Json descriptor={{"type","local-jsx"},{"name","permissions"},
    {"aliases",Json::array({"permission","allowed-tools"})},
    {"supportsNonInteractive",true},
    {"description","Choose the permission mode for this Flight"},
    {"argumentHint","[default|acceptEdits|bypassPermissions|dontAsk|plan]"}};
  return user_input::bindUiCommand(std::move(descriptor),std::move(router),
    [store=std::move(store),transitions=std::move(transitions),configuredDisabled,
        interactive=!config.print](const std::string& args){
      return [store,transitions,configuredDisabled,interactive,args](auto done,auto ready,auto){
        auto panel=std::make_shared<PermissionPanel>(store,transitions,configuredDisabled,interactive,
          std::move(done),std::move(ready));
        panel->start(value::trim(args));
      };
    });
}
} // namespace batbox::application_commands
