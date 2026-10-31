#include "auth/recovery/Handle401.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();
    auth::recovery::Services services;
    services.clearOAuthTokenStash=[&](){trace.push_back("clear");};
    services.readTokensAsync=[&](){
      trace.push_back("read");
      event_loop::Deferred<std::optional<Json>> output(loop);
      if(test.value("readError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("read failed")));
      else if(test.contains("tokens")&&!test["tokens"].is_null())
        output.resolve(test["tokens"]);
      else output.resolve(std::nullopt);
      return output;
    };
    services.checkAndRefresh=[&](int retry,bool force){
      trace.push_back(Json::array({"refresh",retry,force}));
      event_loop::Deferred<bool> output(loop);
      if(test.value("refreshError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("refresh failed")));
      else output.resolve(test.value("refreshResult",false));
      return output;
    };
    auth::recovery::Handle401 handler(loop,services);
    std::vector<std::string> failed;
    if(test.contains("failedTokens"))
      failed=test["failedTokens"].get<std::vector<std::string>>();
    else failed={"failed","failed"};
    std::vector<event_loop::Deferred<bool>> promises;
    for(const auto& token:failed)promises.push_back(handler.handle(token));
    const bool same=promises.size()>1&&promises[0].samePromise(promises[1]);
    Json values=Json::array();
    std::size_t done=0;
    const auto observe=[&](event_loop::Deferred<bool> promise){
      promise.then([&](const bool* value,const std::exception_ptr& error){
        if(error){
          try{std::rethrow_exception(error);}
          catch(const std::exception& caught){
            values.push_back(Json{{"error",caught.what()}});}
        }else values.push_back(Json{{"value",*value}});
        ++done;
      });
    };
    for(auto promise:promises)observe(promise);
    for(int attempt=0;attempt<100&&done<promises.size();++attempt)
      loop.runReady();
    if(done!=promises.size())throw std::runtime_error("401 did not settle");
    if(test.value("secondRound",false)){
      observe(handler.handle(failed[0]));
      for(int attempt=0;attempt<100&&done<promises.size()+1;++attempt)
        loop.runReady();
      if(done!=promises.size()+1)
        throw std::runtime_error("Second 401 did not settle");
    }
    std::cout<<value::stringify(Json{{"same",same},
      {"values",values},{"trace",trace}})<<'\n';
  }
}
