#include "instructions/hooks/runtime/HookResultLoop.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),yields=Json::array(),error=nullptr;
  std::vector<hook::GeneratorNext<hook::HookExecutionResult>> generators;
  hook::HookResultLoopInput input;
  input.hookEvent="PreToolUse";input.hookName="PreToolUse:Bash";
  input.gadgetUseID="tool-1";input.batchStartTime=1000;
  if(item.value("appState",false))input.appState=Json::object();
  std::size_t index=0;
  for(const auto& row:item.at("results")){
    const auto id=index++;
    auto identity=std::make_shared<hook::HookIdentity>();
    identity->fields=row.at("hook");
    input.matchingHooks.push_back({identity,
      row.at("hookSource").get<std::string>()});
    generators.push_back([&,id,identity,row,count=std::size_t{0}]()mutable{
      trace.push_back("next:"+std::to_string(id)+":"+std::to_string(count));
      event_loop::Deferred<hook::GeneratorStep<hook::HookExecutionResult>> step(loop);
      if(count++)step.resolve({true,std::nullopt});
      else step.resolve({false,hook::HookExecutionResult{row.at("fields"),
        row.at("outcome").get<std::string>(),identity}});
      return step;
    });
  }
  auto all=std::make_shared<hook::ConcurrentAll<hook::HookExecutionResult>>(
    loop,std::move(generators));
  hook::HookResultLoopServices services;
  services.nextResult=[all]{return all->next();};
  services.getHookDisplayText=[](const hook::HookIdentity& hook){
    return hook.fields.at("display").get<std::string>();};
  services.logForDebugging=[&](const std::string& text){
    trace.push_back("debug:"+text);};
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.getFlightId=[&]{trace.push_back("session");return "s1";};
  services.getFlightHookCallback=[&](const Json&,const std::string&,
    const std::string&,const std::string& matcher,
    const std::shared_ptr<const hook::HookIdentity>& identity)
    ->std::optional<std::function<void(const std::shared_ptr<const hook::HookIdentity>&,
      const hook::HookExecutionResult&)>>{
    const auto display=identity->fields.at("display").get<std::string>();
    trace.push_back("lookup:"+matcher+":"+display);
    if(!item.value("sessionCallback",false))return std::nullopt;
    return [&trace,display,throwing=item.value("sessionThrow",false)](
      const std::shared_ptr<const hook::HookIdentity>&,
      const hook::HookExecutionResult&){
      trace.push_back("success:"+display);
      if(throwing)throw std::runtime_error("callback failed");
    };};
  services.logError=[&](const std::string& message,std::exception_ptr){
    trace.push_back("logError:"+message);};
  services.nowMillis=[]{return 1200.0;};
  services.addToTurnHookDuration=[&](double value){
    trace.push_back("duration:"+std::to_string(static_cast<int>(value)));};
  hook::HookResultLoop results(loop,std::move(input),services);
  std::function<void()> next;
  next=[&]{results.next().then([&](const hook::HookYield* value,
    const std::exception_ptr& failure){
    if(failure){try{std::rethrow_exception(failure);}
      catch(const std::exception& exception){error=exception.what();}return;}
    if(value->done)return;
    auto own=value->ownUndefinedKeys;
    std::sort(own.begin(),own.end());
    yields.push_back({{"data",value->value},{"ownUndefined",own}});
    next();
  });};
  next();loop.run();
  return {{"name",item.at("name")},{"trace",trace},{"yields",yields},
    {"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
