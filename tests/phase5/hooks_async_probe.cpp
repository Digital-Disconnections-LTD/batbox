#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;
  Json logs=Json::array();
  hook::AsyncHookServices services;
  services.nowMillis=[](){return 123.0;};
  services.logDebug=[&](const std::string& message,const std::string& level){
    logs.push_back(level+":"+message);};
  services.startProgressInterval=[&](const std::string& id,const std::string&,
    const std::string&,std::function<event_loop::Deferred<Json>()>){
    logs.push_back("progress:"+id);
    return [&,id]{logs.push_back("stop:"+id);};
  };
  services.emitHookResponse=[&](const hook::AsyncHookResponseEvent& event){
    Json value={{"hookId",event.hookId},{"hookName",event.hookName},
      {"hookEvent",event.hookEvent},{"output",event.output},
      {"stdout",event.stdoutText},{"stderr",event.stderrText},
      {"exitCode",event.exitCode},{"outcome",event.outcome}};
    logs.push_back("emit:"+value::stringify(value));
  };
  services.invalidateFlightEnvStash=[&]{logs.push_back("invalidate-env");};
  services.stringifyThrown=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string("Error: ")+value.what();}
  };
  hook::AsyncHookRoster roster(loop,std::move(services));
  for(const auto& entry:item.at("hooks")){
    const auto id=entry.at("id").get<std::string>();
    auto status=std::make_shared<std::string>(entry.at("status").get<std::string>());
    auto process=std::make_shared<hook::AsyncHookProcess>();
    process->status=[status]{return *status;};
    process->getStdout=[&,entry]{
      event_loop::Deferred<std::string> value(loop);
      value.resolve(entry.at("stdout").get<std::string>());return value;};
    process->getStderr=[entry]{return entry.value("stderr",std::string{});};
    process->resultCode=[&,entry]{
      event_loop::Deferred<int> value(loop);value.resolve(entry.value("code",0));
      return value;};
    process->cleanup=[&,id]{logs.push_back("cleanup:"+id);};
    process->kill=[&,id,status]{logs.push_back("kill:"+id);*status="killed";};
    hook::AsyncHookRegistration registration;
    registration.processId=id;registration.hookId="hook-"+id;
    registration.hookName="name-"+id;
    registration.hookEvent=entry.value("event",std::string("InstructionsLoaded"));
    registration.command="echo "+id;
    registration.asyncResponse={{"async",true}};
    if(entry.contains("timeout"))
      registration.asyncResponse["asyncTimeout"]=entry.at("timeout");
    registration.process=process;
    roster.registerPendingAsyncHook(registration);
  }
  Json responses=Json::array();
  const auto phase=item.at("phase").get<std::string>();
  if(phase=="poll")roster.checkForAsyncHookResponses().then(
    [&](const std::vector<hook::CheckedAsyncHookResponse>* values,
      const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      for(const auto& value:*values){
        Json row={{"processId",value.processId},{"response",value.response},
          {"hookName",value.hookName},{"hookEvent",value.hookEvent},
          {"stdout",value.stdoutText},{"stderr",value.stderrText},
          {"exitCode",value.exitCode}};
        if(value.gadgetName)row["toolName"]=*value.gadgetName;
        if(value.pluginId)row["pluginId"]=*value.pluginId;
        responses.push_back(std::move(row));
      }
    });
  else if(phase=="finalize")roster.finalizePendingAsyncHooks().then(
    [](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);});
  loop.run();
  Json pending=Json::array();
  for(const auto& value:roster.getPendingAsyncHooks())pending.push_back(value.processId);
  return {{"name",item.at("name")},{"responses",responses},
    {"pendingIds",pending},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
