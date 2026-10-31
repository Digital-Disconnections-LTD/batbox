#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
namespace hooks=batbox::instructions::hooks;
namespace hook_runtime=batbox::instructions::hooks::runtime;
namespace {
hook_runtime::HookKind kind(const std::string& value){
  if(value=="callback")return hook_runtime::HookKind::Callback;
  if(value=="prompt")return hook_runtime::HookKind::Poetry;
  if(value=="agent")return hook_runtime::HookKind::Bat;
  if(value=="function")return hook_runtime::HookKind::Function;
  if(value=="http")return hook_runtime::HookKind::Http;
  return hook_runtime::HookKind::Command;
}
std::string failure(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}catch(const std::exception& value){return value.what();}
}
Json evaluate(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();std::size_t nextId=0;
  std::stop_source stop;if(item.value("aborted",false))stop.request_stop();
  hook_runtime::ExecutorServices services;
  services.simpleMode=[&]{return item.value("simple",false);};
  services.allHooksDisabled=[&]{logs.push_back("disabled?");return item.value("disabled",false);};
  services.skipDueToTrust=[&]{logs.push_back("trust?");return item.value("untrusted",false);};
  if(item.value("hasAppState",false))services.getAppState=[&]{
    logs.push_back("app-state");return Json{{"ready",true}};};
  services.getFlightId=[&]{logs.push_back("session");return std::string("session-1");};
  services.getMatchingHooks=[&](const std::string&,const std::string&,const Json&){
    logs.push_back("matching");
    std::vector<hook_runtime::MatchedHook> matched;
    for(const auto& entry:item.at("hooks")){
      hook_runtime::MatchedHook row;row.kind=kind(entry.at("type").get<std::string>());
      row.hook=entry;
      if(row.kind==hook_runtime::HookKind::Callback){
        row.callback=[&,entry](hook_runtime::CallbackRequest request){
          logs.push_back("callback:"+request.uuid+":"+std::to_string(request.index));
          event_loop::Deferred<Json> result(loop);
          if(entry.contains("callbackError"))result.reject(std::make_exception_ptr(
            std::runtime_error(entry.at("callbackError").get<std::string>())));
          else result.resolve(entry.value("callbackResult",Json::object()));
          return result;};
      }
      matched.push_back(std::move(row));
    }
    event_loop::Deferred<std::vector<hook_runtime::MatchedHook>> result(loop);
    result.resolve(std::move(matched));return result;};
  services.getMatchingHooksWithAppState=[&](const Json& state,const std::string& flight,
    const std::string& event,const Json& input){
    if(state!=Json{{"ready",true}})throw std::runtime_error("unexpected app state");
    return services.getMatchingHooks(flight,event,input);};
  services.randomUUID=[&]{return "uuid-"+std::to_string(++nextId);};
  services.execCommandHook=[&](hook_runtime::CommandRequest request){
    const auto command=request.hook.at("command").get<std::string>();
    logs.push_back("command:"+command+":"+request.uuid+":"+std::to_string(request.index));
    event_loop::Deferred<hook_runtime::CommandOutcome> result(loop);
    if(request.hook.contains("execError"))result.reject(std::make_exception_ptr(
      std::runtime_error(request.hook.at("execError").get<std::string>())));
    else{
      const auto value=request.hook.value("result",Json::object());
      result.resolve({value.value("stdout",std::string{}),
        value.value("stderr",std::string{}),value.value("output",std::string{}),
        value.value("status",0),value.value("aborted",false)});
    }
    return result;};
  services.execHttpHook=[&](hook_runtime::HttpRequest request){
    const auto url=request.hook.at("url").get<std::string>();
    logs.push_back("http:"+url);
    event_loop::Deferred<hook_runtime::HttpOutcome> result(loop);
    if(request.hook.contains("httpError"))result.reject(std::make_exception_ptr(
      std::runtime_error(request.hook.at("httpError").get<std::string>())));
    else{
      const auto value=request.hook.value("httpResult",Json::object());
      hook_runtime::HttpOutcome response;
      response.aborted=value.value("aborted",false);
      response.ok=value.value("ok",true);
      if(value.contains("statusCode"))response.statusCode=value.at("statusCode").get<int>();
      response.body=value.value("body",std::string("{}"));
      if(value.contains("error"))response.error=value.at("error").get<std::string>();
      result.resolve(std::move(response));
    }
    return result;};
  auto parse=[&](const std::string& body){
    hook_runtime::ParsedOutput result;
    if(item.contains("validationError")&&body=="{bad}"){
      result.validationError=item.at("validationError").get<std::string>();
    }else if(body.starts_with("{"))result.json=value::parse(body);
    return result;};
  services.parseHookOutput=parse;services.parseHttpHookOutput=parse;
  services.logDebug=[&](const std::string& text,const std::string& level){
    logs.push_back(level+":"+text);};
  services.logError=[&](const std::string& text){logs.push_back("error:"+text);};
  hooks::HookDispatch dispatch;
  dispatch.hookInput={{"hook_event_name",item.value("event",std::string("InstructionsLoaded"))},
    {"file_path","/work/INSTRUCTIONS.md"},{"memory_type","Project"},
    {"load_reason","session_start"}};
  dispatch.matchQuery="session_start";dispatch.timeoutMs=1200;
  auto pending=hook_runtime::executeHooksOutsideRepl(loop,dispatch,services,stop.get_token());
  Json result;bool done=false;
  pending.then([&](const std::vector<hook_runtime::HookResult>* values,
    const std::exception_ptr& error){
    if(error)result={{"name",item.at("name")},{"error",failure(error)},{"logs",logs}};
    else{Json rows=Json::array();for(const auto& row:*values)rows.push_back(row.toJson());
      result={{"name",item.at("name")},{"result",rows},{"logs",logs}};}
    done=true;});
  loop.run();
  if(!done)throw std::runtime_error("Outside-REPL executor pending");
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json output=Json::array();
  for(const auto& item:value::parse(data))output.push_back(evaluate(item));
  std::cout<<value::stringify(output)<<'\n';
}
