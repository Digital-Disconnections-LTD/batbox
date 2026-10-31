#include "instructions/hooks/runtime/PoetryGadgetWrappers.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::instructions::hooks::runtime;
namespace {
std::optional<std::string> field(const Json& item,const char* key){
  if(item.contains(key)&&item.at(key).is_string())
    return item.at(key).get<std::string>();
  return {};
}
Json stepJson(const HookYield& step){
  Json out={{"done",step.done}};
  if(std::find(step.ownUndefinedKeys.begin(),step.ownUndefinedKeys.end(),
      "value")==step.ownUndefinedKeys.end())out["value"]=step.value;
  return out;
}
Json await(event_loop::Loop& loop,event_loop::Deferred<HookYield> pending){
  std::optional<HookYield> value;std::exception_ptr failure;
  pending.then([&](const HookYield* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.runReady();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Unsettled prompt tool promise");
  return stepJson(*value);
}
Json callJson(const ExecuteHooksInput& call){
  Json out={{"hookInput",call.batch.hookInput},
    {"toolUseID",call.batch.gadgetUseID},{"timeoutMs",*call.timeoutMs}};
  if(call.batch.matchQuery)out["matchQuery"]=*call.batch.matchQuery;
  if(call.batch.gadgetInputSummary)out["toolInputSummary"]=*call.batch.gadgetInputSummary;
  if(call.batch.gadgetUseContext){
    Json context=Json::object();
    if(call.batch.gadgetUseContext->batId)
      context["agentId"]=*call.batch.gadgetUseContext->batId;
    if(call.batch.gadgetUseContext->batType)
      context["agentType"]=*call.batch.gadgetUseContext->batType;
    out["toolUseContext"]=std::move(context);
  }
  return out;
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    event_loop::Loop loop;Json trace=Json::array();
    PoetryGadgetServices services;
    services.getFlightId=[&]{trace.push_back(Json::array({"sessionId"}));
      return "sess";};
    services.hasHookForEvent=[&](const std::string& event,
      const std::optional<Json>& state,const std::string& flight){
      trace.push_back(Json::array({"present",event,*state,flight}));
      return item.at("present").get<bool>();
    };
    services.logForDebugging=[&](const std::string& message,
      const std::optional<Json>& metadata){
      trace.push_back(Json::array({"debug",message,
        metadata?*metadata:Json(nullptr)}));
    };
    services.wrapper.createBaseHookInput=[&](std::optional<std::string> mode,
      std::optional<std::string> flight,std::optional<HookBatInfo> bat){
      Json info={{"agentId",bat&&bat->batId?Json(*bat->batId):Json(nullptr)},
        {"agentType",bat&&bat->batType?Json(*bat->batType):Json(nullptr)}};
      trace.push_back(Json::array({"base",mode?Json(*mode):Json(nullptr),
        flight?Json(*flight):Json(nullptr),info}));
      Json out={{"session_id","sess"},{"transcript_path","/tmp/t"},
        {"cwd","/tmp/c"},
        {"agent_type",bat&&bat->batType?*bat->batType:"base"}};
      if(mode)out["permission_mode"]=*mode;
      if(bat&&bat->batId)out["agent_id"]=*bat->batId;
      return out;
    };
    services.executeWithPoetry=[&](ExecuteHooksInput call,
      RequestPoetryFactory poetry){
      auto payload=callJson(call);
      auto summary=call.batch.gadgetInputSummary;
      return HookYieldCursor{
        [&,payload,poetry,summary]{
          event_loop::Deferred<HookYield> out(loop);
          trace.push_back(Json::array({"execute",payload}));
          if(poetry)poetry("source",summary);
          out.resolve({false,payload,{}});return out;
        },
        [&](std::optional<Json> value){
          event_loop::Deferred<HookYield> out(loop);
          out.resolve({true,value.value_or(Json(nullptr)),
            value?std::vector<std::string>{}:std::vector<std::string>{"value"}});
          return out;
        },
        [&](std::exception_ptr error){
          event_loop::Deferred<HookYield> out(loop);out.reject(error);return out;
        }};
    };
    HookBatchContext context;
    context.batId=field(item,"agentId");
    context.batType=field(item,"agentType");
    context.getAppState=[&]{trace.push_back(Json::array({"appState"}));
      return Json{{"state","ready"}};};
    RequestPoetryFactory poetry;
    if(item.value("requestPrompt",false))poetry=[&](const std::string& source,
      const std::optional<Json>& summary){
      trace.push_back(Json::array({"promptBound",source,
        summary?*summary:Json(nullptr)}));
      return BoundHookPoetry{};
    };
    const auto kind=item.at("kind").get<std::string>();
    HookEventGenerator generator=[&]{
      if(kind=="pre")return executePreGadgetHooks(loop,
        {item.at("toolName").get<std::string>(),
          item.at("toolUseID").get<std::string>(),item.at("toolInput"),
          context,field(item,"permissionMode"),{},600000,poetry,
          field(item,"summary")?std::optional<Json>(*field(item,"summary")):
            std::nullopt},services);
      std::optional<Json> suggestions;
      if(item.contains("permissionSuggestions"))
        suggestions=item.at("permissionSuggestions");
      std::optional<Json> summary;
      if(auto value=field(item,"summary"))summary=*value;
      return executePermissionRequestHooks(loop,
        {item.at("toolName").get<std::string>(),
          item.at("toolUseID").get<std::string>(),item.at("toolInput"),
          context,field(item,"permissionMode"),suggestions,{},600000,
          poetry,summary},services);
    }();
    trace.push_back(Json::array({"created"}));
    trace.push_back(Json::array({"first",await(loop,generator.next())}));
    rows.push_back({{"name",item.at("name")},{"trace",trace}});
  }
  std::cout<<rows.dump()<<'\n';
}
