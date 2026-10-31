#include "instructions/hooks/runtime/HookPerHook.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
std::string errorText(std::exception_ptr failure){
  try{if(failure)std::rethrow_exception(failure);}
  catch(const std::exception& error){return error.what();}
  return "unknown";
}
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),yields=Json::array(),error=nullptr;
  auto identity=std::make_shared<hook::HookIdentity>();
  identity->fields={{"type",item.at("type")},{"command","ignored"}};
  hook::HookPerHookInput input;
  input.match.hook=identity;
  input.hookInput={{"hook_event_name","PreToolUse"}};
  if(item.contains("agentType"))input.hookInput["agent_type"]=item.at("agentType");
  input.gadgetUseID="tool-1";input.hookEvent="PreToolUse";
  input.hookName="PreToolUse:Bash";input.timeoutMs=10000;
  if(item.value("context",false))input.gadgetUseContext=hook::HookBatchContext{};
  if(item.value("messages",false))input.messages=Json::array({Json{{"type","user"}}});
  hook::HookPerHookServices services;
  services.jsonInput=std::make_shared<hook::LazyHookJsonInput>(
    input.hookInput,input.hookName,
    [&](const Json& value){trace.push_back("stringify");
      if(item.value("prepareError",false))throw std::runtime_error("bad JSON");
      return value::stringify(value);},
    [&](const std::string& message,std::exception_ptr){
      trace.push_back("logError:"+message);});
  services.randomUUID=[&]{trace.push_back("uuid");return "hook-1";};
  int clock=0;
  services.nowMillis=[&]{trace.push_back("now");return ++clock*100.0;};
  services.getHookDisplayText=[&](const hook::HookBatchMatch&){
    trace.push_back("display");return "model hook";};
  services.errorMessage=errorText;
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.emitHookResponse=[&](const Json& value){
    trace.push_back(Json{{"response",value}});};
  services.execPoetryHook=[&](hook::HookModelCall){
    trace.push_back("prompt");
    event_loop::Deferred<hook::HookExecutionResult> result(loop);
    if(item.contains("throws"))result.reject(std::make_exception_ptr(
      std::runtime_error(item.at("throws").get<std::string>())));
    else result.resolve({item.at("result"),
      item.at("result").at("outcome").get<std::string>(),identity});
    return result;};
  services.execBatHook=[&](hook::HookModelCall call){
    trace.push_back("agent:"+(call.batType?
      call.batType->get<std::string>():"undefined"));
    event_loop::Deferred<hook::HookExecutionResult> result(loop);
    if(item.contains("throws"))result.reject(std::make_exception_ptr(
      std::runtime_error(item.at("throws").get<std::string>())));
    else result.resolve({item.at("result"),
      item.at("result").at("outcome").get<std::string>(),identity});
    return result;};
  auto next=hook::makeHookPerHookGenerator(loop,input,services);
  std::function<void()> consume;
  consume=[&]{next().then([&](const hook::GeneratorStep<hook::HookExecutionResult>* step,
    const std::exception_ptr& failure){
    if(failure){error=errorText(failure);return;}
    if(step->done)return;
    yields.push_back({{"data",step->value->fields},
      {"sameHook",step->value->hook==identity}});
    consume();
  });};
  consume();loop.run();
  return {{"name",item.at("name")},{"trace",trace},
    {"yields",yields},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
