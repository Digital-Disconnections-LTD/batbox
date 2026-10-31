#include "cli/permissions/PermissionRace.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    Json events=Json::array();
    auto outbound=std::make_shared<cli::structured::AsyncStream<Json>>(loop);
    auto control=std::make_shared<cli::structured::ControlResponses>(
      [&](const Json& value){events.push_back(Json::array({"write",value}));});
    cli::structured::RequestProducerServices requestServices;
    requestServices.randomUUID=[]{return std::string("request-1");};
    requestServices.onControlRequestSent=[&](const Json& value){
      events.push_back(Json::array({"sent",value}));
    };
    cli::structured::RequestProducer requests(loop,control,outbound,
      requestServices);
    gadgets::Gadget gadget;gadget.name="Read";
    gadget.userFacingName=[](const Json&){return std::string("Read a file");};
    auto app=std::make_shared<app_state::AppState>();
    app->fields=std::make_shared<value::SharedRecord>(
      value::SharedRecord::fromJson(Json::object()));
    app->setPermissionContext(std::make_shared<value::SharedRecord>(
      value::SharedRecord::fromJson(Json{{"mode","default"}})));
    std::stop_source parent;
    cli::permissions::PermissionUseContext context;
    context.batId="agent-1";
    context.parentAbort=parent.get_token();
    context.hooks.abortToken=parent.get_token();
    context.hooks.getAppState=[&]{return app;};
    context.hooks.setAppState=[&](const auto& updater){app=updater(app);};
    context.poetry.setAppState=[&](const auto& updater){
      events.push_back(Json::array({"setAppState"}));
      updater(Json{{"toolPermissionContext",Json{{"mode","default"}}}});
    };
    context.poetry.applyPermissionUpdates=[&](const Json& previous,
                                               const Json& updates){
      events.push_back(Json::array({"apply",previous,updates}));
      Json result=previous;result["updates"]=updates;return result;
    };
    context.poetry.persistPermissionUpdates=[&](const Json& updates){
      events.push_back(Json::array({"persist",updates}));
    };
    context.poetry.logForDebugging=[](const std::string&){};
    context.poetry.abort=[]{};
    cli::permissions::PermissionRaceServices services;
    services.randomUUID=[]{return std::string("request-1");};
    services.feature=[](const std::string&){return false;};
    services.parsePermissionGadgetOutput=[](const Json& result){return result;};
    services.notifyFlightStateChanged=[&](const std::string& state){
      events.push_back(Json::array({"notify",state}));
    };
    services.hasPermissionsToUseGadget=[&](const gadgets::Gadget&,const Json&,
        const auto&,const Json&,const std::string&){
      events.push_back(Json::array({"check"}));
      event_loop::Deferred<Json> pending(loop);
      if(test.value("mainReject",false))pending.reject(
        std::make_exception_ptr(std::runtime_error("checker failed")));
      else pending.resolve(test.value("main",Json{{"behavior","ask"},
        {"suggestions",Json::array({Json{{"type","addRules"},
          {"rules",Json::array({"Read"})}}})},
        {"blockedPath","/a"},{"decisionReason",Json{{"type","other"},
          {"reason","asked"}}}}));
      return pending;
    };
    services.hooks.executePermissionRequestHooks=[&](const std::string&,
        const std::string&,const Json&,const auto&,const std::string&,
        const std::optional<Json>&,std::stop_token){
      events.push_back(Json::array({"hook"}));
      auto at=std::make_shared<int>(0);
      instructions::hooks::runtime::HookYieldCursor cursor;
      cursor.next=[&,at]{
        event_loop::Deferred<instructions::hooks::runtime::HookYield>
          pending(loop);
        if(test.value("hookDelayed",false))return pending;
        if(test.value("hookError",false)){
          pending.reject(std::make_exception_ptr(
            std::runtime_error("hook failed")));return pending;
        }
        instructions::hooks::runtime::HookYield value;
        if((*at)++==0&&test.contains("hookDecision"))
          value.value=Json{{"permissionRequestResult",test.at("hookDecision")}};
        else value.done=true;
        pending.resolve(value);return pending;
      };
      return cursor;
    };
    services.hooks.persistPermissionUpdates=[](const Json&){};
    services.hooks.applyPermissionUpdates=[](const value::RecordRef& current,
                                             const Json&){return current;};
    const auto& forceValue=value::property(test,"force");
    const std::optional<Json> force=forceValue.is_null()?std::nullopt:
      std::optional<Json>(forceValue);
    auto promise=cli::permissions::createCanUseGadget(loop,requests,*control,
      gadget,Json{{"file","a"}},context,Json{{"type","assistant"}},
      "tool-1",force,services,[&](const Json& details){
        events.push_back(Json::array({"prompt",details}));
      });
    auto inject=[&]{
      const bool failed=test.value("sdkError",false);
      control->injectControlResponse(Json{{"type","control_response"},
        {"response",Json{{"request_id","request-1"},
          {"subtype",failed?"error":"success"},
          {"response",test.value("sdk",Json{{"behavior","allow"},
            {"updatedInput",Json::object()}})},{"error","host denied"}}}});
    };
    if(test.value("sdkFirst",false)||test.value("sdkError",false))
      loop.queueMicrotask(inject);
    if(test.value("parentAbort",false))
      loop.queueMicrotask([&]{parent.request_stop();});
    loop.runReady();
    const auto& behavior=value::property(value::property(test,"main"),"behavior");
    if(!test.value("sdkFirst",false)&&!test.value("sdkError",false)&&
       !test.value("hookDelayed",false)&&
       !test.value("hookError",false)&&!test.contains("hookDecision")&&
       !test.contains("force")&&!test.value("mainReject",false)&&
       behavior!="allow"&&behavior!="deny"){
      inject();loop.runReady();
    }
    Json value,error;
    promise.then([&](const Json* result,const std::exception_ptr& failure){
      if(result)value=*result;
      if(failure){try{std::rethrow_exception(failure);}
        catch(const std::exception& cause){error=cause.what();}}
    });
    loop.runReady();
    std::cout<<value::stringify(Json{{"value",value},{"error",error},
      {"events",events},{"outbound",Json(outbound->snapshotQueued())},
      {"pending",control->getPendingPermissionRequests().size()},
      {"resolved",control->hasResolvedGadgetUseId("tool-1")}})<<'\n';
  }
}
