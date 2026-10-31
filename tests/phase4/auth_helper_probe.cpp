#include "auth/helper/Helper.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    Json state=value::parse(line),trace=Json::array(),values=Json::array();
    event_loop::Loop loop;
    std::vector<event_loop::Deferred<auth::helper::CommandResult>> pending;
    std::vector<event_loop::Deferred<std::optional<std::string>>> started;
    const auto field=[&](const char* key)->Json{
      return state.contains(key)?state[key]:Json(nullptr);};
    const auto setting=[&](const char* key)->Json{
      auto value=field(key);return value.is_null()?Json::object():value;};
    auth::helper::Services services;
    services.feedbackChannel="Batbox feedback";
    services.configuredHelper=[&](){
      trace.push_back("bare");
      const auto name=field("bare")==true?"flagSettings":"mergedSettings";
      trace.push_back(name==std::string("flagSettings")
        ?Json::array({"source","flagSettings"}):Json("merged"));
      const auto settings=setting(name);
      return settings.contains("apiKeyHelper")
        ?settings["apiKeyHelper"]:Json(nullptr);
    };
    services.settingsForSource=[&](const std::string& source){
      trace.push_back(Json::array({"source",source}));
      if(source=="projectSettings")return setting("projectSettings");
      if(source=="localSettings")return setting("localSettings");
      return Json::object();
    };
    services.environment=[&](const std::string&){
      trace.push_back("ttlEnv");return field("ttlEnv");};
    services.nowMs=[&](){trace.push_back("now");
      return state.value("now",1000.0);};
    services.trustAccepted=[&](){trace.push_back("trust");
      return field("trust")==true;};
    services.trustError=[&](const std::string& message,
        const std::string& detail){
      trace.push_back(Json::array({"trustError",message,detail}));};
    services.consoleError=[&](const std::string& message){
      trace.push_back(Json::array({"console",message}));};
    services.debug=[&](const std::string& message,const std::string& level){
      trace.push_back(Json::array({"debug",message,level}));};
    services.executeShell=[&](const std::string& command,double timeout){
      trace.push_back(Json::array({"exec",command,timeout}));
      event_loop::Deferred<auth::helper::CommandResult> output(loop);
      if(field("defer")==true){pending.push_back(output);return output;}
      if(field("reject")==true)output.reject(std::make_exception_ptr(
        std::runtime_error("spawn failed")));
      else{
        const auto result=field("result");
        auth::helper::CommandResult value;
        value.failed=result.is_object()&&result.value("failed",false);
        value.timedOut=result.is_object()&&result.value("timedOut",false);
        value.exitCode=result.is_object()?result.value("exitCode",0):0;
        value.stdoutText=result.is_object()?result.value("stdout",std::string{}):
          " key ";
        value.stderrText=result.is_object()?result.value("stderr",std::string{}):
          std::string{};
        output.resolve(value);
      }
      return output;
    };
    auth::helper::Stash stash(loop,services);
    const auto await=[&](event_loop::Deferred<std::optional<std::string>> promise){
      bool done=false;
      promise.then([&](const std::optional<std::string>* result,
          const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        values.push_back(*result?Json(**result):Json(nullptr));done=true;
      });
      for(int attempt=0;attempt<100&&!done;++attempt)loop.runReady();
      if(!done)throw std::runtime_error("Helper did not settle");
    };
    for(const auto& action:state["actions"]){
      if(action.is_object()){
        for(auto it=action.begin();it!=action.end();++it)
          state[it.key()]=it.value();
        continue;
      }
      const auto op=action.get<std::string>();
      if(op=="get")await(stash.get(false));
      else if(op=="start")started.push_back(stash.get(false));
      else if(op=="resolve"){
        if(pending.empty())throw std::runtime_error("No pending helper");
        auto promise=pending.front();pending.erase(pending.begin());
        const auto result=field("result");
        auth::helper::CommandResult value;
        value.failed=result.is_object()&&result.value("failed",false);
        value.timedOut=result.is_object()&&result.value("timedOut",false);
        value.exitCode=result.is_object()?result.value("exitCode",0):0;
        value.stdoutText=result.is_object()?result.value("stdout",std::string{}):
          " key ";
        value.stderrText=result.is_object()?result.value("stderr",std::string{}):
          std::string{};
        promise.resolve(value);loop.runReady();
      }else if(op=="awaitAll"){
        Json batch=Json::array();std::size_t done=0;
        for(auto promise:started)promise.then([&](
            const std::optional<std::string>* value,
            const std::exception_ptr& error){
          if(error)std::rethrow_exception(error);
          batch.push_back(*value?Json(**value):Json(nullptr));++done;
        });
        for(int attempt=0;attempt<100&&done<started.size();++attempt)
          loop.runReady();
        if(done!=started.size())throw std::runtime_error("Pending helper");
        values.push_back(batch);
      }
      else if(op=="getNonInteractive")await(stash.get(true));
      else if(op=="cached"){
        const auto value=stash.getStashed();
        values.push_back(value?Json(*value):Json(nullptr));
      }else if(op=="ttl")values.push_back(stash.calculateTTL());
      else if(op=="project")values.push_back(
        stash.isHelperFromProjectOrLocalSettings());
      else if(op=="elapsed")values.push_back(stash.elapsedMs());
      else if(op=="clear")stash.clear();
      else if(op=="prefetch"){
        stash.prefetchIfSafe(false);loop.runReady();
      }else throw std::runtime_error("Unknown helper action");
    }
    std::cout<<value::stringify(Json{{"values",values},{"trace",trace}})
      <<'\n';
  }
}
