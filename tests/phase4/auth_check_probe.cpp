#include "auth/refresh/Check.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();
    std::size_t readIndex=0,lockIndex=0;
    const auto stashed=test.value("cached",Json(nullptr));
    auth::refresh::CheckServices services;
    services.invalidateDiskStash=[&](){trace.push_back("invalidate");
      event_loop::Deferred<event_loop::Unit> output(loop);
      if(test.value("invalidateError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("invalidate failed")));
      else output.resolve({});
      return output;};
    services.stashedTokens=[&]()->std::optional<Json>{
      trace.push_back("cached");
      return stashed.is_null()?std::nullopt:std::optional(stashed);};
    services.clearTokenMemo=[&](){trace.push_back("clearMemo");};
    services.clearKeychainStash=[&](){trace.push_back("clearKeychain");};
    services.readTokensAsync=[&](){trace.push_back("read");
      event_loop::Deferred<std::optional<Json>> output(loop);
      Json value=test.value("fresh",stashed);
      if(test.contains("readTokens")&&
          readIndex<test["readTokens"].size())
        value=test["readTokens"][readIndex++];
      if(value=="ERROR")output.reject(std::make_exception_ptr(
        std::runtime_error("read failed")));
      else output.resolve(value.is_null()?std::nullopt:
        std::optional(value));
      return output;};
    services.nowMs=[&](){trace.push_back("now");
      return test.value("now",1000.0);};
    services.random=[&](){trace.push_back("random");return .25;};
    services.configHome=[&](){trace.push_back("home");
      return std::string("/config");};
    services.mkdirRecursive=[&](const std::string& path){
      trace.push_back(Json::array({"mkdir",path}));
      event_loop::Deferred<event_loop::Unit> output(loop);
      if(test.value("mkdirError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("mkdir failed")));
      else output.resolve({});
      return output;};
    services.lockDirectory=[&](const std::string& path){
      trace.push_back(Json::array({"lock",path}));
      event_loop::Deferred<auth::refresh::ReleaseLock> output(loop);
      std::string outcome="ok";
      if(test.contains("locks")&&lockIndex<test["locks"].size())
        outcome=test["locks"][lockIndex++].get<std::string>();
      if(outcome!="ok")output.reject(std::make_exception_ptr(
        auth::refresh::LockError(outcome,outcome=="ELOCKED"
          ?"locked":"lock failed")));
      else output.resolve([&](){trace.push_back("release");
        event_loop::Deferred<event_loop::Unit> released(loop);
        if(test.value("releaseError",false))released.reject(
          std::make_exception_ptr(std::runtime_error("release failed")));
        else released.resolve({});
        return released;});
      return output;};
    services.sleep=[&](double delay){
      trace.push_back(Json::array({"sleep",delay}));
      event_loop::Deferred<event_loop::Unit> output(loop);
      output.resolve({});return output;};
    services.refreshToken=[&](const std::string& token,
        const std::optional<std::vector<std::string>>& scopes){
      trace.push_back(Json::array({"refresh",token,
        scopes?Json(*scopes):Json(nullptr)}));
      event_loop::Deferred<Json> output(loop);
      if(test.value("refreshError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("refresh failed")));
      else output.resolve(test.value("refreshed",
        Json{{"accessToken","new"}}));
      return output;};
    services.saveTokens=[&](const Json& value){
      trace.push_back(Json::array({"save",value}));
      if(test.value("saveError",false))
        throw std::runtime_error("save failed");};
    services.logError=[&](const std::string& message){
      trace.push_back(Json::array({"error",message}));};
    auth::refresh::CheckController check(loop,services);
    std::vector<event_loop::Deferred<bool>> promises;
    const int count=test.value("parallel",1);
    for(int i=0;i<count;++i)
      promises.push_back(check.check(test.value("retryCount",0),
        test.value("force",false)));
    const bool same=count>1&&promises[0].samePromise(promises[1]);
    Json values=Json::array();std::size_t done=0;
    for(auto promise:promises)promise.then([&](const bool* value,
        const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& caught){
          values.push_back(Json{{"error",caught.what()}});}
      }else values.push_back(Json{{"value",*value}});
      ++done;
    });
    for(int attempt=0;attempt<1000&&done<promises.size();++attempt)
      loop.runReady();
    if(done!=promises.size())
      throw std::runtime_error("OAuth check did not settle");
    std::cout<<value::stringify(Json{{"same",same},
      {"values",values},{"trace",trace}})<<'\n';
  }
}
