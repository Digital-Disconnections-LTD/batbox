#include "memory_prefetch/Prefetch.hpp"
#include <iostream>

using namespace batbox;
namespace {
Json run(const Json& input){
  event_loop::Loop loop;
  memory_prefetch::Services services{loop};
  Json trace=Json::array();
  services.defaultStandardModel=[&]{trace.push_back(Json::array({"model"}));
    return std::string("standard");};
  services.sideQuery=[&](const Json& request,std::stop_token){
    trace.push_back(Json::array({"sideQuery",request}));
    event_loop::Deferred<Json> result(loop);
    const auto& error=value::property(input,"error");
    if(error.is_string())result.reject(std::make_exception_ptr(
      std::runtime_error(error.get<std::string>())));
    else result.resolve(input.value("response",Json{{"content",Json::array()}}));
    return result;
  };
  services.logWarning=[&](const std::string& value){
    trace.push_back(Json::array({"warning",value}));};
  std::vector<instructions::MemoryHeader> memories;
  for(const Json& value:input.at("memories")){
    instructions::MemoryHeader item;
    item.filename=value.at("filename").get<std::string>();
    item.filePath=value.at("filePath").get<std::string>();
    item.mtimeMs=value.at("mtimeMs").get<double>();
    item.description=value::property(value,"description");
    const auto& type=value::property(value,"type");
    if(type.is_string())item.type=type.get<std::string>();
    memories.push_back(std::move(item));
  }
  std::vector<std::string> recent;
  const auto& gadgets=value::property(input,"recentTools");
  if(gadgets.is_array())for(const Json& gadget:gadgets)
    recent.push_back(gadget.get<std::string>());
  std::stop_source stop;
  if(input.value("aborted",false))stop.request_stop();
  auto pending=memory_prefetch::selectRelevantMemories(
    input.at("query").get<std::string>(),memories,recent,stop.get_token(),services);
  Json selected=Json::array();
  std::exception_ptr failure;
  pending.then([&](const std::vector<std::string>* value,const std::exception_ptr& error){
    failure=error;
    if(value)for(const auto& name:*value)selected.push_back(name);
  });
  loop.runReady();
  if(failure)std::rethrow_exception(failure);
  return {{"selected",selected},{"trace",trace}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
