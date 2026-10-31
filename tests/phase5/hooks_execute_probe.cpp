#include "instructions/hooks/runtime/ExecuteHooks.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),yields=Json::array(),error=nullptr;
  hook::ExecuteHooksInput input;
  input.batch.hookInput={{"hook_event_name","PreToolUse"}};
  input.batch.gadgetUseID="tool-1";input.timeoutMs=10000;
  hook::ExecuteHooksServices services;
  services.batch.shouldDisableAllHooksIncludingManaged=[]{return false;};
  services.batch.isSimpleEnvironment=[]{return false;};
  services.batch.shouldSkipHookDueToTrust=[]{return false;};
  services.batch.getFlightId=[&]{trace.push_back("session");return "main-1";};
  services.batch.getMatchingHooks=[&](const std::optional<Json>&,
    const std::string&,const std::string&,const Json&,
    const std::optional<Json>&){
    trace.push_back("match");
    std::vector<hook::HookBatchMatch> matches;
    std::size_t index=0;
    for(const auto& row:item.at("hooks")){
      auto identity=std::make_shared<hook::HookIdentity>();
      const auto display=row.at("display").get<std::string>();
      identity->fields={{"type",row.value("type",std::string("command"))},
        {"command",display},
        {"display",display}};
      hook::HookBatchMatch match;match.hook=identity;
      auto callback=std::make_shared<hook::HookCallback>();
      const auto hookIndex=index++;
      callback->callback=[&,hookIndex](const Json&,const std::string&,
        std::stop_token,std::optional<std::size_t>,
        const std::optional<hook::HookCallbackContext>&)
        ->hook::HookCallbackReply{
        trace.push_back("callback:"+std::to_string(hookIndex));
        return Json{{"async",true}};};
      match.callback=callback;
      matches.push_back(std::move(match));
    }
    event_loop::Deferred<std::vector<hook::HookBatchMatch>> output(loop);
    output.resolve(std::move(matches));return output;};
  services.batch.isInternalHook=[](const hook::HookBatchMatch&){return false;};
  auto display=[&](const hook::HookBatchMatch& match){
    const auto text=match.hook->fields.at("display").get<std::string>();
    trace.push_back("display:"+text);return text;};
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
  services.logError=[&](const std::string& value,std::exception_ptr){
    trace.push_back("logError:"+value);};
  services.perHook.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.perHook.emitHookStarted=[&](const std::string&,
    const std::string&,const std::string&){trace.push_back("started");};
  services.perHook.emitHookResponse=[&](const Json& value){
    trace.push_back(Json{{"response",value}});};
  services.perHook.execCommandHook=[&](hook::HookCommandCall call){
    const auto name=call.hook.at("display").get<std::string>();
    trace.push_back("exec:"+name);
    const auto row=std::find_if(item.at("hooks").begin(),item.at("hooks").end(),
      [&](const Json& value){return value.at("display")==name;});
    hook::CommandOutcome result;
    result.stdoutText=row->value("stdout",std::string{});
    result.stderrText=row->value("stderr",std::string{});
    result.output="combined";result.status=row->at("status").get<int>();
    event_loop::Deferred<hook::CommandOutcome> output(loop);
    output.resolve(std::move(result));return output;};
  services.perHook.parseHookOutput=[&](const std::string&){
    trace.push_back("parse");return hook::HookParseResult{};};
  services.results.getHookDisplayText=[](const hook::HookIdentity& identity){
    return identity.fields.at("display").get<std::string>();};
  services.results.logForDebugging=[&](const std::string& value){
    trace.push_back("debug:"+value);};
  services.results.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.results.addToTurnHookDuration=[&](double value){
    trace.push_back("duration:"+std::to_string(static_cast<int>(value)));};
  hook::ExecuteHooks execute(loop,input,services);
  std::function<void()> next;
  next=[&]{execute.next().then([&](const hook::HookYield* value,
    const std::exception_ptr& failure){
    if(failure){try{std::rethrow_exception(failure);}
      catch(const std::exception& exception){error=exception.what();}return;}
    if(value->done)return;
    yields.push_back(value->value);next();
  });};
  next();loop.run();
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
