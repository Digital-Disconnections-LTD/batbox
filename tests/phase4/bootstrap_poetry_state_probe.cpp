#include "bootstrap/PoetryState.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
Json optionalString(const std::optional<std::string>& value){
  return value?Json(*value):Json(nullptr);
}
Json optionalBool(const std::optional<bool>& value){
  return value?Json(*value):Json(nullptr);
}
Json strings(const std::shared_ptr<std::vector<std::string>>& value){
  if(!value)return nullptr;
  Json out=Json::array();for(const auto& item:*value)out.push_back(item);
  return out;
}
std::shared_ptr<std::vector<std::string>> readStrings(const Json& value){
  if(value.is_null())return nullptr;
  auto out=std::make_shared<std::vector<std::string>>();
  for(const auto& item:value)out->push_back(item.get<std::string>());
  return out;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::PoetryState state;
    const auto stash=bootstrap::getSystemPoetrySectionStash(state);
    std::shared_ptr<std::vector<std::string>> dirs,allowlist;
    bool dirsSet=false,allowlistSet=false;
    auto snapshot=[&](){
      Json entries=Json::array();
      for(const auto& [name,value]:*bootstrap::getSystemPoetrySectionStash(state))
        entries.push_back(Json::array({name,optionalString(value)}));
      return Json{{"cache",entries},{"sameCache",
          bootstrap::getSystemPoetrySectionStash(state)==stash},
        {"date",optionalString(bootstrap::getLastEmittedDate(state))},
        {"dirs",strings(bootstrap::getAdditionalDirectoriesForBatboxMd(state))},
        {"sameDirs",dirsSet?Json(bootstrap::getAdditionalDirectoriesForBatboxMd(
          state)==dirs):Json(nullptr)},
        {"allowlist",strings(bootstrap::getPoetryStash1hAllowlist(state))},
        {"sameAllowlist",allowlistSet?
          Json(bootstrap::getPoetryStash1hAllowlist(state)==allowlist):
          Json(nullptr)},
        {"eligible",optionalBool(bootstrap::getPoetryStash1hEligible(state))},
        {"afk",optionalBool(bootstrap::getAfkModeHeaderLatched(state))},
        {"fast",optionalBool(bootstrap::getFastModeHeaderLatched(state))},
        {"editing",optionalBool(bootstrap::getStashEditingHeaderLatched(state))},
        {"thinking",optionalBool(bootstrap::getThinkingClearLatched(state))}};
    };
    Json trace=Json::array();trace.push_back(snapshot());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="cache"){
        std::optional<std::string> value;
        if(!op["value"].is_null())value=op["value"].get<std::string>();
        bootstrap::setSystemPoetrySectionStashEntry(state,
          op["name"].get<std::string>(),value);
      }else if(kind=="clearCache")
        bootstrap::clearSystemPoetrySectionState(state);
      else if(kind=="date"){
        std::optional<std::string> value;
        if(!op["value"].is_null())value=op["value"].get<std::string>();
        bootstrap::setLastEmittedDate(state,value);
      }else if(kind=="dirs"){
        dirsSet=true;
        dirs=readStrings(op["value"]);
        bootstrap::setAdditionalDirectoriesForBatboxMd(state,dirs);
      }else if(kind=="appendDir")dirs->push_back(
        op["value"].get<std::string>());
      else if(kind=="allowlist"){
        allowlistSet=true;
        allowlist=readStrings(op["value"]);
        bootstrap::setPoetryStash1hAllowlist(state,allowlist);
      }else if(kind=="appendAllowlist")allowlist->push_back(
        op["value"].get<std::string>());
      else if(kind=="eligible"){
        std::optional<bool> value;
        if(!op["value"].is_null())value=op["value"].get<bool>();
        bootstrap::setPoetryStash1hEligible(state,value);
      }else if(kind=="afk")bootstrap::setAfkModeHeaderLatched(state,
        op["value"].get<bool>());
      else if(kind=="fast")bootstrap::setFastModeHeaderLatched(state,
        op["value"].get<bool>());
      else if(kind=="editing")
        bootstrap::setStashEditingHeaderLatched(state,op["value"].get<bool>());
      else if(kind=="thinking")bootstrap::setThinkingClearLatched(state,
        op["value"].get<bool>());
      else if(kind=="clearLatches")
        bootstrap::clearBetaHeaderLatches(state);
      else throw std::runtime_error("Unknown prompt-state operation");
      trace.push_back(snapshot());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
