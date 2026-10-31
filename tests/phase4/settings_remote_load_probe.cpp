#include "settings/remote/FetchAndLoad.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <system_error>

int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();
    SyncStashServices stashServices;
    stashServices.configHome=[](){return "/config";};
    stashServices.readFileSync=[](const std::string&)->std::string{
      throw std::runtime_error("ENOENT");};
    stashServices.resetMergedSettingsStash=[](){};
    SyncStash stash(stashServices);
    stash.setEligibility(true);
    if(test.contains("cached")&&!test["cached"].is_null())
      stash.setFlightStash(test["cached"]);
    FetchAndLoadServices services;
    services.stash=&stash;
    services.isEligible=[&](){return test.value("eligible",true);};
    services.fetchWithRetry=[&](const std::optional<std::string>& checksum){
      trace.push_back({"fetch",checksum?Json(*checksum):Json(nullptr)});
      event_loop::Deferred<FetchResult> done(loop);
      if(test.value("fetchError",false))
        done.reject(std::make_exception_ptr(std::runtime_error("fetch failed")));
      else{
        const auto row=test.value("fetched",Json{{"success",false},
          {"error","failed"}});
        FetchResult result;
        result.success=row.value("success",false);
        if(row.contains("settings"))result.settings=row["settings"];
        if(row.contains("checksum")&&!row["checksum"].is_null())
          result.checksum=row["checksum"].get<std::string>();
        if(row.contains("error")&&!row["error"].is_null())
          result.error=row["error"].get<std::string>();
        done.resolve(std::move(result));
      }
      return done;
    };
    services.checkSecurity=[&](const std::optional<Json>& oldValue,
        const Json& next){
      trace.push_back({"security",oldValue?*oldValue:Json(nullptr),next});
      event_loop::Deferred<SecurityDecision> done(loop);
      if(test.value("securityError",false))
        done.reject(std::make_exception_ptr(
          std::runtime_error("security failed")));
      else{
        const auto decision=test.value("decision",std::string("no_check_needed"));
        done.resolve(decision=="rejected"?SecurityDecision::Rejected:
          decision=="approved"?SecurityDecision::Approved:
          SecurityDecision::NoCheckNeeded);
      }
      return done;
    };
    services.handleSecurity=[&](SecurityDecision decision){
      const auto value=decision==SecurityDecision::Rejected?"rejected":
        decision==SecurityDecision::Approved?"approved":"no_check_needed";
      trace.push_back({"handle",value});
      return decision!=SecurityDecision::Rejected;
    };
    services.saveSettings=[&](const Json& next){
      trace.push_back({"save",next});
      event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
      return done;
    };
    services.unlinkSettings=[&](const std::string& path){
      trace.push_back({"unlink",path});
      event_loop::Deferred<event_loop::Unit> done(loop);
      if(test.contains("unlinkError")){
        const int code=test["unlinkError"]=="ENOENT"?ENOENT:EIO;
        done.reject(std::make_exception_ptr(std::system_error(code,
          std::generic_category(),"unlink failed")));
      }else done.resolve({});
      return done;
    };
    services.debug=[&](const std::string& value){
      trace.push_back({"debug",value});};
    bool complete=false;
    Json result=nullptr;
    fetchAndLoadRemoteManagedSettings(loop,services).then([&](
        const std::optional<Json>* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      if(*value)result=**value;
      complete=true;
    });
    for(int n=0;n<100&&!complete;n++)loop.runReady();
    if(!complete)throw std::runtime_error("Remote load did not settle");
    const auto flight=stash.getRemoteManagedSettingsSyncFromStash();
    std::cout<<value::stringify(Json{{"result",result},
      {"session",flight?*flight:Json(nullptr)},
      {"trace",trace}})<<'\n';
  }
}
