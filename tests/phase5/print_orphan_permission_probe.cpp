#include "cli/headless/OrphanedPermission.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  auto state=std::make_shared<cli::headless::OrphanedPermissionState>();
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=value::parse(line);
    Json events=Json::array();
    cli::headless::OrphanedPermissionServices services;
    services.logForDebugging=[&](const std::string& value){
      events.push_back(Json::array({"debug",value}));
    };
    services.findUnresolvedGadgetUse=[&](const std::string& id){
      events.push_back(Json::array({"find",id}));
      event_loop::Deferred<Json> result(loop);
      if(input.value("findError",false))
        result.reject(std::make_exception_ptr(
          std::runtime_error("find failed")));
      else result.resolve(value::property(input,"assistant"));
      return result;
    };
    services.enqueue=[&](const Json& value){
      events.push_back(Json::array({"enqueue",value}));
    };
    if(input.value("onEnqueued",true))
      services.onEnqueued=[&]{
        events.push_back(Json::array({"onEnqueued"}));
      };
    auto result=cli::headless::handleOrphanedPermissionResponse(
      loop,input.at("message"),state,services);
    const auto before=events;
    std::optional<bool> value;
    std::optional<std::string> error;
    result.then([&](const bool* resolved,const std::exception_ptr& cause){
      if(cause){try{std::rethrow_exception(cause);}
        catch(const std::exception& ex){error=ex.what();}}
      else if(resolved)value=*resolved;
    });
    loop.runReady();
    Json handled=Json::array();
    // Compare membership for every ID encountered in the fixture. Set order
    // is not part of this handler's output contract.
    for(const auto& id:{"u1","u2","u3"})
      if(state->handledGadgetUseIds.contains(id))handled.push_back(id);
    std::cout<<value::stringify(Json{{"before",before},{"after",events},
      {"value",value?Json(*value):Json()},
      {"error",error?Json(*error):Json()},
      {"handled",handled}})<<'\n';
  }
}
