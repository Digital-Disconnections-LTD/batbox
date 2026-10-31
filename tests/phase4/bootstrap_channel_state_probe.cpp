#include "bootstrap/ChannelState.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
batbox::bootstrap::ChannelEntry readEntry(const Json& data){
  batbox::bootstrap::ChannelEntry entry;
  entry.kind=data["kind"].get<std::string>();
  entry.name=data["name"].get<std::string>();
  if(data.contains("marketplace"))
    entry.marketplace=data["marketplace"].get<std::string>();
  if(data.contains("dev"))entry.dev=data["dev"].get<bool>();
  return entry;
}
Json writeEntry(const batbox::bootstrap::ChannelEntry& entry){
  Json out={{"kind",entry.kind},{"name",entry.name}};
  if(entry.marketplace)out["marketplace"]=*entry.marketplace;
  if(entry.dev)out["dev"]=*entry.dev;
  return out;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::ChannelState state;
    std::shared_ptr<std::vector<bootstrap::ChannelEntry>> channels;
    bool channelsSet=false;
    auto snapshot=[&](){
      Json entries=Json::array();
      for(const auto& entry:*bootstrap::getAllowedChannels(state))
        entries.push_back(writeEntry(entry));
      const auto& bat=bootstrap::getMainThreadBatType(state);
      return Json{{"agentType",bat?Json(*bat):Json(nullptr)},
        {"agentTypeSet",bat.has_value()},
        {"remote",bootstrap::getIsRemoteMode(state)},
        {"channels",entries},
        {"sameChannels",channelsSet?
          Json(bootstrap::getAllowedChannels(state)==channels):Json(nullptr)},
        {"hasDev",bootstrap::getHasDevChannels(state)}};
    };
    Json trace=Json::array();trace.push_back(snapshot());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="agent"){
        std::optional<std::string> bat;
        if(!op["value"].is_null())bat=op["value"].get<std::string>();
        bootstrap::setMainThreadBatType(state,bat);
      }else if(kind=="remote")
        bootstrap::setIsRemoteMode(state,op["value"].get<bool>());
      else if(kind=="channels"){
        channelsSet=true;
        channels=std::make_shared<std::vector<bootstrap::ChannelEntry>>();
        for(const auto& item:op["value"])
          channels->push_back(readEntry(item));
        bootstrap::setAllowedChannels(state,channels);
      }else if(kind=="appendChannel")
        channels->push_back(readEntry(op["value"]));
      else if(kind=="hasDev")
        bootstrap::setHasDevChannels(state,op["value"].get<bool>());
      else throw std::runtime_error("Unknown channel-state operation");
      trace.push_back(snapshot());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
