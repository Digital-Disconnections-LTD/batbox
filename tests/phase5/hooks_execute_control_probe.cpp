#include "instructions/hooks/runtime/ExecuteHooks.hpp"
#include <algorithm>
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
  event_loop::Loop loop;Json trace=Json::array(),steps=Json::array(),error=nullptr;
  const auto kind=item.at("kind").get<std::string>();
  hook::ExecuteHooksInput input;
  input.batch.hookInput={{"hook_event_name","PreToolUse"}};
  input.batch.gadgetUseID="tool-1";input.timeoutMs=10000;
  hook::ExecuteHooksServices services;
  services.batch.shouldDisableAllHooksIncludingManaged=[&]{
    trace.push_back("disabled");return false;};
  services.batch.isSimpleEnvironment=[]{return false;};
  services.batch.shouldSkipHookDueToTrust=[]{return false;};
  services.batch.getFlightId=[&]{trace.push_back("session");return "main-1";};
  services.batch.getMatchingHooks=[&](const std::optional<Json>&,
    const std::string&,const std::string&,const Json&,
    const std::optional<Json>&){
    trace.push_back("match");
    auto identity=std::make_shared<hook::HookIdentity>();
    identity->fields={{"type","command"},{"command","echo one"},
      {"display","echo one"}};
    hook::HookBatchMatch match;match.hook=identity;
    std::vector<hook::HookBatchMatch> matches{match};
    if(kind=="return-with-pending-child"){
      auto second=std::make_shared<hook::HookIdentity>();
      second->fields={{"type","command"},{"command","echo two"},
        {"display","echo two"}};
      hook::HookBatchMatch extra;extra.hook=second;
      matches.push_back(std::move(extra));
    }
    event_loop::Deferred<std::vector<hook::HookBatchMatch>> output(loop);
    output.resolve(std::move(matches));return output;};
  services.batch.isInternalHook=[](const hook::HookBatchMatch&){return false;};
  auto display=[&](const hook::HookBatchMatch& match){
    trace.push_back("display");
    return match.hook->fields.at("display").get<std::string>();};
  services.batch.getHookDisplayText=display;
  services.perHook.getHookDisplayText=display;
  int uuid=0,clock=0;
  auto randomUUID=[&]{trace.push_back("uuid");
    return "uuid-"+std::to_string(++uuid);};
  auto nowMillis=[&]{trace.push_back("now");return ++clock*100.0;};
  services.batch.randomUUID=randomUUID;
  services.perHook.randomUUID=randomUUID;
  services.batch.nowMillis=nowMillis;
  services.perHook.nowMillis=nowMillis;
  services.results.nowMillis=nowMillis;
  services.batch.nowIso=[]{return "2026-01-01T00:00:00.000Z";};
  services.jsonStringify=[&](const Json& value){
    trace.push_back("stringify");return value::stringify(value);};
  services.logError=[](const std::string&,std::exception_ptr){};
  services.perHook.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.perHook.emitHookStarted=[&](const std::string&,
    const std::string&,const std::string&){trace.push_back("started");};
  services.perHook.emitHookResponse=[&](const Json&){
    trace.push_back("response");};
  services.perHook.execCommandHook=[&](hook::HookCommandCall call){
    trace.push_back("exec");
    event_loop::Deferred<hook::CommandOutcome> output(loop);
    const auto command=call.hook.at("display").get<std::string>();
    auto complete=[&,output,command]{
      if(kind=="return-with-pending-child")
        trace.push_back("exec-done:"+command);
      hook::CommandOutcome result;
      result.stdoutText="one";result.output="combined";
      output.resolve(std::move(result));};
    if(kind=="pending-command-return"||
      (kind=="return-with-pending-child"&&command=="echo two"))
      loop.setTimeout(1,complete);
    else complete();
    return output;};
  services.perHook.parseHookOutput=[&](const std::string&){
    trace.push_back("parse");return hook::HookParseResult{};};
  services.results.getHookDisplayText=[](const hook::HookIdentity&){
    return "echo one";};
  services.results.addToTurnHookDuration=[&](double value){
    trace.push_back("duration:"+std::to_string(static_cast<int>(value)));};
  hook::ExecuteHooks execute(loop,input,services);
  auto collect=[&](const hook::HookYield* value,const std::exception_ptr& failure){
    if(failure){error=errorText(failure);return;}
    const bool own=value->done&&std::find(value->ownUndefinedKeys.begin(),
      value->ownUndefinedKeys.end(),"value")!=value->ownUndefinedKeys.end();
    steps.push_back({{"done",value->done},
      {"data",own?Json(nullptr):value->value},
      {"ownUndefined",own}});
  };
  auto finalNext=[&]{execute.next().then([&](const hook::HookYield* value,
    const std::exception_ptr& failure){collect(value,failure);});};
  auto close=[&]{
    if(kind.find("throw")!=std::string::npos)
      return execute.throwError(std::make_exception_ptr(
        std::runtime_error("stop")));
    return execute.returnValue(Json("R"));};
  auto closeThenNext=[&]{close().then([&](const hook::HookYield* value,
    const std::exception_ptr& failure){collect(value,failure);finalNext();});};
  if(kind=="return-before"||kind=="throw-before")closeThenNext();
  else if(kind=="queued-return"||kind=="queued-throw"){
    auto first=execute.next();auto closing=close();
    auto settled=std::make_shared<int>(0);
    auto complete=[settled,&finalNext]{if(++*settled==2)finalNext();};
    first.then([&,complete](const hook::HookYield* value,const std::exception_ptr& failure){
      collect(value,failure);complete();});
    closing.then([&,complete](const hook::HookYield* value,const std::exception_ptr& failure){
      collect(value,failure);complete();});
  }else{
    execute.next().then([&](const hook::HookYield* value,
      const std::exception_ptr& failure){
      collect(value,failure);
      if(kind=="return-progress"||kind=="throw-progress"){
        closeThenNext();return;
      }
      if(kind=="return-with-pending-child"){
        execute.next().then([&](const hook::HookYield* second,
          const std::exception_ptr& secondError){
          collect(second,secondError);
          execute.next().then([&](const hook::HookYield* result,
            const std::exception_ptr& resultError){
            collect(result,resultError);closeThenNext();});
        });return;
      }
      auto pending=execute.next();
      if(kind=="pending-command-return"){
        auto closing=close();auto settled=std::make_shared<int>(0);
        auto complete=[settled,&finalNext]{if(++*settled==2)finalNext();};
        pending.then([&,complete](const hook::HookYield* next,
          const std::exception_ptr& nextError){
          collect(next,nextError);complete();});
        closing.then([&,complete](const hook::HookYield* next,
          const std::exception_ptr& nextError){
          collect(next,nextError);complete();});
      }else pending.then([&](const hook::HookYield* next,
        const std::exception_ptr& nextError){
        collect(next,nextError);closeThenNext();});
    });
  }
  loop.run();
  return {{"name",item.at("name")},{"trace",trace},
    {"steps",steps},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
