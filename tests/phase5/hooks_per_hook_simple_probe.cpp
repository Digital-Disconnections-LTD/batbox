#include "instructions/hooks/runtime/HookPerHook.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),yields=Json::array(),error=nullptr;
  hook::FunctionHookServices functionServices;
  functionServices.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  functionServices.logError=[](std::exception_ptr){};
  functionServices.isErrorInstance=[](std::exception_ptr){return true;};
  functionServices.isAbortError=[](std::exception_ptr){return false;};
  functionServices.errorMessage=[](std::exception_ptr){return "error";};
  hook::FunctionHookHost functionHost(loop,functionServices);
  auto identity=std::make_shared<hook::HookIdentity>();
  identity->fields={{"type",item.at("type")},{"errorMessage","blocked"}};
  if(item.contains("timeout"))identity->fields["timeout"]=item.at("timeout");
  hook::HookBatchMatch match;match.hook=identity;
  auto callback=std::make_shared<hook::HookCallback>();
  callback->callback=[&](const Json&,const std::string&,std::stop_token,
    std::optional<std::size_t> index,
    const std::optional<hook::HookCallbackContext>&)->hook::HookCallbackReply{
    trace.push_back("callback:"+std::to_string(index.value_or(0)));
    return Json{{"async",true}};};
  match.callback=callback;
  auto function=std::make_shared<hook::FunctionHook>();
  function->fields=identity->fields;
  if(item.contains("timeout"))function->timeout=item.at("timeout").get<double>();
  function->errorMessage="blocked";
  function->callback=[&](const Json& messages,std::stop_token)->hook::FunctionHookReply{
    trace.push_back("function:"+std::to_string(messages.size()));
    return item.value("passed",true);};
  match.function=function;
  hook::HookPerHookInput input;
  input.match=match;input.hookInput={{"hook_event_name","PreToolUse"}};
  input.gadgetUseID="tool-1";input.hookEvent="PreToolUse";
  input.hookName="PreToolUse:Bash";input.timeoutMs=10000;
  if(item.value("messages",false))input.messages=Json::array({Json{{"type","user"}}});
  hook::HookPerHookServices services;
  services.functionHost=&functionHost;
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  auto next=hook::makeHookPerHookGenerator(loop,input,services);
  std::function<void()> consume;
  consume=[&]{next().then([&](const hook::GeneratorStep<hook::HookExecutionResult>* step,
    const std::exception_ptr& failure){
    if(failure){try{std::rethrow_exception(failure);}
      catch(const std::exception& exception){error=exception.what();}return;}
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
