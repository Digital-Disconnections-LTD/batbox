#include "instructions/hooks/runtime/IndigenousCommandFull.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
struct Fixture {
  event_loop::Loop loop;
  std::vector<std::string> events;
  hook::AsyncHookRoster roster;
  hook::IndigenousCommandConfig config;
  hook::FullCommandServices services;
  Fixture():roster(loop,[this]{
    hook::AsyncHookServices value;
    value.nowMillis=[](){return 123.0;};
    value.logDebug=[this](const std::string& text,const std::string&){
      events.push_back("registry:"+text);};
    value.startProgressInterval=[](const std::string&,const std::string&,
      const std::string&,std::function<event_loop::Deferred<Json>()>){return []{};};
    value.emitHookResponse=[this](const hook::AsyncHookResponseEvent& value){
      events.push_back("emit:"+value.output);};
    value.invalidateFlightEnvStash=[]{return;};
    value.stringifyThrown=[](std::exception_ptr){return std::string("Error");};
    return value;
  }()){
    config.shellPath="/bin/sh";
    config.getProjectRoot=[](){return std::string("/tmp");};
    config.getCwd=[](){return std::string("/tmp");};
    config.getOriginalCwd=[](){return std::string("/tmp");};
    config.subprocessEnv=[](){return std::map<std::string,std::string>{{"PATH","/usr/bin:/bin"}};};
    config.pathExists=[this](const std::string&){
      event_loop::Deferred<bool> result(loop);result.resolve(true);return result;};
    config.shellPrefix=[](){return std::optional<std::string>{};};
    config.logDebug=[this](const std::string& text,const std::string&){
      events.push_back("debug:"+text);};
    config.logDiagnostics=[this](const std::string& name,const Json&){
      events.push_back("diag:"+name);};
    config.nowMillis=[](){return 123.0;};
    services.roster=&roster;
    services.parseJson=[](const std::string& text){return value::parse(text);};
    services.stringifyThrown=[](std::exception_ptr error){
      try{std::rethrow_exception(error);}catch(const std::exception& value){
        return std::string(value.what());}
    };
    services.startProgressInterval=[this](const std::string&,
      const std::string&,const std::string&,
      std::function<event_loop::Deferred<Json>()>){
      events.push_back("progress:start");
      return [this]{events.push_back("progress:stop");};};
    services.background.yieldToIo=[this]{
      event_loop::Deferred<event_loop::Unit> result(loop);
      loop.postMission([result]{result.resolve({});});return result;};
    services.background.emitHookResponse=[this](const hook::AsyncHookResponseEvent& value){
      events.push_back("rewake:"+value.output);};
    services.background.wrapInSystemReminder=[](const std::string& value){
      return "<"+value+">";};
    services.background.enqueuePendingNotification=[this](const std::string& value,
      const std::string& mode){events.push_back("notify:"+mode+":"+value);};
    services.background.onDetachedError=[](std::exception_ptr error){
      std::rethrow_exception(error);};
  }
  hook::CommandOutcome run(const Json& definition,
    std::function<event_loop::Deferred<Json>(const Json&)> requestPoetry={}){
    auto call=hook::indigenousFullCommandHook(loop,config,services);
    hook::FullCommandRequest request;
    request.base.hook=definition;
    request.base.hookEvent="InstructionsLoaded";
    request.base.hookName="sample";
    request.base.jsonInput="{\"input\":1}";
    request.base.uuid="hook-1";
    request.requestPoetry=std::move(requestPoetry);
    hook::CommandOutcome result;
    bool settled=false;
    call(std::move(request)).then([&](const hook::CommandOutcome* value,
      const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      result=*value;settled=true;});
    loop.run();assert(settled);return result;
  }
};
}
int main(){
  {
    Fixture fixture;
    auto result=fixture.run({{"type","command"},
      {"command","read line; printf 'got:%s\\n' \"$line\"; printf 'err\\n' >&2"}});
    assert(result.status==0&&!result.backgrounded);
    assert(result.stdoutText=="got:{\"input\":1}\n");
    assert(result.stderrText=="err\n");
  }
  {
    Fixture fixture;
    auto result=fixture.run({{"type","command"},{"async",true},
      {"command","sleep 0.03; printf 'later\\n'"}});
    assert(result.status==0&&result.backgrounded);
    assert(fixture.roster.size()==1);
    std::vector<hook::CheckedAsyncHookResponse> responses;
    fixture.roster.checkForAsyncHookResponses().then(
      [&](const auto* values,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);responses=*values;});
    fixture.loop.run();
    assert(responses.size()==1&&responses[0].stdoutText=="later\n");
  }
  {
    Fixture fixture;
    auto result=fixture.run({{"type","command"},
      {"command","printf '{\"async\":true}\\n'; sleep 0.03; printf 'body\\n'"}});
    assert(result.status==0&&!result.backgrounded);
    assert(result.stdoutText.find("{\"async\":true}")==0);
    assert(fixture.roster.size()==1);
  }
  {
    Fixture fixture;
    auto poetry=[&](const Json& request){
      assert(request.at("prompt")=="p1");
      event_loop::Deferred<Json> result(fixture.loop);
      result.resolve({{"prompt_response","p1"},{"selected","yes"}});
      return result;};
    auto result=fixture.run({{"type","command"},
      {"command","read line; printf '{\"prompt\":\"p1\",\"message\":\"Choose\",\"options\":[]}\\n'; read answer; printf '%s\\n' \"$answer\""}},poetry);
    assert(result.status==0);
    assert(result.stdoutText=="{\"prompt_response\":\"p1\",\"selected\":\"yes\"}\n");
    assert(result.output.find("\"prompt\":\"p1\"")!=std::string::npos);
  }
  {
    Fixture fixture;
    auto result=fixture.run({{"type","command"},{"asyncRewake",true},
      {"command","printf 'blocked\\n' >&2; exit 2"}});
    assert(result.backgrounded);
    bool notification=false;
    for(const auto& event:fixture.events)
      if(event.starts_with("notify:task-notification:"))notification=true;
    assert(notification);
    assert(fixture.roster.size()==0);
  }
  std::cout<<"native full hook command sync, config async, dynamic async, prompt and rewake pass\n";
}
