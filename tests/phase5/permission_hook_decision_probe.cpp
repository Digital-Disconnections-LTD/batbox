#include "cli/permissions/HookDecision.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    Json events=Json::array();
    auto initial=std::make_shared<app_state::AppState>();
    initial->fields=std::make_shared<value::SharedRecord>(
      value::SharedRecord::fromJson(Json{{"other","retained"}}));
    initial->setPermissionContext(std::make_shared<value::SharedRecord>(
      value::SharedRecord::fromJson(Json{{"mode","default"},
        {"rules",Json::array({"Read"})}})));
    auto app=initial;
    cli::permissions::SdkHookContext context;
    context.getAppState=[&]{
      events.push_back(Json::array({"getAppState"}));return app;
    };
    context.hookContext.getAppState=[&]{return app->snapshot();};
    context.setAppState=[&](const auto& updater){
      events.push_back(Json::array({"setAppState"}));app=updater(app);
    };
    cli::permissions::HookDecisionServices services;
    services.executePermissionRequestHooks=[&](const std::string& name,
        const std::string& id,const Json& input,const auto& passedContext,
        const std::string& mode,const std::optional<Json>& suggestions,
        std::stop_token){
      events.push_back(Json::array({"execute",name,id,input,mode,
        suggestions?*suggestions:Json(),
        static_cast<bool>(passedContext.getAppState)}));
      const auto yields=test.value("yields",Json::array());
      auto at=std::make_shared<std::size_t>(0);
      instructions::hooks::runtime::HookYieldCursor cursor;
      cursor.next=[&,yields,at]{
        events.push_back(Json::array({"next"}));
        event_loop::Deferred<instructions::hooks::runtime::HookYield>
          pending(loop);
        instructions::hooks::runtime::HookYield value;
        if(*at>=yields.size())value.done=true;
        else value.value=yields.at((*at)++);
        pending.resolve(std::move(value));return pending;
      };
      return cursor;
    };
    services.persistPermissionUpdates=[&](const Json& updates){
      events.push_back(Json::array({"persist",updates}));
    };
    services.applyPermissionUpdates=[&](const value::RecordRef& current,
                                         const Json& updates){
      events.push_back(Json::array({"apply",current->snapshot(),updates}));
      if(test.value("sameContext",false))return current;
      Json next=current->snapshot();next["updates"]=updates;
      return std::make_shared<value::SharedRecord>(
        value::SharedRecord::fromJson(next));
    };
    const auto& suggestionsValue=value::property(test,"suggestions");
    const std::optional<Json> suggestions=suggestionsValue.is_null()?
      std::nullopt:std::optional<Json>(suggestionsValue);
    auto pending=cli::permissions::executePermissionRequestHooksForSDK(
      loop,"Read","tool-1",Json{{"file","a"}},context,suggestions,
      services);
    std::optional<std::optional<Json>> value;
    std::exception_ptr error;
    pending.then([&](const std::optional<Json>* result,const auto& failure){
      error=failure;if(result)value=*result;
    });
    loop.runReady();
    Json errorText;
    if(error){try{std::rethrow_exception(error);}
      catch(const std::exception& failure){errorText=failure.what();}}
    if(!value&&!error)throw std::runtime_error("Hook decision unsettled");
    std::cout<<value::stringify(Json{{"events",events},
      {"value",value&&*value?**value:Json()},{"error",errorText},
      {"appState",app->snapshot()},{"sameState",app==initial}})<<'\n';
  }
}
