#include "diagnostics/BufferedWriter.hpp"
#include "runtime/JsonValue.hpp"
#include <deque>
#include <iostream>
#include <map>

int main(){
  using batbox::Json;
  const auto input=Json::parse(std::cin);
  std::map<int,std::function<void()>> timers;
  std::deque<std::function<void()>> immediates;
  int nextTimer=1;
  Json writes=Json::array(),out=Json::array();
  batbox::diagnostics::BufferedWriterServices services;
  services.writeFn=[&](const std::string& value){writes.push_back(value);};
  services.setTimeout=[&](double,std::function<void()> callback){
    const auto id=nextTimer++;timers[id]=std::move(callback);
    return [&,id]{timers.erase(id);};
  };
  services.setImmediate=[&](std::function<void()> callback){
    immediates.push_back(std::move(callback));};
  const auto& options=input["options"];
  if(options.contains("flushIntervalMs"))services.flushIntervalMs=
    options["flushIntervalMs"].get<double>();
  if(options.contains("maxBufferSize"))services.maxBufferSize=
    options["maxBufferSize"].get<std::size_t>();
  if(options.contains("maxBufferBytes"))services.maxBufferBytes=
    options["maxBufferBytes"].get<double>();
  if(options.contains("immediateMode"))services.immediateMode=
    options["immediateMode"].get<bool>();
  batbox::diagnostics::BufferedWriter writer(services);
  for(const auto& action:input["actions"]){
    const auto kind=action["kind"].get<std::string>();
    if(kind=="write")writer.write(action["value"].get<std::string>());
    if(kind=="flush")writer.flush();
    if(kind=="dispose")writer.dispose();
    if(kind=="timer"&&!timers.empty()){
      auto current=timers.begin();auto callback=std::move(current->second);
      timers.erase(current);callback();
    }
    if(kind=="immediate"&&!immediates.empty()){
      auto callback=std::move(immediates.front());immediates.pop_front();callback();
    }
    out.push_back({{"writes",writes},{"timers",timers.size()},
      {"immediates",immediates.size()}});
  }
  std::cout<<out.dump()<<'\n';
}
