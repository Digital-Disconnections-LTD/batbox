#include "instructions/hooks/runtime/HookEventWrappers.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::instructions::hooks::runtime;
namespace {
Json stepJson(const HookYield& step){
  Json out={{"done",step.done}};
  if(std::find(step.ownUndefinedKeys.begin(),step.ownUndefinedKeys.end(),
      "value")==step.ownUndefinedKeys.end())out["value"]=step.value;
  return out;
}
Json await(event_loop::Loop& loop,event_loop::Deferred<HookYield> pending){
  std::optional<HookYield> result;
  std::exception_ptr failure;
  pending.then([&](const HookYield* value,const std::exception_ptr& error){
    if(error)failure=error;else result=*value;
  });
  loop.runReady();
  if(failure)std::rethrow_exception(failure);
  if(!result)throw std::runtime_error("Unsettled wrapper probe promise");
  return stepJson(*result);
}
std::optional<std::string> textField(const Json& item,const char* key){
  if(item.contains(key)&&item.at(key).is_string())
    return item.at(key).get<std::string>();
  return {};
}
double numberField(const Json& item,const char* key,double fallback){
  return item.contains(key)?item.at(key).get<double>():fallback;
}
bool boolField(const Json& item,const char* key){
  return item.contains(key)&&item.at(key)==true;
}
Json callJson(const ExecuteHooksInput& input){
  Json result={{"hookInput",input.batch.hookInput},
    {"toolUseID",input.batch.gadgetUseID},{"timeoutMs",*input.timeoutMs}};
  if(input.batch.matchQuery)result["matchQuery"]=*input.batch.matchQuery;
  if(input.forceSyncExecution)result["forceSyncExecution"]=true;
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);
  Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    event_loop::Loop loop;
    Json trace=Json::array();
    HookEventWrapperServices services;
    services.createBaseHookInput=[&](std::optional<std::string> mode,
      std::optional<std::string> flight,std::optional<HookBatInfo>){
      trace.push_back(Json::array({"base",mode?Json(*mode):Json(nullptr),
        flight?Json(*flight):Json(nullptr)}));
      Json result={{"session_id",flight.value_or("sess")},
        {"transcript_path","/tmp/t"},{"cwd","/tmp/c"},
        {"agent_type","base"}};
      if(mode)result["permission_mode"]=*mode;
      return result;
    };
    services.randomUUID=[&]{trace.push_back(Json::array({"uuid"}));
      return "uuid-1";};
    services.executeHooks=[&](ExecuteHooksInput call){
      auto count=std::make_shared<int>(0);
      auto finished=std::make_shared<bool>(false);
      auto payload=callJson(call);
      return HookYieldCursor{
        [&,count,finished,payload]{
          event_loop::Deferred<HookYield> out(loop);
          if(*count==0){
            trace.push_back(Json::array({"execute",payload}));
            Json value={{"hookInput",payload.at("hookInput")},
              {"toolUseID",payload.at("toolUseID")},
              {"timeoutMs",payload.at("timeoutMs")}};
            if(payload.contains("matchQuery"))
              value["matchQuery"]=payload.at("matchQuery");
            if(payload.contains("forceSyncExecution"))
              value["forceSyncExecution"]=true;
            ++*count;out.resolve({false,value,{}});
          }else if(!*finished){
            trace.push_back(Json::array({"finally"}));
            *finished=true;out.resolve({true,Json(nullptr),{"value"}});
          }else out.resolve({true,Json(nullptr),{"value"}});
          return out;
        },
        [&,finished](std::optional<Json> value){
          event_loop::Deferred<HookYield> out(loop);
          if(!*finished){trace.push_back(Json::array({"finally"}));
            *finished=true;}
          out.resolve({true,value.value_or(Json(nullptr)),
            value?std::vector<std::string>{}:std::vector<std::string>{"value"}});
          return out;
        },
        [&](std::exception_ptr error){
          event_loop::Deferred<HookYield> out(loop);out.reject(error);return out;
        }};
    };
    HookEventGenerator generator= [&]{
      const std::string kind=item.at("kind").get<std::string>();
      if(kind=="session")return executeFlightStartHooks(loop,
        {item.at("source").get<std::string>(),textField(item,"sessionId"),
          textField(item,"agentType"),textField(item,"model"),{},
          numberField(item,"timeoutMs",600000),boolField(item,"forceSyncExecution")},
        services);
      if(kind=="setup")return executeSetupHooks(loop,
        {item.at("trigger").get<std::string>(),{},
          numberField(item,"timeoutMs",600000),boolField(item,"forceSyncExecution")},
        services);
      return executeSubagentStartHooks(loop,
        {item.at("agentId").get<std::string>(),
          item.at("agentType").get<std::string>(),{},
          numberField(item,"timeoutMs",600000)},services);
    }();
    trace.push_back(Json::array({"created"}));
    if(boolField(item,"earlyReturn")){
      trace.push_back(Json::array({"return",await(loop,generator.returnValue("closed"))}));
      trace.push_back(Json::array({"after",await(loop,generator.next())}));
    }else{
      trace.push_back(Json::array({"first",await(loop,generator.next())}));
      trace.push_back(Json::array({"return",await(loop,generator.returnValue("closed"))}));
      trace.push_back(Json::array({"after",await(loop,generator.next())}));
    }
    rows.push_back({{"name",item.at("name")},{"trace",trace}});
  }
  std::cout<<rows.dump()<<'\n';
}
