#include "auth/refresh/Invalidate.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();std::size_t next=0;
    auth::refresh::InvalidateServices services;
    services.configHome=[&](){trace.push_back("home");
      return std::string("/config");};
    services.statMtimeMs=[&](const std::string& path){
      trace.push_back(Json::array({"join","/config",
        ".batbox-credentials.json"}));
      trace.push_back(Json::array({"stat",path}));
      event_loop::Deferred<double> output(loop);
      const auto value=test["mtimes"][next++];
      if(value=="ERROR")output.reject(std::make_exception_ptr(
        std::runtime_error("missing")));
      else output.resolve(value.get<double>());
      return output;
    };
    services.clearFullStash=[&](){trace.push_back("clearFull");};
    services.clearMemoOnly=[&](){trace.push_back("clearMemo");};
    auth::refresh::DiskInvalidation invalidation(loop,services);
    for(const auto& ignored:test["mtimes"]){
      (void)ignored;
      bool done=false;
      invalidation.invalidateIfDiskChanged().then([&](
          const event_loop::Unit*,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        done=true;
      });
      for(int attempt=0;attempt<100&&!done;++attempt)loop.runReady();
      if(!done)throw std::runtime_error("Invalidation did not settle");
    }
    std::cout<<value::stringify(Json{{"trace",trace}})<<'\n';
  }
}
