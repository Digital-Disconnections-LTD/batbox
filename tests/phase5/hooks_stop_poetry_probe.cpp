#include "instructions/hooks/runtime/StopPoetryWrappers.hpp"
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
  if(!value)throw std::runtime_error("Unsettled stop wrapper promise");
  return stepJson(*value);
}
Json callJson(const ExecuteHooksInput& call){
  Json out={{"hookInput",call.batch.hookInput},
    {"toolUseID",call.batch.gadgetUseID},{"timeoutMs",*call.timeoutMs}};
  if(call.batch.signal)out["signal"]=Json::object();
  if(call.messages)out["messages"]=*call.messages;
  if(call.batch.gadgetUseContext){
    Json context={{"abortController",{{"signal",Json::object()}}}};
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
    StopPoetryServices services;
    services.poetry.getFlightId=[&]{trace.push_back(Json::array({"sessionId"}));
      return "sess";};
    services.poetry.hasHookForEvent=[&](const std::string& event,
      const std::optional<Json>& state,const std::string& flight){
      trace.push_back(Json::array({"present",event,
        state?*state:Json(nullptr),flight}));
      return item.at("present").get<bool>();
    };
    services.getLastAssistantMessage=[&](const Json& messages){
      trace.push_back(Json::array({"lastMessage"}));
      for(auto it=messages.rbegin();it!=messages.rend();++it)
        if(it->value("type","")=="assistant")return *it;
      return Json(nullptr);
    };
    services.extractTextContent=[&](const Json& blocks,const std::string& separator){
      trace.push_back(Json::array({"extract",separator}));
      std::string result;bool first=true;
      for(const auto& block:blocks)if(block.value("type","")=="text"){
        if(!first)result+=separator;
        first=false;
        result+=block.at("text").get<std::string>();
      }
      return result;
    };
    services.getBatTranscriptPath=[&](const std::string& id){
      trace.push_back(Json::array({"agentPath",id}));
      return "/tmp/"+id+".jsonl";
    };
    services.poetry.wrapper.createBaseHookInput=[&](
      std::optional<std::string> mode,std::optional<std::string> flight,
      std::optional<HookBatInfo> bat){
      trace.push_back(Json::array({"base",mode?Json(*mode):Json(nullptr),
        flight?Json(*flight):Json(nullptr),
        bat?Json::object():Json(nullptr)}));
      Json out={{"session_id","sess"},{"transcript_path","/tmp/t"},
        {"cwd","/tmp/c"},{"agent_type","base"}};
      if(mode)out["permission_mode"]=*mode;
      return out;
    };
    services.poetry.wrapper.randomUUID=[&]{
      trace.push_back(Json::array({"uuid"}));return "uuid-1";};
    services.poetry.executeWithPoetry=[&](ExecuteHooksInput call,
      RequestPoetryFactory poetry){
      auto payload=callJson(call);
      return HookYieldCursor{
        [&,payload,poetry]{
          event_loop::Deferred<HookYield> out(loop);
          trace.push_back(Json::array({"execute",payload}));
          if(poetry)poetry("source",Json(nullptr));
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
    RequestPoetryFactory poetry;
    if(item.value("requestPrompt",false))poetry=[&](const std::string& source,
      const std::optional<Json>& summary){
      trace.push_back(Json::array({"promptBound",source,
        summary?*summary:Json(nullptr)}));
      return BoundHookPoetry{};
    };
    const std::string kind=item.at("kind").get<std::string>();
    HookEventGenerator generator=[&]{
      if(kind=="stop"){
        StopHookCall call;
        call.permissionMode=field(item,"permissionMode");
        call.stopHookActive=item.value("stopHookActive",false);
        call.subagentId=field(item,"subagentId");
        if(item.contains("messages"))call.messages=item.at("messages");
        call.batType=field(item,"agentType");
        call.requestPoetry=poetry;
        if(item.contains("agentId")){
          HookBatchContext context;
          context.batId=field(item,"agentId");
          context.batType=field(item,"agentType");
          context.getAppState=[&]{trace.push_back(Json::array({"appState"}));
            return Json{{"state","ready"}};};
          call.gadgetUseContext=std::move(context);
        }
        return executeStopHooks(loop,std::move(call),services);
      }
      HookBatchContext context;
      context.batId=field(item,"agentId");
      context.batType=field(item,"agentType");
      context.getAppState=[&]{trace.push_back(Json::array({"appState"}));
        return Json{{"state","ready"}};};
      return executeUserPoetrySubmitHooks(loop,
        {item.at("prompt").get<std::string>(),
          item.at("permissionMode").get<std::string>(),context,
          std::stop_source{}.get_token(),poetry},services);
    }();
    trace.push_back(Json::array({"created"}));
    trace.push_back(Json::array({"first",await(loop,generator.next())}));
    rows.push_back({{"name",item.at("name")},{"trace",trace}});
  }
  std::cout<<rows.dump()<<'\n';
}
