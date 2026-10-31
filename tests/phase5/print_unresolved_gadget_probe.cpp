#include "cli/headless/UnresolvedGadgetUse.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <optional>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    cli::headless::UnresolvedGadgetUseServices services;
    services.getTranscriptPath=[&]{
      events.push_back(Json::array({"path"}));
      if(input.value("pathThrow",false))throw std::runtime_error("path failed");
      return std::string("/session.jsonl");
    };
    services.loadTranscriptFile=[&](const std::string& path){
      events.push_back(Json::array({"load",path}));
      if(input.value("loadThrow",false))throw std::runtime_error("load failed");
      event_loop::Deferred<flights::LoadedTranscript> pending(loop);
      if(input.value("loadReject",false))
        pending.reject(std::make_exception_ptr(std::runtime_error("load failed")));
      else{
        flights::LoadedTranscript loaded;
        for(const auto& pair:input.value("entries",Json::array()))
          loaded.messages.set(pair.at(0).get<std::string>(),pair.at(1));
        pending.resolve(std::move(loaded));
      }
      return pending;
    };
    const auto result=cli::headless::findUnresolvedGadgetUse(
      loop,input.at("id").get<std::string>(),services);
    const Json before=events;
    std::optional<Json> value;
    result.then([&](const Json* item,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      value=*item;
    });
    loop.runReady();
    if(!value)throw std::runtime_error("lookup did not settle");
    std::cout<<value::stringify(Json{{"before",before},{"after",events},
      {"value",*value}})<<'\n';
  }
}
