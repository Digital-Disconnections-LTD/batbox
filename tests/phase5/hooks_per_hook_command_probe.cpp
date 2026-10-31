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
  identity->fields={{"type","command"},{"command","echo test"}};
  hook::HookPerHookInput input;
  input.match.hook=identity;
  input.hookInput={{"hook_event_name","PreToolUse"}};
  input.gadgetUseID="tool-1";input.hookEvent="PreToolUse";
  input.hookName="PreToolUse:Bash";input.timeoutMs=10000;
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
    trace.push_back("display");return "echo test";};
  services.errorMessage=errorText;
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.processJson.createAttachmentMessage=[](const Json& attachment,
    const std::vector<std::string>&){
    return Json{{"attachment",attachment}};};
  services.bold=[](const std::string& value){return value;};
  services.emitHookStarted=[&](const std::string&,const std::string&,
    const std::string&){trace.push_back("started");};
  services.emitHookResponse=[&](const Json& value){
    trace.push_back(Json{{"response",value}});};
  services.execCommandHook=[&](hook::HookCommandCall){
    trace.push_back("exec");
    event_loop::Deferred<hook::CommandOutcome> result(loop);
    if(item.contains("throws"))
      result.reject(std::make_exception_ptr(
        std::runtime_error(item.at("throws").get<std::string>())));
    else{
      hook::CommandOutcome value;
      value.stdoutText=item.value("stdout",std::string{});
      value.stderrText=item.value("stderr",std::string{});
      value.output="combined";value.status=item.value("status",0);
      value.aborted=item.value("aborted",false);
      value.backgrounded=item.value("backgrounded",false);
      result.resolve(std::move(value));
    }
    return result;};
  services.parseHookOutput=[&](const std::string&){
    trace.push_back("parse");hook::HookParseResult result;
    if(item.contains("validationError"))
      result.validationError=item.at("validationError").get<std::string>();
    if(item.value("asyncJson",false))result.json=Json{{"async",true}};
    if(item.contains("json"))result.json=item.at("json");
    if(item.contains("plainText"))
      result.plainText=item.at("plainText").get<std::string>();
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
