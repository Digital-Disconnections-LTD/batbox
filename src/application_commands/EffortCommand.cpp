#include "application_commands/EffortCommand.hpp"
#include "application/Async.hpp"
#include "application_config/EndpointStore.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>

namespace batbox::application_commands {
namespace {
constexpr const char* usage="Usage: /effort [low|medium|high|auto]";
std::string current(const std::shared_ptr<app_state::AppStateStore>& store){
  const auto snapshot=store->getState()->snapshot();
  const auto& effort=value::property(snapshot,"effortValue");
  return effort.is_string()?effort.get<std::string>():"auto";
}
interactive::CommandCompletion completion(std::string message){
  interactive::CommandCompletion result;
  result.result=std::move(message);
  result.options.display=interactive::DoneOptions::Display::System;
  return result;
}
void project(const std::shared_ptr<app_state::AppStateStore>& store,
    const std::string& effort){
  store->setState([&](const app_state::AppStateRef& previous){
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    Json settings=value::property(previous->snapshot(),"settings");
    if(!settings.is_object())settings=Json::object();
    if(effort=="auto")settings.erase("effortLevel");
    else settings["effortLevel"]=effort;
    next->fields->set("settings",settings);
    next->fields->set("effortValue",effort=="auto"?Json():Json(effort));
    return next;
  });
}
}
user_input::CommandEntry makeEffortCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,
    const application::Config& config,
    std::shared_ptr<app_state::AppStateStore> store,
    std::function<bool()> supportsEffort){
  if(!store)throw std::invalid_argument("Effort command needs app state");
  Json descriptor={{"type","local-jsx"},{"name","effort"},
    {"description","Set the model server's reasoning effort"},
    {"argumentHint","[low|medium|high|auto]"},
    {"supportsNonInteractive",true},{"immediate",true}};
  const auto settingsPath=application::configurationDirectory(config)/"settings.json";
  return user_input::bindUiCommand(std::move(descriptor),std::move(router),
    [&owner,settingsPath,store=std::move(store),supportsEffort=std::move(supportsEffort)]
      (const std::string& args){
      return [&owner,settingsPath,store,supportsEffort,args](auto done,auto ready,auto error){
        auto effort=value::trim(args);
        std::transform(effort.begin(),effort.end(),effort.begin(),
          [](unsigned char ch){return static_cast<char>(std::tolower(ch));});
        const auto finish=[done,ready](std::string message){
          done(completion(std::move(message)));
          ready(std::nullopt,{});
        };
        if(effort.empty()||effort=="current"||effort=="status"){
          finish("Effort level: "+current(store)+". "+usage);return;
        }
        if(effort=="help"||effort=="-h"||effort=="--help"){
          finish(usage);return;
        }
        if(effort!="low"&&effort!="medium"&&effort!="high"&&effort!="auto"){
          finish("Invalid effort level. "+std::string(usage));return;
        }
        if(effort!="auto"&&supportsEffort&&!supportsEffort()){
          finish("Selected model does not support server-side effort. Enable effort in its endpoint profile.");
          return;
        }
        application::runInWorker<event_loop::Unit>(owner,
          [settingsPath,effort]{
            if(effort=="auto")
              application_config::removeConfigSetting(settingsPath,"effortLevel");
            else application_config::patchConfigSettings(settingsPath,
              Json{{"effortLevel",effort}});
            return event_loop::Unit{};
          }).then([store,effort,finish,error](const event_loop::Unit*,
              const std::exception_ptr& failure){
            if(failure){
              try{std::rethrow_exception(failure);}
              catch(const std::exception& problem){error(problem.what());}
              return;
            }
            try{
              project(store,effort);
              finish("Effort level set to "+effort+" for upcoming model requests.");
            }catch(const std::exception& problem){error(problem.what());}
          });
      };
    });
}
} // namespace batbox::application_commands
