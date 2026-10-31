#include "instructions/hooks/runtime/HookCallback.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;std::stop_source source;
  Json trace=Json::array(),passed=nullptr,value=nullptr,error=nullptr;
  auto configured=std::make_shared<hook::HookCallback>();
  configured->fields={{"type","callback"}};
  configured->callback=[&](const Json& hookInput,const std::string& gadgetUseID,
    std::stop_token signal,std::optional<std::size_t> index,
    const std::optional<hook::HookCallbackContext>& context)->hook::HookCallbackReply{
    trace.push_back("callback");
    passed={{"hookInput",hookInput},{"toolUseID",gadgetUseID},
      {"hasSignal",signal.stop_possible()},
      {"hookIndex",index?Json(*index):Json(nullptr)},
      {"hasContext",context.has_value()},
      {"contextKeys",context?Json::array({"getAppState",
        "updateAttributionState"}):Json::array()}};
    if(item.contains("executionError"))
      throw std::runtime_error(item.at("executionError").get<std::string>());
    if(item.value("promise",false)){
      event_loop::Deferred<Json> reply(loop);reply.resolve(item.at("json"));
      return reply;
    }
    return item.at("json");
  };
  hook::HookCallbackCall call;
  call.gadgetUseID="tool-1";call.hookEvent="PreToolUse";
  call.hookInput={{"hook_event_name","PreToolUse"}};
  call.signal=source.get_token();call.hook=configured;
  if(item.contains("hookIndex"))call.hookIndex=item.at("hookIndex").get<std::size_t>();
  if(item.value("context",false))call.gadgetUseContext=hook::HookCallbackContext{
    []{return Json{{"state",true}};},[](const Json&){}};
  hook::ProcessHookJsonServices processor;
  processor.createAttachmentMessage=[](const Json& attachment,
    const std::vector<std::string>& undefined){
    return Json{{"attachment",attachment},{"ownUndefined",undefined}};
  };
  hook::executeHookCallback(loop,call,processor).then(
    [&](const hook::HookCallbackResult* result,
      const std::exception_ptr& failure){
      if(failure)try{std::rethrow_exception(failure);}
        catch(const std::exception& exception){error=exception.what();}
      else{
        auto own=result->ownUndefinedKeys;
        std::sort(own.begin(),own.end());
        value={{"data",result->fields},{"ownUndefined",own},
          {"sameHook",result->hook==configured}};
      }
    });
  loop.run();
  return {{"name",item.at("name")},{"trace",trace},{"passed",passed},
    {"value",value},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
