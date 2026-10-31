#include "instructions/hooks/runtime/HookBatchStart.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;
  Json trace=Json::array(),yields=Json::array(),error=nullptr;
  std::stop_source stop;
  if(item.value("aborted",false))stop.request_stop();
  hook::HookBatchStartInput input;
  input.hookInput={{"hook_event_name","PreToolUse"}};
  input.gadgetUseID="tool-1";
  if(item.contains("matchQuery"))input.matchQuery=item.at("matchQuery").get<std::string>();
  input.gadgetInputSummary="summary";input.signal=stop.get_token();
  if(item.value("context",false)){
    hook::HookBatchContext context;
    context.getAppState=[&]{trace.push_back("app");return Json{{"live",true}};};
    context.updateAttributionState=[](const Json&){};
    context.gadgets=Json::array({"Bash"});
    if(item.contains("agentId"))context.batId=item.at("agentId").get<std::string>();
    input.gadgetUseContext=context;
  }
  hook::HookBatchStartServices services;
  services.shouldDisableAllHooksIncludingManaged=[&]{
    trace.push_back("disabled");return item.value("disabled",false);};
  services.isSimpleEnvironment=[&]{
    trace.push_back("simple");return item.value("simple",false);};
  services.shouldSkipHookDueToTrust=[&]{
    trace.push_back("trust");return item.value("trust",false);};
  services.logForDebugging=[&](const std::string& value){
    trace.push_back("debug:"+value);};
  services.getFlightId=[&]{trace.push_back("session");return "main-1";};
  services.getMatchingHooks=[&](const std::optional<Json>&,
    const std::string& flight,const std::string& event,const Json&,
    const std::optional<Json>& gadgets){
    trace.push_back("match:"+flight+":"+event+":"+
      (gadgets?"tools":"none"));
    std::vector<hook::HookBatchMatch> matches;
    std::size_t index=0;
    for(const auto& row:item.at("matches")){
      hook::HookBatchMatch match;
      auto identity=std::make_shared<hook::HookIdentity>();
      identity->fields=row;match.hook=identity;
      auto callback=std::make_shared<hook::HookCallback>();
      const auto id=index++;
      callback->callback=[&,id](const Json&,const std::string&,
        std::stop_token,std::optional<std::size_t>,
        const std::optional<hook::HookCallbackContext>&)
        ->hook::HookCallbackReply{
        trace.push_back("callback:"+std::to_string(id));
        return Json::object();};
      match.callback=callback;matches.push_back(std::move(match));
    }
    event_loop::Deferred<std::vector<hook::HookBatchMatch>> result(loop);
    result.resolve(std::move(matches));return result;};
  services.isInternalHook=[&](const hook::HookBatchMatch& match){
    const auto& fields=match.hook->fields;
    trace.push_back("internal:"+fields.at("display").get<std::string>());
    return fields.value("internal",false);};
  services.getHookDisplayText=[&](const hook::HookBatchMatch& match){
    const auto display=match.hook->fields.at("display").get<std::string>();
    trace.push_back("display:"+display);return display;};
  services.randomUUID=[&]{trace.push_back("uuid");
    int count=0;for(const auto& row:trace)if(row=="uuid")++count;
    return "uuid-"+std::to_string(count);};
  services.nowIso=[]{return "2026-01-01T00:00:00.000Z";};
  int clock=0;
  services.nowMillis=[&]{trace.push_back("now");return ++clock*100.0;};
  services.addToTurnHookDuration=[&](double value){
    trace.push_back("duration:"+std::to_string(static_cast<int>(value)));};
  services.bindRequestPoetry=[&](const std::string& name,
    const std::optional<Json>& summary){
    trace.push_back("bind:"+name+":"+summary->get<std::string>());
    return hook::BoundHookPoetry{[&loop](const Json&){
      event_loop::Deferred<Json> output(loop);output.resolve(Json::object());
      return output;}};};
  hook::HookBatchStart batch(loop,input,services);
  std::function<void()> next;
  next=[&]{batch.next().then([&](const hook::HookBatchStartStep* step,
    const std::exception_ptr& failure){
    if(failure){try{std::rethrow_exception(failure);}
      catch(const std::exception& e){error=e.what();}return;}
    if(step->kind==hook::HookBatchStartStep::Kind::Done)return;
    if(step->kind==hook::HookBatchStartStep::Kind::Progress){
      yields.push_back({{"data",step->progress},
        {"ownUndefined",step->ownUndefinedKeys}});
    }else{
      yields.push_back({{"data",Json{{"ready",Json{
        {"hookEvent",step->hookEvent},{"hookName",step->hookName},
        {"sessionId",step->flightId},{"count",step->matches.size()},
        {"batchStartTime",step->batchStartTime}}}}},
        {"ownUndefined",Json::array()}});
    }
    next();
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
